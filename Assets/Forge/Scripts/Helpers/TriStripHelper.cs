using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

/// <summary>
/// Result produced by <see cref="TriStripHelper.BuildStrips"/>.
/// Contains one or more strips per material group, in ascending material index order.
/// A single material may produce multiple strips when its triangles are not all edge-connected.
/// </summary>
public class TriStripResult
{
    /// <summary>
    /// One or more strips per material group, in material index order.
    /// A single material may produce multiple strips when its triangles
    /// are not all edge-connected.
    /// </summary>
    public List<TriStrip> Strips = new List<TriStrip>();
}

/// <summary>
/// An ordered triangle strip ready for VIF packing.
/// Indices reference entries in the per-LOD vertex_info table.
/// </summary>
public class TriStrip
{
    /// <summary>Ordered vertex indices referencing vertex_info entries.</summary>
    public List<byte> Indices = new List<byte>();
    /// <summary>Material index into the materials list.</summary>
    public int MaterialIndex;
}

/// <summary>
/// Converts a triangle list into triangle strips, grouped by material index.
///
/// Ported from wrench's weave_tristrips algorithm (src/core/tristrip.cpp +
/// src/core/mesh_graph.cpp).  Key characteristics:
///
///   • Builds a half-edge MeshGraph tracking vertex→edges, edge→faces (up to 2),
///     and face→strip membership.  Faces sharing an edge with 3+ others are
///     flagged "evil" and emitted as single-triangle strips to avoid problems.
///
///   • For each remaining face, up to 10 candidate start-faces are tried (using
///     leaf-first selection: 0, then 1, then 2, then 3 available neighbours).
///
///   • For each start-face, all 6 orientations (3 edges × 2 vertex orderings)
///     are woven into candidate forward+backward strips.  The best strip is
///     chosen by utility = face_count − zero_area_tri_count × 2.5.
///
///   • When a directional extension hits a dead end it attempts a "swap": inserts
///     a zero-area (degenerate) bridge triangle and continues, producing longer
///     strips at a penalty to the utility score.
///
///   • The winning FaceStrip is then converted to a flat index list, reordering
///     the first face's vertices for correct winding continuity.
///
/// Binary-level concerns (VertexCountAndFlag, AdGifOffset, padding) are left
/// to the tfrag builder (Step 05).
/// </summary>
public static class TriStripHelper
{
    // =========================================================================
    // Internal graph / strip types
    // =========================================================================

    private const int Null = -1;

    // A face as recorded in a candidate strip.  index == Null means it is a
    // zero-area (degenerate) bridge triangle inserted during weaving.
    private struct StripFace
    {
        public int V0, V1, V2;
        public int Index; // original triangle index, or Null for zero-area

        public StripFace(int v0, int v1, int v2, int index)
        {
            V0 = v0; V1 = v1; V2 = v2; Index = index;
        }

        public bool IsZeroArea => V0 == V1 || V0 == V2 || V1 == V2;

        public bool ContainsVertex(int v) => V0 == v || V1 == v || V2 == v;
    }

    private struct FaceStrip
    {
        public int FaceBegin;
        public int FaceCount;
        public int ZeroAreaTriCount;
    }

    private class FaceStrips
    {
        public List<FaceStrip>  Strips = new List<FaceStrip>();
        public List<StripFace>  Faces  = new List<StripFace>();
    }

    // -------------------------------------------------------------------------
    // MeshGraph – half-edge adjacency structure
    // -------------------------------------------------------------------------

    private class MeshGraph
    {
        private struct VertexInfo { public List<int> Edges; }
        private struct EdgeInfo   { public int V0, V1, F0, F1; }

        private struct FaceInfo
        {
            public int V0, V1, V2;
            public int Material;
            public int StripIndex;    // -1 = not yet in any committed strip
            public bool InTempStrip;  // currently being considered
            public bool IsEvil;       // edge shared by 3+ faces
        }

        private readonly VertexInfo[] _vertices;
        private readonly List<EdgeInfo> _edges = new List<EdgeInfo>();
        private readonly FaceInfo[] _faces;

