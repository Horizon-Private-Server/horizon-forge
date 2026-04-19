using System;
using System.Collections.Generic;
using UnityEngine;

/// <summary>Output produced by the tfrag builder.</summary>
public class TfragBuildResult
{
    /// <summary>The 0x40-byte tfrag header.</summary>
    public byte[] HeaderBytes = new byte[0];
    /// <summary>The VIF data blob (all LOD regions + rgba + lights + mspheres + cube).</summary>
    public byte[] DataBytes = new byte[0];
    /// <summary>The populated Tfrag object (for inspection/testing).</summary>
    public TfragUnpack Tfrag = new();
    /// <summary>The chunk's source mesh data.</summary>
	public RenderedMeshData MeshData = new();
}

/// <summary>Material descriptor referenced by TfragBuildTriangle.MaterialIndex.</summary>
public struct TfragBuildMaterial
{
    /// <summary>Texture index (written to D1Tex01.DataLo).</summary>
    public int TextureIndex;
    /// <summary>Clamp S (written to D3Clamp1.DataLo bit 0-1).</summary>
    public bool ClampS;
    /// <summary>Clamp T (written to D3Clamp1.DataLo bit 2-3).</summary>
    public bool ClampT;
}

public static class TfragBuilder
{
    /// <summary>
    /// Builds a set of tfrag chunks from the provided <paramref name="mesh"/>.
    /// </summary>
    /// <param name="mesh">Mesh to build from.</param>
    /// <param name="materials">Materials corresponding to the mesh's submeshes.</param>
    /// <param name="chunkOptions">Chunking options controlling per-chunk limits.</param>
    /// <returns>A list of tfrag chunks.</returns>
    /// <exception cref="InvalidOperationException">Thrown when no valid tfrags are produced.</exception>
	public static List<TfragBuildResult> Build(Mesh mesh, Material[] materials, MeshChunkerHelper.MeshChunkOptions chunkOptions = null, float mipDistance = 16f)
	{
		var meshData = RenderedMeshData.FromUnityMesh(mesh, materials, defaultVertexColor: new Color32(0x80, 0x80, 0x80, 0x80));
		return Build(new List<RenderedMeshData>() { meshData }, chunkOptions ?? MeshChunkerHelper.MeshChunkOptions.Tfrag, mipDistance);
	}

    /// <summary>
    /// Builds a set of tfrag chunks from the provided <paramref name="inputs"/>.
    /// </summary>
    /// <param name="inputs">Pre-processed mesh data objects (positions, UVs, normals, colors, and textures) to build from.</param>
    /// <param name="chunkOptions">Chunking options controlling per-chunk limits.</param>
    /// <returns>A list of tfrag chunks.</returns>
    /// <exception cref="InvalidOperationException">Thrown when no valid tfrags are produced.</exception>
	public static List<TfragBuildResult> Build(List<RenderedMeshData> inputs, MeshChunkerHelper.MeshChunkOptions chunkOptions = null, float mipDistance = 16f)
	{
        var allResults = new List<TfragBuildResult>();

		foreach (var input in inputs)
		{
            List<RenderedMeshData> chunks = MeshChunkerHelper.ChunkObject(input, input.Materials, chunkOptions ?? MeshChunkerHelper.MeshChunkOptions.Tfrag);
			
			foreach (var chunk in chunks)
			{
				if (chunk.Triangles.Count == 0)
				{
                    Debug.LogWarning($"  [WARNING] Object '{input.Name}' chunk {chunks.IndexOf(chunk)} has 0 triangles — skipping.");
                    continue;
				}

                try
                {
                    TfragBuildResult result = BuildSingle(chunk, mipDistance);
                    allResults.Add(result);
                }
                catch (Exception ex)
                {
                    Debug.LogWarning(
                        $"  [ERROR] Failed to build tfrag for object '{input.Name}' chunk {chunks.IndexOf(chunk)}: {ex.Message}" +
                        $" (vertices={chunk.Positions.Count}, triangles={chunk.Triangles.Count}, materials={chunk.Materials.Count})");
                }
			}
		}

        if (allResults.Count == 0)
            throw new InvalidOperationException("No valid tfrags were produced");

		return allResults;
	}

