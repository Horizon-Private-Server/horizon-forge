using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase, AddComponentMenu("")]
public class RatchetCamera : RenderSelectionBase, IPVarObject
{
    const int CAMERA_VERSION = 0;

    [ReadOnly] public int RCVersion = 0;
    public int CameraType = 0;

    [SerializeField, HideInInspector] private int _version = 0;
    [SerializeField, HideInInspector] public byte[] PVars;
    [SerializeField, HideInInspector] public SerializableStringDictionary PVarValues;
    [SerializeField, HideInInspector] public SerializableMonoBehaviourDictionary PVarReferences;
    [HideInInspector] public string[] PVarStrings;

    public int GetRCVersion() => RCVersion;
    public byte[] GetPVarData() => PVars;
    public SerializableStringDictionary GetPVarValues() => PVarValues;
    public SerializableMonoBehaviourDictionary GetPVarReferences() => PVarReferences;
    public string[] GetPVarStrings() => PVarStrings;
    public PvarOverlay GetPVarOverlay() => PvarOverlay.GetPvarOverlay(this.RCVersion, cameraType: CameraType);
    public void SetPVarData(byte[] pvarData) => PVars = pvarData;
    public void SetPVarValues(SerializableStringDictionary pvarValues) => PVarValues = pvarValues;
    public void SetPVarReferences(SerializableMonoBehaviourDictionary pvarRefs) => PVarReferences = pvarRefs;
    public void SetPVarStrings(string[] strings) => PVarStrings = strings;


    private bool changed = true;
    private bool lastHidden = false;
    private bool lastPicking = false;
    private bool lastSelected = false;
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

        return false;
    }

    private void OnEnable()
    {
        if (PVarValues != null) PVarValues.Owner = this;
        if (PVarReferences != null) PVarReferences.Owner = this;
    }

    private void OnValidate()
    {
        Upgrade();
        UpdateTransform();
        UpdateMaterials();
        InitializePVarReferences();
    }

    private void UpdateTransform()
    {
        if (assetInstance)
        {
            assetInstance.transform.localScale = Vector3.one;
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
        var prefab = UnityHelper.GetCuboidPrefab(CuboidMaskType.Camera);
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

    #region PVars

    public void InitializePVarReferences(bool useDefault = false)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var pvarOverlay = PvarOverlay.GetPvarOverlay(RCVersion, cameraType: this.CameraType);
        if (pvarOverlay != null)
        {
            UnityHelper.ValidatePVars(mapConfig, this);

            if (useDefault && !string.IsNullOrEmpty(pvarOverlay.Name))
                this.name = pvarOverlay.Name;

            UnityHelper.InitializePVars(mapConfig, this, useDefault: useDefault);
        }
    }

    public void UpdatePVars()
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        UnityHelper.ValidatePVars(mapConfig, this);
        UnityHelper.UpdatePVars(mapConfig, this, this.RCVersion);
    }

    #endregion

    #region Binary

    public void Read(BinaryReader reader)
    {
        CameraType = reader.ReadInt32();
        var pos = new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle()).SwizzleXZY();
        var euler = new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle()).SwizzleXZY();

        this.transform.position = pos;
        this.transform.rotation = Quaternion.Euler(euler) * Quaternion.Euler(0, 90, 0);
        this.transform.localScale = Vector3.one;
    }

    public void Write(BinaryWriter writer)
    {
        var euler = MathHelper.WrapEuler((this.transform.rotation * Quaternion.Euler(0, -90f, 0)).eulerAngles) * Mathf.Deg2Rad;

        writer.Write(CameraType);
        writer.Write(this.transform.position.x);
        writer.Write(this.transform.position.z);
        writer.Write(this.transform.position.y);
        writer.Write(euler.x);
        writer.Write(euler.z);
        writer.Write(euler.y);
        writer.Write(0);
    }

    #endregion

    #region Versioning

    public void InitializeVersion()
    {
        _version = CAMERA_VERSION;
    }

    private void Upgrade()
    {
        // wait for import to finish before upgrading
        if (LevelImporterWindow.IsImporting) return;

        // upgrade
        if (_version < CAMERA_VERSION)
        {
            while (_version < CAMERA_VERSION)
            {
                RunMigration(_version + 1);
                ++_version;
            }

            Debug.Log($"Camera upgraded to v{_version}");
            UnityHelper.MarkActiveSceneDirty();
        }
        else if (_version > CAMERA_VERSION)
        {
            _version = CAMERA_VERSION;
        }
    }

    private void RunMigration(int version)
    {
        switch (version)
        {
        }
    }

    #endregion

}