        public int FaceCount => _faces.Length;

        public MeshGraph(List<RenderedMeshData.Triangle> triangles, int vertexCount)
        {
            _vertices = new VertexInfo[vertexCount];
            for (int i = 0; i < vertexCount; i++)
                _vertices[i].Edges = new List<int>();

            _faces = new FaceInfo[triangles.Count];
            for (int i = 0; i < triangles.Count; i++)
            {
                ref FaceInfo fi = ref _faces[i];
                fi.V0 = triangles[i].V0;
                fi.V1 = triangles[i].V1;
                fi.V2 = triangles[i].V2;
                fi.Material   = triangles[i].MaterialIndex;
                fi.StripIndex = Null;
            }

            // Build edges and mark evil faces.
            for (int fi = 0; fi < _faces.Length; fi++)
            {
                ref FaceInfo face = ref _faces[fi];
                int[] verts = new int[] { face.V0, face.V1, face.V2 };

                for (int j = 0; j < 3; j++)
                {
                    int va = verts[j];
                    int vb = verts[(j + 1) % 3];
                    int lo = Math.Min(va, vb), hi = Math.Max(va, vb);

                    int edgeIdx = FindEdge(lo, hi);

                    if (edgeIdx == Null)
                    {
                        edgeIdx = _edges.Count;
                        _edges.Add(new EdgeInfo { V0 = lo, V1 = hi, F0 = fi, F1 = Null });
                        _vertices[lo].Edges.Add(edgeIdx);
                        _vertices[hi].Edges.Add(edgeIdx);
                    }
                    else
                    {
                        EdgeInfo e = _edges[edgeIdx];
                        if (e.F0 == Null)
                        {
                            e.F0 = fi; _edges[edgeIdx] = e;
                        }
                        else if (e.F1 == Null)
                        {
                            e.F1 = fi; _edges[edgeIdx] = e;
                        }
                        else
                        {
                            // Third face on this edge – unlink earlier faces and mark evil.
                            for (int k = j - 1; k >= 0; k--)
                            {
                                int va2 = verts[k];
                                int vb2 = verts[(k + 1) % 3];
                                int lo2 = Math.Min(va2, vb2), hi2 = Math.Max(va2, vb2);
                                int removeIdx = FindEdge(lo2, hi2);
                                if (removeIdx != Null)
                                {
                                    EdgeInfo re = _edges[removeIdx];
                                    if (re.F0 == fi) { re.F0 = Null; _edges[removeIdx] = re; }
                                    if (re.F1 == fi) { re.F1 = Null; _edges[removeIdx] = re; }
                                }
                            }
                            // Create remaining edge objects that weren't yet made.
                            for (int k = j + 1; k < 3; k++)
                            {
                                int va2 = verts[k];
                                int vb2 = verts[(k + 1) % 3];
                                int lo2 = Math.Min(va2, vb2), hi2 = Math.Max(va2, vb2);
                                if (FindEdge(lo2, hi2) == Null)
                                {
                                    int newIdx = _edges.Count;
                                    _edges.Add(new EdgeInfo { V0 = lo2, V1 = hi2, F0 = Null, F1 = Null });
                                    _vertices[lo2].Edges.Add(newIdx);
                                    _vertices[hi2].Edges.Add(newIdx);
                                }
                            }
                            face.IsEvil = true;
                            break;
                        }
                    }
                }
            }
        }

        // -- Queries ----------------------------------------------------------

        public bool FaceIsEvil(int face) => _faces[face].IsEvil;

        public void FaceVertices(int face, out int v0, out int v1, out int v2)
        {
            v0 = _faces[face].V0; v1 = _faces[face].V1; v2 = _faces[face].V2;
        }

        public int FaceMaterial(int face) => _faces[face].Material;

        /// <summary>Returns the canonical edge index for vertices (v0,v1) regardless of order, or Null.</summary>
        public int Edge(int v0, int v1)
        {
            int lo = Math.Min(v0, v1), hi = Math.Max(v0, v1);
            return FindEdge(lo, hi);
        }

