using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class RenderHandle
{
    const string CHILD_ASSET_PS2_NAME = "renderHandleAssetInstance";

    public GameObject AssetInstance { get; private set; }

    public Matrix4x4 Reflection { get => _reflection; set { _changed |= value != _reflection; _reflection = value; } }
    public Quaternion Rotation { get => _rotation; set { _changed |= value != _rotation; _rotation = value; } }
    public Vector3 Offset { get => _offset; set { _changed |= value != _offset; _offset = value; } }
    public Vector3 Scale { get => _scale; set { _changed |= value != _scale; _scale = value; } }
    public bool IsSelected { get => _isSelected; set { _changed |= value != _isSelected; _isSelected = value; } }
    public bool IsHidden { get => _isHidden; set { _changed |= value != _isHidden; _isHidden = value; } }
    public bool IsPicking { get => _isPicking; set { _changed |= value != _isPicking; _isPicking = value; } }
    public int WorldLightIndex { get => _worldLightingIndex; set { _changed |= value != _worldLightingIndex; _worldLightingIndex = value; } }
    public Color IdColor { get => _idColor; set { _changed |= value != _idColor; _idColor = value; } }

    private bool _changed = false;
    private Matrix4x4 _reflection = Matrix4x4.identity;
    private Quaternion _rotation = Quaternion.identity;
    private Vector3 _offset = Vector3.zero;
    private Vector3 _scale = Vector3.one;
    private bool _isSelected = false;
    private bool _isHidden = false;
    private bool _isPicking = false;
    private int _worldLightingIndex = 0;
    private Color _idColor = Color.clear;
    private GameObject _prefab = null;
    private IRenderHandlePrefab _assetInstanceHandle = null;
    private Action<Renderer, MaterialPropertyBlock> _updateRenderer;

    public RenderHandle(Action<Renderer, MaterialPropertyBlock> updateRenderer)
    {
        _updateRenderer = updateRenderer;
    }

    public void DestroyAsset()
    {
        DestroyPS2Asset();
    }

    public void UpdateMaterials()
    {
        UpdatePS2AssetMaterials();

        _changed = false;
    }

    #region PS2 Asset

    public void Update(GameObject parent, GameObject prefab)
    {
        if (prefab != _prefab || !AssetInstance)
        {
            CreatePS2Asset(parent, prefab);
        }

        if (AssetInstance && !AssetInstance.hideFlags.HasFlag(HideFlags.HideInInspector))
        {
            UnityHelper.RecurseHierarchy(AssetInstance.transform, (t) =>
            {
                Hide(t.gameObject);
            });
        }

        if (_changed)
        {
            UpdateMaterials();
            _changed = false;
        }

        if (AssetInstance)
        {
            AssetInstance.transform.localRotation = Rotation;
            AssetInstance.transform.localPosition = Offset;
            AssetInstance.transform.localScale = Scale;
        }
    }

    private void CreatePS2Asset(GameObject parent, GameObject prefab)
    {
        DestroyPS2Asset();

        // destroy all hidden children
        for (int i = 0; i < parent.transform.childCount; ++i)
        {
            var child = parent.transform.GetChild(i);
            if (child.name == CHILD_ASSET_PS2_NAME)
            {
                GameObject.DestroyImmediate(child.gameObject);
                --i;
            }
        }

        _prefab = prefab;
        if (prefab)
        {
            try
            {
                AssetInstance = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
                if (AssetInstance)
                {
                    _assetInstanceHandle = AssetInstance.GetComponent<IRenderHandlePrefab>();

                    UnityHelper.RecurseHierarchy(AssetInstance.transform, (t) => t.gameObject.layer = parent.layer);
                    UnityHelper.RecurseHierarchy(AssetInstance.transform, (t) =>
                    {
                        Hide(t.gameObject);
                    });

                    AssetInstance.name = CHILD_ASSET_PS2_NAME;
                    AssetInstance.transform.localRotation = Rotation;
                    AssetInstance.transform.localPosition = Offset;
                    AssetInstance.transform.localScale = Scale;
                    AssetInstance.transform.SetParent(parent.transform, false);
                    UpdatePS2AssetMaterials();
                }
            }
            catch (Exception ex)
            {
                Debug.LogError(ex);
                DestroyPS2Asset();
            }
        }
    }

    private void UpdatePS2AssetMaterials()
    {
        if (!AssetInstance) return;

        var mpb = new MaterialPropertyBlock();
        var renderers = AssetInstance.GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            // fix reflection causing objects to clip early
            // expands bounds so renderer renders even in periphery
            renderer.ResetLocalBounds();
            renderer.ResetBounds();
            var b = renderer.bounds;
            b.size += 1000f * Vector3.one;
            //var b = new Bounds(_reflection.MultiplyPoint(renderer.bounds.center)/100f, _reflection.GetScale().magnitude * renderer.bounds.size);
            renderer.bounds = b;
            renderer.enabled = !IsHidden;

            renderer.GetPropertyBlock(mpb);
            mpb.SetMatrix("_Reflection2", Reflection);
            mpb.SetInteger("_Selected", IsSelected ? 1 : 0);
            mpb.SetInteger("_Faded2", IsHidden ? 1 : 0);
            mpb.SetInteger("_Picking", IsPicking ? 1 : 0);
            mpb.SetInteger("_WorldLightIndex", WorldLightIndex);
            mpb.SetInteger("_Reflection", Reflection != Matrix4x4.identity ? 1 : 0);
            mpb.SetColor("_IdColor", _idColor);

            // pass to parent
            if (_updateRenderer != null)
                _updateRenderer(renderer, mpb);

            renderer.SetPropertyBlock(mpb);

            renderer.allowOcclusionWhenDynamic = false;
            //var mf = renderer.GetComponent<MeshFilter>();
            //if (mf && mf.sharedMesh)
            //    mf.sharedMesh.RecalculateBounds();

        }

        _assetInstanceHandle?.UpdateMaterials();
    }

    private void DestroyPS2Asset()
    {
        if (AssetInstance) GameObject.DestroyImmediate(AssetInstance);
        AssetInstance = null;
        _assetInstanceHandle = null;
    }

    #endregion

    private void Hide(GameObject go)
    {
        go.hideFlags = HideFlags.DontSave | HideFlags.NotEditable | HideFlags.HideInInspector | HideFlags.HideInHierarchy;
        //go.hideFlags = HideFlags.DontSave | HideFlags.NotEditable | HideFlags.HideInInspector;
        //go.hideFlags = HideFlags.DontSave;
    }

}

public interface IRenderHandlePrefab
{
    void UpdateMaterials();
}
