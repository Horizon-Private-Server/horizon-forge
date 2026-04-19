using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public static class TfragWriter
{
    /// <summary>
    /// Serializes a single <see cref="Tfrag"/> into header bytes and a data blob.
    /// </summary>
    /// <param name="tfrag">Fully-populated tfrag to serialize.</param>
    /// <param name="game">Game variant — affects light offset field semantics.</param>
    /// <returns>A <see cref="TfragBuildResult"/> containing the 0x40-byte header and data blob.</returns>
    public static TfragBuildResult WriteSingleTfrag(TfragUnpack tfrag)
    {
        // Step 1: Allocate VU memory addresses.
        // preSentinelMap holds all addresses before absent-region sentinels (−1) are applied;
        // it is needed by BuildVuHeader to match wrench's behaviour of writing valid addresses
        // for absent optional regions (they point to the next contiguous region, not 0).
        TfragMemoryMap memoryMap = AllocateVuMemory(tfrag, out TfragMemoryMap preSentinelMap);

        // Step 2: Build VU header counts/addresses from the pre-sentinel memory map.
        tfrag.CommonVuHeader = BuildVuHeader(tfrag, preSentinelMap);

        // Step 2a: Recompute RgbaVertsLoc now that VU addresses are known.
        // The VU microprogram uses stcycl(wl=1, cl=2) when uploading positions, which means
        // each V3_16 position occupies two consecutive VU slots: the even slot holds X/Y/Z
        // and the odd slot (addr+1) is the fill slot where the VU program expects RGBA data.
        // Therefore rgba_verts_loc = PositionsCommonAddr + 1.
        tfrag.RgbaVertsLoc = (byte)(preSentinelMap.PositionsCommonAddr + 1);

        // Step 3: Build the TfragHeader and write all command list regions.
        var header = new TfragHeader();
        header.bSphere         = tfrag.Bsphere;
        header.lod_2_rgba_cnt  = tfrag.Lod2RgbaCount;
        header.lod_1_rgba_cnt  = tfrag.Lod1RgbaCount;
        header.lod_0_rgba_cnt  = tfrag.Lod0RgbaCount;
        header.base_only       = tfrag.BaseOnly != 0;
        header.rgba_verts_loc  = tfrag.RgbaVertsLoc;
        header.flags           = tfrag.Flags;
        header.dir_lights_one  = 0xff;          // always 0xFF per wrench
        header.point_lights    = 0xFFFF;      // always 0xFFFF per wrench
        header.occl_index      = tfrag.OcclIndex;
        header.mip_dist        = tfrag.MipDist;
        header.lod_2_ofs       = 0;

        // tex_cnt = number of texture primitives in common region
        header.tex_cnt = (byte)tfrag.CommonTextures.Count;

        // vert_cnt = total positions across all LODs
        header.vert_cnt = (byte)(
            tfrag.CommonPositions.Count +
            tfrag.Lod01Positions.Count +
            tfrag.Lod0Positions.Count);

        // tri_cnt = triangle count derived from LOD0 strips
        header.tri_cnt = (byte)CountTriangles(tfrag);

        byte[] dataBlob = WriteCommandLists(tfrag, ref header);

        // Step 4: Serialize TfragHeader → 0x40 bytes.
        byte[] headerBytes = SerializeTfragHeader(header);

        if (headerBytes.Length != 0x40)
            throw new InvalidOperationException(
                $"TfragWriter: serialized header is {headerBytes.Length} bytes, expected 0x40.");

        return new TfragBuildResult
        {
            HeaderBytes = headerBytes,
            DataBytes   = dataBlob,
            Tfrag       = tfrag
        };
    }

    // -------------------------------------------------------------------------
    // WriteCommandLists
    // -------------------------------------------------------------------------

    /// <summary>
    /// Writes all VIF command list regions into a single byte array and fills
    /// in the offset / size fields of <paramref name="header"/>.
    /// Ports wrench's <c>write_tfrag_command_lists</c>.
    /// </summary>
    private static byte[] WriteCommandLists(TfragUnpack tfrag, ref TfragHeader header)
    {
        // Pre-compute STROW values needed throughout.
        var indicesStrow = new VifHelper.Strow
        {
            R0 = tfrag.MemoryMap.VertexInfoCommonAddr,
            R1 = tfrag.MemoryMap.VertexInfoCommonAddr,
            R2 = tfrag.MemoryMap.VertexInfoCommonAddr,
            R3 = tfrag.MemoryMap.VertexInfoCommonAddr
        };
        var singleVertexInfoStrow = new VifHelper.Strow
        {
            R0 = 0x45000000,
            R1 = 0x45000000,
            R2 = 0,
            R3 = tfrag.MemoryMap.PositionsCommonAddr
        };
        var doubleVertexInfoStrow = new VifHelper.Strow
        {
            R0 = 0x45000000,
            R1 = 0x45000000,
            R2 = tfrag.MemoryMap.PositionsCommonAddr,
            R3 = tfrag.MemoryMap.PositionsCommonAddr
        };
		var swizzledBasePosition = new VifHelper.Strow
		{
			R0 = tfrag.BasePosition.R0,
			R1 = tfrag.BasePosition.R2,
			R2 = tfrag.BasePosition.R1,
			R3 = tfrag.BasePosition.R3	
		};

        using var vif = new VifDataWriter();

        // ----------------------------------------------------------------
        // LOD 2
        // (lod_2_ofs = 0, already set)
        // ----------------------------------------------------------------
        WriteStrow(vif, indicesStrow);
        vif.WriteStmod(1);   // stmod = replace mode on
        WriteUnpackBytes(vif, tfrag.Lod2Indices, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.IndicesAddr);
        vif.WriteStmod(0);   // stmod = replace mode off
        WriteUnpackStrips(vif, tfrag.Lod2Strips, tfrag.MemoryMap.StripsAddr);

        // ----------------------------------------------------------------
        // Common region
        // ----------------------------------------------------------------
        PadTo16(vif);
        int commonOfs = vif.Position;
        header.shared_ofs = (ushort)commonOfs;

        // VU header unpack (V4-16, UNSIGNED)
        WriteUnpackStruct(vif, SerializeTfragHeaderUnpack(tfrag.CommonVuHeader),
            VifHelper.VifVnVl.V4_16, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.HeaderCommonAddr);

        // tex_ofs points to the byte AFTER the next UNPACK command word (4 bytes ahead).
        // wrench: header.tex_ofs = dest.tell() + 4 - tfrag_ofs
        // We are about to write the UNPACK command word for textures (4 bytes), then payload.
        // So tex_ofs should be set to (current position + 4) i.e. start of texture payload.
        header.tex_ofs = (ushort)(vif.Position + 4);

        // Texture AD-GIFs unpack (V4-32, SIGNED)
        WriteUnpackTextures(vif, tfrag.CommonTextures, tfrag.MemoryMap.AdGifsCommonAddr);

        WriteStrow(vif, singleVertexInfoStrow);
        vif.WriteStmod(1);   // stmod = replace mode on

        // Common vertex info (V4-16, SIGNED)
        WriteUnpackVertexInfo(vif, tfrag.CommonVertexInfo, tfrag.MemoryMap.VertexInfoCommonAddr);

        // base_position STROW (the VU program reads this to get the base world position)
        WriteStrow(vif, swizzledBasePosition);

        vif.WriteStcycl(1, 2);   // stcycl wl=1, cl=2 → 0x01000102

        // For DL: record light_vert_start_off = current position + 4 (the UNPACK command word for positions)
        // if (game == Game.DL)
            header.light_vert_start_off = (ushort)(vif.Position + 4);

        // Common positions (V3-16, SIGNED)
        WriteUnpackPositions(vif, tfrag.CommonPositions, tfrag.MemoryMap.PositionsCommonAddr);

        vif.WriteStcycl(4, 4);   // stcycl wl=4, cl=4 → 0x01000404
        vif.WriteStmod(0);       // stmod = replace mode off

        // ----------------------------------------------------------------
        // LOD 1
        // ----------------------------------------------------------------
        PadTo16(vif);
        int lod1Ofs = vif.Position;
        header.lod_1_ofs = (ushort)lod1Ofs;

        WriteUnpackStrips(vif, tfrag.Lod1Strips, tfrag.MemoryMap.StripsAddr);
        WriteStrow(vif, indicesStrow);
        vif.WriteStmod(1);
        WriteUnpackBytes(vif, tfrag.Lod1Indices, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.IndicesAddr);

        // ----------------------------------------------------------------
        // LOD 01 (combined)
        // ----------------------------------------------------------------
        PadTo16(vif);
        int lod01Ofs = vif.Position;
        header.lod_0_ofs = (ushort)lod01Ofs;   // wrench uses lod_0_ofs for the LOD01 start

        bool lod01HasIndices = tfrag.Lod01ParentIndices.Count > 0 || tfrag.Lod01UnknownIndices2.Count > 0;
        if (lod01HasIndices)
            WriteStrow(vif, indicesStrow);

        bool lod01NeedsStmod =
            tfrag.Lod01ParentIndices.Count > 0     ||
            tfrag.Lod01UnknownIndices2.Count > 0   ||
            tfrag.Lod01VertexInfo.Count > 0        ||
            tfrag.Lod01Positions.Count > 0         ||
            tfrag.Lod0Positions.Count > 0;
        if (lod01NeedsStmod)
            vif.WriteStmod(1);

        if (tfrag.Lod01ParentIndices.Count > 0)
            WriteUnpackBytes(vif, tfrag.Lod01ParentIndices, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.ParentIndicesLod01Addr);

        if (tfrag.Lod01UnknownIndices2.Count > 0)
            WriteUnpackBytes(vif, tfrag.Lod01UnknownIndices2, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.UnkIndices2Lod01Addr);

        if (tfrag.Lod01VertexInfo.Count > 0)
            WriteStrow(vif, doubleVertexInfoStrow);

        if (tfrag.Lod01VertexInfo.Count > 0)
            WriteUnpackVertexInfo(vif, tfrag.Lod01VertexInfo, tfrag.MemoryMap.VertexInfoLod01Addr);

        // Always write base_position STROW and stcycl before LOD01 positions
        WriteStrow(vif, swizzledBasePosition);
        vif.WriteStcycl(1, 2);  // 0x01000102

        if (tfrag.Lod01Positions.Count > 0)
            WriteUnpackPositions(vif, tfrag.Lod01Positions, tfrag.MemoryMap.PositionsLod01Addr);

        // ----------------------------------------------------------------
        // LOD 0
        // ----------------------------------------------------------------
        PadTo16(vif);
        int lod0Ofs = vif.Position;

        if (tfrag.Lod0Positions.Count > 0)
            WriteUnpackPositions(vif, tfrag.Lod0Positions, tfrag.MemoryMap.PositionsLod0Addr);

        vif.WriteStmod(0);       // stmod off
        vif.WriteStcycl(4, 4);  // 0x01000404

        WriteUnpackStrips(vif, tfrag.Lod0Strips, tfrag.MemoryMap.StripsAddr);
        WriteStrow(vif, indicesStrow);
        vif.WriteStmod(1);
        WriteUnpackBytes(vif, tfrag.Lod0Indices, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.IndicesAddr);

        if (tfrag.Lod0ParentIndices.Count > 0)
            WriteUnpackBytes(vif, tfrag.Lod0ParentIndices, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.ParentIndicesLod0Addr);

        if (tfrag.Lod0UnknownIndices2.Count > 0)
            WriteUnpackBytes(vif, tfrag.Lod0UnknownIndices2, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.UNSIGNED, tfrag.MemoryMap.UnkIndices2Lod0Addr);

        if (tfrag.Lod0VertexInfo.Count > 0)
            WriteStrow(vif, doubleVertexInfoStrow);

        if (tfrag.Lod0VertexInfo.Count > 0)
            WriteUnpackVertexInfo(vif, tfrag.Lod0VertexInfo, tfrag.MemoryMap.VertexInfoLod0Addr);

        vif.WriteStmod(0);   // stmod off  (0x005000000 in wrench — same opcode, pad byte)

        // ----------------------------------------------------------------
        // End of VIF command lists — pad to 0x10.
        // ----------------------------------------------------------------
        PadTo16(vif);
        int endOfs = vif.Position;

        // Fill in VIF command list sizes (in QW = 16-byte units).
        header.common_size = (byte)((lod1Ofs  - commonOfs) / 0x10);
        header.lod_2_size  = (byte)((lod1Ofs  - 0        ) / 0x10);
        header.lod_1_size  = (byte)((lod0Ofs  - commonOfs) / 0x10);
        header.lod_0_size  = (byte)((endOfs   - lod01Ofs ) / 0x10);

        // ----------------------------------------------------------------
        // RGBA
        // ----------------------------------------------------------------
        using var data = new MemoryStream();
        using var dw   = new BinaryWriter(data);

        // Copy VIF command list data
        byte[] vifBytes = vif.ToArray();
        dw.Write(vifBytes);

        // Pad to 0x10
        PadStreamTo16(dw, data);
        header.rgba_ofs  = (ushort)data.Position;
        header.rgba_size = (byte)((tfrag.Rgbas.Count + 3) / 4);
        WriteRgba(dw, tfrag.Rgbas);

        // ----------------------------------------------------------------
        // Lights
        // ----------------------------------------------------------------
        PadStreamTo16(dw, data);
        header.light_ofs = (ushort)data.Position;
        WriteLights(dw, swizzledBasePosition, tfrag.Lights);

        // For non-DL games: light_vert_start_off is actually light_end_ofs (msphere_ofs).
        // We set it after writing mspheres below.

        // ----------------------------------------------------------------
        // Mspheres
        // ----------------------------------------------------------------
        PadStreamTo16(dw, data);
        header.msphere_ofs  = (ushort)data.Position;
        header.msphere_cnt  = (byte)tfrag.Msphere.Count;

        // if (game != Game.DL)
        //    header.light_vert_start_off = (ushort)data.Position; // light_end_ofs for non-DL

        WriteMspheres(dw, tfrag.Msphere);

        // ----------------------------------------------------------------
        // Cube
        // ----------------------------------------------------------------
        PadStreamTo16(dw, data);
        header.cube_ofs = (ushort)data.Position;

        // Derive center and half-extents from the bSphere for the cube data.
        // The cube is written as 8 corner TfragVec4i entries (×1024 quantized).
        // We reconstruct half-extents from mspheres if available, otherwise from bSphere.
        WriteCubeFromBsphere(dw, tfrag.Bsphere);

        dw.Flush();
        return data.ToArray();
    }

    // -------------------------------------------------------------------------
    // WriteRgba
    // -------------------------------------------------------------------------

    /// <summary>
    /// Writes each <see cref="Color32"/> entry — 4 bytes each.
    /// </summary>
    private static void WriteRgba(BinaryWriter writer, List<Color32> rgbas)
    {
        foreach (var rgba in rgbas)
        {
            writer.Write(rgba.r);
            writer.Write(rgba.g);
            writer.Write(rgba.b);
            writer.Write(rgba.a);
        }
    }

    // -------------------------------------------------------------------------
    // WriteLights
    // -------------------------------------------------------------------------

    /// <summary>
    /// Writes the base position STROW (16 bytes) followed by each <see cref="TfragLight"/> entry (8 bytes each).
    /// Ports wrench: dest.write(tfrag.base_position); dest.write_multiple(tfrag.lights);
    /// </summary>
    private static void WriteLights(BinaryWriter writer, VifHelper.Strow basePosition, List<TfragLight> lights)
    {
        // Base position as 4 × int32 (the VifHelper.Strow fields)
        writer.Write(basePosition.R0);
        writer.Write(basePosition.R1);
        writer.Write(basePosition.R2);
        writer.Write(basePosition.R3);

        foreach (var light in lights)
        {
            writer.Write(light.Unknown0);
            writer.Write(light.Intensity);
            writer.Write(light.Azimuth);
            writer.Write(light.Elevation);
            writer.Write(light.Color);
            writer.Write(light.Pad);
        }
    }

    // -------------------------------------------------------------------------
    // WriteMspheres
    // -------------------------------------------------------------------------

    /// <summary>
    /// Writes each bounding sphere entry as 16 bytes:
    /// f32 x, f32 y, f32 z (world-space centre, pre-scaled ×1024), s16 radius (×1024), s8 unk, s8 texIdx.
    /// </summary>
    private static void WriteMspheres(BinaryWriter writer, List<TfragMsphere> mspheres)
    {
        foreach (var ms in mspheres)
        {
            writer.Write(ms.X);
            writer.Write(ms.Z);
            writer.Write(ms.Y);
            writer.Write(ms.Radius);
            writer.Write(ms.Unk);
            writer.Write(ms.TexIdx);
        }
    }

    // -------------------------------------------------------------------------
    // WriteCube
    // -------------------------------------------------------------------------

    /// <summary>
    /// Writes the 8-corner cube data: 8 × 8 bytes = 0x40 bytes total.
    /// Each corner is <c>center ± halfExtents</c> per axis, quantized to <c>int16 × 1024</c>.
    /// The last 2 bytes of each 8-byte entry are padding (zero).
    /// </summary>
    private static void WriteCube(BinaryWriter writer, Vector3 center, Vector3 halfExtents)
    {
        // Corner axes matching wrench's CUBE_AXES and TfragHelper.CUBE_AXES
        ReadOnlySpan<(float sx, float sy, float sz)> axes = stackalloc (float, float, float)[]
        {
            ( 1,  1,  1),
            ( 1, -1,  1),
            (-1,  1,  1),
            (-1, -1,  1),
            ( 1,  1, -1),
            ( 1, -1, -1),
            (-1,  1, -1),
            (-1, -1, -1),
        };

        foreach (var (sx, sy, sz) in axes)
        {
            float cx = center.x + halfExtents.x * sx;
            float cy = center.y + halfExtents.y * sy;
            float cz = center.z + halfExtents.z * sz;

            writer.Write((short)MathF.Round(cx * 16f));
            writer.Write((short)MathF.Round(cz * 16f));
            writer.Write((short)MathF.Round(cy * 16f));
            writer.Write((short)0); // padding
        }
    }

    // -------------------------------------------------------------------------
    // AllocateVuMemory
    // -------------------------------------------------------------------------

    /// <summary>
    /// Computes VU memory addresses for each data region based on element counts.
    /// Ports wrench's <c>allocate_tfrags_vu</c>.
    /// Also writes the computed map back to <see cref="Tfrag.MemoryMap"/> for downstream use.
    /// </summary>
    /// <param name="preSentinelMap">
    /// Receives a copy of the memory map with all addresses computed sequentially
    /// <em>before</em> absent-region sentinels (−1) are applied.  This must be used
    /// when populating the VU header, because wrench writes VU header addresses from
    /// the pre-sentinel values so that absent optional regions still point to the
    /// start of the next contiguous region rather than 0.
    /// </param>
    /// <returns>The fully populated <see cref="TfragMemoryMap"/> (with sentinels applied).</returns>
    private static TfragMemoryMap AllocateVuMemory(TfragUnpack tfrag, out TfragMemoryMap preSentinelMap)
    {
        const int VU1_BUFFER_SIZE = 0x148;

        // Pad index arrays (must be multiples of 4 bytes for V4_8 UNPACK alignment).
        PadIndexArray(tfrag.Lod2Indices);
        PadIndexArray(tfrag.Lod1Indices);
        PadIndexArray(tfrag.Lod01ParentIndices);
        PadIndexArray(tfrag.Lod01UnknownIndices2);
        PadIndexArray(tfrag.Lod0Indices);
        PadIndexArray(tfrag.Lod0ParentIndices);
        PadIndexArray(tfrag.Lod0UnknownIndices2);

        // Calculate sizes in VU memory (in QW = 1 VU word = 1 slot each for V4_8/V4_16/V4_32,
        // but V3_16 positions take 2 slots per position since each position is 6 bytes → 2 × QW/2 → rounds up).
        // Wrench formula: positions_size = count * 2 (in VU qwords, V3_16 takes 2 qw per 4 elems = 1.5 per elem average, but wrench just uses count*2 in vu units)
        int headerCommonSize  = 5;  // TfragHeaderUnpack = 0x28 bytes = 5 × V4_16 slots
        int matrixSize        = 4;  // 4 QW reserved after header for matrix
        int adGifsCommonSize  = tfrag.CommonTextures.Count * (0x50 / 16);  // TfragTexturePrimitive = 0x50 bytes = 5 QW each
        // Each V3_16 position occupies 2 VU slots, and the count must be rounded up to
        // the next multiple of 4 so the RGBA fill slots align to a 4-entry boundary.
        // The original game always has positions_common_size = AlignUp(count, 4) * 2.
        int positionsCommonSize  = AlignUp(tfrag.CommonPositions.Count,  4) * 2;
        int positionsLod01Size   = AlignUp(tfrag.Lod01Positions.Count,   4) * 2;
        int positionsLod0Size    = AlignUp(tfrag.Lod0Positions.Count,    4) * 2;
        int vertexInfoCommonSize = tfrag.CommonVertexInfo.Count;   // each V4_16 = 8 bytes = 1 QW per 2 → 1 VU slot
        int vertexInfoLod01Size  = tfrag.Lod01VertexInfo.Count;
        int vertexInfoLod0Size   = tfrag.Lod0VertexInfo.Count;

        int parentIndicesLod01Size  = AlignUp(tfrag.Lod01ParentIndices.Count,  4) / 4;
        int unkIndices2Lod01Size    = AlignUp(tfrag.Lod01UnknownIndices2.Count,4) / 4;
        int parentIndicesLod0Size   = AlignUp(tfrag.Lod0ParentIndices.Count,   4) / 4;
        int unkIndices2Lod0Size     = AlignUp(tfrag.Lod0UnknownIndices2.Count, 4) / 4;

        int maxIndices = Math.Max(Math.Max(
            tfrag.Lod0Indices.Count,
            tfrag.Lod1Indices.Count),
            tfrag.Lod2Indices.Count);
        int indicesSize = AlignUp(maxIndices, 4) / 4;

        int maxStrips = Math.Max(Math.Max(
            tfrag.Lod0Strips.Count,
            tfrag.Lod1Strips.Count),
            tfrag.Lod2Strips.Count);
        int stripsSize = maxStrips;

        // Calculate addresses in VU memory.
        var m = new TfragMemoryMap();
        m.HeaderCommonAddr       = 0;
        m.AdGifsCommonAddr       = m.HeaderCommonAddr      + headerCommonSize + matrixSize;
        m.PositionsCommonAddr    = m.AdGifsCommonAddr       + adGifsCommonSize;
        m.PositionsLod01Addr     = m.PositionsCommonAddr    + positionsCommonSize;
        m.PositionsLod0Addr      = m.PositionsLod01Addr     + positionsLod01Size;
        m.VertexInfoCommonAddr   = m.PositionsLod0Addr      + positionsLod0Size + tfrag.PositionsSlack;
        m.VertexInfoLod01Addr    = m.VertexInfoCommonAddr   + vertexInfoCommonSize;
        m.VertexInfoLod0Addr     = m.VertexInfoLod01Addr    + vertexInfoLod01Size;
        m.ParentIndicesLod01Addr = m.VertexInfoLod0Addr     + vertexInfoLod0Size;
        m.UnkIndices2Lod01Addr   = m.ParentIndicesLod01Addr + parentIndicesLod01Size;
        m.ParentIndicesLod0Addr  = m.UnkIndices2Lod01Addr   + unkIndices2Lod01Size;
        m.UnkIndices2Lod0Addr    = m.ParentIndicesLod0Addr  + parentIndicesLod0Size;
        m.IndicesAddr            = m.UnkIndices2Lod0Addr    + unkIndices2Lod0Size;
        m.StripsAddr             = m.IndicesAddr             + indicesSize;

        int endAddr = m.StripsAddr + stripsSize;
        if (endAddr > VU1_BUFFER_SIZE)
            Debug.LogWarning(
                $"TfragWriter: VU memory map end 0x{endAddr:X} exceeds VU1 buffer size 0x{VU1_BUFFER_SIZE:X}.");

        // Save the pre-sentinel map for use by BuildVuHeader.
        // Wrench writes VU header addresses BEFORE applying sentinels, so absent optional
        // regions receive the address of the next contiguous region rather than 0.
        preSentinelMap = m;

        // Sentinel -1 for absent optional regions (used by WriteCommandLists to skip unpacks).
        if (positionsLod01Size     == 0) m.PositionsLod01Addr      = -1;
        if (positionsLod0Size      == 0) m.PositionsLod0Addr       = -1;
        if (vertexInfoLod01Size    == 0) m.VertexInfoLod01Addr     = -1;
        if (vertexInfoLod0Size     == 0) m.VertexInfoLod0Addr      = -1;
        if (parentIndicesLod01Size == 0) m.ParentIndicesLod01Addr  = -1;
        if (parentIndicesLod0Size  == 0) m.ParentIndicesLod0Addr   = -1;

        // Write the sentinel map back to the tfrag so downstream methods (WriteCommandLists) can read it.
        tfrag.MemoryMap = m;
        return m;
    }

    // -------------------------------------------------------------------------
    // BuildVuHeader
    // -------------------------------------------------------------------------

    /// <summary>
    /// Populates and returns a <see cref="TfragHeaderUnpack"/> struct with counts and addresses
    /// derived from <paramref name="memoryMap"/> and the tfrag's data lists.
    /// Ports wrench's address/count writes in <c>allocate_tfrags_vu</c>.
    /// </summary>
    private static TfragHeaderUnpack BuildVuHeader(TfragUnpack tfrag, TfragMemoryMap memoryMap)
    {
        TfragMemoryMap m = memoryMap;

        // Start from the existing VU header so unknown fields are preserved.
        TfragHeaderUnpack h = tfrag.CommonVuHeader;

        h.PositionsCommonCount   = (ushort)tfrag.CommonPositions.Count;
        h.PositionsLod01Count    = (ushort)tfrag.Lod01Positions.Count;
        h.PositionsLod0Count     = (ushort)tfrag.Lod0Positions.Count;

        // All addresses below come from the pre-sentinel map — no -1 values exist here.
        // Absent optional regions still receive valid addresses (they point to the next
        // contiguous region), which matches wrench's behaviour exactly.
        h.PositionsCommonAddr    = (ushort)m.PositionsCommonAddr;
        h.VertexInfoCommonAddr   = (ushort)m.VertexInfoCommonAddr;
        h.VertexInfoLod01Addr    = (ushort)m.VertexInfoLod01Addr;
        h.VertexInfoLod0Addr     = (ushort)m.VertexInfoLod0Addr;
        h.IndicesAddr            = (ushort)m.IndicesAddr;
        h.ParentIndicesLod01Addr = (ushort)m.ParentIndicesLod01Addr;
        h.UnkIndices2Lod01Addr   = (ushort)m.UnkIndices2Lod01Addr;
        h.ParentIndicesLod0Addr  = (ushort)m.ParentIndicesLod0Addr;
        h.UnkIndices2Lod0Addr    = (ushort)m.UnkIndices2Lod0Addr;
        h.StripsAddr             = (ushort)m.StripsAddr;
        h.TextureAdGifsAddr      = (ushort)m.AdGifsCommonAddr;

        // Unknown2/6/A are the texture counts per LOD tier.
        // Unknown10/14/18 encode the VU address boundary between position-linked
        // and "extra" (UV-seam duplicate) vertex_info entries within each LOD tier.
        // The VU microprogram uses these to decide whether to apply position-collapse
        // logic to a vertex_info entry.  Leaving them at zero causes the VU program
        // to treat ALL vertex_info entries as "extra", leading to incorrect
        // position lookups that shift vertices depending on the camera angle.
        h.Unknown2  = (ushort)tfrag.CommonTextures.Count;
        h.Unknown6  = 0;   // no LOD01-specific textures in single-LOD convention
        h.UnknownA  = 0;   // no LOD0-specific textures  in single-LOD convention
        h.Unknown10 = (ushort)(m.VertexInfoCommonAddr + tfrag.CommonPositions.Count);
        h.Unknown14 = (ushort)(m.VertexInfoLod01Addr  + tfrag.Lod01Positions.Count);
        h.Unknown18 = (ushort)(m.VertexInfoLod0Addr   + tfrag.Lod0Positions.Count);

        return h;
    }

    // -------------------------------------------------------------------------
    // VIF write helpers
    // -------------------------------------------------------------------------

    /// <summary>Writes a STROW command (0x30000000) + 4 × int32 payload via the <see cref="VifDataWriter"/>.</summary>
    private static void WriteStrow(VifDataWriter vif, VifHelper.Strow strow) =>
        vif.WriteStrow(strow);

    /// <summary>Writes an UNPACK V4_8 command for a raw byte list.</summary>
    private static void WriteUnpackBytes(VifDataWriter vif, List<byte> items, VifHelper.VifVnVl vnvl, VifHelper.VifUsn usn, int addr)
    {
        if (items.Count == 0) return;
        vif.WriteUnpack(items.ToArray(), vnvl, usn, addr);
    }

    /// <summary>Writes an UNPACK V4_8 command for a list of <see cref="TfragStrip"/> (4 bytes each, SIGNED).</summary>
    private static void WriteUnpackStrips(VifDataWriter vif, List<TfragStrip> strips, int addr)
    {
        if (strips.Count == 0) return;
        var bytes = SerializeStrips(strips);
        vif.WriteUnpack(bytes, VifHelper.VifVnVl.V4_8, VifHelper.VifUsn.SIGNED, addr);
    }

    /// <summary>Writes an UNPACK V4_32 command for a list of <see cref="TfragTexturePrimitive"/> (0x50 bytes each, SIGNED).</summary>
    private static void WriteUnpackTextures(VifDataWriter vif, List<TfragTexturePrimitive> textures, int addr)
    {
        if (textures.Count == 0) return;
        var bytes = SerializeTextures(textures);
        vif.WriteUnpack(bytes, VifHelper.VifVnVl.V4_32, VifHelper.VifUsn.SIGNED, addr);
    }

    /// <summary>Writes an UNPACK V4_16 command for a list of <see cref="TfragVertexInfo"/> (8 bytes each, SIGNED).</summary>
    private static void WriteUnpackVertexInfo(VifDataWriter vif, List<TfragVertexInfo> infos, int addr)
    {
        if (infos.Count == 0) return;
        var bytes = SerializeVertexInfo(infos);
        vif.WriteUnpack(bytes, VifHelper.VifVnVl.V4_16, VifHelper.VifUsn.SIGNED, addr);
    }

    /// <summary>Writes an UNPACK V3_16 command for a list of <see cref="TfragVertexPosition"/> (6 bytes each, SIGNED).</summary>
    private static void WriteUnpackPositions(VifDataWriter vif, List<TfragVertexPosition> positions, int addr)
    {
        if (positions.Count == 0) return;
        var bytes = SerializePositions(positions);
        vif.WriteUnpack(bytes, VifHelper.VifVnVl.V3_16, VifHelper.VifUsn.SIGNED, addr);
    }

    /// <summary>Writes an UNPACK command for an arbitrary struct byte payload.</summary>
    private static void WriteUnpackStruct(VifDataWriter vif, byte[] payload, VifHelper.VifVnVl vnvl, VifHelper.VifUsn usn, int addr)
    {
        if (payload.Length == 0) return;
        vif.WriteUnpack(payload, vnvl, usn, addr);
    }

    // -------------------------------------------------------------------------
    // Serialization helpers
    // -------------------------------------------------------------------------

    public static byte[] SerializeTfragHeader(TfragHeader header)
    {
        using var ms     = new MemoryStream(0x40);
        using var writer = new BinaryWriter(ms);
        writer.Write(header.bSphere.x * 1024f);
        writer.Write(header.bSphere.z * 1024f);
        writer.Write(header.bSphere.y * 1024f);
        writer.Write(header.bSphere.w * 1024f);
        writer.Write(header.pData);
        writer.Write(header.lod_2_ofs);
        writer.Write(header.shared_ofs);
        writer.Write(header.lod_1_ofs);
        writer.Write(header.lod_0_ofs);
        writer.Write(header.tex_ofs);
        writer.Write(header.rgba_ofs);
        writer.Write(header.common_size);
        writer.Write(header.lod_2_size);
        writer.Write(header.lod_1_size);
        writer.Write(header.lod_0_size);
        writer.Write(header.lod_2_rgba_cnt);
        writer.Write(header.lod_1_rgba_cnt);
        writer.Write(header.lod_0_rgba_cnt);
        writer.Write(header.base_only);
        writer.Write(header.tex_cnt);
        writer.Write(header.rgba_size);
        writer.Write(header.rgba_verts_loc);
        writer.Write(header.occl_index_stash);
        writer.Write(header.msphere_cnt);
        writer.Write(header.flags);
        writer.Write(header.msphere_ofs);
        writer.Write(header.light_ofs);
        writer.Write(header.light_vert_start_off);
        writer.Write(header.dir_lights_one);
        writer.Write(header.dir_lights_upd);
        writer.Write(header.point_lights);
        writer.Write(header.cube_ofs);
        writer.Write(header.occl_index);
        writer.Write(header.vert_cnt);
        writer.Write(header.tri_cnt);
        writer.Write(header.mip_dist);
        writer.Flush();
        return ms.ToArray();
    }

    private static byte[] SerializeTfragHeaderUnpack(TfragHeaderUnpack h)
    {
        using var ms     = new MemoryStream(0x28);
        using var writer = new BinaryWriter(ms);
        writer.Write(h.PositionsCommonCount);
        writer.Write(h.Unknown2);
        writer.Write(h.PositionsLod01Count);
        writer.Write(h.Unknown6);
        writer.Write(h.PositionsLod0Count);
        writer.Write(h.UnknownA);
        writer.Write(h.PositionsCommonAddr);
        writer.Write(h.VertexInfoCommonAddr);
        writer.Write(h.Unknown10);
        writer.Write(h.VertexInfoLod01Addr);
        writer.Write(h.Unknown14);
        writer.Write(h.VertexInfoLod0Addr);
        writer.Write(h.Unknown18);
        writer.Write(h.IndicesAddr);
        writer.Write(h.ParentIndicesLod01Addr);
        writer.Write(h.UnkIndices2Lod01Addr);
        writer.Write(h.ParentIndicesLod0Addr);
        writer.Write(h.UnkIndices2Lod0Addr);
        writer.Write(h.StripsAddr);
        writer.Write(h.TextureAdGifsAddr);
        writer.Flush();
        return ms.ToArray();
    }

    private static byte[] SerializeStrips(List<TfragStrip> strips)
    {
        using var ms     = new MemoryStream(strips.Count * 4);
        using var writer = new BinaryWriter(ms);
        foreach (var s in strips)
        {
            writer.Write(s.VertexCountAndFlag);
            writer.Write(s.EndOfPacketFlag);
            writer.Write(s.AdGifOffset);
            writer.Write(s.Pad);
        }
        writer.Flush();
        return ms.ToArray();
    }

    private static byte[] SerializeTextures(List<TfragTexturePrimitive> textures)
    {
        using var ms     = new MemoryStream(textures.Count * 0x50);
        using var writer = new BinaryWriter(ms);
        foreach (var tex in textures)
        {
            WriteGifAdData16(writer, tex.D1Tex01);
            WriteGifAdData16(writer, tex.D2Tex11);
            WriteGifAdData16(writer, tex.D3Clamp1);
            WriteGifAdData16(writer, tex.D4Miptbp11);
            WriteGifAdData16(writer, tex.D5Miptbp21);
        }
        writer.Flush();
        return ms.ToArray();
    }

    private static void WriteGifAdData16(BinaryWriter w, GifAdData16 d)
    {
        w.Write(d.DataLo);
        w.Write(d.DataHi);
        w.Write(d.Address);
        w.Write(d.Pad9);
        w.Write(d.PadA);
        w.Write(d.PadC);
    }

    private static byte[] SerializeVertexInfo(List<TfragVertexInfo> infos)
    {
        using var ms     = new MemoryStream(infos.Count * 8);
        using var writer = new BinaryWriter(ms);
        foreach (var vi in infos)
        {
            writer.Write(vi.S);
            writer.Write(vi.T);
            writer.Write(vi.Parent);
            writer.Write(vi.Vertex);
        }
        writer.Flush();
        return ms.ToArray();
    }

    private static byte[] SerializePositions(List<TfragVertexPosition> positions)
    {
        using var ms     = new MemoryStream(positions.Count * 6);
        using var writer = new BinaryWriter(ms);
        foreach (var p in positions)
        {
            writer.Write(p.X);
            writer.Write(p.Z);
            writer.Write(p.Y);
        }
        writer.Flush();
        return ms.ToArray();
    }

    // -------------------------------------------------------------------------
    // Cube helpers
    // -------------------------------------------------------------------------

    /// <summary>
    /// Derives cube center and half-extents from the bounding sphere and writes the cube data.
    /// For a minimal-valid tfrag the cube is a box centered on the bSphere centre with
    /// side length = bSphere radius (conservative AABB enclosing the sphere).
    /// </summary>
    private static void WriteCubeFromBsphere(BinaryWriter writer, Vector4 bsphere)
    {
        // bSphere is in X/Z/Y/W layout (swizzled in binary), but in-memory it is X/Y/Z/W.
        var center      = new Vector3(bsphere.x, bsphere.y, bsphere.z);
        var halfExtents = new Vector3(bsphere.w, bsphere.w, bsphere.w);
        WriteCube(writer, center, halfExtents);
    }

    // -------------------------------------------------------------------------
    // Triangle count
    // -------------------------------------------------------------------------

    /// <summary>
    /// Counts triangles from LOD0 strips, mirroring wrench's <c>count_triangles</c>.
    /// </summary>
    private static int CountTriangles(TfragUnpack tfrag)
    {
        int triangles = 0;
        foreach (var strip in tfrag.Lod0Strips)
        {
            int vertexCount = strip.VertexCountAndFlag;
            if (vertexCount <= 0)
            {
                if (vertexCount == 0)
                    break;
                vertexCount += 128;
            }
            triangles += vertexCount - 2;
        }
        return triangles;
    }

    // -------------------------------------------------------------------------
    // Padding helpers
    // -------------------------------------------------------------------------

    /// <summary>Pads the <see cref="VifDataWriter"/> position to the next 16-byte boundary.</summary>
    private static void PadTo16(VifDataWriter vif) => vif.Pad(0x10, 0);

    /// <summary>Pads a <see cref="BinaryWriter"/> backed by <paramref name="ms"/> to 16-byte alignment.</summary>
    private static void PadStreamTo16(BinaryWriter writer, MemoryStream ms)
    {
        int pos = (int)ms.Position;
        int rem = pos % 16;
        if (rem == 0) return;
        int pad = 16 - rem;
        for (int i = 0; i < pad; i++)
            writer.Write((byte)0);
    }

    // -------------------------------------------------------------------------
    // Index array padding
    // -------------------------------------------------------------------------

    /// <summary>
    /// Pads an index byte array to a 4-byte boundary (V4_8 UNPACK alignment).
    /// Mirrors wrench's <c>pad_index_array</c>.
    /// </summary>
    private static void PadIndexArray(List<byte> indices)
    {
        if (indices.Count % 2 != 0)
            indices.Add(0);
        if (indices.Count % 4 != 0)
        {
            indices.Add(0);
            indices.Add(0);
        }
    }

    // -------------------------------------------------------------------------
    // Math helpers
    // -------------------------------------------------------------------------

    private static int AlignUp(int value, int align) =>
        (value + align - 1) / align * align;

}