        /// <summary>Returns the edge index for edge j of the given face (edge between v[j] and v[(j+1)%3]).</summary>
        public int EdgeOfFace(int face, int j)
        {
            int[] verts = FaceVertsArray(face);
            return Edge(verts[j], verts[(j + 1) % 3]);
        }

        public void EdgeVertices(int edge, out int v0, out int v1)
        { v0 = _edges[edge].V0; v1 = _edges[edge].V1; }

        /// <summary>Given an edge and one face on that edge, returns the other face (or Null).</summary>
        public int OtherFace(int edge, int face)
        {
            if (edge == Null) return Null;
            EdgeInfo e = _edges[edge];
            if (e.F0 == face) return e.F1;
            return e.F0;
        }

        /// <summary>Returns the vertex in the given face that is neither v0 nor v1, or Null.</summary>
        public int NextIndex(int v0, int v1, int face)
        {
            int[] verts = FaceVertsArray(face);
            foreach (int v in verts)
                if (v != v0 && v != v1) return v;
            return Null;
        }

        public bool IsInStrip(int face) => _faces[face].StripIndex != Null || _faces[face].InTempStrip;

        public bool CanBeAddedToStrip(int face, int material) =>
            _faces[face].Material == material && !IsInStrip(face);

        /// <summary>Count how many not-yet-stripped neighbours this face has for the given material.</summary>
        public int NeighbourCount(int face, int material)
        {
            int count = 0;
            for (int j = 0; j < 3; j++)
            {
                int e = EdgeOfFace(face, j);
                if (e == Null) continue;
                int other = OtherFace(e, face);
                if (other != Null && CanBeAddedToStrip(other, material))
                    count++;
            }
            return count;
        }

        public void PutInStrip(int face, int stripIndex) => _faces[face].StripIndex = stripIndex;
        public void PutInTempStrip(int face) => _faces[face].InTempStrip = true;
        public void DiscardTempStrip()
        {
            for (int i = 0; i < _faces.Length; i++)
                _faces[i].InTempStrip = false;
        }

        // -- Helpers ----------------------------------------------------------

        private int FindEdge(int lo, int hi)
        {
            foreach (int ei in _vertices[lo].Edges)
            {
                EdgeInfo e = _edges[ei];
                if (e.V0 == lo && e.V1 == hi) return ei;
            }
            return Null;
        }

        private int[] FaceVertsArray(int face) => new int[] { _faces[face].V0, _faces[face].V1, _faces[face].V2 };
    }

    // =========================================================================
    // Public entry point
    // =========================================================================

    /// <summary>
    /// Builds triangle strips from the given triangle list using wrench's
    /// weave_tristrips algorithm.
    /// </summary>
    /// <param name="triangles">Input triangles. Vertex indices must fit in a byte (0–255).</param>
    /// <param name="forceEvenStrips">
    /// When <c>true</c>, any strip whose vertex count is odd is padded to the next even
    /// length by duplicating its last index.  This inserts a zero-area degenerate quad at
    /// the tail of the strip.
    ///
    /// Tfrags require even-count strips because the hardware/microcode processes two
    /// vertices at a time to form quads; an odd-count strip leaves a dangling triangle
    /// that the VU program cannot pair, causing incorrect rendering.
    /// </param>
    /// <returns>A <see cref="TriStripResult"/> with strips in ascending material index order.</returns>
    public static TriStripResult BuildStrips(List<RenderedMeshData.Triangle> triangles, bool forceEvenStrips = false)
    {
        var result = new TriStripResult();
        if (triangles.Count == 0) return result;

        // Validate indices and determine vertex count.
        int vertexCount = 0;
        foreach (var t in triangles)
        {
            ValidateIndex(t.V0); ValidateIndex(t.V1); ValidateIndex(t.V2);
            vertexCount = Math.Max(vertexCount, Math.Max(t.V0, Math.Max(t.V1, t.V2)) + 1);
        }

        // Collect unique material indices in ascending order.
        var materialOrder = triangles
            .Select(t => t.MaterialIndex)
            .Distinct()
            .OrderBy(m => m)
            .ToList();

        var graph = new MeshGraph(triangles, vertexCount);

        // For each material group (ascending order), build strips.
        foreach (int material in materialOrder)
        {
            FaceStrips faceStrips = WeaveTristripsForMaterial(graph, material, triangles.Count);

            // Convert each FaceStrip to a TfragBuildStrip.
            foreach (FaceStrip fs in faceStrips.Strips)
            {
                List<int> indices = FacestripToTristrip(fs, faceStrips.Faces);
                result.Strips.Add(new TriStrip
                {
                    Indices       = indices.Select(i => checked((byte)i)).ToList(),
                    MaterialIndex = material
                });
            }
        }

        // If requested, pad any odd-length strip to an even length by duplicating the
        // last index.  The duplicated vertex produces a zero-area (degenerate) triangle
        // that pairs with the previously dangling triangle to form a degenerate quad.
        // The hardware/VU program skips zero-area quads without visible artefacts.
        if (forceEvenStrips)
        {
            foreach (var strip in result.Strips)
            {
                if (strip.Indices.Count % 2 != 0)
                    strip.Indices.Add(strip.Indices[strip.Indices.Count - 1]);
            }
        }

        return result;
    }

