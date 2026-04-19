using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// Geometry data for a single rendered mesh (or mesh chunk).
/// All vertex arrays (Positions, Normals, UVs, Colors) must be the same length.
/// Triangles reference indices into those arrays; Materials are referenced by triangle MaterialIndex.
/// </summary>
public class RenderedMeshData
{
	/// <summary>Three vertex indices and a material index describing one triangle.</summary>
	public struct Triangle
	{
		public int V0, V1, V2;
		public int MaterialIndex;
	}

	public struct MaterialDef
	{
		public string Name;
		public Texture2D Texture;
		public Color Tint;
	}

	/// <summary>Mesh name.</summary>
	public string Name;

	/// <summary>World-space vertex positions.</summary>
	public List<Vector3> Positions = new List<Vector3>();

	/// <summary>Per-vertex normals (same count as Positions).</summary>
	public List<Vector3> Normals = new List<Vector3>();

	/// <summary>Per-vertex UV coordinates (same count as Positions).</summary>
	public List<Vector2> UVs = new List<Vector2>();

	/// <summary>Per-vertex RGBA colors (same count as Positions).</summary>
	public List<Color32> Colors = new List<Color32>();

	/// <summary>
	/// Triangle indices into the Positions/Normals/UVs/Colors arrays.
	/// Each Triangle references 3 vertex indices + a material index.
	/// </summary>
	public List<Triangle> Triangles = new List<Triangle>();

	/// <summary>Material definitions. Referenced by triangle material indices.</summary>
	public List<MaterialDef> Materials = new List<MaterialDef>();

	// -----------------------------------------------------------------
	// Combine
	// -----------------------------------------------------------------

	/// <summary>
	/// Merges a list of <see cref="RenderedMeshData"/> instances into a single one.
	/// <para>
	/// Vertex arrays are concatenated with appropriate index offsets applied to each
	/// chunk's triangles.  Materials are de-duplicated by <see cref="MaterialDef.Texture"/>
	/// reference: if the same <see cref="Texture"/> object appears in multiple chunks it is
	/// stored only once in the combined <see cref="Materials"/> list and all triangle
	/// <c>MaterialIndex</c> values are remapped accordingly.
	/// </para>
	/// </summary>
	/// <param name="chunks">The list of chunks to combine. May be empty.</param>
	/// <returns>
	/// A new <see cref="RenderedMeshData"/> containing all geometry from every chunk,
	/// or an empty instance when <paramref name="chunks"/> is empty.
	/// </returns>
	public static RenderedMeshData Combine(IList<RenderedMeshData> chunks)
	{
		var result = new RenderedMeshData();
		if (chunks == null || chunks.Count == 0) return result;

		// Build a global de-duplicated material list (de-duped by Texture reference).
		var globalMaterials = new List<MaterialDef>();
		var textureToGlobal = new Dictionary<Texture, int>();

		int MapMaterial(MaterialDef mat)
		{
			if (mat.Texture == null)
			{
				// null textures are treated as distinct per-chunk slots (not merged).
				int nullIdx = globalMaterials.Count;
				globalMaterials.Add(mat);
				return nullIdx;
			}
			if (textureToGlobal.TryGetValue(mat.Texture, out int existing))
				return existing;
			int newIdx = globalMaterials.Count;
			textureToGlobal[mat.Texture] = newIdx;
			globalMaterials.Add(mat);
			return newIdx;
		}

		result.Name = $"{chunks[0].Name}_combined_{chunks.Count}";

		foreach (RenderedMeshData chunk in chunks)
		{
			// Build per-chunk material index → global material index mapping.
			var localToGlobal = new int[chunk.Materials.Count];
			for (int i = 0; i < chunk.Materials.Count; i++)
				localToGlobal[i] = MapMaterial(chunk.Materials[i]);

			int vertexOffset = result.Positions.Count;

			// Append vertex data.
			result.Positions.AddRange(chunk.Positions);
			result.Normals.AddRange(chunk.Normals);
			result.UVs.AddRange(chunk.UVs);
			result.Colors.AddRange(chunk.Colors);

			// Append triangles with remapped vertex and material indices.
			foreach (Triangle tri in chunk.Triangles)
			{
				result.Triangles.Add(new Triangle
				{
					V0 = tri.V0 + vertexOffset,
					V1 = tri.V1 + vertexOffset,
					V2 = tri.V2 + vertexOffset,
					MaterialIndex = tri.MaterialIndex < localToGlobal.Length
						? localToGlobal[tri.MaterialIndex]
						: tri.MaterialIndex
				});
			}
		}

		result.Materials = globalMaterials;
		return result;
	}

	// -----------------------------------------------------------------
	// Conversion helpers
	// -----------------------------------------------------------------