    /// <summary>
    /// Converts raw geometry in <paramref name="input"/> into a binary tfrag.
    /// All geometry lives in the Common LOD region (single-LOD convention).
    /// </summary>
    /// <param name="input">Geometry to build from.</param>
    /// <param name="game">Game variant — passed through to <see cref="TfragWriter"/>.</param>
    /// <returns>A <see cref="TfragBuildResult"/> with <c>HeaderBytes</c>, <c>DataBytes</c>, and <c>Tfrag</c>.</returns>
    public static TfragBuildResult BuildSingle(RenderedMeshData input, float mipDistance)
    {
        // ------------------------------------------------------------------
        // 0. Convert input.Materials (List<MaterialDef>) → List<TfragBuildMaterial>.
        //    TextureIndex = local index within the chunk.
        //    ClampS/ClampT are read from the Unity texture's wrapModeU/wrapModeV.
        // ------------------------------------------------------------------
        var materials = new List<TfragBuildMaterial>(input.Materials.Count);
        for (int i = 0; i < input.Materials.Count; i++)
        {
            Texture tex = input.Materials[i].Texture;
            materials.Add(new TfragBuildMaterial
            {
                TextureIndex = i,
                ClampS = tex != null && tex.wrapModeU == TextureWrapMode.Clamp,
                ClampT = tex != null && tex.wrapModeV == TextureWrapMode.Clamp
            });
        }

        // ------------------------------------------------------------------
        // 1. Deduplicate positions → build unique position list.
        //    Map from input vertex index → position index.
        // ------------------------------------------------------------------
        var uniquePositions = new List<Vector3>();
        var vertexToPositionIndex = new int[input.Positions.Count];

        for (int i = 0; i < input.Positions.Count; i++)
        {
            Vector3 pos = input.Positions[i];
            int found = uniquePositions.IndexOf(pos);
            if (found < 0)
            {
                found = uniquePositions.Count;
                uniquePositions.Add(pos);
            }
            vertexToPositionIndex[i] = found;
        }

        // ------------------------------------------------------------------
        // 2. Compute base position (AABB midpoint quantized ×1024).
        // ------------------------------------------------------------------
        VifHelper.Strow basePosition = ComputeBasePosition(uniquePositions);

        // Decode base position back to world-space float for displacement math.
        var basePos = new Vector3(
            basePosition.R0 / 1024f,
            basePosition.R1 / 1024f,
            basePosition.R2 / 1024f);

        // ------------------------------------------------------------------
        // 3. Build TfragVertexPosition entries (displacements relative to base).
        // ------------------------------------------------------------------
        var commonPositions = new List<TfragVertexPosition>(uniquePositions.Count);
        foreach (Vector3 pos in uniquePositions)
        {
            Vector3 disp = pos - basePos;
            commonPositions.Add(new TfragVertexPosition
            {
                X = CheckedToInt16(disp.x * 1024f, "displacement X", $"vertex at ({pos.x}, {pos.y}, {pos.z}) relative to base ({basePos.x}, {basePos.y}, {basePos.z})"),
                Y = CheckedToInt16(disp.y * 1024f, "displacement Y", $"vertex at ({pos.x}, {pos.y}, {pos.z}) relative to base ({basePos.x}, {basePos.y}, {basePos.z})"),
                Z = CheckedToInt16(disp.z * 1024f, "displacement Z", $"vertex at ({pos.x}, {pos.y}, {pos.z}) relative to base ({basePos.x}, {basePos.y}, {basePos.z})")
            });
        }

        // ------------------------------------------------------------------
        // 4. Build TfragVertexInfo entries (one per input vertex).
        //    UV encoded as int16 (UV × 4096).
        //    Vertex = positionIndex × 2.
        //    Parent = 0x1000 (no LOD parent, single-LOD convention).
        // ------------------------------------------------------------------
        var commonVertexInfo = new List<TfragVertexInfo>(input.Positions.Count);
        for (int i = 0; i < input.Positions.Count; i++)
        {
            Vector2 uv = input.UVs[i];
            int vertexField = vertexToPositionIndex[i] * 2;
            if (vertexField < short.MinValue || vertexField > short.MaxValue)
                throw new OverflowException($"Tfrag vertex position index overflow: index {vertexToPositionIndex[i]} × 2 = {vertexField} is outside int16 range. Too many unique positions (max ~16383).");

			CheckedToInt16(uv.x * 4096f, "uv X", $"vertex at index {i} with uv ({uv.x}, {uv.y})");
			CheckedToInt16(uv.y * 4096f, "uv Y", $"vertex at index {i} with uv ({uv.x}, {uv.y})");
            commonVertexInfo.Add(new TfragVertexInfo
            {
                S      = EncodeUv(uv.x),
                T      = EncodeUv(1 - uv.y),
                Parent = 0x1000,
                Vertex = (short)vertexField
            });
        }

        // ------------------------------------------------------------------
        // 5. Build TfragTexturePrimitive entries (one per material).
        // ------------------------------------------------------------------
        var commonTextures = new List<TfragTexturePrimitive>(materials.Count);
        foreach (TfragBuildMaterial mat in materials)
            commonTextures.Add(BuildTexturePrimitive(mat, mipDistance));

        // ------------------------------------------------------------------
        // 6. Build triangle strips.
        // ------------------------------------------------------------------
		TriStripResult stripResult = TriStripHelper.BuildStrips(input.Triangles, forceEvenStrips: false);

        // Build the TfragStrip descriptors and shared indices list from the
        // TriStripResult. The strips share a single flat index list; each
        // TfragStrip records the vertex count for its slice.
        var sharedIndices = new List<byte>();
        var stripDescriptors = new List<TfragStrip>();

        // Gather strips in ascending material-index order (already guaranteed
        // by TriStripBuilder) and compute AdGifOffset per material change.
        int currentAdGifOffset = 0;
        int prevMaterialIndex  = -1;

        foreach (TriStrip buildStrip in stripResult.Strips)
        {
            int vertexCount = buildStrip.Indices.Count;
            bool newMaterial = buildStrip.MaterialIndex != prevMaterialIndex;

            if (newMaterial)
            {
                // Each new material group gets the AdGif offset into the
                // texture primitive table (in units of 5 QW = 0x50 bytes per primitive).
                // The VU program uses the AdGifOffset field of the strip to
                // look up which texture to use. The offset is the primitive
                // index × 5 (size of one TfragTexturePrimitive in QW).
                currentAdGifOffset = buildStrip.MaterialIndex * 5;
                prevMaterialIndex  = buildStrip.MaterialIndex;
            }

            // VertexCountAndFlag: negative means "use ADgif"; the VU program
            // treats the sign bit as a flag. Magnitude is vertexCount.
            // The convention from extracted tfrags:
            //   first strip of a material group → negative (-(128 - vertexCount))
            //   i.e. VertexCountAndFlag = -(128 - vertexCount) for flagged strips
            // Wrench uses: vertex_count_and_flag = -((sbyte)(128 - count)) for
            // strips that carry an AdGif. We replicate that here.
            //
            // For single-LOD all strips always reference an AdGif, so always negative.
            if (vertexCount > 127)
                throw new OverflowException($"Tfrag strip vertex count overflow: strip has {vertexCount} vertices, which exceeds the sbyte-encoded limit of 127.");
            sbyte vertexCountAndFlag = (sbyte)(-(128 - vertexCount));

            // Append this strip's indices to the shared list.
            int startIndexOffset = sharedIndices.Count;
            sharedIndices.AddRange(buildStrip.Indices);

            if (currentAdGifOffset < sbyte.MinValue || currentAdGifOffset > sbyte.MaxValue)
                throw new OverflowException($"Tfrag AdGif offset overflow: material index {buildStrip.MaterialIndex} produces AdGif offset {currentAdGifOffset}, which is outside sbyte range. Too many materials (max 25).");

            stripDescriptors.Add(new TfragStrip
            {
                VertexCountAndFlag = vertexCountAndFlag,
                EndOfPacketFlag    = 0,
                AdGifOffset        = (sbyte)currentAdGifOffset,
                Pad                = 0
            });
        }

        // Append terminator strip (VertexCountAndFlag = 0, EndOfPacketFlag = -1, AdGifOffset = -1).
        stripDescriptors.Add(new TfragStrip
        {
            VertexCountAndFlag = 0,
            EndOfPacketFlag    = -1,
            AdGifOffset        = -1,
            Pad                = -1
        });

        // ------------------------------------------------------------------
        // 7. Compute bounding sphere.
        // ------------------------------------------------------------------
        Vector4 bsphere = ComputeBsphere(uniquePositions);

        // ------------------------------------------------------------------
        // 8. Encode normals → TfragLight entries.
        //    One light per unique position (same deduplication as RGBAs).
        //    The number of lights must equal uniquePositions.Count so it
        //    matches header.vert_cnt and the RGBA table size (before padding).
        //    wrench reads: lights[header.vert_count] (i.e. one per position).
        // ------------------------------------------------------------------
        var perPosNormal   = new Vector3[uniquePositions.Count];
        var perPosNormalSet = new bool[uniquePositions.Count];
        var perPosColorForLights = new Color32[uniquePositions.Count];
        for (int i = 0; i < uniquePositions.Count; i++)
            perPosColorForLights[i] = new Color32(0x80, 0x80, 0x80, 0x80);

        for (int i = 0; i < input.Positions.Count; i++)
        {
            int posIdx = vertexToPositionIndex[i];
            if (!perPosNormalSet[posIdx])
            {
                if (i < input.Normals.Count)
                    perPosNormal[posIdx] = input.Normals[i];
                if (i < input.Colors.Count)
                    perPosColorForLights[posIdx] = input.Colors[i];
                perPosNormalSet[posIdx] = true;
            }
        }

        List<TfragLight> lights = EncodeNormals(
            new List<Vector3>(perPosNormal),
            new List<Color32>(perPosColorForLights));

        // ------------------------------------------------------------------
        // 9. Build TfragRgba list — one entry per unique position, padded up
        //    to the next multiple of 4.
        //    The VU microprogram interleaves positions and RGBAs in VU memory
        //    using stcycl(1,2): each position at addr N gets its RGBA at addr N+1.
        //    The positions region is allocated as AlignUp(count, 4)*2 VU slots, so
        //    the RGBA list must also be padded to AlignUp(count, 4) entries so
        //    the DMA uploads fill every reserved slot.
        // ------------------------------------------------------------------
        var rgbas = new List<Color32>(uniquePositions.Count);
        {
            // Allocate slots for each unique position, defaulting to opaque grey.
            var perPosColor = new Color32[uniquePositions.Count];
            var perPosSet   = new bool[uniquePositions.Count];
            for (int i = 0; i < uniquePositions.Count; i++)
                perPosColor[i] = new Color32(0x80, 0x80, 0x80, 0x80);

            // Fill from input: first vertex that maps to each position wins.
            for (int i = 0; i < input.Positions.Count; i++)
            {
                int posIdx = vertexToPositionIndex[i];
                if (!perPosSet[posIdx] && i < input.Colors.Count)
                {
                    perPosColor[posIdx] = input.Colors[i];
                    perPosSet[posIdx]   = true;
                }
            }
            rgbas.AddRange(perPosColor);

            // Pad to the next multiple of 4 with zero entries.
            // The VU position region is sized as AlignUp(count, 4)*2, so there
            // must be a matching number of RGBA entries to fill those fill slots.
            int paddedCount = ((uniquePositions.Count + 3) / 4) * 4;
            while (rgbas.Count < paddedCount)
                rgbas.Add(new Color32(0, 0, 0, 0));
        }

        // ------------------------------------------------------------------
        // 10. Compute mspheres — one per tristrip, in the same order as
        //     stripDescriptors (excluding the terminator entry).
        // ------------------------------------------------------------------
        var mspheres = ComputeMspheres(stripResult.Strips, uniquePositions, vertexToPositionIndex, materials);

        // ------------------------------------------------------------------
        // 11. Assemble the Tfrag object.
        // ------------------------------------------------------------------
        var tfrag = new TfragUnpack
        {
            Bsphere       = bsphere,
            BasePosition  = basePosition,

            // Single-LOD convention: LOD-specific regions are empty.
            // Common region carries all geometry.
            CommonPositions  = commonPositions,
            CommonVertexInfo = commonVertexInfo,
            CommonTextures   = commonTextures,

            // Lod2/Lod1/Lod0 share the same strips and indices (single-LOD).
            Lod2Strips  = new List<TfragStrip>(stripDescriptors),
            Lod1Strips  = new List<TfragStrip>(stripDescriptors),
            Lod0Strips  = new List<TfragStrip>(stripDescriptors),
            Lod2Indices = new List<byte>(sharedIndices),
            Lod1Indices = new List<byte>(sharedIndices),
            Lod0Indices = new List<byte>(sharedIndices),

            // LOD-specific positions / vertex info are empty (single-LOD).
            Lod01Positions  = new List<TfragVertexPosition>(),
            Lod0Positions   = new List<TfragVertexPosition>(),
            Lod01VertexInfo = new List<TfragVertexInfo>(),
            Lod0VertexInfo  = new List<TfragVertexInfo>(),

            // Parent/unknown index lists empty for single-LOD.
            Lod01ParentIndices   = new List<byte>(),
            Lod01UnknownIndices2 = new List<byte>(),
            Lod0ParentIndices    = new List<byte>(),
            Lod0UnknownIndices2  = new List<byte>(),

            // rgba counts: all LODs share the same color table.
            Lod2RgbaCount = CheckedToByte(rgbas.Count, "RGBA count (Lod2)", "too many unique vertex positions"),
            Lod1RgbaCount = CheckedToByte(rgbas.Count, "RGBA count (Lod1)", "too many unique vertex positions"),
            Lod0RgbaCount = CheckedToByte(rgbas.Count, "RGBA count (Lod0)", "too many unique vertex positions"),

            Rgbas    = rgbas,
            Lights   = lights,
            Msphere  = mspheres,

            // Standard flags for a single-LOD base-only tfrag.
            BaseOnly     = 1,
            Flags        = 1,
            MipDist      = 0x8000,

            // rgba_verts_loc: computed in TfragWriter.writeSingleTfrag after VU memory
            // allocation, as PositionsCommonAddr + 1 (the fill slot in the stcycl(1,2) layout).
            // Set to 0 here; TfragWriter overwrites it before writing the header.
            RgbaVertsLoc = 0,

            PositionsSlack = 0,
            OcclIndex    = 0,

            MemoryMap = TfragMemoryMap.CreateDefault()
        };

        // ------------------------------------------------------------------
        // 12. Serialize to binary.
        // ------------------------------------------------------------------
        var result = TfragWriter.WriteSingleTfrag(tfrag);
		result.MeshData = input;
		return result;
    }

