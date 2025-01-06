using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[Obsolete]
public enum CuboidType
{
    None,
    Player,
    HillSquare,
    HillCircle,
    Camera,
}

[Obsolete]
public enum CuboidSubType
{
    Default,
    BlueFlagSpawn,
    RedFlagSpawn,
    GreenFlagSpawn,
    OrangeFlagSpawn,
}

[Serializable, Flags]
public enum CuboidMaskType
{
    None = 0,
    Player = 1 << 0,
    BlueFlagSpawn = 1 << 1,
    RedFlagSpawn = 1 << 2,
    GreenFlagSpawn = 1 << 3,
    OrangeFlagSpawn = 1 << 4,
    HillSquare = 1 << 5,
    HillCircle = 1 << 6,
    Camera = 1 << 7,
}

[ExecuteInEditMode, SelectionBase, AddComponentMenu("")]
public class Cuboid : RenderSelectionBase
{
    const int CUBOID_VERSION = 1;

    [Obsolete, SerializeField, HideInInspector]
    private CuboidType Type;
    [Obsolete, SerializeField, HideInInspector]
    private CuboidSubType Subtype;

    [SerializeField] public CuboidMaskType CuboidType;
    [SerializeField, HideInInspector] private int _version = 0;

    private bool changed = true;
    private bool lastHidden = false;
    private bool lastPicking = false;
    private bool lastSelected = false;
    private CuboidMaskType lastType = (CuboidMaskType)100;
    private GameObject assetInstance;

    public Renderer[] GetRenderers() => assetInstance?.GetComponentsInChildren<Renderer>();
    public bool IsPlayerSpawn => CuboidType.HasFlag(CuboidMaskType.Player) || CuboidType.HasFlag(CuboidMaskType.BlueFlagSpawn)
        || CuboidType.HasFlag(CuboidMaskType.RedFlagSpawn) || CuboidType.HasFlag(CuboidMaskType.GreenFlagSpawn)
        || CuboidType.HasFlag(CuboidMaskType.OrangeFlagSpawn);

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

        if (CuboidType != lastType)
        {
            changed = true;
            if (assetInstance) DestroyImmediate(assetInstance.gameObject);
            lastType = CuboidType;
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
        Upgrade();
        UpdateTransform();
        UpdateMaterials();
    }

    private void UpdateTransform()
    {
        if (assetInstance)
        {
            if (CuboidType.HasFlag(CuboidMaskType.HillCircle))
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
        var prefab = UnityHelper.GetCuboidPrefab(CuboidType);
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
        this.transform.rotation = rot;
        this.transform.localScale = scale;
    }

    public void Write(BinaryWriter writer)
    {
        var worldMatrix = this.transform.localToWorldMatrix; //Matrix4x4.TRS(this.transform.position, this.transform.rotation, this.transform.lossyScale);
        var trs = worldMatrix.SwizzleXZY();
        var inverse = worldMatrix.inverse.SwizzleXZY();
        var offset = writer.BaseStream.Position;

        for (int i = 0; i < 16; ++i)
            writer.Write(trs[i]);
        for (int i = 0; i < 12; ++i)
            writer.Write(inverse[i]);

        var iEuler = -MathHelper.WrapEuler((this.transform.rotation * Quaternion.Euler(0, -90, 0)).eulerAngles).SwizzleXZY();
        writer.Write(iEuler.x * Mathf.Deg2Rad);
        writer.Write(iEuler.y * Mathf.Deg2Rad);
        writer.Write(iEuler.z * Mathf.Deg2Rad);
        writer.Write(0f);

        if (CuboidType.HasFlag(CuboidMaskType.HillCircle))
        {
            writer.BaseStream.Position = offset + 0x20;
            var v = new Vector3(trs[8], trs[9], trs[10]).normalized * 2;
            writer.Write(v.x);
            writer.Write(v.y);
            writer.Write(v.z);
            writer.BaseStream.Position = offset + 0x80;
        }
        else if (CuboidType.HasFlag(CuboidMaskType.HillSquare))
        {
            writer.BaseStream.Position = offset + 0x20;
            var v = new Vector3(trs[8], trs[9], trs[10]).normalized;
            writer.Write(v.x);
            writer.Write(v.y);
            writer.Write(v.z);
            writer.BaseStream.Position = offset + 0x80;
        }
        else if (CuboidType.HasFlag(CuboidMaskType.Player))
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


    #region Versioning

    public void InitializeVersion()
    {
        _version = CUBOID_VERSION;
    }

    private void Upgrade()
    {
        // wait for import to finish before upgrading
        if (LevelImporterWindow.IsImporting) return;

        // upgrade
        if (_version < CUBOID_VERSION)
        {
            while (_version < CUBOID_VERSION)
            {
                RunMigration(_version + 1);
                ++_version;
            }

            Debug.Log($"Cuboid upgraded to v{_version}");
            UnityHelper.MarkActiveSceneDirty();
        }
        else if (_version > CUBOID_VERSION)
        {
            _version = CUBOID_VERSION;
        }
    }

    private void RunMigration(int version)
    {
        switch (version)
        {
            case 1: // CONVERT CUBOIDTYPE + CUBOIDSUBTYPE TO CUBOIDMASKTYPE
                {
                    if (Type == global::CuboidType.Player)
                    {
                        CuboidType |= CuboidMaskType.Player;

                        switch (Subtype)
                        {
                            case CuboidSubType.BlueFlagSpawn: CuboidType |= CuboidMaskType.BlueFlagSpawn; break;
                            case CuboidSubType.RedFlagSpawn: CuboidType |= CuboidMaskType.RedFlagSpawn; break;
                            case CuboidSubType.GreenFlagSpawn: CuboidType |= CuboidMaskType.GreenFlagSpawn; break;
                            case CuboidSubType.OrangeFlagSpawn: CuboidType |= CuboidMaskType.OrangeFlagSpawn; break;
                        }
                    }
                    else if (Type == global::CuboidType.HillSquare)
                    {
                        CuboidType |= CuboidMaskType.HillSquare;
                    }
                    else if (Type == global::CuboidType.HillCircle)
                    {
                        CuboidType |= CuboidMaskType.HillCircle;
                    }
                    else if (Type == global::CuboidType.Camera)
                    {
                        CuboidType |= CuboidMaskType.Camera;
                    }
                    break;
                }
        }
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
