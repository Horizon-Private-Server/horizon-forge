using UnityEngine;
using System.Collections.Generic;

public static class MeshChunkerHelper
{
	/// <summary>Controls the chunking behavior. All fields have sensible defaults.</summary>
	public class MeshChunkOptions
	{
		/// <summary>Max triangles per chunk. Lower = simpler, more chunks.</summary>
		public int MaxTrianglesPerChunk = 80;
		/// <summary>Max vertices per chunk.</summary>
		public int MaxVerticesPerChunk = 200;
		/// <summary>Max materials per chunk.</summary>
		public int MaxMaterialsPerChunk = 20;
		/// <summary>Max half-extent per axis in world units for spatial cells.</summary>
		public float MaxHalfExtent = 31.0f;

		public static readonly MeshChunkOptions Tfrag = new()
		{
			MaxVerticesPerChunk = 16,
			MaxTrianglesPerChunk = 32,
			MaxMaterialsPerChunk = 5,
			MaxHalfExtent = 31.0f
		};
	}

	// -----------------------------------------------------------------
	// TriGroup — atomic unit for the spatial / complexity split phases.
	// A quad pair (two triangles sharing an edge) is kept together so
	// the downstream tristrip builder can produce efficient quad strips.
	// -----------------------------------------------------------------

	private struct TriGroup
	{
		/// <summary>Index of the first (or only) triangle in the group.</summary>
		public int Tri0;
		/// <summary>Index of the quad-partner triangle, or -1 for a lone triangle.</summary>
		public int Tri1;

		public bool IsQuad => Tri1 >= 0;

		/// <summary>Number of triangles represented by this group (1 or 2).</summary>
		public int TriCount => IsQuad ? 2 : 1;
	}

	// -----------------------------------------------------------------
	// Entry point
	// -----------------------------------------------------------------

	/// <summary>
	/// Takes one Unity Mesh and the shared materials list and returns a list of
	/// RenderedMeshData chunks, one per chunk.
	/// Pipeline: SpatialSplit → ComplexitySplit → MaterialSplit → BuildChunkInput.
	/// </summary>
	public static List<RenderedMeshData> ChunkObject(
		RenderedMeshData obj,
		List<RenderedMeshData.MaterialDef> globalMaterials,
		MeshChunkOptions options)
	{
		if (obj.Triangles.Count == 0)
		{
			Debug.LogWarning($"MeshChunker: object '{obj.Name}' has 0 triangles — skipping.");
			return new List<RenderedMeshData>();
		}

		// --- Phase 0: Subdivide oversized triangles ---
		// Any triangle whose per-axis vertex extent exceeds the spatial cell diameter
		// (MaxHalfExtent * 2) cannot fit within a spatial cell.  Split it along its
		// longest edge until every triangle fits.
		int trisBefore = obj.Triangles.Count;
		SubdivideOversizedTriangles(obj, options.MaxHalfExtent * 2f);
		int trisAdded = obj.Triangles.Count - trisBefore;
		if (trisAdded > 0)
			Debug.Log($"  [Phase 0] Subdivided oversized triangles: {trisBefore} → {obj.Triangles.Count} triangles (+{trisAdded})");

		// --- Phase 0b: Build quad groups ---
		// Pair up triangles that share an edge and the same material into quads.
		// Both triangles of a quad will always travel together through subsequent phases,
		// preserving quad topology for the tristrip builder.
		var allGroups = BuildQuadGroups(obj);

		// --- Phase 1: Spatial Split (group-aware) ---
		var spatialGroups = new List<List<TriGroup>>();
		SpatialSplitGroups(obj, allGroups, spatialGroups, options.MaxHalfExtent);

		// --- Phase 2: Complexity Split (group-aware, per spatial group) ---
		var complexityGroups = new List<List<TriGroup>>();
		foreach (var group in spatialGroups)
		{
			var sub = new List<List<TriGroup>>();
			ComplexitySplitGroups(obj, group, options, sub);
			complexityGroups.AddRange(sub);
		}

		// --- Phase 3 + 4: Texture Split then Build ---
		var result = new List<RenderedMeshData>();
		foreach (var group in complexityGroups)
		{
			// Expand TriGroups back to a flat triangle-index list for the existing phases.
			List<int> triangleIndices = ExpandGroups(group);

			var materialBatches = MaterialSplit(obj, triangleIndices, options);
			foreach (var batch in materialBatches)
				result.Add(BuildChunkInput(obj, batch, globalMaterials));
		}

		return result;
	}