public struct TfragHeader
{
    public Vector4 bSphere;
    public uint pData;
    public ushort lod_2_ofs;
    public ushort shared_ofs;
    public ushort lod_1_ofs;
    public ushort lod_0_ofs;
    public ushort tex_ofs;
    public ushort rgba_ofs;
    public byte common_size;
    public byte lod_2_size;
    public byte lod_1_size;
    public byte lod_0_size;
    public byte lod_2_rgba_cnt;
    public byte lod_1_rgba_cnt;
    public byte lod_0_rgba_cnt;
    public bool base_only;
    public byte tex_cnt;
    public byte rgba_size;
    public byte rgba_verts_loc;
    public byte occl_index_stash;
    public byte msphere_cnt;
    public byte flags;
    public ushort msphere_ofs;
    public ushort light_ofs;
    public ushort light_vert_start_off;
    public byte dir_lights_one;
    public byte dir_lights_upd;
    public ushort point_lights;
    public ushort cube_ofs;
    public ushort occl_index;
    public byte vert_cnt;
    public byte tri_cnt;
    public ushort mip_dist;

    public void Write(BinaryWriter writer)
    {
        writer.Write(bSphere.x * 1024f);
        writer.Write(bSphere.z * 1024f);
        writer.Write(bSphere.y * 1024f);
        writer.Write(bSphere.w * 1024f);
        writer.Write(pData);
        writer.Write(lod_2_ofs);
        writer.Write(shared_ofs);
        writer.Write(lod_1_ofs);
        writer.Write(lod_0_ofs);
        writer.Write(tex_ofs);
        writer.Write(rgba_ofs);
        writer.Write(common_size);
        writer.Write(lod_2_size);
        writer.Write(lod_1_size);
        writer.Write(lod_0_size);
        writer.Write(lod_2_rgba_cnt);
        writer.Write(lod_1_rgba_cnt);
        writer.Write(lod_0_rgba_cnt);
        writer.Write(base_only);
        writer.Write(tex_cnt);
        writer.Write(rgba_size);
        writer.Write(rgba_verts_loc);
        writer.Write(occl_index_stash);
        writer.Write(msphere_cnt);
        writer.Write(flags);
        writer.Write(msphere_ofs);
        writer.Write(light_ofs);
        writer.Write(light_vert_start_off);
        writer.Write(dir_lights_one);
        writer.Write(dir_lights_upd);
        writer.Write(point_lights);
        writer.Write(cube_ofs);
        writer.Write(occl_index);
        writer.Write(vert_cnt);
        writer.Write(tri_cnt);
        writer.Write(mip_dist);
    }
}