    // =========================================================================
    // Weave algorithm (ported from weave_tristrips / weave_multiple_strips_and_pick_the_best)
    // =========================================================================

    private static FaceStrips WeaveTristripsForMaterial(
        MeshGraph graph,
        int material,
        int totalFaceCount)
    {
        var output = new FaceStrips();

        for (;;)
        {
            FaceStrip? maybeStrip = WeaveMultipleStripsAndPickTheBest(output, graph, material, totalFaceCount);
            if (maybeStrip == null) break;

            FaceStrip strip = maybeStrip.Value;
            // Commit: mark all real faces as belonging to this strip.
            int stripIdx = output.Strips.Count;
            for (int i = 0; i < strip.FaceCount; i++)
            {
                StripFace sf = output.Faces[strip.FaceBegin + i];
                if (sf.Index != Null)
                    graph.PutInStrip(sf.Index, stripIdx);
            }
            output.Strips.Add(strip);
        }

        return output;
    }

    private static FaceStrip? WeaveMultipleStripsAndPickTheBest(
        FaceStrips dest,
        MeshGraph graph,
        int material,
        int totalFaceCount)
    {
        var temp = new FaceStrips();
        int[] nextFaces = new int[4]; // cursor per neighbour-count bucket
        int lastStartFace = Null;

        for (int attempt = 0; attempt < 10; attempt++)
        {
            int startFace = FindStartFace(graph, material, totalFaceCount, nextFaces);
            if (startFace == Null) return null;
            if (startFace == lastStartFace) break;

            // Evil face: emit as single triangle, no strip.
            if (graph.FaceIsEvil(startFace))
            {
                graph.FaceVertices(startFace, out int v0, out int v1, out int v2);
                var evilStrip = new FaceStrip
                {
                    FaceBegin = dest.Faces.Count,
                    FaceCount = 1
                };
                dest.Faces.Add(new StripFace(v0, v1, v2, startFace));
                return evilStrip;
            }

            lastStartFace = startFace;

            for (int j = 0; j < 3; j++)
            {
                int e = graph.EdgeOfFace(startFace, j);
                if (e == Null) continue;
                WeaveStrip(temp, startFace, e, false, graph, material);
                WeaveStrip(temp, startFace, e, true,  graph, material);
            }
        }

        if (temp.Strips.Count == 0) return null;

        // Pick the strip with the best utility score.
        int bestIdx = -1;
        float bestUtility = float.NegativeInfinity;
        for (int i = 0; i < temp.Strips.Count; i++)
        {
            float utility = temp.Strips[i].FaceCount - temp.Strips[i].ZeroAreaTriCount * 2.5f;
            if (utility > bestUtility) { bestUtility = utility; bestIdx = i; }
        }

        // Copy the best strip from temp into dest.
        FaceStrip best = temp.Strips[bestIdx];
        var result = new FaceStrip
        {
            FaceBegin       = dest.Faces.Count,
            FaceCount       = best.FaceCount,
            ZeroAreaTriCount = best.ZeroAreaTriCount
        };
        for (int i = 0; i < best.FaceCount; i++)
            dest.Faces.Add(temp.Faces[best.FaceBegin + i]);

        return result;
    }

