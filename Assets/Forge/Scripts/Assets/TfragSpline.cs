using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class TfragSpline : BaseAssetGenerator
{
    public enum TfragSplineGenMode
    {
        FixedCount,
        Curvature
    }

    public enum TfragSplineWidthMode
    {
        Constant,
        Spline
    }

    public enum TfragSplineLerpMode
    {
        Time,
        Distance
    }

    [Header("Tfrag")]
    [Range(2f, 16f)] public float m_TfragSize = 4f;
    [Min(1)] public int m_SliceCount = 2;
    [Min(1)] public TfragSplineWidthMode m_SliceMode = TfragSplineWidthMode.Constant;
    [Min(1)] public TfragSplineLerpMode m_LerpMode = TfragSplineLerpMode.Time;
    public bool m_FlatNormal = false;
    public bool m_RecalculateNormals;
    public bool m_FlipNormal;

    [Header("Textures")]
    public List<TfragSplineTexture> m_Textures = new List<TfragSplineTexture>();

    [Header("Collider")]
    public bool m_InstancedCollider;
    public bool m_ColliderFlipNormal;

    [Header("Spline")]
    public bool m_Loop = false;
    [Tooltip("First value indicates where the gap begins, in units along spline. Second value indicates the length of the gap in units along spline.")]
    public List<Vector2> m_Gaps = new List<Vector2>();
    [ReadOnly] public int m_ComputedNumPoints = 0;

    [Header("Gizmos")]
    public bool m_DrawPoints = false;
    public bool m_DrawRotation = false;
    public bool m_AutoRegenerate = false;

    private TfragSplineVertex[] m_CachedVertices; 
    private List<TfragSplineBuiltPoint> m_BuiltPath = new List<TfragSplineBuiltPoint>();
    private List<TfragSplinePoint> m_CachedPathPoints;
    private Hash128 m_CachedPathPointsHash;

    [SerializeField, HideInInspector] private MeshCollider m_InstancedMeshCollider;
    [SerializeField, HideInInspector] private MeshFilter m_InstancedColliderMeshFilter;
    [SerializeField, HideInInspector] private InstancedMeshCollider m_Collider;
    [SerializeField, HideInInspector] private Hash128 m_LastGeneratedHash;

    private int GetVerticesPerSlice()
    {
        if (m_SliceMode == TfragSplineWidthMode.Constant) return m_SliceCount + 1;

        // make sure all vertices have a spline
        // and that all splines have the same # of vertices
        var count = -1;
        foreach (var vertex in m_CachedVertices)
        {
            // needs spline
            if (!vertex.WidthSpline)
                return -1;

            var numPoints = vertex.WidthSpline.ComputePath().Length;
            if (count >= 0 && numPoints != count)
                return -1;

            count = numPoints;
        }

        return count;
    }

    protected void Start()
    {
        InvalidateCache();
    }

    protected void OnValidate()
    {
        if (UnityHelper.IsObjectPrefabFile(this.gameObject)) return;

        OnChange();
    }

    public void OnChange()
    {
        InvalidateCache();
        BuildSpline();
        if (m_AutoRegenerate)
            Dispatcher.RunOnMainThread(() => Generate());
    }

    #region Generate

    private Hash128 ComputeHash()
    {
        var hash = new Hash128();
        hash = hash.Append(this.transform.localToWorldMatrix);

        // add vertices
        var vertices = GetVertices();
        foreach (var vertex in vertices)
        {
            hash.Append(vertex.ComputeHash().ToString());
        }

        // add gaps
        if (m_Gaps != null)
        {
            foreach (var gap in m_Gaps)
            {
                hash = hash.Append(gap);
            }
        }

        // add textures
        if (m_Textures != null)
        {
            foreach (var texture in m_Textures)
            {
                hash.Append(texture.m_Texture.GetHash().ToString());
                hash = hash.Append(texture.m_UvOffset);
                hash = hash.Append(texture.m_UvTiling);
                hash.Append(texture.m_UvWorldSpaceU ? 1 : 0);
                hash.Append(texture.m_UvWorldSpaceV ? 1 : 0);
                hash.Append(texture.m_UvRotation);
                hash = hash.Append((Vector2)texture.m_AppearAfter);
                hash.Append((int)texture.m_TextureSize);
                hash = hash.Append(texture.m_Tint);
                hash.Append(texture.m_CollisionId);
            }
        }

        // add spline params
        hash.Append(m_Loop ? 1 : 0);
        hash.Append(m_InstancedCollider ? 1 : 0);
        hash.Append(m_ColliderFlipNormal ? 1 : 0);
        hash.Append(m_RecalculateNormals ? 1 : 0);
        hash.Append(m_FlipNormal ? 1 : 0);
        hash.Append(m_FlatNormal ? 1 : 0);
        hash.Append(m_SliceCount);
        hash.Append((int)m_SliceMode);
        hash.Append((int)m_LerpMode);
        hash.Append(m_TfragSize);

        return hash;
    }

    public bool Validate()
    {
        if (m_SliceMode == TfragSplineWidthMode.Spline)
        {
            // make sure all vertices have a spline
            // and that all splines have the same # of vertices
            var count = -1;
            var vertices = GetVertices();
            foreach (var vertex in vertices)
            {
                // needs spline
                if (!vertex.WidthSpline)
                {
                    Debug.LogError("Missing a width spline", vertex);
                    return false;
                }

                var numPoints = vertex.WidthSpline.ComputePath().Length;
                if (count >= 0 && numPoints != count)
                {
                    Debug.LogError($"Width spline must have same number of computed points ({numPoints})", vertex);
                    return false;
                }

                count = numPoints;
            }
        }

        return true;
    }

    public void Regenerate()
    {
        m_LastGeneratedHash = default;
        Generate();
    }

    public override void Generate()
    {
        if (!Validate()) return;

        var hash = ComputeHash();
        if (hash == m_LastGeneratedHash) return;

        this.transform.localScale = Vector3.one;
        var buildTransform = this.transform.Find("build");
        var buildGo = buildTransform ? buildTransform.gameObject : new GameObject("build");
        buildGo.transform.SetParent(this.transform, false);
        buildGo.transform.position = Vector3.zero;
        buildGo.transform.rotation = Quaternion.identity;
        buildGo.transform.localScale = Vector3.one;
        buildGo.hideFlags = HideFlags.HideInHierarchy | HideFlags.HideInInspector;

        // convert
        InvalidateCache();
        BuildSpline();
        ToMesh(out var meshVertices, out var meshNormals, out var meshUvs, out var meshColors, out var meshTriangles, out var meshTextures, out var meshCollisionIds);
        UpdateCollider(buildGo, meshVertices, meshNormals, meshUvs, meshColors, meshTriangles, meshCollisionIds);
        UpdateChunks(buildGo, meshVertices, meshNormals, meshUvs, meshColors, meshTriangles, meshTextures);

        m_LastGeneratedHash = hash;
    }

    private void UpdateCollider(GameObject parentGo, Vector3[] meshVertices, Vector3[] meshNormals, Vector2[] meshUvs, Color[] meshColors, int[] meshTriangles, int[] meshCollisionIds)
    {
        if (!m_InstancedCollider)
        {
            if (m_InstancedMeshCollider)
                DestroyImmediate(m_InstancedMeshCollider.gameObject);
            return;
        }

        if (!m_InstancedMeshCollider)
        {
            var go = new GameObject("collider");
            go.transform.SetParent(parentGo.transform, false);
            go.hideFlags = HideFlags.HideInHierarchy | HideFlags.HideInInspector;
            m_InstancedMeshCollider = go.AddComponent<MeshCollider>();
            m_InstancedColliderMeshFilter = go.AddComponent<MeshFilter>();
            m_Collider = go.AddComponent<InstancedMeshCollider>();
            m_Collider.m_Render = false;
        }

        // create mesh & collider
        Mesh mesh = new Mesh();
        mesh.SetVertices(meshVertices);
        mesh.SetTriangles(meshTriangles, 0);
        mesh.SetUVs(0, meshUvs);
        mesh.SetNormals(meshNormals);
        mesh.RecalculateBounds();

        if (m_RecalculateNormals)
        {
            mesh.RecalculateNormals();

            for (int i = 0; i < mesh.normals.Length; ++i)
            {
                meshNormals[i] = mesh.normals[i];
            }
        }

        // get list of collision ids
        // use submeshes to group collisions
        var uniqueColIds = meshCollisionIds.Distinct().ToArray();
        if (uniqueColIds.Length > 0)
        {
            mesh.subMeshCount = uniqueColIds.Length;
            for (int i = 0; i < uniqueColIds.Length; ++i)
            {
                var facesWithColId = new List<int>();
                for (int f = 0; f < meshTriangles.Length / 3; ++f)
                {
                    if (meshCollisionIds[f] == uniqueColIds[i])
                    {
                        facesWithColId.Add(f); 
                    }
                }

                var newTriangles = new int[facesWithColId.Count * 3];
                var triIndex = 0;
                foreach (var f in facesWithColId)
                {
                    if (m_ColliderFlipNormal)
                    {
                        newTriangles[triIndex++] = meshTriangles[(f * 3) + 2];
                        newTriangles[triIndex++] = meshTriangles[(f * 3) + 1];
                        newTriangles[triIndex++] = meshTriangles[(f * 3) + 0];
                    }
                    else
                    {
                        newTriangles[triIndex++] = meshTriangles[(f * 3) + 0];
                        newTriangles[triIndex++] = meshTriangles[(f * 3) + 1];
                        newTriangles[triIndex++] = meshTriangles[(f * 3) + 2];
                    }
                }

                mesh.SetTriangles(newTriangles, i, true);
            }
        }

        m_Collider.m_UseColliderIdOverrides = true;
        m_Collider.m_ColliderIdOverrides = uniqueColIds;
        m_Collider.m_Normals = CollisionRenderHandleNormalMode.FrontSide;
        m_InstancedMeshCollider.sharedMesh = mesh;
        m_InstancedColliderMeshFilter.sharedMesh = mesh;
        m_Collider.UpdateAsset();
    }

    private void UpdateChunks(GameObject parentGo, Vector3[] meshVertices, Vector3[] meshNormals, Vector2[] meshUvs, Color[] meshColors, int[] meshTriangles, Texture2D[] meshTextures)
    {
        var segmentCount = m_BuiltPath.Count;
        var sliceSegmentCount = GetVerticesPerSlice();
        var chunks = HierarchicalSorting.Sort(parentGo.GetComponentsInChildren<TfragChunk>(true));

        List<Vector3> allOctants = null;
        var vertexPerRow = segmentCount;
        var vertexPerColumn = sliceSegmentCount;
        var facePerRow = vertexPerRow - 1;
        var facePerColumn = vertexPerColumn - 1;
        var chunkPerRow = Mathf.CeilToInt(facePerRow / 2f);
        var chunkPerColumn = Mathf.CeilToInt(facePerColumn / 2f);
        var textures = new List<Texture2D>();
        var materials = new List<Material>();
        var chunkCount = chunkPerRow * chunkPerColumn;
        var computedChunkCount = 0;
        var triOfs = new int[] { 0, 1, 2, 4 };
        var universalShader = Shader.Find("Horizon Forge/Universal");


        // generate
        for (int i = 0; i < chunkCount; ++i)
        {
            var chunk = chunks.ElementAtOrDefault(computedChunkCount);
            MeshFilter chunkMeshFilter = null;
            MeshRenderer chunkMeshRenderer = null;
            if (!chunk)
            {
                if (allOctants == null) allOctants = UnityHelper.GetAllOctants();

                var go = new GameObject(computedChunkCount.ToString());
                go.transform.SetParent(parentGo.transform, false);
                go.hideFlags = HideFlags.HideInHierarchy | HideFlags.HideInInspector;
                chunkMeshFilter = go.AddComponent<MeshFilter>();
                chunkMeshRenderer = go.AddComponent<MeshRenderer>();
                chunk = go.AddComponent<TfragChunk>();
                chunk.Octants = allOctants.ToArray();
            }
            else
            {
                chunkMeshFilter = chunk.GetComponent<MeshFilter>();
                chunkMeshRenderer = chunk.GetComponent<MeshRenderer>();
            }

            byte[] headerBytes = null;
            byte[] dataBytes = null;
            var quadValid = new List<bool>();
            var quads = new List<int[]>();
            var colors = new List<Color>();
            var vertices = new List<Vector3>();
            var normals = new List<Vector3>();
            var unityNormals = new List<Vector3>();
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
                var fIdx = ((fx * facePerColumn) + fy) * 2;
                var triIdx = fIdx * 3;
                var vIdx = 0;

                if (!isValid) continue;
                if (triIdx >= meshTriangles.Length) continue;

                var tex = meshTextures[fIdx];
                if (!tex) continue;

                var quad = new int[4];
                for (int t = 0; t < 4; ++t)
                {
                    var tOfs = triIdx + triOfs[t];
                    vIdx = meshTriangles[tOfs];

                    quad[t] = vertices.Count;
                    vertices.Add(meshVertices[vIdx]);
                    normals.Add(m_FlatNormal ? Vector3.up : this.transform.localToWorldMatrix.MultiplyVector(meshNormals[vIdx]));
                    unityNormals.Add(m_FlatNormal ? Vector3.forward : meshNormals[vIdx]);
                    uvs.Add(meshUvs[vIdx]);

                    // calculate color
                    //var vertexWorldSpace = m_Terrain.transform.localToWorldMatrix.MultiplyPoint(meshVertices[vIdx]);
                    //var shading = Mathf.Pow(Mathf.Clamp01(Mathf.Abs(Vector3.Dot(meshNormals[vIdx], Vector3.up))), m_Shading * 10);
                    //var noise = Mathf.Pow(Mathf.Clamp01(Mathf.PerlinNoise(vertexWorldSpace.x / m_NoiseScale, vertexWorldSpace.z / m_NoiseScale)), m_Noise * 3f);
                    var color = meshColors[vIdx] * 0.5f; // * shading * noise;
                    color.a = 1;
                    colors.Add(color);
                }

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

                texs.Add(texIdx);
                texClamps.Add(false);
                quads.Add(quad);
                quadValid.Add(isValid);
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
            newMesh.SetNormals(unityNormals);
            newMesh.subMeshCount = quads.Count;
            for (int f = 0; f < quads.Count; ++f)
            {
                var quad = quads[f];
                //if (quad.Any(x => colors[x].a == 0)) continue;
                if (colors[quad[1]].a == 0)
                    continue;
                newMesh.SetIndices(new int[] { quad[0], quad[1], quad[2], quad[1], quad[3], quad[2] }, MeshTopology.Triangles, f);
            }

            chunk.gameObject.name = computedChunkCount.ToString();
            chunk.HeaderBytes = headerBytes;
            chunk.DataBytes = dataBytes;
            chunk.gameObject.layer = LayerMask.NameToLayer("TFRAG");
            chunk.gameObject.hideFlags = HideFlags.HideInHierarchy | HideFlags.HideInInspector;
            chunkMeshFilter.sharedMesh = newMesh;
            chunkMeshRenderer.sharedMaterials = texs.Select(x => materials[x]).ToArray();
            ++computedChunkCount;
        }

        // remove excess
        for (int i = computedChunkCount; i < chunks.Length; ++i)
            GameObject.DestroyImmediate(chunks[i].gameObject);
    }

    private void ToMesh(out Vector3[] vertices, out Vector3[] normals, out Vector2[] uvs, out Color[] colors, out int[] triangles, out Texture2D[] textures, out int[] collisionIds)
    {
        var segmentCount = m_BuiltPath.Count;
        var sliceSegmentCount = GetVerticesPerSlice();
        var vertexCount = Math.Max(0, (segmentCount - 1) * (sliceSegmentCount - 1) * 4);
        var triangleCount = Math.Max(0, (segmentCount - 1) * (sliceSegmentCount - 1) * 6);
        var textureCollection = new Dictionary<TfragSplineTexture, Texture2D>();
        var currentLength = 0f;

        vertices = new Vector3[vertexCount];
        normals = new Vector3[vertexCount];
        uvs = new Vector2[vertexCount];
        colors = new Color[vertexCount];
        triangles = new int[triangleCount];
        textures = new Texture2D[triangleCount / 3];
        collisionIds = new int[triangleCount / 3];

        var lastValidTris = 0;
        var vertIndex = 0;
        var triIndex = 0;
        for (int i = 0; i < segmentCount - 1; i++)
        {
            var point0 = m_BuiltPath[i];
            var point1 = m_BuiltPath[i + 1];
            Vector3 p0 = point0.Transform.GetColumn(3);
            Vector3 p1 = point1.Transform.GetColumn(3);

            for (int j = 0; j < sliceSegmentCount - 1; ++j)
            {
                if (m_Gaps != null && m_Gaps.Any(gap => currentLength >= gap.x && currentLength <= (gap.x + gap.y)))
                {
                    vertIndex += 4;
                    triangles[triIndex++] = triangles[lastValidTris + 0];
                    triangles[triIndex++] = triangles[lastValidTris + 1];
                    triangles[triIndex++] = triangles[lastValidTris + 2];
                    triangles[triIndex++] = triangles[lastValidTris + 3];
                    triangles[triIndex++] = triangles[lastValidTris + 4];
                    triangles[triIndex++] = triangles[lastValidTris + 5];
                    continue;
                }

                var sliceT = j / (float)(sliceSegmentCount - 1);
                var sliceTN = (j+1) / (float)(sliceSegmentCount - 1);
                var textureDef = m_Textures.FirstOrDefault(x => i >= x.m_AppearAfter.x && j >= x.m_AppearAfter.y);
                var tint = textureDef?.m_Tint.SetAlpha(1) ?? Color.white;

                Vector3 width0 = point0.Transform.MultiplyPoint(point0.Point.GetWidth(m_SliceMode, sliceT));
                Vector3 width1 = point1.Transform.MultiplyPoint(point1.Point.GetWidth(m_SliceMode, sliceT));
                Vector3 width0N = point0.Transform.MultiplyPoint(point0.Point.GetWidth(m_SliceMode, sliceTN));
                Vector3 width1N = point1.Transform.MultiplyPoint(point1.Point.GetWidth(m_SliceMode, sliceTN));

                var m0 = point0.Transform;
                var m1 = point1.Transform;
                Vector3 normal0L = m0.MultiplyVector(point0.Point.GetNormal(m_SliceMode, sliceT)).normalized;
                Vector3 normal0R = m0.MultiplyVector(point0.Point.GetNormal(m_SliceMode, sliceTN)).normalized;
                Vector3 normal1L = m1.MultiplyVector(point1.Point.GetNormal(m_SliceMode, sliceT)).normalized;
                Vector3 normal1R = m1.MultiplyVector(point1.Point.GetNormal(m_SliceMode, sliceTN)).normalized;

                Vector3 v0L = width0;
                Vector3 v0R = width0N;
                Vector3 v1L = width1;
                Vector3 v1R = width1N;

                if (m_FlatNormal)
                {
                    normal0R = normal0L = point0.Transform.MultiplyVector(Vector3.forward);
                    normal1R = normal1L = point1.Transform.MultiplyVector(Vector3.forward);
                }
                if (m_FlipNormal)
                {
                    normal0L *= -1;
                    normal0R *= -1;
                    normal1L *= -1;
                    normal1R *= -1;
                }

                // compute uvs
                var faceUvs = ComputeQuadUVs(new Vector3[] { v0L, v0R, v1L, v1R }, sliceT, sliceTN, i, i + 1, textureDef);

                // v0
                vertices[vertIndex] = v0L;
                uvs[vertIndex] = faceUvs[0];
                normals[vertIndex] = normal0L;
                colors[vertIndex] = tint;
                ++vertIndex;

                // v1
                vertices[vertIndex] = v0R;
                uvs[vertIndex] = faceUvs[1];
                normals[vertIndex] = normal0R;
                colors[vertIndex] = tint;
                ++vertIndex;

                // v2
                vertices[vertIndex] = v1L;
                uvs[vertIndex] = faceUvs[2];
                normals[vertIndex] = normal1L;
                colors[vertIndex] = tint;
                ++vertIndex;

                // v3
                vertices[vertIndex] = v1R;
                uvs[vertIndex] = faceUvs[3];
                normals[vertIndex] = normal1R;
                colors[vertIndex] = tint;
                ++vertIndex;

                // get texture
                var texture = UnityHelper.DefaultTexture;
                if (textureDef != null && textureDef.m_Texture && !textureCollection.TryGetValue(textureDef, out texture))
                {
                    var texSize = (int)Mathf.Pow(2, 5 + (int)textureDef.m_TextureSize);
                    textureCollection[textureDef] = texture = UnityHelper.CloneTexture(textureDef.m_Texture, hasAlpha: false, resizeWidth: texSize, resizeHeight: texSize);
                }

                // build 2 tris (1 quad face)
                var vertBaseIdx = vertIndex - 4;
                lastValidTris = triIndex;
                if (m_FlipNormal && false)
                {
                    textures[triIndex / 3] = texture;
                    collisionIds[triIndex / 3] = CollisionHelper.ParseId(textureDef?.m_CollisionId);
                    triangles[triIndex++] = vertBaseIdx + 0;
                    triangles[triIndex++] = vertBaseIdx + 1;
                    triangles[triIndex++] = vertBaseIdx + 2;

                    textures[triIndex / 3] = texture;
                    collisionIds[triIndex / 3] = CollisionHelper.ParseId(textureDef?.m_CollisionId);
                    triangles[triIndex++] = vertBaseIdx + 1;
                    triangles[triIndex++] = vertBaseIdx + 3;
                    triangles[triIndex++] = vertBaseIdx + 2;
                }
                else
                {
                    textures[triIndex / 3] = texture;
                    collisionIds[triIndex / 3] = CollisionHelper.ParseId(textureDef?.m_CollisionId);
                    triangles[triIndex++] = vertBaseIdx + 2;
                    triangles[triIndex++] = vertBaseIdx + 3;
                    triangles[triIndex++] = vertBaseIdx + 0;

                    textures[triIndex / 3] = texture;
                    collisionIds[triIndex / 3] = CollisionHelper.ParseId(textureDef?.m_CollisionId);
                    triangles[triIndex++] = vertBaseIdx + 3;
                    triangles[triIndex++] = vertBaseIdx + 1;
                    triangles[triIndex++] = vertBaseIdx + 0;
                }
            }

            currentLength += (p1 - p0).magnitude;
        }
    }

    private Vector2[] ComputeQuadUVs(Vector3[] points, float x0, float x1, float y0, float y1, TfragSplineTexture textureDef)
    {
        var dirX = Math.Sign(x1 - x0);
        var dirY = Math.Sign(y1 - y0);
        var tiling = textureDef?.m_UvTiling ?? Vector2.one;
        var offset = textureDef?.m_UvOffset ?? Vector2.zero;
        var offsets = new Vector2[4];
        var radians = (textureDef?.m_UvRotation ?? 0) * 90f * Mathf.Deg2Rad;

        offsets[0] = new Vector2(x0, y0) + offset;
        offsets[1] = new Vector2(x1, y0) + offset;
        offsets[2] = new Vector2(x0, y1) + offset;
        offsets[3] = new Vector2(x1, y1) + offset;

        if (textureDef?.m_UvWorldSpaceU == true || textureDef?.m_UvWorldSpaceV == true)
        {
            var normal = new Plane(points[0], points[1], points[2]).normal;
            var binormal = Vector3.Cross(points[1] - points[0], normal).normalized;
            var binormal2 = Vector3.Cross(binormal, normal).normalized;
            Func<Vector3, Vector2> getVec2 = (v) => new Vector2(v.x + v.y, v.z + v.y);
            if (Mathf.Abs(Vector3.Dot(Vector3.up, normal)) < 0.5f)
                getVec2 = (v) => new Vector2(v.y, v.x + v.z);

            var wuv0 = getVec2(points[0]) + offset;
            var wuv1 = getVec2(points[1]) + offset;
            var wuv2 = getVec2(points[2]) + offset;
            var wuv3 = getVec2(points[3]) + offset;

            if (textureDef.m_UvWorldSpaceU)
            {
                offsets[0].x = wuv0.x;
                offsets[1].x = wuv1.x;
                offsets[2].x = wuv2.x;
                offsets[3].x = wuv3.x;
            }
            if (textureDef.m_UvWorldSpaceV)
            {
                offsets[0].y = wuv0.y;
                offsets[1].y = wuv1.y;
                offsets[2].y = wuv2.y;
                offsets[3].y = wuv3.y;
            }
        }

        // clamp uvs to [0,1]
        // required to fix texture repeating issues on hardware
        var baseUv = offsets[0] * tiling;
        var uv0R = (offsets[1] * tiling);
        var uv1L = (offsets[2] * tiling);
        var uv1R = (offsets[3] * tiling);
        //var uv0R = (offsets[1] * tiling).ClampUVRelativeTo(baseUv, Vector2.right * dirX * tiling);
        //var uv1L = (offsets[2] * tiling).ClampUVRelativeTo(baseUv, Vector2.up * dirY * tiling);
        //var uv1R = (offsets[3] * tiling).ClampUVRelativeTo(baseUv, new Vector2(dirX, dirY) * tiling);
        var uv0L = baseUv.ClampUV();

        // make relative to base uv
        var delta = uv0L - baseUv;
        uv0R += delta;
        uv1L += delta;
        uv1R += delta;

        // apply uv rotation
        var rotatePivot = Vector2.zero;
        uv0L = uv0L.RotateAround(radians, rotatePivot).Round();
        uv0R = uv0R.RotateAround(radians, rotatePivot).Round();
        uv1L = uv1L.RotateAround(radians, rotatePivot).Round();
        uv1R = uv1R.RotateAround(radians, rotatePivot).Round();

        // correct negatives
        if (uv0L.x < 0 || uv0R.x < 0 || uv1L.x < 0 || uv1R.x < 0)
        {
            uv0L.x += 1;
            uv0R.x += 1;
            uv1L.x += 1;
            uv1R.x += 1;
        }
        if (uv0L.y < 0 || uv0R.y < 0 || uv1L.y < 0 || uv1R.y < 0)
        {
            uv0L.y += 1;
            uv0R.y += 1;
            uv1L.y += 1;
            uv1R.y += 1;
        }

        return new Vector2[] {
            uv0L,
            uv0R,
            uv1L,
            uv1R
        };
    }

    #endregion

    #region Bake

    public override void OnPreBake(BakeType type)
    {
        if (type != BakeType.OCCLUSION && type != BakeType.BUILD && type != BakeType.MAPRENDER) return;

        // render tfrags
        //SetVisible(true);
    }

    public override void OnPostBake(BakeType type)
    {
        if (type != BakeType.OCCLUSION && type != BakeType.BUILD && type != BakeType.MAPRENDER) return;

        // return to normal render mode
        //SetVisible(m_RenderGenerated);
    }

    #endregion

    #region Create Asset


    [MenuItem("GameObject/Forge/Misc/Tfrag Spline/Default", priority = 10)]
    public static void CreateTfragSplineDefault()
    {
        var tfragSplineGo = new GameObject("Tfrag Spline");
        var tfragSpline = tfragSplineGo.AddComponent<TfragSpline>();

        // add vertices
        {
            var vertGo0 = new GameObject("0");
            var vertGo1 = new GameObject("1");

            vertGo0.transform.SetParent(tfragSplineGo.transform, false);
            vertGo1.transform.SetParent(tfragSplineGo.transform, false);
            vertGo1.transform.localPosition = Vector3.right * 10;

            var vert0 = vertGo0.AddComponent<TfragSplineVertex>();
            var vert1 = vertGo1.AddComponent<TfragSplineVertex>();

            vert0.SetOffsets(Vector3.forward * -5, Vector3.forward * 5);
            vert1.SetOffsets(Vector3.forward * -5, Vector3.forward * 5);
        }

        // place under selected object
        // or try and spawn on top of scene camera
        if (Selection.activeGameObject)
            tfragSplineGo.transform.SetParent(Selection.activeGameObject.transform, false);
        else if (SceneView.lastActiveSceneView.camera)
            tfragSplineGo.transform.position = SceneView.lastActiveSceneView.camera.transform.position + (SceneView.lastActiveSceneView.camera.transform.forward * 5);

        Selection.activeGameObject = tfragSplineGo;
    }

    [MenuItem("GameObject/Forge/Misc/Tfrag Spline/Path", priority = 10)]
    public static void CreateTfragSplinePath()
    {
        var prefab = UnityHelper.GetMiscPrefab("Tfrag Spline Path");
        if (!prefab) return;

        var tfragSplineGo = Instantiate(prefab);
        if (!tfragSplineGo) return;

        // place under selected object
        // or try and spawn on top of scene camera
        if (Selection.activeGameObject)
            tfragSplineGo.transform.SetParent(Selection.activeGameObject.transform, false);
        else if (SceneView.lastActiveSceneView.camera)
            tfragSplineGo.transform.position = SceneView.lastActiveSceneView.camera.transform.position + (SceneView.lastActiveSceneView.camera.transform.forward * 5);

        Selection.activeGameObject = tfragSplineGo;

        // generate
        tfragSplineGo.name = "Tfrag Spline Path";
        var tfragSpline = tfragSplineGo.GetComponent<TfragSpline>();
        if (tfragSpline)
            tfragSpline.Generate();
    }

    #endregion

    #region Cache

    public void InvalidateCache()
    {
        m_CachedVertices = null;
    }

    #endregion

    #region Spline

    public TfragSplineVertex[] GetVertices()
    {
        if (m_CachedVertices != null && m_CachedVertices.All(x => x))
            return m_CachedVertices;

        m_CachedVertices = GetComponentsInChildren<TfragSplineVertex>();
        if (m_Loop && m_CachedVertices != null)
        {
            Array.Resize(ref m_CachedVertices, m_CachedVertices.Length + 1);
            m_CachedVertices[m_CachedVertices.Length - 1] = m_CachedVertices[0];
        }

        return m_CachedVertices;
    }

    public void RefreshVertices()
    {
        //Vertices = GetComponentsInChildren<SplineVertex>().ToList();
        BuildSpline();
    }

    private void BuildSpline()
    {
        // build path
        var path = ComputePathPoints();
        if (path == null)
            return;

        // add vertices
        m_BuiltPath.Clear();
        var count = path.Count;
        for (int i = 0; i < count; ++i)
        {
            var nextI = (i + 1) >= count ? (m_Loop ? 0 : (i - 1)) : (i + 1);
            var pos1 = path[i].GetPosition();
            var pos2 = path[nextI].GetPosition();
            var tan = (pos2 - pos1).normalized;
            if (nextI < i) tan *= -1;
            if (tan == Vector3.zero && i > 0) tan = i > 0 ? (pos1 - path[i - 1].GetPosition()).normalized : this.transform.forward;

            var mVertex = Matrix4x4.TRS(pos1, Quaternion.LookRotation(tan, this.transform.up), Vector3.one);
            if (mVertex.determinant < 0)
                mVertex = Matrix4x4.TRS(pos1, Quaternion.LookRotation(tan, -this.transform.up), Vector3.one);

            m_BuiltPath.Add(new TfragSplineBuiltPoint(mVertex, path[i]));
        }
    }

    #endregion

    #region Bezier

    public TfragSplineNear GetNearestPoint(Vector3 position)
    {
        const float step = 0.01f;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2) return new TfragSplineNear();

        Vector3 nearest = vertices[0].Control;
        TfragSplineVertex nearestPoint = vertices[0];
        float nearestTime = 0f;
        float nearestDist = float.MaxValue;

        for (int i = 0; i < (vertices.Length - 1); ++i)
        {
            for (float t = 0f; t <= 1f; t += step)
            {
                var p = GetPosition(vertices[i], vertices[i + 1], t);
                var d = Vector3.Distance(position, p);
                if (d < nearestDist)
                {
                    nearest = p;
                    nearestPoint = vertices[i];
                    nearestTime = t;
                    nearestDist = d;
                }
            }
        }

        return new TfragSplineNear()
        {
            Vertex = nearestPoint,
            Position = nearest,
            Time = nearestTime
        };
    }

    public float GetDistanceToPointOnCurve(TfragSplineVertex vertex, float time)
    {
        float distance = 0f;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2) return 0;

        for (int i = 0; i < (vertices.Length - 1); ++i)
        {
            var p = vertices[i];
            if (p == vertex)
                return distance += GetSegmentLength(p, vertices[i + 1], 0, time);

            distance += GetSegmentLength(p, vertices[i + 1], 0, 1f);
        }

        return distance;
    }

    public TfragSplinePoint GetPointOnPath(float distance)
    {
        float currentDistance = 0f;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2) return new TfragSplinePoint(null, null, 0, 0);

        for (int i = 0; i < vertices.Length - 1; ++i)
        {
            // find segment
            var segmentStartDistance = currentDistance;
            float segmentLength = GetSegmentLength(vertices[i], vertices[i + 1], 0, 1);
            if ((currentDistance + segmentLength) < distance)
            {
                currentDistance += segmentLength;
                continue;
            }

            // find time
            Vector3 lastPosition = GetPosition(vertices[i], vertices[i + 1], 0);
            for (float t = 0f; t <= 1f; t += 0.001f)
            {
                var p = GetPosition(vertices[i], vertices[i + 1], t);
                var d = Vector3.Distance(lastPosition, p);
                currentDistance += d;
                if (currentDistance >= distance)
                    return new TfragSplinePoint(vertices[i], vertices[i + 1], t, m_LerpMode == TfragSplineLerpMode.Time ? t : ((currentDistance - segmentStartDistance) / segmentLength));

                lastPosition = p;
            }
        }

        return new TfragSplinePoint(vertices[vertices.Length - 2], vertices[vertices.Length - 1], 1, 1);
    }

    public float GetTimeToDistanceOnPoint(TfragSplineVertex a, TfragSplineVertex b, float distance)
    {
        float t = 0f;
        float currentDistance = 0f;

        // find time
        Vector3 lastPosition = a.Control;
        for (t = 0f; t <= 1f; t += 0.001f)
        {
            var p = GetPosition(a, b, t);
            var d = Vector3.Distance(lastPosition, p);
            currentDistance += d;
            if (currentDistance >= distance)
                return t;

            lastPosition = p;
        }

        return t;
    }

    public float GetSegmentLength(TfragSplineVertex a, TfragSplineVertex b, float tStart, float tEnd)
    {
        const float step = 0.01f;
        float distance = 0f;

        var lastPos = GetPosition(a, b, tStart);

        for (float t = tStart + step; t <= tEnd; t += step)
        {
            var curPos = GetPosition(a, b, t);
            distance += Vector3.Distance(curPos, lastPos);
            lastPos = curPos;
        }

        return distance;
    }

    public List<TfragSplinePoint> ComputePathPoints()
    {
        var pointsT = new List<TfragSplinePoint>();

        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2)
            return null;

        var hash = ComputeHash();
        if (m_CachedPathPointsHash == hash && m_CachedPathPoints != null)
            return m_CachedPathPoints.ToList();

        // compute length of curve
        var length = 0f;
        for (int i = 0; i < (vertices.Length - 1); ++i)
            length += GetSegmentLength(vertices[i], vertices[i + 1], 0, 1);

        var lengthStep = m_TfragSize;
        var currentLength = 0f;
        while (currentLength < length)
        {
            var point = GetPointOnPath(currentLength);
            pointsT.Add(point);
            currentLength += lengthStep;
        }

        // add end
        if (currentLength != length)
        {
            pointsT.Add(new TfragSplinePoint(vertices[vertices.Length - 2], vertices[vertices.Length - 1], 1, 1));
        }

        m_CachedPathPointsHash = hash;
        m_CachedPathPoints = pointsT.ToList();
        return pointsT;
    }

    public Vector3[] ComputePath()
    {
        var pointsT = ComputePathPoints();

        // build points
        Vector3[] points = new Vector3[pointsT.Count];
        for (int i = 0; i < pointsT.Count; ++i)
        {
            points[i] = pointsT[i].GetPosition();
        }

        return points;
    }

    private Vector3 GetPosition(TfragSplineVertex a, TfragSplineVertex b, float t)
    {
        float invT = 1 - t;
        return (a.Control * Mathf.Pow(invT, 3)) +
            (a.HandleOut * 3 * t * Mathf.Pow(invT, 2)) +
            (b.HandleIn * 3 * Mathf.Pow(t, 2) * invT) +
            (b.Control * Mathf.Pow(t, 3));
    }

    private Vector3 GetTangent(TfragSplineVertex a, TfragSplineVertex b, float t)
    {
        float delta = 0.001f;
        if (t >= 1f)
        {
            return ((GetPosition(a, b, t) - GetPosition(a, b, t - delta)) / delta).normalized;
        }
        else
        {
            return ((GetPosition(a, b, t + delta) - GetPosition(a, b, t)) / delta).normalized;
        }
    }

    private Vector3 GetNormal(TfragSplineVertex a, TfragSplineVertex b, float t)
    {
        var tan = GetTangent(a, b, t);
        var normal = Vector3.Cross(tan, Vector3.right);
        if (normal.sqrMagnitude == 0f)
            return Vector3.Cross(tan, Vector3.up);

        return normal;
    }

    public class TfragSplinePoint
    {
        public TfragSplineVertex From { get; set; }
        public TfragSplineVertex To { get; set; }
        public float T { get; set; }
        public float L { get; set; }
        

        public Vector3 GetConstantWidth(float sliceT) => Vector3.Lerp(From.GetConstantWidth(sliceT), To.GetConstantWidth(sliceT), L);
        public Vector3 GetConstantNormal(float sliceT) => Vector3.Slerp(From.GetConstantNormal(sliceT), To.GetConstantNormal(sliceT), L);
        public Vector3 GetSplineWidth(float sliceT) => Vector3.Slerp(From.GetSplineWidth(sliceT), To.GetSplineWidth(sliceT), L);
        public Vector3 GetSplineNormal(float sliceT) => Vector3.Slerp(From.GetSplineNormal(sliceT), To.GetSplineNormal(sliceT), L);
        public Vector3 GetWidth(TfragSplineWidthMode mode, float sliceT) => mode == TfragSplineWidthMode.Spline ? GetSplineWidth(sliceT) : GetConstantWidth(sliceT);
        public Vector3 GetPosition() => GetPosition(From, To, T);

        public Vector3 GetNormal(TfragSplineWidthMode mode, float sliceT)
        {
            var wdt = mode == TfragSplineWidthMode.Spline ? (1f / (From.WidthSpline.ComputePath().Length - 1)) : 0.01f;
            var pdt = 0.01f;
            var pos0 = GetPosition(From, To, T - pdt);
            var pos1 = GetPosition(From, To, T);
            var pos2 = GetPosition(From, To, T + pdt);

            var widthTangent = sliceT >= wdt ? (GetWidth(mode, sliceT) - GetWidth(mode, sliceT - wdt)) : (GetWidth(mode, sliceT + wdt) - GetWidth(mode, sliceT));
            var splineTangentWS = T > pdt ? (pos1 - pos0) : (pos2 - pos1);
            var splineTangent = Vector3.Slerp(From.transform.worldToLocalMatrix.MultiplyVector(splineTangentWS), To.transform.worldToLocalMatrix.MultiplyVector(splineTangentWS), T).normalized;

            return Vector3.Cross(splineTangent, widthTangent).normalized;
        }

        public TfragSplinePoint(TfragSplineVertex from, TfragSplineVertex to, float t, float l)
        {
            From = from;
            To = to;
            T = t;
            L = l;
        }

        private static Vector3 GetPosition(TfragSplineVertex a, TfragSplineVertex b, float t)
        {
            float invT = 1 - t;
            return (a.Control * Mathf.Pow(invT, 3)) +
                (a.HandleOut * 3 * t * Mathf.Pow(invT, 2)) +
                (b.HandleIn * 3 * Mathf.Pow(t, 2) * invT) +
                (b.Control * Mathf.Pow(t, 3));
        }
    }

    #endregion

    #region Gizmos

    private void OnDrawGizmos()
    {
        if (!IsSplineSelected()) return;

        var gizmosMatrix = Gizmos.matrix;
        var vertices = GetVertices();
        if (vertices == null || vertices.Length < 2)
            return;

        var path = ComputePathPoints();
        if (path == null) return;

        var count = path.Count;
        for (int i = 0; i < count; ++i)
        {
            var nextI = (i + 1) >= count ? (m_Loop ? 0 : (i - 1)) : (i + 1);
            var pos1 = path[i].GetPosition();
            var pos2 = path[nextI].GetPosition();
            var tan = (pos2 - pos1).normalized;
            if (nextI < i) tan *= -1;
            if (tan == Vector3.zero && i > 0) tan = i > 0 ? (pos1 - path[i - 1].GetPosition()).normalized : this.transform.forward;

            var mVertex = Matrix4x4.TRS(pos1, Quaternion.LookRotation(tan, this.transform.up), Vector3.one);
            if (mVertex.determinant < 0)
                mVertex = Matrix4x4.TRS(pos1, Quaternion.LookRotation(tan, -this.transform.up), Vector3.one);

            Gizmos.color = Color.white;
            Gizmos.DrawLine(pos1, pos2);

            if (m_DrawPoints)
            {
                Gizmos.DrawSphere(pos1, 0.3f);
                Handles.Label(pos1 + Vector3.up * 0.4f, $"{i}");
            }

            // draw rotation
            if (m_DrawRotation)
            {
                Gizmos.matrix = mVertex;
                Gizmos.color = Color.blue;
                Gizmos.DrawLine(Vector3.zero, Vector3.forward);
                Gizmos.color = Color.green;
                Gizmos.DrawLine(Vector3.zero, Vector3.up);
                Gizmos.color = Color.red;
                Gizmos.DrawLine(Vector3.zero, Vector3.right);
                Gizmos.matrix = gizmosMatrix;
            }
        }

        m_ComputedNumPoints = path.Count;
    }

    private void DrawLineGizmos(TfragSplineVertex a, TfragSplineVertex b, float time, Vector3 drawFrom)
    {
        // draw
        var pos = GetPosition(a, b, time);
        var tan = GetTangent(a, b, time);
        var normal = Vector3.up; // GetNormal(Points[i], Points[i + 1], t);
        var bitangent = Vector3.Cross(tan, normal);
        var rot = Quaternion.LookRotation(tan, normal);

        Gizmos.DrawLine(drawFrom, pos);

        // draw rotation
        if (m_DrawRotation)
        {
            Gizmos.color = Color.blue;
            Gizmos.DrawLine(pos, pos + tan);
            Gizmos.color = Color.green;
            Gizmos.DrawLine(pos, pos + normal);
            Gizmos.color = Color.red;
            Gizmos.DrawLine(pos, pos + bitangent);
        }
    }

    public bool IsSplineSelected()
    {
        // this is selected
        if (Selection.activeGameObject == this.gameObject) return true;

        var vertices = GetVertices();
        if (vertices == null) return false;

        // any child vertex is selected
        if (vertices.Any(v => v.gameObject == Selection.activeGameObject))
            return true;

        return false;
    }

    #endregion


}

public class TfragSplineNear
{
    public TfragSplineVertex Vertex { get; set; }
    public float Time { get; set; }
    public Vector3 Position { get; set; }
}

public class TfragSplineBuiltPoint
{
    public Matrix4x4 Transform { get; set; }
    public TfragSpline.TfragSplinePoint Point { get; set; }

    public TfragSplineBuiltPoint(Matrix4x4 transform, TfragSpline.TfragSplinePoint point)
    {
        Transform = transform;
        Point = point;
    }
}

[Serializable]
public class TfragSplineTexture
{
    [Header("Texture")]
    public Texture2D m_Texture;
    [ColorUsage(showAlpha: false)] public Color m_Tint = Color.white;
    public TextureSize m_TextureSize = TextureSize._128;
    [CollisionId] public string m_CollisionId = "2f";

    [Header("UV")]
    public bool m_UvWorldSpaceU;
    public bool m_UvWorldSpaceV;
    public Vector2 m_UvTiling = Vector2.one;
    public Vector2 m_UvOffset = Vector2.zero;
    [Range(0, 4)] public int m_UvRotation = 0;

    [Header("Spline")]
    public Vector2Int m_AppearAfter;
}
