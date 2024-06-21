using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase]
public class Shrub : RenderSelectionBase, IAsset, IInstancedCollider
{
    [HideInInspector] public int OClass;
    public Matrix4x4 Reflection = Matrix4x4.identity;
    public int GroupId;
    public float RenderDistance = 64;
    [ColorUsage(false)] public Color Tint = Color.white;

    [Header("Collision")]
    [Tooltip("Enable instanced collider.")] public bool InstancedCollider;
    [Tooltip("Render instanced collider.")] public bool RenderInstancedCollider;
    [Tooltip("When set, instanced collider will use the corresponding model. Model must have correct collision materials configured.")] public GameObject InstancedColliderOverride;

    public GameObject GameObject => this ? this.gameObject : null;
    public bool IsHidden => renderHandle?.IsHidden ?? false;

    public override Matrix4x4 GetSelectionReflectionMatrix() => this.Reflection;
    public override Renderer[] GetSelectionRenderers() => GetRenderers();

    public Renderer[] GetRenderers() => renderHandle?.AssetInstance?.GetComponentsInChildren<Renderer>();
    public GameObject GetAssetInstance() => renderHandle?.AssetInstance;
    public CollisionRenderHandle GetInstancedCollider() => collisionRenderHandle;
    public bool HasInstancedCollider() => InstancedCollider && Reflection.isIdentity;

    private RenderHandle renderHandle = null;
    private CollisionRenderHandle collisionRenderHandle = new CollisionRenderHandle(null);

    private void OnEnable()
    {
        AssetUpdater.RegisterAsset(this);
        UpdateAsset();
    }

    private void OnDisable()
    {
        AssetUpdater.UnregisterAsset(this);

        renderHandle?.DestroyAsset();
        collisionRenderHandle?.DestroyAsset();
    }

    public void UpdateAsset()
    {
        if (renderHandle == null) renderHandle = new RenderHandle(OnRenderHandleRender);

        var prefab = UnityHelper.GetAssetPrefab(FolderNames.ShrubFolder, OClass.ToString());
        var collisionPrefab = UnityHelper.GetAssetColliderPrefab(FolderNames.ShrubFolder, OClass.ToString());
        renderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject);
        renderHandle.IsSelected = Selection.activeGameObject == this.gameObject || Selection.gameObjects.Contains(this.gameObject);
        renderHandle.IsPicking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);
        renderHandle.Reflection = Reflection;
        renderHandle.Rotation = Quaternion.Euler(0, -90f, 0);
        renderHandle.Update(this.gameObject, prefab);

        // configure collider
        var useCollider = InstancedCollider && Reflection.isIdentity;
        if (useCollider)
        {
            collisionRenderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject) || !RenderInstancedCollider;
            collisionRenderHandle.IsSelected = renderHandle.IsSelected;
            collisionRenderHandle.IsPicking = renderHandle.IsPicking;
            collisionRenderHandle.Rotation = Quaternion.Euler(0, -90f, 0);
            collisionRenderHandle.Update(this.gameObject, InstancedColliderOverride ? InstancedColliderOverride : collisionPrefab);
        }
        else
        {
            collisionRenderHandle.Update(this.gameObject, null);
        }

        UpdateMaterials();
    }

    public void UpdateMaterials()
    {
        renderHandle?.UpdateMaterials();
        collisionRenderHandle?.UpdateMaterials();
    }

    public void ResetAsset()
    {
        renderHandle?.DestroyAsset();
        collisionRenderHandle?.DestroyAsset();
        UpdateAsset();
    }

    public void Read(BinaryReader reader, int racVersion)
    {
        var shrubClass = reader.ReadInt32();
        var renderDist = reader.ReadSingle();
        _ = reader.ReadBytes(8);

        var worldMatrix = Matrix4x4.identity;
        for (int i = 0; i < 16; ++i)
            worldMatrix[i] = reader.ReadSingle();
        worldMatrix = worldMatrix.SwizzleXZY();
        worldMatrix.GetReflectionMatrix(out var pos, out var rot, out var scale, out var reflection);

        var r = (byte)reader.ReadInt32();
        var g = (byte)reader.ReadInt32();
        var b = (byte)reader.ReadInt32();

        this.OClass = shrubClass;
        this.transform.position = pos;
        this.transform.rotation = rot;
        this.transform.localScale = scale;
        this.Reflection = reflection;
        this.Tint = new Color(r / 128f, g / 128f, b / 128f, 1);
        this.RenderDistance = renderDist;
    }

    private void OnRenderHandleRender(Renderer renderer, MaterialPropertyBlock mpb)
    {
        mpb.SetColor("_Color", Tint);
    }

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