	// -----------------------------------------------------------------
	// Phase 0b: Build quad groups
	// -----------------------------------------------------------------

	/// <summary>
	/// Greedily pairs triangles that form quads (same material, share exactly
	/// one edge, and together use exactly 4 unique vertex indices).
	/// Each pairing is greedy and each triangle may belong to at most one quad.
	/// Unpaired triangles become lone <see cref="TriGroup"/>s.
	/// </summary>
	private static List<TriGroup> BuildQuadGroups(RenderedMeshData obj)
	{
		int triCount = obj.Triangles.Count;
		var groups   = new List<TriGroup>(triCount);
		var paired   = new bool[triCount];

		// Build edge → list-of-triangle-indices adjacency.
		// Key: packed edge (lo vertex | hi vertex << 16).  For meshes with ≤65535 verts
		// this packing is collision-free; larger meshes fall back gracefully because
		// hash collisions merely prevent a pairing — they don't corrupt data.
		var edgeToTris = new Dictionary<long, List<int>>(triCount * 2);

		for (int ti = 0; ti < triCount; ti++)
		{
			RenderedMeshData.Triangle tri = obj.Triangles[ti];
			int[] verts = { tri.V0, tri.V1, tri.V2 };
			for (int j = 0; j < 3; j++)
			{
				long key = EdgeKey(verts[j], verts[(j + 1) % 3]);
				if (!edgeToTris.TryGetValue(key, out var list))
				{
					list = new List<int>(2);
					edgeToTris[key] = list;
				}
				list.Add(ti);
			}
		}

		// Greedy pairing pass.
		for (int ti = 0; ti < triCount; ti++)
		{
			if (paired[ti]) continue;

			RenderedMeshData.Triangle tri = obj.Triangles[ti];
			int[] verts = { tri.V0, tri.V1, tri.V2 };
			int  mat    = tri.MaterialIndex;
			int  partner = -1;

			// Try each edge of this triangle to find the best unmatched neighbour.
			// "Best" = the one that shares the longest edge with us, because in a
			// regular quad mesh the diagonal (the edge that splits the quad into two
			// triangles) is always longer than the boundary edges.  Preferring the
			// longest shared edge avoids the "rhombus" mis-pairing that occurs when
			// the greedy pass accidentally pairs one triangle from each of two
			// adjacent quads via their short boundary edge.
			float bestEdgeLenSq = -1f;

			for (int j = 0; j < 3; j++)
			{
				int vA = verts[j];
				int vB = verts[(j + 1) % 3];
				long key = EdgeKey(vA, vB);
				if (!edgeToTris.TryGetValue(key, out var neighbours)) continue;

				float edgeLenSq = (obj.Positions[vA] - obj.Positions[vB]).sqrMagnitude;

				foreach (int nb in neighbours)
				{
					if (nb == ti || paired[nb]) continue;
					if (obj.Triangles[nb].MaterialIndex != mat) continue;

					// Verify exactly 4 unique vertices (proper quad, not a degenerate).
					RenderedMeshData.Triangle nbTri = obj.Triangles[nb];
					var unique = new HashSet<int> { tri.V0, tri.V1, tri.V2, nbTri.V0, nbTri.V1, nbTri.V2 };
					if (unique.Count != 4) continue;

					// Keep the candidate with the longest shared edge.
					if (edgeLenSq > bestEdgeLenSq)
					{
						bestEdgeLenSq = edgeLenSq;
						partner = nb;
					}
				}
			}

			if (partner >= 0)
			{
				paired[ti]      = true;
				paired[partner] = true;
				groups.Add(new TriGroup { Tri0 = ti, Tri1 = partner });
			}
			else
			{
				groups.Add(new TriGroup { Tri0 = ti, Tri1 = -1 });
			}
		}

		return groups;
	}