    // -------------------------------------------------------------------------
    // ComputeBasePosition
    // -------------------------------------------------------------------------

    /// <summary>
    /// Computes the AABB midpoint of <paramref name="positions"/> and returns it
    /// as a <see cref="VifHelper.Strow"/> with components quantized by ×1024.
    /// This is the base position used as the VIF STROW row register for vertex decompression.
    /// </summary>
    private static VifHelper.Strow ComputeBasePosition(List<Vector3> positions)
    {
        if (positions.Count == 0)
            return new VifHelper.Strow { R0 = 0, R1 = 0, R2 = 0, R3 = 0 };

        Vector3 min = positions[0];
        Vector3 max = positions[0];

        foreach (Vector3 p in positions)
        {
            min = Vector3.Min(min, p);
            max = Vector3.Max(max, p);
        }

        Vector3 center = (min + max) * 0.5f;

        return new VifHelper.Strow
        {
            R0 = (int)MathF.Round(center.x * 1024f),
            R1 = (int)MathF.Round(center.y * 1024f),
            R2 = (int)MathF.Round(center.z * 1024f),
            R3 = 0
        };
    }

    // -------------------------------------------------------------------------
    // ComputeBsphere
    // -------------------------------------------------------------------------

    /// <summary>
    /// Computes a bounding sphere for <paramref name="positions"/> as
    /// <c>Vector4(cx, cy, cz, radius)</c>.
    /// Center is the AABB midpoint; radius is the maximum Euclidean distance
    /// from the center to any position.
    /// </summary>
    private static Vector4 ComputeBsphere(List<Vector3> positions)
    {
        if (positions.Count == 0)
            return Vector4.zero;

        Vector3 min = positions[0];
        Vector3 max = positions[0];

        foreach (Vector3 p in positions)
        {
            min = Vector3.Min(min, p);
            max = Vector3.Max(max, p);
        }

        Vector3 center = (min + max) * 0.5f;
        float   radius = 0f;

        foreach (Vector3 p in positions)
        {
            float dist = Vector3.Distance(center, p);
            if (dist > radius)
                radius = dist;
        }

        return new Vector4(center.x, center.y, center.z, radius);
    }

