using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;

public class ConvertToTfrags : BaseAssetGenerator
{
    public static bool m_RenderGenerated = false;

    public bool m_FlatNormal = false;

    [Header("Vertex Colors")]
    [ColorUsage(showAlpha: false)] public Color m_Tint = Color.white;
    [Range(0f, 1f)] public float m_Shading = 0f;
    [Range(0f, 1f)] public float m_Noise = 0f;
    [Range(1f, 50f)] public float m_NoiseScale = 20f;

    private MeshRenderer[] m_MeshRenderers = new MeshRenderer[0];
	[SerializeField] private List<MaterialConfig> m_MaterialConfigs = new List<MaterialConfig>();
    [SerializeField, HideInInspector] private Hash128 m_LastGeneratedHash;

	
	[Serializable]
	class MaterialConfig
	{
		// Name used as Key (match by material name)
		[HideInInspector] public string Name;
		[ReadOnly] public Texture Texture;

		public TextureSize MaxTextureSize = TextureSize._128;
		public Color TintColor = Color.white;
		public bool CorrectForAlphaBloom = true; // divides alpha channel by 2
		public Texture2D TextureOverride;
	}

    #region Generate
    
    public void ValidateOrThrow()
    {
		m_MeshRenderers = GetRenderers();
        if (m_MeshRenderers.Length == 0) throw new Exception("ConvertToTfrags: Missing MeshRenderer(s).");
    }

    public void Regenerate()
    {
        m_LastGeneratedHash = default;
        Generate();
    }

