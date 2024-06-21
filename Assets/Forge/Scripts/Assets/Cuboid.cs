using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public enum CuboidType
{
    None,
    Player,
    HillSquare,
    HillCircle,
    Camera,
}

public enum CuboidSubType
{
    Default,
    BlueFlagSpawn,
    RedFlagSpawn,
    GreenFlagSpawn,
    OrangeFlagSpawn,
}

[ExecuteInEditMode, SelectionBase]
public class Cuboid : RenderSelectionBase
{
    public CuboidType Type;
    public CuboidSubType Subtype;

    private bool changed = true;
    private bool lastHidden = false;
    private bool lastPicking = false;
    private bool lastSelected = false;
    private CuboidType lastType = (CuboidType)100;
    private CuboidSubType lastSubtype = (CuboidSubType)100;
    private GameObject assetInstance;

    public Renderer[] GetRenderers() => assetInstance?.GetComponentsInChildren<Renderer>();

    private void Start()
    {
        Update();
    }

    void Update()
    {
        var hidden = SceneVisibilityManager.instance.IsHidden(this.gameObject);
        if (hidden != lastHidden)
        {
            changed = true;
            lastHidden = hidden;
        }

        var picking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);
        if (picking != lastPicking)
        {
            changed = true;
            lastPicking = picking;
        }

        var selected = IsSelected();
        if (selected != lastSelected)
        {
            changed = true;
            lastSelected = selected;
        }

        if (Type != lastType || Subtype != lastSubtype)
        {
            changed = true;
            if (assetInstance) DestroyImmediate(assetInstance.gameObject);
            lastType = Type;
            lastSubtype = Subtype;
        }

        if (!assetInstance)
        {
            RefreshAsset();
        }

        if (changed) UpdateMaterials();
        UpdateTransform();
    }

    private bool IsSelected()
    {
        if (!Selection.activeGameObject) return false;
        if (Selection.activeGameObject == this.gameObject) return true;

        var area = Selection.activeGameObject.GetComponent<Area>();
        if (area && area.Cuboids.Contains(this)) return true;

        return false;
    }

    private void OnValidate()
    {
        UpdateTransform();
        UpdateMaterials();
    }

    private void UpdateTransform()
    {
        if (assetInstance)
        {
            if (Type == CuboidType.HillCircle)
            {
                var scale = Vector3.one;
                scale.x = this.transform.localScale.z / this.transform.localScale.x;
                assetInstance.transform.localScale = scale;
            }
            else
            {
                assetInstance.transform.localScale = Vector3.one;
            }
        }

    }

    private void UpdateMaterials()
    {
        var renderers = GetComponentsInChildren<Renderer>();
        var mpb = new MaterialPropertyBlock();

        foreach (var renderer in renderers)
        {
            renderer.GetPropertyBlock(mpb);
            mpb.SetInteger("_Selected", lastSelected ? 1 : 0);
            mpb.SetInteger("_Faded2", lastHidden ? 1 : 0);
            mpb.SetInteger("_Picking", lastPicking ? 1 : 0);
            mpb.SetInteger("_WorldLightIndex", -1);
            renderer.SetPropertyBlock(mpb);

            renderer.allowOcclusionWhenDynamic = false;
        }

        changed = false;
    }

    public void RefreshAsset()
    {
        // destroy all children
        while (transform.childCount > 0)
            DestroyImmediate(transform.GetChild(0).gameObject);

        // instantiate
        var prefab = UnityHelper.GetCuboidPrefab(Type, Subtype);
        if (prefab)
        {
            GameObject go = null;
            try
            {
                go = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
                if (go)
                {
                    assetInstance = go;
                    go.transform.SetParent(this.transform, false);

                    UnityHelper.RecurseHierarchy(go.transform, (t) =>
                    {
                        t.gameObject.layer = gameObject.layer;
                        t.gameObject.hideFlags = HideFlags.DontSave | HideFlags.NotEditable | HideFlags.HideInInspector | HideFlags.HideInHierarchy;
                    });

                    UpdateMaterials();
                }
            }
            catch (Exception ex)
            {
                Debug.LogError(ex);
                if (go) DestroyImmediate(go);
            }
        }
    }

    #region Binary

    public void Read(BinaryReader reader)
    {
        var worldMatrix = Matrix4x4.identity;
        for (int i = 0; i < 16; ++i)
            worldMatrix[i] = reader.ReadSingle();

        for (int i = 0; i < 12; ++i)
            reader.ReadSingle();

        worldMatrix = worldMatrix.SwizzleXZY();
        worldMatrix.GetReflectionMatrix(out var pos, out var rot, out var scale, out var reflection);

        this.transform.position = pos;
        this.transform.rotation = rot * Quaternion.Euler(0, 90f, 0);
        this.transform.localScale = scale;
    }

    public void Write(BinaryWriter writer)
    {
        var trs = this.transform.localToWorldMatrix.SwizzleXZY();
        var inverse = this.transform.worldToLocalMatrix.SwizzleXZY();
        var offset = writer.BaseStream.Position;

        for (int i = 0; i < 16; ++i)
            writer.Write(trs[i]);
        for (int i = 0; i < 12; ++i)
            writer.Write(inverse[i]);

        var iEuler = (Quaternion.Inverse(this.transform.rotation) * Quaternion.Euler(0, 90f, 0)).eulerAngles.SwizzleXZY();
        writer.Write(iEuler.x * Mathf.Deg2Rad);
        writer.Write(iEuler.y * Mathf.Deg2Rad);
        writer.Write(iEuler.z * Mathf.Deg2Rad);
        writer.Write(0f);

        if (Type == CuboidType.HillCircle)
        {
            writer.BaseStream.Position = offset + 0x28;
            writer.Write(2f);
            writer.BaseStream.Position = offset + 0x80;
        }
        else if (Type == CuboidType.HillSquare)
        {
            writer.BaseStream.Position = offset + 0x28;
            writer.Write(1f);
            writer.BaseStream.Position = offset + 0x80;
        }
        else if (Type == CuboidType.Player)
        {
            writer.BaseStream.Position = offset + 0x70;
            writer.Write(0f);
            writer.Write(0f);
            writer.BaseStream.Position = offset + 0x80;
        }
    }

    #endregion

    #region Occlusion Bake

    public void OnPreBake(Color32 uidColor)
    {
        var mpb = new MaterialPropertyBlock();
        var renderers = GetRenderers();
        if (renderers != null)
        {
            foreach (var renderer in renderers)
            {
                renderer.GetPropertyBlock(mpb);

                mpb.SetColor("_IdColor", uidColor);
                //mpb.SetInteger("_Id", OcclusionId);
                //mpb.SetFloat("_DoubleSidedEnable", 1);
                renderer.SetPropertyBlock(mpb);
            }
        }
    }

    public void OnPostBake()
    {

    }

    #endregion


    public bool IsInCuboid(Vector3 position)
    {
        var dt = this.transform.worldToLocalMatrix.MultiplyPoint(position);
        var size = 1f;
        if (dt.x < size && dt.x > -size && dt.y < size && dt.y > -size && dt.z < size && dt.z > -size)
            return true;

        return false;
    }
}
