using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

[RequireComponent(typeof(UnityColliderToInstancedCollider))]
public class UnityTerrainToTfrags : BaseAssetGenerator
{
    public static bool m_RenderGenerated = false;

    [Range(4f, 8f)] public float m_TfragSize = 4f;
    [Range(0f, 4f)] public float m_TfragTextureClassificationSharpness = 1f;
    public TextureSize m_TextureSize = TextureSize._128;
    public bool m_FlatNormal = false;

    [Header("Vertex Colors")]
    [ColorUsage(showAlpha: false)] public Color m_Tint = Color.white;
    [Range(0f, 1f)] public float m_Shading = 0f;
    [Range(0f, 1f)] public float m_Noise = 0f;
    [Range(1f, 50f)] public float m_NoiseScale = 20f;

    private Terrain m_Terrain;
    [SerializeField, HideInInspector] private Hash128 m_LastGeneratedHash;

    #region Generate
    
    public void ValidateOrThrow()
    {
        m_Terrain = GetComponent<Terrain>();
        if (!m_Terrain) throw new Exception("UnityTerrainToTfrags: Missing Terrain component.");
        if (m_Terrain.terrainData.terrainLayers.Length > 4) throw new Exception("UnityTerrainToTfrags: Terrain component has more than 4 layers.");
    }

    public void Regenerate()
    {
        m_LastGeneratedHash = default;
        Generate();
    }

