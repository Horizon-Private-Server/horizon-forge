using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.EventSystems;

[ExecuteInEditMode, SelectionBase]
public class Tie : RenderSelectionBase, IOcclusionData, IAsset, IInstancedCollider
{
    public static bool RenderOctants;
    private static readonly Color TIE_COLOR = new Color(1, 1, 1, 1);

    [HideInInspector] public int OClass;
    public int GroupId;

    [SerializeField, HideInInspector, ColorUsage(showAlpha: false), Tooltip("Set all vertex colors to the given color. Vertex colors do not (yet) render in Forge.")]
    public Color ColorDataValue = new Color(0.5f, 0.5f, 0.5f, 1f);
    [SerializeField, HideInInspector]
    public byte[] ColorData;

    [HideInInspector, SerializeField] private Vector3[] _octants;
    [HideInInspector, SerializeField] private int _occlusionId;

    public Matrix4x4 Reflection = Matrix4x4.identity;

    [Header("Collision")]
    [Tooltip("Enable instanced collider.")] public bool InstancedCollider;
    [Tooltip("Render instanced collider.")] public bool RenderInstancedCollider;
    public ColliderIdOverride[] InstancedColliderIdOverrides;
    [Tooltip("When set, instanced collider will use the corresponding model. Model must have correct collision materials configured.")] public GameObject InstancedColliderOverride;

    public Vector3[] Octants { get => _octants; set => _octants = value; }
    public int OcclusionId { get => _occlusionId; set => _occlusionId = value; }
    public OcclusionDataType OcclusionType => OcclusionDataType.Tie;

    public GameObject GameObject => this ? this.gameObject : null;
    public bool IsHidden => renderHandle?.IsHidden ?? false;

    public override Matrix4x4 GetSelectionReflectionMatrix() => this.Reflection;
    public override Renderer[] GetSelectionRenderers() => GetRenderers();

    public Renderer[] GetRenderers() => renderHandle?.AssetInstance?.GetComponentsInChildren<Renderer>();
    public GameObject GetAssetInstance() => renderHandle?.AssetInstance;
    public CollisionRenderHandle GetInstancedCollider() => collisionRenderHandle;
    public bool HasInstancedCollider() => InstancedCollider && Reflection.isIdentity;
    public bool HasNonUniformVertexColors() => ColorData != null && ColorData.Length > 3 && ColorData.Skip(3).Any(x => x != 0xff);
    public Color GetBaseVertexColor() => (ColorData != null && ColorData.Length > 2) ? new Color(ColorData[0] / 128f, ColorData[1] / 128f, ColorData[2] / 128f, 1f) : ColorDataValue;


    private RenderHandle renderHandle = null;
    private CollisionRenderHandle collisionRenderHandle = new CollisionRenderHandle(null);

    private void OnEnable()
    {
        IOcclusionData.AllOcclusionDatas.Remove(this);
        IOcclusionData.AllOcclusionDatas.Add(this);
        IOcclusionData.ForceUniqueOcclusionId(this);

        AssetUpdater.RegisterAsset(this);
        UpdateAsset();
    }

    private void OnDisable()
    {
        AssetUpdater.UnregisterAsset(this);
        IOcclusionData.AllOcclusionDatas.Remove(this);

        renderHandle?.DestroyAsset();
        collisionRenderHandle?.DestroyAsset();
    }

    private void OnDrawGizmosSelected()
    {
        if (!RenderOctants) return;
        if (Selection.activeGameObject != this.gameObject) return;

        if (Octants != null)
        {
            Gizmos.matrix = Matrix4x4.identity;
            Gizmos.color = Color.blue;
            foreach (var octant in Octants)
            {
                Gizmos.DrawWireCube(octant + Vector3.one * 2f, Vector3.one * 0.5f);
            }
        }
    }

    public void UpdateAsset()
    {
        if (renderHandle == null) renderHandle = new RenderHandle(OnRenderHandleRender);

        var prefab = UnityHelper.GetAssetPrefab(FolderNames.TieFolder, OClass.ToString());
        var collisionPrefab = UnityHelper.GetAssetColliderPrefab(FolderNames.TieFolder, OClass.ToString());
        renderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject);
        renderHandle.IsSelected = Selection.activeGameObject == this.gameObject || Selection.gameObjects.Contains(this.gameObject);
        renderHandle.IsPicking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);
        renderHandle.Reflection = Reflection;
        renderHandle.Update(this.gameObject, prefab);

        // configure collider
        if (HasInstancedCollider())
        {
            collisionRenderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject) || !RenderInstancedCollider;
            collisionRenderHandle.IsSelected = renderHandle.IsSelected;
            collisionRenderHandle.IsPicking = renderHandle.IsPicking;
            collisionRenderHandle.CollisionIdOverrides = InstancedColliderOverride ? null : InstancedColliderIdOverrides;
            collisionRenderHandle.Update(this.gameObject, InstancedColliderOverride ? InstancedColliderOverride : collisionPrefab);
        }
        else
        {
            collisionRenderHandle.Update(this.gameObject, null);
        }

        UpdateMaterials();
    }

    public void ValidateColors()
    {
        // determine color size
        var prefab = UnityHelper.GetAssetPrefab(FolderNames.TieFolder, OClass.ToString(), false);
        var assetPath = AssetDatabase.GetAssetPath(prefab);
        var assetDir = Path.GetDirectoryName(assetPath);
        var tieBinFilePath = Path.Combine(assetDir, "core.bin");
        if (File.Exists(tieBinFilePath))
        {
            var tieBytes = File.ReadAllBytes(tieBinFilePath);
            if (tieBytes != null && tieBytes.Length > 0x3C)
            {
                var ambientSize = BitConverter.ToInt16(tieBytes, 0x3a);
                if (ColorData == null || ColorData.Length != ambientSize)
                    ColorData = TieAsset.GenerateUniformColor(ambientSize, ColorDataValue * 0.5f);

                UpdateMaterials();
            }
        }
    }

    public void ResetAsset()
    {
        renderHandle?.DestroyAsset();
        collisionRenderHandle?.DestroyAsset();
        UpdateAsset();
    }

    public void UpdateMaterials()
    {
        renderHandle?.UpdateMaterials();
        collisionRenderHandle?.UpdateMaterials();
    }

    private void OnRenderHandleRender(Renderer renderer, MaterialPropertyBlock mpb)
    {
        mpb.SetColor("_Color", GetBaseVertexColor());
    }

    #region Binary

    public void Read(BinaryReader reader, int racVersion)
    {
        var tieClass = reader.ReadInt32();
        _ = reader.ReadBytes(8);
        var occlusionId = reader.ReadInt32();

        var worldMatrix = Matrix4x4.identity;
        for (int i = 0; i < 16; ++i)
            worldMatrix[i] = reader.ReadSingle();
        worldMatrix = worldMatrix.SwizzleXZY();
        worldMatrix.GetReflectionMatrix(out var pos, out var rot, out var scale, out var reflection);

        this.OClass = tieClass;
        this.transform.position = pos;
        this.transform.rotation = rot;
        this.transform.localScale = scale;
        this.Reflection = reflection;
        this.OcclusionId = occlusionId;
    }

    #endregion

    #region Occlusion Bake

    public void OnPreBake(Color32 uidColor)
    {
        if (renderHandle == null) UpdateAsset();
        renderHandle.IdColor = uidColor;
        collisionRenderHandle.IdColor = uidColor;
        UpdateAsset();
    }

    public void OnPostBake()
    {

    }

    #endregion
}