    // -------------------------------------------------------------------------
    // FindStartFace – leaf-first (0 → 3 neighbours)
    // -------------------------------------------------------------------------

    private static int FindStartFace(
        MeshGraph graph,
        int material,
        int totalFaceCount,
        int[] nextFaces)
    {
        for (int targetNeighbours = 0; targetNeighbours <= 3; targetNeighbours++)
        {
            int face = nextFaces[targetNeighbours];
            int start = face;
            do
            {
                if (graph.CanBeAddedToStrip(face, material) &&
                    graph.NeighbourCount(face, material) == targetNeighbours)
                {
                    nextFaces[targetNeighbours] = (face + 1) % totalFaceCount;
                    return face;
                }
                face = (face + 1) % totalFaceCount;
            } while (face != start);
        }
        return Null;
    }

    // -------------------------------------------------------------------------
    // WeaveStrip – tries one edge orientation (forward + backward)
    // -------------------------------------------------------------------------

    private static void WeaveStrip(
        FaceStrips dest,
        int startFace,
        int startEdge,
        bool toV1,
        MeshGraph graph,
        int material)
    {
        graph.EdgeVertices(startEdge, out int ev0, out int ev1);
        int v0 = toV1 ? ev0 : ev1;
        int v1 = toV1 ? ev1 : ev0;
        int v2 = graph.NextIndex(v0, v1, startFace);
        if (v2 == Null) return;

        // Mark start face in temp strip so it isn't found by sub-weaves.
        graph.PutInTempStrip(startFace);

        var temp = new FaceStrips();

        FaceStrip forward  = WeaveStripInOneDirection(temp, startFace, v1, v2, graph, material);
        FaceStrip backward = WeaveStripInOneDirection(temp, startFace, v1, v0, graph, material);

        // Merge: backward (reversed) + start face + forward
        var merged = new FaceStrip
        {
            FaceBegin        = dest.Faces.Count,
            FaceCount        = backward.FaceCount + 1 + forward.FaceCount,
            ZeroAreaTriCount = backward.ZeroAreaTriCount + forward.ZeroAreaTriCount
        };
        for (int i = backward.FaceCount - 1; i >= 0; i--)
            dest.Faces.Add(temp.Faces[backward.FaceBegin + i]);
        dest.Faces.Add(new StripFace(v0, v1, v2, startFace));
        for (int i = 0; i < forward.FaceCount; i++)
            dest.Faces.Add(temp.Faces[forward.FaceBegin + i]);

        dest.Strips.Add(merged);

        // Discard temp so these faces can be considered again for other candidates.
        graph.DiscardTempStrip();
    }

    // -------------------------------------------------------------------------
    // WeaveStripInOneDirection – greedy extension with swap/bridge
    // -------------------------------------------------------------------------

    private static FaceStrip WeaveStripInOneDirection(
        FaceStrips dest,
        int startFace,
        int v1,
        int v2,
        MeshGraph graph,
        int material)
    {
        var strip = new FaceStrip { FaceBegin = dest.Faces.Count };

        int v0 = Null;
        int f0 = startFace;

        for (;;)
        {
            int e01 = graph.Edge(v1, v2);
            int f1 = e01 != Null ? graph.OtherFace(e01, f0) : Null;

            if (f1 == Null || !graph.CanBeAddedToStrip(f1, material))
            {
                // Attempt swap: insert a zero-area bridge triangle.
                if (v0 == Null) break;
                int e02 = graph.Edge(v0, v2);
                int f2 = e02 != Null ? graph.OtherFace(e02, f0) : Null;
                if (f2 == Null || !graph.CanBeAddedToStrip(f2, material)) break;

                // Replace the last appended face with a zero-area bridge (v0, v1, v0).
                dest.Faces[dest.Faces.Count - 1] = new StripFace(v0, v1, v0, Null);
                strip.ZeroAreaTriCount++;

                v2 = v0;
                v0 = Null;
                f1 = f0;
                // fall through — recompute v3 using the new (v1,v2=v0) edge
            }

            int v3 = graph.NextIndex(v1, v2, f1);
            if (v3 == Null) return strip;

            strip.FaceCount++;
            dest.Faces.Add(new StripFace(v1, v2, v3, f1));
            graph.PutInTempStrip(f1);

            v0 = v1; v1 = v2; v2 = v3;
            f0 = f1;
        }

        return strip;
    }