    // -------------------------------------------------------------------------
    // EncodeNormals
    // -------------------------------------------------------------------------

    /// <summary>
    /// Encodes each normal in <paramref name="normals"/> into a <see cref="TfragLight"/>
    /// using the same spherical azimuth/elevation packing as <c>TfragHelper.PackNormal</c>.
    /// One entry is produced per vertex (same count as <paramref name="normals"/>).
    /// The <paramref name="colors"/> list supplies the per-vertex RGBA packed into the Color field.
    /// </summary>
    private static List<TfragLight> EncodeNormals(List<Vector3> normals, List<Color32> colors)
    {
        var lights = new List<TfragLight>(normals.Count);

        for (int i = 0; i < normals.Count; i++)
        {
            Vector3 normal = normals[i];
            Color32 rgba = i < colors.Count ? colors[i] : new Color32(0x80, 0x80, 0x80, 0x80);

			float factor = Mathf.Clamp01(normal.magnitude);
			byte factor8 = (byte)(factor * 0x7F);
			byte vertIdx = 0;
			var normalNormalized = normal.normalized;

			// find closest base normal
			var azimuthIdx = (byte)(Mathf.Atan2(normalNormalized.z, normalNormalized.x) * (128f / Mathf.PI));
			var elevationIdx = (byte)(Mathf.Asin(normalNormalized.y) * (128f / Mathf.PI));

			// convert color to 16 bit
			ushort col16 = (ushort)((rgba.r >> 3) | ((rgba.g >> 3) << 5) | ((rgba.b >> 3) << 10) | ((rgba.a >> 7) << 15));
			
			lights.Add(new TfragLight
            {
                Unknown0  = (sbyte)0,
                Intensity = (sbyte)factor8,
                Azimuth   = (sbyte)azimuthIdx,
                Elevation = (sbyte)elevationIdx,
                Color     = (short)col16,
                Pad       = 0
            });

            // Unpack the 8-byte packed value into TfragLight fields.
            // Layout (from PackNormal):
            //   bits  0- 7: Unknown0 (vertIdx)
            //   bits  8-15: Intensity (factor8)
            //   bits 16-23: Azimuth
            //   bits 24-31: Elevation
            //   bits 32-47: Color (16-bit RGB5A1)
            //   bits 48-63: Pad (0)
            // lights.Add(new TfragLight
            // {
            //     Unknown0  = (sbyte)(packed & 0xFF),
            //     Intensity = (sbyte)((packed >>  8) & 0xFF),
            //     Azimuth   = (sbyte)((packed >> 16) & 0xFF),
            //     Elevation = (sbyte)((packed >> 24) & 0xFF),
            //     Color     = (short)((packed >> 32) & 0xFFFF),
            //     Pad       = 0
            // });
        }

        return lights;
    }