	/// <summary>
	/// Creates a <see cref="RenderedMeshData"/> from a Unity <see cref="Mesh"/> and an
	/// array of Unity materials (one per submesh).
	/// <para>
	/// Vertex positions, normals, UVs and colors are copied directly.
	/// If the mesh has no vertex colors, every vertex receives the default color
	/// <c>(128, 128, 128, 128)</c>.  Each submesh becomes a group of triangles sharing
	/// the same <c>MaterialIndex</c> (equal to the submesh index), and a
	/// <see cref="MaterialDef"/> is built from <c>material.mainTexture</c> and stored in
	/// <see cref="Materials"/>.
	/// </para>
	/// </summary>
	/// <param name="mesh">The source Unity mesh.</param>
	/// <param name="materials">
	/// Unity materials array — must have at least as many entries as <c>mesh.subMeshCount</c>.
	/// A <c>null</c> entry or a missing texture produces a default <see cref="MaterialDef"/> in
	/// <see cref="Materials"/>.
	/// </param>
	/// <returns>A new <see cref="RenderedMeshData"/> representing the full mesh.</returns>
	public static RenderedMeshData FromUnityMesh(Mesh mesh, UnityEngine.Material[] materials, Color? defaultVertexColor = null)
	{
		var data = new RenderedMeshData
		{
			Name = mesh.name
		};

		// Vertices
		data.Positions.AddRange(mesh.vertices);
		data.Normals.AddRange(mesh.normals);
		data.UVs.AddRange(mesh.uv);

		// Colors — default to opaque white when absent
		if (mesh.colors32 != null && mesh.colors32.Length == mesh.vertexCount)
		{
			data.Colors.AddRange(mesh.colors32);
		}
		else
		{
			for (int i = 0; i < mesh.vertexCount; i++)
				data.Colors.Add(defaultVertexColor ?? Color.white);
		}

		// Materials — one per submesh, sourced from the Unity material
		for (int sub = 0; sub < mesh.subMeshCount; sub++)
		{
			var matDef = new MaterialDef { Tint = Color.white };
			if (materials != null && sub < materials.Length && materials[sub] != null)
			{
				matDef.Name    = materials[sub].name;
				matDef.Texture = materials[sub].mainTexture as Texture2D;
				matDef.Tint    = materials[sub].color;
			}
			data.Materials.Add(matDef);
		}

		// Triangles — one MaterialIndex per submesh
		for (int sub = 0; sub < mesh.subMeshCount; sub++)
		{
			int[] indices = mesh.GetTriangles(sub);
			for (int i = 0; i < indices.Length; i += 3)
			{
				data.Triangles.Add(new Triangle
				{
					V0 = indices[i],
					V1 = indices[i + 1],
					V2 = indices[i + 2],
					MaterialIndex = sub
				});
			}
		}

		return data;
	}

	/// <summary>
	/// Converts this <see cref="RenderedMeshData"/> back into a Unity <see cref="Mesh"/>
	/// and a matching Unity <see cref="UnityEngine.Material"/> array.
	/// <para>
	/// One submesh is created per unique <c>MaterialIndex</c> found in
	/// <see cref="Triangles"/>, in ascending index order.  A new Unity
	/// <see cref="UnityEngine.Material"/> is created for each submesh with its
	/// <c>_MainTex</c> and <c>_Color</c> properties assigned from <see cref="Materials"/>.
	/// Pass <paramref name="shader"/> to control which shader the generated materials use;
	/// when <c>null</c> the <c>Horizon Forge/Universal</c> shader is used as a fallback.
	/// </para>
	/// </summary>
	/// <param name="shader">
	/// Shader to use for the generated materials, or <c>null</c> to use
	/// <c>Shader.Find("Horizon Forge/Universal")</c>.
	/// </param>
	/// <returns>
	/// A tuple of the new <see cref="Mesh"/> and the corresponding Unity
	/// <see cref="UnityEngine.Material"/> array (one entry per unique material index).
	/// </returns>
	public (Mesh mesh, UnityEngine.Material[] materials) ToUnityMesh(Shader shader = null)
	{
		// Resolve shader
		if (shader == null)
			shader = Shader.Find("Horizon Forge/Universal");

		// Build Unity Mesh
		var mesh = new Mesh();
		mesh.name = Name;
		mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;

		mesh.SetVertices(Positions);

		if (Normals.Count == Positions.Count)
			mesh.SetNormals(Normals);

		if (UVs.Count == Positions.Count)
			mesh.SetUVs(0, UVs);

		if (Colors.Count == Positions.Count)
			mesh.SetColors(Colors);

		// Collect unique material indices in ascending order
		var uniqueMaterialIndices = new SortedSet<int>();
		foreach (var tri in Triangles)
			uniqueMaterialIndices.Add(tri.MaterialIndex);

		// Map from MaterialIndex → submesh index
		var matIndexToSubmesh = new Dictionary<int, int>();
		int submeshIdx = 0;
		foreach (int matIdx in uniqueMaterialIndices)
			matIndexToSubmesh[matIdx] = submeshIdx++;

		// Build per-submesh triangle lists
		int submeshCount = matIndexToSubmesh.Count;
		var submeshTriangles = new List<int>[submeshCount];
		for (int i = 0; i < submeshCount; i++)
			submeshTriangles[i] = new List<int>();

		foreach (var tri in Triangles)
		{
			int si = matIndexToSubmesh[tri.MaterialIndex];
			submeshTriangles[si].Add(tri.V0);
			submeshTriangles[si].Add(tri.V1);
			submeshTriangles[si].Add(tri.V2);
		}

		mesh.subMeshCount = submeshCount;
		for (int i = 0; i < submeshCount; i++)
			mesh.SetTriangles(submeshTriangles[i], i);

		mesh.RecalculateBounds();

		// Build Unity materials from MaterialDef entries
		var materials = new UnityEngine.Material[submeshCount];
		foreach (var kvp in matIndexToSubmesh)
		{
			int matIdx = kvp.Key;
			int si = kvp.Value;

			var mat = new UnityEngine.Material(shader);
			if (matIdx < Materials.Count)
			{
				MaterialDef def = Materials[matIdx];
				if (def.Texture != null)
					mat.SetTexture("_MainTex", def.Texture);
				mat.color = def.Tint;
				mat.SetColor("_Color", def.Tint);
				if (!string.IsNullOrEmpty(def.Name))
					mat.name = def.Name;
			}

			materials[si] = mat;
		}

		return (mesh, materials);
	}
}