    // =========================================================================
    // FaceStrip → flat index list  (ported from facestrip_to_tristrip)
    // =========================================================================

    private static List<int> FacestripToTristrip(FaceStrip faceStrip, List<StripFace> faces)
    {
        var output = new List<int>();
        if (faceStrip.FaceCount == 0) return output;

        // Process first face – reorder its vertices so the strip continues correctly.
        StripFace first = faces[faceStrip.FaceBegin];

        if (faceStrip.FaceCount >= 2)
        {
            StripFace second = faces[faceStrip.FaceBegin + 1];
            int unique = UniqueVertexFromRhs(second, first);
            if (unique != Null)
            {
                if      (unique == first.V1) Swap(ref first.V0, ref first.V1);
                else if (unique == first.V2) Swap(ref first.V0, ref first.V2);
            }

            if (faceStrip.FaceCount >= 3)
            {
                StripFace third = faces[faceStrip.FaceBegin + 2];
                if (second.IsZeroArea)
                {
                    int pivot = second.V1;
                    if (first.V1 == pivot) Swap(ref first.V1, ref first.V2);
                }
                else
                {
                    GetSharedVertices(third, first, out int sh0, out int sh1);
                    if (sh0 != Null && sh1 == Null && sh0 == first.V1)
                        Swap(ref first.V1, ref first.V2);
                }
            }
        }

        output.Add(first.V0);
        output.Add(first.V1);
        output.Add(first.V2);

        StripFace last = first;
        for (int j = 1; j < faceStrip.FaceCount; j++)
        {
            StripFace face = faces[faceStrip.FaceBegin + j];
            int unique = UniqueVertexFromRhs(last, face);
            if (unique != Null)
            {
                output.Add(unique);
                last.V0 = last.V1;
                last.V1 = last.V2;
                last.V2 = unique;
            }
            else
            {
                // Zero-area bridge: emit last vertex of the new face.
                output.Add(face.V2);
                last = face;
            }
        }

        return output;
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    /// <summary>
    /// Returns the vertex in <paramref name="rhs"/> that is not present in
    /// <paramref name="lhs"/>, or Null if all three vertices are shared.
    /// </summary>
    private static int UniqueVertexFromRhs(StripFace lhs, StripFace rhs)
    {
        int[] rv = new int[] { rhs.V0, rhs.V1, rhs.V2 };
        foreach (int v in rv)
            if (v != lhs.V0 && v != lhs.V1 && v != lhs.V2)
                return v;
        return Null;
    }

    /// <summary>
    /// Returns the first vertex in <paramref name="rhs"/> that also appears in
    /// <paramref name="lhs"/> (up to two shared vertices; sh1 is Null when only
    /// one is found).
    /// </summary>
    private static void GetSharedVertices(StripFace lhs, StripFace rhs, out int sh0, out int sh1)
    {
        sh0 = Null; sh1 = Null;
        int[] rv = new int[] { rhs.V0, rhs.V1, rhs.V2 };
        foreach (int v in rv)
        {
            if (lhs.ContainsVertex(v))
            {
                if (sh0 == Null) sh0 = v;
                else { sh1 = v; return; }
            }
        }
    }

    private static void Swap(ref int a, ref int b) { int t = a; a = b; b = t; }

    private static void ValidateIndex(int index)
    {
        if ((uint)index > 255)
            throw new OverflowException(
                $"Vertex index {index} does not fit in a byte (0-255). " +
                "Ensure vertex_info has at most 256 entries per tfrag.");
    }
}