	/// <summary>Returns a stable, order-independent key for an undirected edge (vA, vB).</summary>
	private static long EdgeKey(int vA, int vB)
	{
		int lo = vA < vB ? vA : vB;
		int hi = vA < vB ? vB : vA;
		return ((long)hi << 32) | (uint)lo;
	}

	// -----------------------------------------------------------------
	// Expand groups → triangle indices
	// -----------------------------------------------------------------

	private static List<int> ExpandGroups(List<TriGroup> groups)
	{
		var indices = new List<int>(groups.Count * 2);
		foreach (var g in groups)
		{
			indices.Add(g.Tri0);
			if (g.IsQuad) indices.Add(g.Tri1);
		}
		return indices;
	}

	// -----------------------------------------------------------------
	// Phase 1: Spatial Split (group-aware)
	// -----------------------------------------------------------------

	private static void SpatialSplitGroups(
		RenderedMeshData obj,
		List<TriGroup> groups,
		List<List<TriGroup>> results,
		float maxHalfExtent)
	{
		// Base case: the actual vertex AABB of this group fits within the spatial cell limit.
		var (aabbMin, aabbMax) = ComputeAABBGroups(obj, groups);
		Vector3 extents = aabbMax - aabbMin;

		if (extents.x <= maxHalfExtent * 2f &&
			extents.y <= maxHalfExtent * 2f &&
			extents.z <= maxHalfExtent * 2f)
		{
			results.Add(groups);
			return;
		}

		// Find longest axis of the actual vertex AABB.
		int axis = LongestAxis(extents);
		float aabbMid = axis == 0 ? (aabbMin.x + aabbMax.x) * 0.5f
					  : axis == 1 ? (aabbMin.y + aabbMax.y) * 0.5f
								  : (aabbMin.z + aabbMax.z) * 0.5f;

		// First attempt: partition by AABB midpoint using group centroid.
		var left  = new List<TriGroup>();
		var right = new List<TriGroup>();
		foreach (var g in groups)
		{
			Vector3 c  = ComputeGroupCentroid(obj, g);
			float   cv = axis == 0 ? c.x : axis == 1 ? c.y : c.z;
			if (cv <= aabbMid) left.Add(g);
			else               right.Add(g);
		}

		// Degenerate: all centroids are on one side — fall back to median centroid split.
		if (left.Count == 0 || right.Count == 0)
		{
			if (groups.Count < 2)
			{
				results.Add(groups);
				return;
			}

			var sorted = new List<(float cv, TriGroup g)>(groups.Count);
			foreach (var g in groups)
			{
				Vector3 c  = ComputeGroupCentroid(obj, g);
				float   cv = axis == 0 ? c.x : axis == 1 ? c.y : c.z;
				sorted.Add((cv, g));
			}
			sorted.Sort((a, b) => a.cv.CompareTo(b.cv));

			int half = sorted.Count / 2;
			left  = new List<TriGroup>(half);
			right = new List<TriGroup>(sorted.Count - half);
			for (int i = 0; i < half; i++)
				left.Add(sorted[i].g);
			for (int i = half; i < sorted.Count; i++)
				right.Add(sorted[i].g);
		}

		SpatialSplitGroups(obj, left,  results, maxHalfExtent);
		SpatialSplitGroups(obj, right, results, maxHalfExtent);
	}

	// -----------------------------------------------------------------
	// Phase 2: Complexity Split (group-aware)
	// -----------------------------------------------------------------

