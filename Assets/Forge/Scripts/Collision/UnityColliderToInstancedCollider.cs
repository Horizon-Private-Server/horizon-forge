using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode]
[DisallowMultipleComponent]
public class UnityColliderToInstancedCollider : RenderSelectionBase, IAsset, IInstancedCollider
{
    public Collider m_Collider;
    [CollisionId] public string m_MaterialId = "2f";
    public CollisionRenderHandleNormalMode m_Normals;
    [Range(-2f, 2f)] public float m_RecalculateNormalsFactor = 1;
    [Range(0.1f, 4f), Delayed] public float m_Resolution = 1f;
    [Range(4f, 8f), Delayed] public float m_TfragSize = 4f;
    public bool m_OcclusionBakeIgnore = false;
    public bool m_Render = true;

    [SerializeField, HideInInspector] public List<TfragLayerToCollisionId> m_TerrainLayerCollisionIds = new List<TfragLayerToCollisionId>();
    [SerializeField, HideInInspector] private bool m_ForceRegenerateMesh = false;

    public GameObject GameObject => this ? this.gameObject : null;
    public bool IsHidden => collisionRenderHandle?.IsHidden ?? false;
    public CollisionRenderHandle GetInstancedCollider() => collisionRenderHandle;
    public bool HasInstancedCollider() => this.isActiveAndEnabled;

    private CollisionRenderHandle collisionRenderHandle = new CollisionRenderHandle(null);

    private void OnEnable()
    {
        m_Collider = GetComponent<Collider>();

        // check for conflicting components
        if (GetComponent<Tie>() || GetComponent<Shrub>())
        {
            Dispatcher.RunOnMainThread(() =>
            {
                if (!this || !this.gameObject) return;

                if (EditorUtility.DisplayDialog($"{this.gameObject.name} cannot have an UnityColliderToForgeCollider", "Tie and Shrubs may not have UnityColliderToForgeColliders. Please use the built in InstancedCollider toggle.\n\nThe UnityColliderToForgeCollider will be removed.", "Okay"))
                {
                    DestroyImmediate(this);
                    return;
                }
            });
        }

        AssetUpdater.RegisterAsset(this);
        UpdateAsset();
        collisionRenderHandle?.UpdateMaterials();
    }

    private void OnDisable()
    {
        AssetUpdater.UnregisterAsset(this);
        collisionRenderHandle?.DestroyAsset();
    }

    private void OnValidate()
    {
        if (UnityHelper.IsObjectPrefabFile(this.gameObject)) return;

        collisionRenderHandle?.UpdateMaterials();
    }

    public void ForceRegenerateCollider()
    {
        m_ForceRegenerateMesh = true;
        UpdateAsset();
    }