    public override void Generate()
    {
		m_MeshRenderers = GetRenderers();

        var chunks = GetChunkInstances();
        var createdChunks = new List<TfragChunk>();
        var mapConfig = FindObjectOfType<MapConfig>();
        var occlusionDb = mapConfig.GetOcclusionDatabase();

        ValidateOrThrow();
		ValidateTextureConfigAgainstRenderers(m_MeshRenderers);

        try
        {
            // var textures = new List<Texture2D>();
            // var materials = new List<Material>();
			var builtChunks = new List<TfragBuildResult>();
            List<Vector3> allOctants = null;

            // check if we need to regenerate
            if (m_LastGeneratedHash.isValid && ComputeHash(m_MeshRenderers, m_MaterialConfigs) == m_LastGeneratedHash)
                return;

            // convert
			foreach (var renderer in m_MeshRenderers)
			{
				if (!renderer.gameObject.activeInHierarchy || renderer.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy))
					continue;

				var mf = renderer.GetComponent<MeshFilter>();
				if (!mf || !mf.sharedMesh)
					continue;

				// convert to mesh data
				var meshData = RenderedMeshData.FromUnityMesh(mf.sharedMesh, renderer.sharedMaterials, defaultVertexColor: new Color32(0x80, 0x80, 0x80, 0x80));
				
				// apply material config
				for (int i = 0; i < meshData.Materials.Count; ++i)
				{
					var material = meshData.Materials[i];
					var config = m_MaterialConfigs.FirstOrDefault(x => x.Name == material.Name);
					if (config == null)
						continue;

					var tint = config.TintColor;
					if (config.CorrectForAlphaBloom)
						tint.a *= 0.5f;


					// var texAssetPath = AssetDatabase.GetAssetPath(material.Texture);
					// if (!String.IsNullOrEmpty(texAssetPath))
					// {
					// 	var maxDimension = (int)Mathf.Pow(2, 5 + (int)config.MaxTextureSize);
					// 	var importer = TextureImporter.GetAtPath(texAssetPath) as TextureImporter;
					// 	if (importer)
					// 		importer.maxTextureSize = maxDimension;
					// }

					var maxDimension = (int)Mathf.Pow(2, 5 + (int)config.MaxTextureSize);
					var texture = config.TextureOverride;
					if (!texture) texture = material.Texture;
					if (!texture) texture = UnityHelper.DefaultTexture;
					material.Texture = UnityHelper.CloneTexture(texture, maxDimension, hasAlpha: true, tint);
					material.Tint = Color.white;
					meshData.Materials[i] = material;
				}

				// flatten normal
				if (m_FlatNormal)
				{
					for (int i = 0; i < meshData.Normals.Count; ++i)
					{
						meshData.Normals[i] = Vector3.up;
					}
				}

				// apply vertex color filter
				for (int i = 0; i < meshData.Colors.Count; ++i)
				{
					var vertexColor = (Color)meshData.Colors[i];
				
					// calculate color
					var vertexWorldSpace = renderer.transform.localToWorldMatrix.MultiplyPoint(meshData.Positions[i]);
					var shading = Mathf.Pow(Mathf.Clamp01(Mathf.Abs(Vector3.Dot(meshData.Normals[i], Vector3.up))), m_Shading * 10);
					var noise = Mathf.Pow(Mathf.Clamp01(Mathf.PerlinNoise(vertexWorldSpace.x / m_NoiseScale, vertexWorldSpace.z / m_NoiseScale)), m_Noise * 3f);
					var alpha = vertexColor.a;
					var color = vertexColor * m_Tint * 1 * shading * noise;
					color.a = alpha;
					meshData.Colors[i] = color;
				}
				
				// mip distance has to be 16 to render correctly
				var rendererBuiltChunks = TfragBuilder.Build(new List<RenderedMeshData>() { meshData }, mipDistance: 16f);
				var chunkIndex = builtChunks.Count;
				builtChunks.AddRange(rendererBuiltChunks);

				// update tfrag chunks
				foreach (var builtChunk in rendererBuiltChunks)
				{
					var builtMeshData = builtChunk.MeshData.ToUnityMesh();
					var chunk = chunks.FirstOrDefault(c => c && c.name == chunkIndex.ToString());
					bool isNew = !chunk;
					MeshFilter chunkMeshFilter = null;
					MeshRenderer chunkMeshRenderer = null;
					
					// create new if not already exists
					// otherwise just update the existing chunk
					if (!chunk)
					{
						if (allOctants == null) allOctants = UnityHelper.GetAllOctants();

						var go = new GameObject(chunkIndex.ToString());
						//go.transform.SetParent(this.transform, false);
						chunkMeshFilter = go.AddComponent<MeshFilter>();
						chunkMeshRenderer = go.AddComponent<MeshRenderer>();
						chunk = go.AddComponent<TfragChunk>();
						createdChunks.Add(chunk);
						//occlusionDb.SetOctants(chunk, allOctants.ToArray());
					}
					else
					{
						chunkMeshFilter = chunk.GetComponent<MeshFilter>();
						chunkMeshRenderer = chunk.GetComponent<MeshRenderer>();
					}

					chunk.gameObject.name = chunkIndex.ToString();
					chunk.HeaderBytes = builtChunk.HeaderBytes;
					chunk.DataBytes = builtChunk.DataBytes;
					chunk.gameObject.layer = LayerMask.NameToLayer("TFRAG");
					chunk.transform.SetParent(renderer.transform, false);
					chunkMeshFilter.sharedMesh = builtMeshData.mesh;
					chunkMeshRenderer.sharedMaterials = builtMeshData.materials; //texs.Select(x => materials[x]).ToArray();
					Hide(chunk.gameObject, m_RenderGenerated);

					// 
					++chunkIndex;
				}
			}

            // set newly created chunks to always visible
            if (createdChunks.Any())
            {
                occlusionDb.BulkCreate(createdChunks);
                occlusionDb.SetOctants(createdChunks, allOctants.ToArray());
            }

            // update hash
            m_LastGeneratedHash = ComputeHash(m_MeshRenderers, m_MaterialConfigs);

			// remove excess
			foreach (var chunk in chunks)
				if (chunk && (!int.TryParse(chunk.gameObject.name, out var chunkIdx) || chunkIdx >= builtChunks.Count))
					GameObject.DestroyImmediate(chunk.gameObject);
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
        }
        finally
        {
            // cleanup
        }
    }

	private void ValidateTextureConfigAgainstRenderers(MeshRenderer[] meshRenderers)
	{
		HashSet<RenderedMeshData.MaterialDef> foundMaterials = new HashSet<RenderedMeshData.MaterialDef>();

		foreach (var renderer in meshRenderers)
		{
			foreach (var material in renderer.sharedMaterials)
			{
				foundMaterials.Add(new RenderedMeshData.MaterialDef()
				{
					Name = material.name,
					Texture = material.mainTexture as Texture2D,
					Tint = material.color
				});
			}
		}

		// remove old configs
		for (int i = 0; i < m_MaterialConfigs.Count; ++i)
		{
			var config = m_MaterialConfigs[i];
			if (!foundMaterials.Any(m => m.Name == config.Name))
			{
				m_MaterialConfigs.Remove(config);
				--i;
			}
		}

		// add new configs
		foreach (var mat in foundMaterials)
		{
			var config = m_MaterialConfigs.FirstOrDefault(c => c.Name == mat.Name);
			if (config == null)
			{
				m_MaterialConfigs.Add(new MaterialConfig()
				{
					Name = mat.Name,
					Texture = mat.Texture,
					TintColor = mat.Tint
				});
			}
			else
			{
				config.Texture = mat.Texture;
			}
		}
	}

    #endregion

    #region Bake

    public override void OnPreBake(BakeType type)
    {
        if (type != BakeType.OCCLUSION && type != BakeType.BUILD && type != BakeType.MAPRENDER) return;

        // always disable collider
        var collider = GetComponent<TerrainCollider>();
        if (collider) collider.enabled = false;

        // render tfrags
        SetVisible(true);
    }

    public override void OnPostBake(BakeType type)
    {
        if (type != BakeType.OCCLUSION && type != BakeType.BUILD && type != BakeType.MAPRENDER) return;

        // always enable collider
        var collider = GetComponent<TerrainCollider>();
        if (collider) collider.enabled = true;

        // return to normal render mode
        SetVisible(m_RenderGenerated);
    }

    #endregion

	private MeshRenderer[] GetRenderers()
	{
		var renderers = GetComponentsInChildren<MeshRenderer>(false)
			.Where(x => !x.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy) && !x.GetComponent<TfragChunk>())
			.ToArray();

		return HierarchicalSorting.Sort(renderers);
	}

    private TfragChunk[] GetChunkInstances()
    {
        return HierarchicalSorting.Sort(this.GetComponentsInChildren<TfragChunk>(true));
    }

	private Hash128 ComputeHash(MeshRenderer[] meshRenderers, IEnumerable<MaterialConfig> textureConfigs)
	{
        Hash128 hash = new Hash128();

		foreach (var renderer in meshRenderers)
		{
            if (!renderer.gameObject.activeInHierarchy || renderer.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy))
                continue;

            var mf = renderer.GetComponent<MeshFilter>();
            var mesh = mf ? mf.sharedMesh : null;

            // include mesh hash
            if (mf.sharedMesh)
                hash.Append(mf.sharedMesh.ComputeHash());

            // include texture hash
            if (renderer.sharedMaterials != null)
            {
                foreach (var mat in renderer.sharedMaterials)
                {
                    if (mat.mainTexture)
                        hash.Append(mat.mainTexture.imageContentsHash.ToString());
                    hash.Append(mat.color.GetHashCode());
                }
            }
		}

		// include texture configs
		foreach (var config in textureConfigs)
		{
			if (config != null)
			{
				hash.Append($"{config.Name}-{config.CorrectForAlphaBloom}-{config.TintColor}-{(int)config.MaxTextureSize}");
				if (config.TextureOverride)
					hash.Append(config.TextureOverride.imageContentsHash.ToString());
			}
		}

        return hash;
	}

    public void SetVisible(bool visible)
    {
        var chunks = GetChunkInstances();
        if (chunks != null)
        {
            foreach (var chunk in chunks)
            {
                if (chunk) Hide(chunk.gameObject, visible);
            }
        }

		m_MeshRenderers = GetRenderers();
		foreach (var renderer in m_MeshRenderers)
			renderer.enabled = !visible;
    }

    private void Hide(GameObject go, bool visible)
    {
        go.hideFlags = (visible ? HideFlags.None : HideFlags.HideInHierarchy);
        go.SetActive(visible);
    }
}