	private static void ComplexitySplitGroups(
		RenderedMeshData obj,
		List<TriGroup> groups,
		MeshChunkOptions options,
		List<List<TriGroup>> results)
	{
		// Base case: fits within limits.
		int triCount  = CountTrianglesInGroups(groups);
		int vertCount = CountUniqueVerticesInGroups(obj, groups);
		if (triCount  <= options.MaxTrianglesPerChunk &&
			vertCount <= options.MaxVerticesPerChunk)
		{
			results.Add(groups);
			return;
		}

		// Compute AABB and split along longest axis at median group centroid.
		var (aabbMin, aabbMax) = ComputeAABBGroups(obj, groups);
		Vector3 extents = aabbMax - aabbMin;
		int axis = LongestAxis(extents);

		var centroids = new List<(float value, TriGroup g)>(groups.Count);
		foreach (var g in groups)
		{
			Vector3 c  = ComputeGroupCentroid(obj, g);
			float   cv = axis == 0 ? c.x : axis == 1 ? c.y : c.z;
			centroids.Add((cv, g));
		}
		centroids.Sort((a, b) => a.value.CompareTo(b.value));
		float mid = centroids[centroids.Count / 2].value;

		var left  = new List<TriGroup>();
		var right = new List<TriGroup>();
		foreach (var g in groups)
		{
			Vector3 c  = ComputeGroupCentroid(obj, g);
			float   cv = axis == 0 ? c.x : axis == 1 ? c.y : c.z;
			if (cv <= mid) left.Add(g);
			else           right.Add(g);
		}

		// Degenerate: one side is empty (e.g. all centroids share the same coordinate,
		// which is common in flat/planar quad grids).  Fall back to a guaranteed 50/50
		// split using the already-sorted centroids list so we always make progress.
		if (left.Count == 0 || right.Count == 0)
		{
			// Single group — cannot split further; accept as-is even if oversized.
			if (groups.Count <= 1)
			{
				results.Add(groups);
				return;
			}

			int half = centroids.Count / 2;
			left  = new List<TriGroup>(half);
			right = new List<TriGroup>(centroids.Count - half);
			for (int i = 0; i < half; i++)
				left.Add(centroids[i].g);
			for (int i = half; i < centroids.Count; i++)
				right.Add(centroids[i].g);
		}

		ComplexitySplitGroups(obj, left,  options, results);
		ComplexitySplitGroups(obj, right, options, results);
	}

	// -----------------------------------------------------------------
	// Phase 3: Material Split  (unchanged — operates on triangle indices)
	// -----------------------------------------------------------------

	private static List<List<int>> MaterialSplit(
		RenderedMeshData obj,
		List<int> triangleIndices,
		MeshChunkOptions options)
	{
		// Collect unique material indices.
		var uniqueMats = new HashSet<int>();
		foreach (int ti in triangleIndices)
			uniqueMats.Add(obj.Triangles[ti].MaterialIndex);

		if (uniqueMats.Count <= options.MaxMaterialsPerChunk)
			return new List<List<int>> { triangleIndices };

		// Sort triangles by MaterialIndex.
		var sorted = new List<int>(triangleIndices);
		sorted.Sort((a, b) => obj.Triangles[a].MaterialIndex.CompareTo(obj.Triangles[b].MaterialIndex));

		// Greedily batch into groups of ≤ MaxMaterialsPerChunk unique materials.
		var batches = new List<List<int>>();
		var currentBatch = new List<int>();
		var currentMats = new HashSet<int>();

		foreach (int ti in sorted)
		{
			int mat = obj.Triangles[ti].MaterialIndex;
			if (!currentMats.Contains(mat) && currentMats.Count >= options.MaxMaterialsPerChunk)
			{
				batches.Add(currentBatch);
				currentBatch = new List<int>();
				currentMats = new HashSet<int>();
			}
			currentBatch.Add(ti);
			currentMats.Add(mat);
		}
		if (currentBatch.Count > 0)
			batches.Add(currentBatch);

		return batches;
	}

	// -----------------------------------------------------------------
	// Phase 4: Build Chunk Input  (unchanged)
	// -----------------------------------------------------------------