    // -------------------------------------------------------------------------
    // BuildTexturePrimitive
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // BuildTexturePrimitive
    // -------------------------------------------------------------------------

    /// <summary>
    /// Constructs a <see cref="TfragTexturePrimitive"/> from the given <paramref name="material"/>.
    /// All GIF A+D register addresses and default data values match observed extracted tfrags.
    /// </summary>
    // -------------------------------------------------------------------------
    // ComputeLodK
    // -------------------------------------------------------------------------

    /// <summary>
    /// Computes the GS TEX1_1 LOD K parameter from a mip distance value.
    /// Ported directly from wrench's <c>compute_lod_k()</c> in <c>shrub.cpp</c>.
    /// The K value controls which mipmap level the PS2 GS hardware selects at a
    /// given screen-space distance. It is packed into <c>D2Tex11.DataLo</c>.
    /// </summary>
    /// <param name="distance">
    /// The effective mip distance for this tfrag's material.
    /// Larger values cause lower-resolution mipmaps to be used sooner (farther away).
    /// Typical terrain values are in the range 16–128.
    /// </param>
    private static int ComputeLodK(float distance)
    {
        if (distance < 0.0001f) distance = 0.0001f;
        // Matches wrench: (s32)(u32)(u16) round(-log2(distance) * 16 - 73)
        // The cast chain zero-extends the signed s16 into a positive u32/s32 for storage.
        short k = (short)Mathf.Round(-(Mathf.Log(distance) / Mathf.Log(2.0f)) * 16f - 73f);
        return (int)(ushort)k;
    }