    public override void Generate()
    {
        m_Terrain = GetComponent<Terrain>();

        var triOfs = new int[] { 0, 1, 2, 4 };
        var universalShader = Shader.Find("Horizon Forge/Universal");
        var chunks = GetChunkInstances();
        var createdChunks = new List<TfragChunk>();
        var chunkCount = 0;
        var mapConfig = FindObjectOfType<MapConfig>();
        var occlusionDb = mapConfig.GetOcclusionDatabase();

        ValidateOrThrow();

        try
        {
            var vertexPerRow = Mathf.CeilToInt(m_Terrain.terrainData.size.x / m_TfragSize) + 1;
            var vertexPerColumn = Mathf.CeilToInt(m_Terrain.terrainData.size.z / m_TfragSize) + 1;
            var facePerRow = vertexPerRow - 1;
            var facePerColumn = vertexPerColumn - 1;
            var chunkPerRow = Mathf.CeilToInt(facePerRow / 2f);
            var chunkPerColumn = Mathf.CeilToInt(facePerColumn / 2f);
            var textures = new List<Texture2D>();
            var materials = new List<Material>();
            var uvCenter = Vector2.one * 0.5f;
            List<Vector3> allOctants = null;
            chunkCount = chunkPerRow * chunkPerColumn;

            // check if we need to regenerate
            if (m_LastGeneratedHash.isValid && TerrainHelper.ComputeHash(m_Terrain.terrainData) == m_LastGeneratedHash)
                return;

            // convert
            TerrainHelper.ToMesh(m_Terrain, out var terrainVertices, out var terrainNormals, out var terrainUvs, out var terrainColors, out var terrainTriangles, out var terrainTextures, faceSize: m_TfragSize, splatRamp: m_TfragTextureClassificationSharpness, textureSize: m_TextureSize);

            // generate
            for (int i = 0; i < chunkCount; ++i)
            {
                var chunk = chunks.FirstOrDefault(c => c && c.name == i.ToString());
                bool isNew = !chunk;
                MeshFilter chunkMeshFilter = null;
                MeshRenderer chunkMeshRenderer = null;
              
                byte[] headerBytes = null;
                byte[] dataBytes = null;
                var quadValid = new List<bool>();
                var quads = new List<int[]>();
                var colors = new List<Color>();
                var vertices = new List<Vector3>();
                var normals = new List<Vector3>();
                var uvs = new List<Vector2>();
                var texs = new List<int>();
                var texClamps = new List<bool>();
                var newMesh = new Mesh();

                // select faces in groups of 2x2s
                var x = (i % chunkPerRow) * 2;
                var y = (i / chunkPerRow) * 2;
                var texIdx = 0;
                for (int f = 0; f < 4; ++f)
                {
                    var fx = x + (f % 2);
                    var fy = y + (f / 2);
                    var isValid = fx < facePerRow && fy < facePerColumn;
                    var fIdx = ((fy * facePerRow) + fx) * 2;
                    var triIdx = fIdx * 3;
                    var vIdx = 0;

                    if (!isValid) continue;

                    var quad = new int[4];
                    for (int t = 0; t < 4; ++t)
                    {
                        var tOfs = triIdx + triOfs[t];
                        vIdx = terrainTriangles[tOfs];

                        quad[t] = vertices.Count;
                        vertices.Add(terrainVertices[vIdx]);
                        normals.Add(m_FlatNormal ? Vector3.up : terrainNormals[vIdx]);
                        uvs.Add(terrainUvs[vIdx]);

                        // calculate color
                        var vertexWorldSpace = m_Terrain.transform.localToWorldMatrix.MultiplyPoint(terrainVertices[vIdx]);
                        var shading = Mathf.Pow(Mathf.Clamp01(Mathf.Abs(Vector3.Dot(terrainNormals[vIdx], Vector3.up))), m_Shading * 10);
                        var noise = Mathf.Pow(Mathf.Clamp01(Mathf.PerlinNoise(vertexWorldSpace.x / m_NoiseScale, vertexWorldSpace.z / m_NoiseScale)), m_Noise * 3f);
                        var alpha = terrainColors[vIdx].a;
                        var color = terrainColors[vIdx] * m_Tint * 0.5f * shading * noise;
                        color.a = alpha;
                        colors.Add(color);
                    }

                    var tex = terrainTextures[fIdx];
                    texIdx = textures.IndexOf(tex);
                    if (texIdx < 0)
                    {
                        texIdx = textures.Count;
                        textures.Add(tex);

                        // configure material
                        var mat = new Material(universalShader);
                        mat.SetTexture("_MainTex", tex);
                        mat.SetColor("_Color", new Color(1, 1, 1, 0.5f));
                        mat.name = tex.name;
                        materials.Add(mat);
                    }

                    texClamps.Add(tex.wrapMode == TextureWrapMode.Clamp);
                    texs.Add(texIdx);
                    quads.Add(quad);
                    quadValid.Add(isValid);
                }

                var quadsWithNoHole = quads.Count(x => x.Any(v => colors[v].a != 0));
                if (quadsWithNoHole == 0)
                {
                    if (chunk) GameObject.DestroyImmediate(chunk.gameObject);
                    //Dispatcher.RunOnMainThread(() => GameObject.DestroyImmediate(chunk.gameObject));
                    continue;
                }

                if (quads.Count == 4)
                    TfragHelper.GenerateTfrag_2x2(vertices, normals, colors, uvs, quads, texs, texClamps, out headerBytes, out dataBytes);
                else if (quads.Count == 2)
                    TfragHelper.GenerateTfrag_1x2(vertices, normals, colors, uvs, quads, texs, texClamps, out headerBytes, out dataBytes);
                else if (quads.Count == 1)
                    TfragHelper.GenerateTfrag_1x1(vertices, normals, colors, uvs, quads, texs, texClamps, out headerBytes, out dataBytes);
                else if (quads.Count == 0)
                    continue;
                else
                    throw new NotImplementedException();

                newMesh.SetVertices(vertices);
                newMesh.SetUVs(0, uvs);
                newMesh.SetColors(colors);
                newMesh.SetNormals(normals);
                newMesh.subMeshCount = quads.Count;
                for (int f = 0; f < quads.Count; ++f)
                {
                    var quad = quads[f];
                    if (quad.All(x => colors[x].a == 0))
                        continue;
                    //if (colors[quad[f]].a == 0) continue;
                    newMesh.SetIndices(new int[] { quad[0], quad[1], quad[2], quad[1], quad[3], quad[2] }, MeshTopology.Triangles, f);
                }

                if (!chunk)
                {
                    if (allOctants == null) allOctants = UnityHelper.GetAllOctants();

                    var go = new GameObject(i.ToString());
                    go.transform.SetParent(this.transform, false);
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

                chunk.gameObject.name = i.ToString();
                chunk.HeaderBytes = headerBytes;
                chunk.DataBytes = dataBytes;
                chunk.gameObject.layer = LayerMask.NameToLayer("TFRAG");
                chunkMeshFilter.sharedMesh = newMesh;
                chunkMeshRenderer.sharedMaterials = texs.Select(x => materials[x]).ToArray();
                Hide(chunk.gameObject, m_RenderGenerated);
            }

            // set newly created chunks to always visible
            if (createdChunks.Any())
            {
                occlusionDb.BulkCreate(createdChunks);
                occlusionDb.SetOctants(createdChunks, allOctants.ToArray());
            }

            // update hash
            m_LastGeneratedHash = TerrainHelper.ComputeHash(m_Terrain.terrainData);
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
        }
        finally
        {
            // cleanup
        }

        // remove excess
        foreach (var chunk in chunks)
            if (chunk && (!int.TryParse(chunk.gameObject.name, out var chunkIdx) || chunkIdx >= chunkCount))
                GameObject.DestroyImmediate(chunk.gameObject);
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

    private TfragChunk[] GetChunkInstances()
    {
        return HierarchicalSorting.Sort(this.GetComponentsInChildren<TfragChunk>(true));
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

        m_Terrain = GetComponent<Terrain>();
        if (m_Terrain) m_Terrain.drawHeightmap = !visible;
    }

    private void Hide(GameObject go, bool visible)
    {
        go.hideFlags = (visible ? HideFlags.None : HideFlags.HideInHierarchy);
        go.SetActive(visible);
    }
}