	private static RenderedMeshData BuildChunkInput(
		RenderedMeshData obj,
		List<int> triangleIndices,
		List<RenderedMeshData.MaterialDef> globalMaterials)
	{
		// --- Vertex compaction ---
		// Collect unique vertex indices in encounter order.
		var oldToNew = new Dictionary<int, int>();
		var newPositions = new List<Vector3>();
		var newNormals = new List<Vector3>();
		var newUVs = new List<Vector2>();
		var newColors = new List<Color32>();

		int MapVertex(int oldIdx)
		{
			if (oldToNew.TryGetValue(oldIdx, out int newIdx))
				return newIdx;
			newIdx = newPositions.Count;
			oldToNew[oldIdx] = newIdx;
			newPositions.Add(obj.Positions[oldIdx]);
			newNormals.Add(obj.Normals.Count > oldIdx ? obj.Normals[oldIdx] : Vector3.up);
			newUVs.Add(obj.UVs.Count > oldIdx ? obj.UVs[oldIdx] : Vector2.zero);
			newColors.Add(obj.Colors.Count > oldIdx ? obj.Colors[oldIdx] : new Color32(128, 128, 128, 128));
			return newIdx;
		}

		// --- Material compaction ---
		// Build globalMaterialIndex → localMaterialIndex mapping.
		var globalToLocal = new Dictionary<int, int>();
		var chunkMaterials = new List<RenderedMeshData.MaterialDef>();

		int MapMaterial(int globalIdx)
		{
			if (globalToLocal.TryGetValue(globalIdx, out int localIdx))
				return localIdx;
			localIdx = chunkMaterials.Count;
			globalToLocal[globalIdx] = localIdx;
			chunkMaterials.Add(globalIdx < globalMaterials.Count ? globalMaterials[globalIdx] : default);
			return localIdx;
		}

		// --- Remap triangles ---
		var newTriangles = new List<RenderedMeshData.Triangle>(triangleIndices.Count);
		foreach (int ti in triangleIndices)
		{
			RenderedMeshData.Triangle tri = obj.Triangles[ti];
			newTriangles.Add(new RenderedMeshData.Triangle
			{
				V0 = MapVertex(tri.V0),
				V1 = MapVertex(tri.V1),
				V2 = MapVertex(tri.V2),
				MaterialIndex = MapMaterial(tri.MaterialIndex)
			});
		}

		return new RenderedMeshData
		{
			Positions = newPositions,
			Normals = newNormals,
			UVs = newUVs,
			Colors = newColors,
			Triangles = newTriangles,
			Materials = chunkMaterials
		};
	}

	// -----------------------------------------------------------------
	// Phase 0: Subdivide oversized triangles
	// -----------------------------------------------------------------

	// Maximum bisection depth per original triangle to guard against degenerate geometry.
	private const int MaxSubdivisionDepth = 16;

	/// <summary>
	/// Iterates over every triangle in <paramref name="obj"/> and replaces any triangle
	/// whose per-axis vertex extent exceeds <paramref name="maxExtent"/> with two smaller
	/// triangles created by bisecting the longest edge.  Newly created vertices are appended
	/// to <paramref name="obj"/>'s vertex arrays and the triangle list is updated in place.
	/// Recursion is capped at <see cref="MaxSubdivisionDepth"/> levels per triangle.
	/// </summary>
	private static void SubdivideOversizedTriangles(RenderedMeshData obj, float maxExtent)
	{
		// Use a work-queue so we don't need actual recursion and can manage depth easily.
		// Each entry is (triangleIndex, depth).
		var queue = new Queue<(int triIdx, int depth)>();

		// Seed the queue with all existing triangles.
		for (int i = 0; i < obj.Triangles.Count; i++)
			queue.Enqueue((i, 0));

		while (queue.Count > 0)
		{
			var (triIdx, depth) = queue.Dequeue();
			RenderedMeshData.Triangle tri = obj.Triangles[triIdx];

			Vector3 p0 = obj.Positions[tri.V0];
			Vector3 p1 = obj.Positions[tri.V1];
			Vector3 p2 = obj.Positions[tri.V2];

			// Check per-axis extent of this triangle's AABB.
			Vector3 triMin = Vector3.Min(Vector3.Min(p0, p1), p2);
			Vector3 triMax = Vector3.Max(Vector3.Max(p0, p1), p2);
			Vector3 extent = triMax - triMin;

			if (extent.x <= maxExtent && extent.y <= maxExtent && extent.z <= maxExtent)
				continue; // Fits — no subdivision needed.

			if (depth >= MaxSubdivisionDepth)
				continue; // Safety cap: stop even if still oversized.

			// Find the longest edge.
			float d01 = (p0 - p1).sqrMagnitude;
			float d12 = (p1 - p2).sqrMagnitude;
			float d20 = (p2 - p0).sqrMagnitude;

			int vA, vB, vC; // vA–vB is the edge to bisect; vC is the opposite vertex.
			if (d01 >= d12 && d01 >= d20) { vA = tri.V0; vB = tri.V1; vC = tri.V2; }
			else if (d12 >= d20)          { vA = tri.V1; vB = tri.V2; vC = tri.V0; }
			else                          { vA = tri.V2; vB = tri.V0; vC = tri.V1; }

			// Interpolate a midpoint vertex at t=0.5 along the bisected edge.
			int midIdx = AddMidpointVertex(obj, vA, vB);

			// Replace the current triangle with the first new triangle (reuse slot).
			obj.Triangles[triIdx] = new RenderedMeshData.Triangle
			{
				V0 = vA, V1 = midIdx, V2 = vC,
				MaterialIndex = tri.MaterialIndex
			};

			// Append the second new triangle.
			int newTriIdx = obj.Triangles.Count;
			obj.Triangles.Add(new RenderedMeshData.Triangle
			{
				V0 = midIdx, V1 = vB, V2 = vC,
				MaterialIndex = tri.MaterialIndex
			});

			// Enqueue both for further inspection.
			queue.Enqueue((triIdx,    depth + 1));
			queue.Enqueue((newTriIdx, depth + 1));
		}
	}