    /// <summary>
    /// Constructs a <see cref="TfragTexturePrimitive"/> from the given <paramref name="material"/>.
    /// All GIF A+D register addresses and default data values match observed extracted tfrags.
    /// </summary>
    private static TfragTexturePrimitive BuildTexturePrimitive(TfragBuildMaterial material, float mipDistance)
    {
        // D3Clamp1.DataLo: bit 0-1 = clamp S, bit 2-3 = clamp T.
        int clampLo = 0;
        if (material.ClampS) clampLo |= 0x3;   // bits 0-1
        if (material.ClampT) clampLo |= 0xC;   // bits 2-3

        // D3Clamp1.DataHi: 1 when either clamp is set (matches observed data).
        int clampHi = (material.ClampS || material.ClampT) ? 1 : 0;

        return new TfragTexturePrimitive
        {
            // D1 TEX0_1: texture index in DataLo; Address = 0x06 (GS_TEX0_1 register).
            D1Tex01 = new GifAdData16
            {
                DataLo  = material.TextureIndex,
                DataHi  = 0,
                Address = 0x06,
                Pad9    = 0,
                PadA    = 0,
                PadC    = 0
            },
            // D2 TEX1_1: default mipmap/filter settings; Address = 0x14 (GS_TEX1_1).
            // DataLo = 0xFF77 (observed default), DataHi = 4.
            D2Tex11 = new GifAdData16
            {
                DataLo  = ComputeLodK(mipDistance),
                DataHi  = 4,
                Address = 0x14,
                Pad9    = 0,
                PadA    = 0,
                PadC    = 0
            },
            // D3 CLAMP_1: texture wrapping; Address = 0x08 (GS_CLAMP_1).
            D3Clamp1 = new GifAdData16
            {
                DataLo  = clampLo,
                DataHi  = clampHi,
                Address = 0x08,
                Pad9    = 0,
                PadA    = 0,
                PadC    = 0
            },
            // D4 MIPTBP1_1: mip-table base pointer 1; Address = 0x34 (GS_MIPTBP1_1).
            D4Miptbp11 = new GifAdData16
            {
                DataLo  = 0,
                DataHi  = 0,
                Address = 0x34,
                Pad9    = 0,
                PadA    = 0,
                PadC    = 0
            },
            // D5 MIPTBP2_1: mip-table base pointer 2; Address = 0x36 (GS_MIPTBP2_1).
            D5Miptbp21 = new GifAdData16
            {
                DataLo  = 0,
                DataHi  = 0,
                Address = 0x36,
                Pad9    = 0,
                PadA    = 0,
                PadC    = 0
            }
        };
    }

    // -------------------------------------------------------------------------
    // ComputeMspheres
    // -------------------------------------------------------------------------

    /// <summary>
    /// Computes per-quad (triangle-pair) bounding spheres matching the original game's
    /// msphere granularity: one msphere per pair of consecutive triangles within each
    /// tristrip, plus an additional msphere for any trailing "hanging" triangle when a
    /// strip has an odd triangle count.
    ///
    /// The <c>TexIdx</c> on every msphere is the <em>global</em> texture index
    /// (<see cref="TfragBuildMaterial.TextureIndex"/>) of the strip's material.
    /// </summary>
    private static List<TfragMsphere> ComputeMspheres(
        List<TriStrip>    strips,
        List<Vector3>            uniquePositions,
        int[]                    vertexToPositionIndex,
        List<TfragBuildMaterial> materials)
    {
        var mspheres = new List<TfragMsphere>();

        foreach (TriStrip strip in strips)
        {
            int vertexCount  = strip.Indices.Count;
            int triangleCount = vertexCount - 2;

            if (triangleCount <= 0)
                continue;

            // Global texture index for this strip's material.
            sbyte globalTexIdx = (sbyte)(strip.MaterialIndex < materials.Count
                ? materials[strip.MaterialIndex].TextureIndex
                : strip.MaterialIndex);

            // Emit one msphere per pair of consecutive triangles (quad), plus one for
            // any remaining hanging triangle.  triBase is the index of the first
            // triangle in the pair (0-based within the strip's triangle list).
            //
            // Triangle i (0-based) uses vertices i, i+1, i+2 from the strip index list.
            // A "quad" consists of triangle triBase and triangle triBase+1:
            //   vertex indices triBase .. triBase+3  (4 vertices span both triangles).
            for (int triBase = 0; triBase < triangleCount; triBase += 2)
            {
                // Number of triangles in this group (1 for hanging, 2 for a full quad).
                int trisInGroup = Math.Min(2, triangleCount - triBase);

                // Vertex span for this group: vertices triBase .. triBase + trisInGroup + 1.
                int spanStart = triBase;
                int spanEnd   = triBase + trisInGroup + 1; // exclusive

                var groupPositions = new List<Vector3>(spanEnd - spanStart);
                for (int v = spanStart; v < spanEnd; v++)
                    groupPositions.Add(uniquePositions[vertexToPositionIndex[strip.Indices[v]]]);

                Vector4 sphere = ComputeBsphere(groupPositions);
                var center = new Vector3(sphere.x, sphere.y, sphere.z);

                // Msphere XYZ is stored as world-space floats pre-scaled by 1024.
                // Radius is stored as ushort ×1024.
                mspheres.Add(new TfragMsphere
                {
                    X      = center.x * 1024f,
                    Y      = center.y * 1024f,
                    Z      = center.z * 1024f,
                    Radius = (ushort)Math.Clamp((int)MathF.Round(sphere.w * 1024f), 0, ushort.MaxValue),
                    Unk    = -1,
                    TexIdx = globalTexIdx
                });
            }
        }

        return mspheres;
    }