    public void UpdateAsset()
    {
        // configure collider
        if (HasInstancedCollider())
        {
            collisionRenderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject) || !m_Render;
            collisionRenderHandle.IsSelected = Selection.activeGameObject == this.gameObject || Selection.gameObjects.Contains(this.gameObject);
            collisionRenderHandle.IsPicking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);
            collisionRenderHandle.Normals = m_Normals;
            collisionRenderHandle.RecalculateNormalsFactor = m_RecalculateNormalsFactor;
            collisionRenderHandle.OcclusionIgnore = m_OcclusionBakeIgnore;
            collisionRenderHandle.Update(this.gameObject, GetMesh(), GetMeshMaterials());
        }
        else
        {
            collisionRenderHandle.Update(this.gameObject, null, null);
        }
    }

    private Mesh GetMesh()
    {
        var force = m_ForceRegenerateMesh;
        m_ForceRegenerateMesh = false;

        if (!m_Collider) return null;
        if (m_Collider is MeshCollider meshCollider) return meshCollider.sharedMesh;
        if (m_Collider is TerrainCollider terrainCollider) return GenerateFromTerrainCollider(terrainCollider, force);
        if (m_Collider is BoxCollider boxCollider) return GenerateFromBoxCollider(boxCollider);
        if (m_Collider is SphereCollider sphereCollider) return GenerateFromSphereCollider(sphereCollider);

        return null;
    }

    private int[] GetMeshMaterials()
    {
        if (m_Collider is TerrainCollider terrainCollider) return GetLayerCollisionIds(terrainCollider.terrainData);

        return new int[] { CollisionHelper.ParseId(m_MaterialId) };
    }

    #region Terrain

    private Mesh GenerateFromTerrainCollider(TerrainCollider terrainCollider, bool force)
    {
        var splatRamp = 1f;
        if (terrainCollider.GetComponent<UnityTerrainToTfrags>() is UnityTerrainToTfrags unityTerrainToTfrags)
            splatRamp = unityTerrainToTfrags.m_TfragTextureClassificationSharpness;

        return TerrainHelper.GetCollider(terrainCollider, faceSize: m_TfragSize, force: force, splatRamp: splatRamp);
    }

    private int[] GetLayerCollisionIds(TerrainData terrainData)
    {
        // default when no layers configured
        if (terrainData.terrainLayers.Length == 0)
            return new int[] { 0x2f };

        if (m_TerrainLayerCollisionIds == null) m_TerrainLayerCollisionIds = new List<TfragLayerToCollisionId>();

        var collisionIds = new int[terrainData.terrainLayers.Length];
        var defaultColId = CollisionHelper.ParseId(m_MaterialId);
        for (int i = 0; i < terrainData.terrainLayers.Length; ++i)
        {
            var layer = terrainData.terrainLayers[i];
            var mapping = m_TerrainLayerCollisionIds.FirstOrDefault(x => x.Layer == layer);
            if (mapping.Layer != layer)
            {
                m_TerrainLayerCollisionIds.Add(new TfragLayerToCollisionId() { Layer = layer });
            }

            if (!string.IsNullOrEmpty(mapping.CollisionId))
                collisionIds[i] = CollisionHelper.ParseId(mapping.CollisionId, defaultColId);
            else
                collisionIds[i] = defaultColId;
        }

        return collisionIds;
    }

    #endregion

    #region Mesh Generation

    private Mesh GenerateFromBoxCollider(BoxCollider boxCollider)
    {
        var mesh = new Mesh();

        var vertices = new Vector3[8];
        var triangles = new int[3 * 2 * 6];

        vertices[0] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(1, 1, 1)) * 0.5f;
        vertices[1] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(1, 1, -1)) * 0.5f;
        vertices[2] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(1, -1, 1)) * 0.5f;
        vertices[3] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(1, -1, -1)) * 0.5f;
        vertices[4] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(-1, 1, 1)) * 0.5f;
        vertices[5] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(-1, 1, -1)) * 0.5f;
        vertices[6] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(-1, -1, 1)) * 0.5f;
        vertices[7] = boxCollider.center + Vector3.Scale(boxCollider.size, new Vector3(-1, -1, -1)) * 0.5f;

        var i = 0;
        triangles[i++] = 2; triangles[i++] = 1; triangles[i++] = 0;
        triangles[i++] = 2; triangles[i++] = 3; triangles[i++] = 1;
        triangles[i++] = 0; triangles[i++] = 1; triangles[i++] = 4;
        triangles[i++] = 1; triangles[i++] = 5; triangles[i++] = 4;
        triangles[i++] = 4; triangles[i++] = 2; triangles[i++] = 0;
        triangles[i++] = 4; triangles[i++] = 6; triangles[i++] = 2;
        triangles[i++] = 4; triangles[i++] = 5; triangles[i++] = 6;
        triangles[i++] = 5; triangles[i++] = 7; triangles[i++] = 6;
        triangles[i++] = 6; triangles[i++] = 3; triangles[i++] = 2;
        triangles[i++] = 6; triangles[i++] = 7; triangles[i++] = 3;
        triangles[i++] = 1; triangles[i++] = 3; triangles[i++] = 5;
        triangles[i++] = 3; triangles[i++] = 7; triangles[i++] = 5;

        mesh.SetVertices(vertices);
        mesh.SetTriangles(triangles, 0);

        return mesh;
    }

    private Mesh GenerateFromSphereCollider(SphereCollider sphereCollider)
    {
        const float tau = Mathf.PI * 2;
        
        var radius = sphereCollider.radius;
        var resolution = Mathf.Clamp(m_Resolution * sphereCollider.transform.lossyScale.Max() * radius, 0.1f, 50);
        var dtheta = Mathf.Clamp(tau / resolution, 0.01f, Mathf.PI / 2f);
        var thetaCount = Mathf.CeilToInt(Mathf.PI / dtheta) + 1;
        var phiCount = Mathf.CeilToInt(tau / dtheta) + 1;

        var mesh = new Mesh();
        var vertexCount = thetaCount * phiCount;
        var vertices = new Vector3[vertexCount];
        var triangles = new int[3 * 2 * vertexCount];

        // build vertices
        int vIdx = 0, iIdx = 0;
        for (var iTheta = 0; iTheta < thetaCount; ++iTheta)
        {
            var theta = Mathf.Clamp(iTheta * dtheta, 0, Mathf.PI);

            for (var iPhi = 0; iPhi < phiCount; ++iPhi)
            {
                var phi = Mathf.Clamp(iPhi * dtheta, 0, tau) - Mathf.PI;

                // add vertex
                vertices[vIdx++] = MathHelper.PolarToCartesian(theta, phi, radius);

                // add triangles
                if (iTheta > 0 && iPhi > 0)
                {
                    var v0 = vIdx - phiCount - 2;
                    var v1 = vIdx - phiCount - 1;
                    var v2 = vIdx - 2;
                    var v3 = vIdx - 1;

                    triangles[iIdx++] = v0;
                    triangles[iIdx++] = v1;
                    triangles[iIdx++] = v2;
                    triangles[iIdx++] = v1;
                    triangles[iIdx++] = v3;
                    triangles[iIdx++] = v2;
                }
            }
        }

        mesh.SetVertices(vertices);
        mesh.SetTriangles(triangles, 0);

        return mesh;
    }

    #endregion

    #region Asset Selection

    public void OnPreBake(Color32 uidColor)
    {
        if (collisionRenderHandle == null) UpdateAsset();
        collisionRenderHandle.IdColor = uidColor;
        UpdateAsset();
    }

    public void OnPostBake()
    {

    }

    #endregion
}

[Serializable]
public struct TfragLayerToCollisionId
{
    public TerrainLayer Layer;
    [CollisionId] public string CollisionId;
}