	/// <summary>
	/// Creates a new vertex at the midpoint of vertices <paramref name="vA"/> and
	/// <paramref name="vB"/>, linearly interpolating all attribute arrays (Position,
	/// Normal, UV, Color).  Appends the new vertex to <paramref name="obj"/> and
	/// returns its index.
	/// </summary>
	private static int AddMidpointVertex(RenderedMeshData obj, int vA, int vB)
	{
		int newIdx = obj.Positions.Count;

		// Position: simple average.
		obj.Positions.Add((obj.Positions[vA] + obj.Positions[vB]) * 0.5f);

		// Normal: average then renormalize (guard against zero-length).
		if (obj.Normals.Count > 0)
		{
			Vector3 na = vA < obj.Normals.Count ? obj.Normals[vA] : Vector3.up;
			Vector3 nb = vB < obj.Normals.Count ? obj.Normals[vB] : Vector3.up;
			Vector3 avg = (na + nb) * 0.5f;
			float len = avg.magnitude;
			obj.Normals.Add(len > 1e-6f ? avg / len : Vector3.up);
		}

		// UV: simple average.
		if (obj.UVs.Count > 0)
		{
			Vector2 ua = vA < obj.UVs.Count ? obj.UVs[vA] : Vector2.zero;
			Vector2 ub = vB < obj.UVs.Count ? obj.UVs[vB] : Vector2.zero;
			obj.UVs.Add((ua + ub) * 0.5f);
		}

		// Color: component-wise average.
		if (obj.Colors.Count > 0)
		{
			Color32 ca = vA < obj.Colors.Count ? obj.Colors[vA] : new Color32(128, 128, 128, 128);
			Color32 cb = vB < obj.Colors.Count ? obj.Colors[vB] : new Color32(128, 128, 128, 128);
			obj.Colors.Add(new Color32(
				(byte)((ca.r + cb.r) / 2),
				(byte)((ca.g + cb.g) / 2),
				(byte)((ca.b + cb.b) / 2),
				(byte)((ca.a + cb.a) / 2)
			));
		}

		return newIdx;
	}

	// -----------------------------------------------------------------
	// Group-aware helper functions
	// -----------------------------------------------------------------

	/// <summary>
	/// Computes the centroid of a <see cref="TriGroup"/>.
	/// For a lone triangle this is the triangle's centroid.
	/// For a quad pair this is the average of all 4 unique vertex positions,
	/// which is the true geometric centre of the quad.
	/// </summary>
	private static Vector3 ComputeGroupCentroid(RenderedMeshData obj, TriGroup group)
	{
		RenderedMeshData.Triangle t0 = obj.Triangles[group.Tri0];
		if (!group.IsQuad)
			return (obj.Positions[t0.V0] + obj.Positions[t0.V1] + obj.Positions[t0.V2]) / 3f;

		// Quad: average of 4 unique vertex positions.
		RenderedMeshData.Triangle t1 = obj.Triangles[group.Tri1];
		var unique = new HashSet<int> { t0.V0, t0.V1, t0.V2, t1.V0, t1.V1, t1.V2 };
		Vector3 sum = Vector3.zero;
		foreach (int v in unique)
			sum += obj.Positions[v];
		return sum / unique.Count;
	}