    // -------------------------------------------------------------------------
    // UV encoding helper
    // -------------------------------------------------------------------------

    /// <summary>Encodes a single UV coordinate as <c>int16</c> (UV × 4096), clamped to short range.</summary>
    private static short EncodeUv(float uv)
    {
        float scaled = uv * 4096f;
        if (scaled > short.MaxValue) scaled = short.MaxValue;
        if (scaled < short.MinValue) scaled = short.MinValue;
		if (scaled < 0) scaled *= 2f; // negative uv encoded at double scale
        return (short)Mathf.Clamp(Mathf.Round(scaled), short.MinValue, short.MaxValue);
    }

    // -------------------------------------------------------------------------
    // Overflow-checking cast helpers
    // -------------------------------------------------------------------------

    /// <summary>
    /// Rounds <paramref name="value"/> and casts it to <c>short</c>, throwing an
    /// <see cref="OverflowException"/> if the result would be outside int16 range.
    /// </summary>
    /// <param name="value">The float value to round and cast.</param>
    /// <param name="fieldName">Human-readable name of the field (used in the error message).</param>
    /// <param name="context">Additional context about the source data (used in the error message).</param>
    private static short CheckedToInt16(float value, string fieldName, string context)
    {
        float rounded = MathF.Round(value);
        if (rounded < short.MinValue || rounded > short.MaxValue)
            throw new OverflowException(
                $"Tfrag {fieldName} overflowed int16 range: rounded value {rounded} (from {value}) is outside [{short.MinValue}, {short.MaxValue}]. Context: {context}");
        return (short)rounded;
    }

    /// <summary>
    /// Casts <paramref name="value"/> to <c>byte</c>, throwing an
    /// <see cref="OverflowException"/> if the result would be outside byte range.
    /// </summary>
    /// <param name="value">The integer value to cast.</param>
    /// <param name="fieldName">Human-readable name of the field (used in the error message).</param>
    /// <param name="context">Additional context about the source data (used in the error message).</param>
    private static byte CheckedToByte(int value, string fieldName, string context)
    {
        if (value < byte.MinValue || value > byte.MaxValue)
            throw new OverflowException(
                $"Tfrag {fieldName} overflowed byte range: value {value} is outside [{byte.MinValue}, {byte.MaxValue}]. Context: {context}");
        return (byte)value;
    }

}

public struct TfragVertexEx
{
    public Vector3 position;
    public Vector3 normal;
    public Color color;
    public Vector2 uv;
    public int parent;
    public int baseVertexIdx;
}

/// <summary>GIF A+D data entry — 0x10 bytes.</summary>
public struct GifAdData16
{
    public int DataLo;    // 0x00
    public int DataHi;    // 0x04
    public byte Address;  // 0x08
    public byte Pad9;     // 0x09
    public ushort PadA;   // 0x0A
    public uint PadC;     // 0x0C
}

/// <summary>Texture primitive sent via GIF A+D — 0x50 bytes (five GifAdData16 entries).</summary>
public struct TfragTexturePrimitive
{
    public GifAdData16 D1Tex01;
    public GifAdData16 D2Tex11;
    public GifAdData16 D3Clamp1;
    public GifAdData16 D4Miptbp11;
    public GifAdData16 D5Miptbp21;
}

/// <summary>Bounding sphere entry — 16 bytes (f32 x, f32 y, f32 z, s16 radius, s8 unk, s8 texIdx).</summary>
public struct TfragMsphere
{
    public float X, Y, Z;    // world-space centre (pre-scaled by 1024 on disk)
    public ushort Radius;    // radius × 1024
    public sbyte Unk;        // almost always -1
    public sbyte TexIdx;     // texture index
}

/// <summary>Directional light entry — 8 bytes.</summary>
public struct TfragLight
{
    public sbyte Unknown0;   // 0x00
    public sbyte Intensity;  // 0x01
    public sbyte Azimuth;    // 0x02
    public sbyte Elevation;  // 0x03
    public short Color;      // 0x04
    public short Pad;        // 0x06
}