	private static (Vector3 min, Vector3 max) ComputeAABBGroups(RenderedMeshData obj, List<TriGroup> groups)
	{
		Vector3 min = new Vector3(float.MaxValue,  float.MaxValue,  float.MaxValue);
		Vector3 max = new Vector3(float.MinValue, float.MinValue, float.MinValue);
		foreach (var g in groups)
		{
			RenderedMeshData.Triangle t0 = obj.Triangles[g.Tri0];
			Expand(ref min, ref max, obj.Positions[t0.V0]);
			Expand(ref min, ref max, obj.Positions[t0.V1]);
			Expand(ref min, ref max, obj.Positions[t0.V2]);
			if (g.IsQuad)
			{
				RenderedMeshData.Triangle t1 = obj.Triangles[g.Tri1];
				Expand(ref min, ref max, obj.Positions[t1.V0]);
				Expand(ref min, ref max, obj.Positions[t1.V1]);
				Expand(ref min, ref max, obj.Positions[t1.V2]);
			}
		}
		return (min, max);
	}

	private static int CountTrianglesInGroups(List<TriGroup> groups)
	{
		int count = 0;
		foreach (var g in groups)
			count += g.TriCount;
		return count;
	}

	private static int CountUniqueVerticesInGroups(RenderedMeshData obj, List<TriGroup> groups)
	{
		var seen = new HashSet<int>();
		foreach (var g in groups)
		{
			RenderedMeshData.Triangle t0 = obj.Triangles[g.Tri0];
			seen.Add(t0.V0); seen.Add(t0.V1); seen.Add(t0.V2);
			if (g.IsQuad)
			{
				RenderedMeshData.Triangle t1 = obj.Triangles[g.Tri1];
				seen.Add(t1.V0); seen.Add(t1.V1); seen.Add(t1.V2);
			}
		}
		return seen.Count;
	}

	// -----------------------------------------------------------------
	// Internal utilities  (unchanged)
	// -----------------------------------------------------------------

	private static Vector3 ComputeTriangleCentroid(RenderedMeshData obj, int triangleIndex)
	{
		RenderedMeshData.Triangle tri = obj.Triangles[triangleIndex];
		return (obj.Positions[tri.V0] + obj.Positions[tri.V1] + obj.Positions[tri.V2]) / 3f;
	}

	private static (Vector3 min, Vector3 max) ComputeAABB(RenderedMeshData obj, List<int> triangleIndices)
	{
		Vector3 min = new Vector3(float.MaxValue, float.MaxValue, float.MaxValue);
		Vector3 max = new Vector3(float.MinValue, float.MinValue, float.MinValue);
		foreach (int ti in triangleIndices)
		{
			RenderedMeshData.Triangle tri = obj.Triangles[ti];
			Expand(ref min, ref max, obj.Positions[tri.V0]);
			Expand(ref min, ref max, obj.Positions[tri.V1]);
			Expand(ref min, ref max, obj.Positions[tri.V2]);
		}
		return (min, max);
	}

	private static int CountUniqueVertices(RenderedMeshData obj, List<int> triangleIndices)
	{
		var seen = new HashSet<int>();
		foreach (int ti in triangleIndices)
		{
			RenderedMeshData.Triangle tri = obj.Triangles[ti];
			seen.Add(tri.V0);
			seen.Add(tri.V1);
			seen.Add(tri.V2);
		}
		return seen.Count;
	}

	private static void Expand(ref Vector3 min, ref Vector3 max, Vector3 p)
	{
		if (p.x < min.x) min.x = p.x;
		if (p.y < min.y) min.y = p.y;
		if (p.z < min.z) min.z = p.z;
		if (p.x > max.x) max.x = p.x;
		if (p.y > max.y) max.y = p.y;
		if (p.z > max.z) max.z = p.z;
	}

	/// <summary>Returns 0 for X, 1 for Y, 2 for Z — whichever component is largest.</summary>
	private static int LongestAxis(Vector3 extents)
	{
		if (extents.x >= extents.y && extents.x >= extents.z) return 0;
		if (extents.y >= extents.z) return 1;
		return 2;
	}
}