/// <summary>VU header unpack data — 0x28 bytes (20 ushort fields).</summary>
public struct TfragHeaderUnpack
{
    public ushort PositionsCommonCount;    // 0x00
    public ushort Unknown2;               // 0x02
    public ushort PositionsLod01Count;    // 0x04
    public ushort Unknown6;               // 0x06
    public ushort PositionsLod0Count;     // 0x08
    public ushort UnknownA;               // 0x0A
    public ushort PositionsCommonAddr;    // 0x0C
    public ushort VertexInfoCommonAddr;   // 0x0E
    public ushort Unknown10;              // 0x10
    public ushort VertexInfoLod01Addr;    // 0x12
    public ushort Unknown14;              // 0x14
    public ushort VertexInfoLod0Addr;     // 0x16
    public ushort Unknown18;              // 0x18
    public ushort IndicesAddr;            // 0x1A
    public ushort ParentIndicesLod01Addr; // 0x1C
    public ushort UnkIndices2Lod01Addr;   // 0x1E
    public ushort ParentIndicesLod0Addr;  // 0x20
    public ushort UnkIndices2Lod0Addr;    // 0x22
    public ushort StripsAddr;             // 0x24
    public ushort TextureAdGifsAddr;      // 0x26
}

/// <summary>Packed vertex position — 6 bytes.</summary>
public struct TfragVertexPosition
{
    public short X, Y, Z;
}

/// <summary>Per-vertex UV and index info — 8 bytes.</summary>
public struct TfragVertexInfo
{
    public short S;       // UV S (fixed-point 12-bit)
    public short T;       // UV T (fixed-point 12-bit)
    public short Parent;  // parent vertex index
    public short Vertex;  // vertex index
}

/// <summary>Triangle strip descriptor — 4 bytes.</summary>
public struct TfragStrip
{
    public sbyte VertexCountAndFlag;  // sign bit = flag, magnitude = count
    public sbyte EndOfPacketFlag;
    public sbyte AdGifOffset;
    public sbyte Pad;
}

/// <summary>
/// VU memory address map for a single tfrag.
/// All fields default to -1 (sentinel for "not yet assigned").
/// </summary>
public struct TfragMemoryMap
{
    public int HeaderCommonAddr;
    public int AdGifsCommonAddr;
    public int PositionsCommonAddr;
    public int PositionsLod01Addr;
    public int PositionsLod0Addr;
    public int VertexInfoCommonAddr;
    public int VertexInfoLod01Addr;
    public int VertexInfoLod0Addr;
    public int ParentIndicesLod01Addr;
    public int UnkIndices2Lod01Addr;
    public int ParentIndicesLod0Addr;
    public int UnkIndices2Lod0Addr;
    public int IndicesAddr;
    public int StripsAddr;

    /// <summary>Returns a TfragMemoryMap with all address fields initialised to -1.</summary>
    public static TfragMemoryMap CreateDefault()
    {
        return new TfragMemoryMap
        {
            HeaderCommonAddr = -1,
            AdGifsCommonAddr = -1,
            PositionsCommonAddr = -1,
            PositionsLod01Addr = -1,
            PositionsLod0Addr = -1,
            VertexInfoCommonAddr = -1,
            VertexInfoLod01Addr = -1,
            VertexInfoLod0Addr = -1,
            ParentIndicesLod01Addr = -1,
            UnkIndices2Lod01Addr = -1,
            ParentIndicesLod0Addr = -1,
            UnkIndices2Lod0Addr = -1,
            IndicesAddr = -1,
            StripsAddr = -1
        };
    }
}

/// <summary>Full parsed tfrag with all LOD levels and associated data.</summary>
public class TfragUnpack
{
    public Vector4 Bsphere;

    // Counts and flags
    public byte Lod2RgbaCount;
    public byte Lod1RgbaCount;
    public byte Lod0RgbaCount;
    public byte BaseOnly;
    public byte RgbaVertsLoc;
    public byte Flags;
    public ushort OcclIndex;
    public ushort MipDist;

    // Base position from VIF STROW
    public VifHelper.Strow BasePosition;

    // Indices per LOD (byte lists)
    public List<byte> Lod2Indices = new();
    public List<byte> Lod1Indices = new();
    public List<byte> Lod0Indices = new();
    public List<byte> Lod01ParentIndices = new();
    public List<byte> Lod01UnknownIndices2 = new();
    public List<byte> Lod0ParentIndices = new();
    public List<byte> Lod0UnknownIndices2 = new();

    // Triangle strips per LOD
    public List<TfragStrip> Lod2Strips = new();
    public List<TfragStrip> Lod1Strips = new();
    public List<TfragStrip> Lod0Strips = new();

    // Common VU header
    public TfragHeaderUnpack CommonVuHeader;

    // Texture primitives
    public List<TfragTexturePrimitive> CommonTextures = new();

    // Vertex info per LOD
    public List<TfragVertexInfo> CommonVertexInfo = new();
    public List<TfragVertexInfo> Lod01VertexInfo = new();
    public List<TfragVertexInfo> Lod0VertexInfo = new();

    // Vertex positions per LOD
    public List<TfragVertexPosition> CommonPositions = new();
    public List<TfragVertexPosition> Lod01Positions = new();
    public List<TfragVertexPosition> Lod0Positions = new();

    // Colors
    public List<Color32> Rgbas = new();

    // Lighting/normals
    public List<TfragLight> Lights = new();

    // Bounding spheres
    public List<TfragMsphere> Msphere = new();

    // Memory map
    public TfragMemoryMap MemoryMap = TfragMemoryMap.CreateDefault();

    // Slack value for positions
    public ushort PositionsSlack;
}
