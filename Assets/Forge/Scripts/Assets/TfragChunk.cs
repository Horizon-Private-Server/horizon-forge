using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase, AddComponentMenu("")]
public class TfragChunk : MonoBehaviour, IOcclusionData, IAsset
{
    public static bool RenderOctants;

    [SerializeField, HideInInspector] public byte[] HeaderBytes;
    [SerializeField, HideInInspector] public byte[] DataBytes;
    public float DZOBrightness = 1f;

    [HideInInspector, SerializeField] private Vector3[] _octants;
    [SerializeField] private int _occlusionId;

    [SerializeField] private List<TfragManipulator> _manipulators;
    private MaterialPropertyBlock _mpb;
    private Renderer[] _renderers;

    public Vector3[] Octants { get => _octants; set => _octants = value; }
    public int OcclusionId { get => _occlusionId; set => _occlusionId = value; }
    public OcclusionDataType OcclusionType => OcclusionDataType.Tfrag;

    public GameObject GameObject => this ? this.gameObject : null;
    public bool IsHidden => SceneVisibilityManager.instance.IsHidden(this.gameObject);

    private void Start()
    {
        IOcclusionData.AllOcclusionDatas.Remove(this);
        IOcclusionData.AllOcclusionDatas.Add(this);
        IOcclusionData.ForceUniqueOcclusionId(this);

        AssetUpdater.RegisterAsset(this);
        UpdateAsset();
    }

    private void OnDestroy()
    {
        IOcclusionData.AllOcclusionDatas.Remove(this);
        AssetUpdater.UnregisterAsset(this);
    }

    private void OnDrawGizmosSelected()
    {
        UpdateManipulators();
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

    public void OnPreBake(Color32 uidColor)
    {
        this.gameObject.layer = LayerMask.NameToLayer("TFRAG");

        var mpb = new MaterialPropertyBlock();
        var renderers = GetComponentsInChildren<MeshRenderer>();
        if (renderers != null)
        {
            foreach (var renderer in renderers)
            {
                renderer.GetPropertyBlock(mpb);

                mpb.SetColor("_IdColor", uidColor);
                mpb.SetInteger("_Id", OcclusionId);
                mpb.SetInteger("_Picking", !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject) ? 1 : 0);
                mpb.SetInteger("_Selected", Selection.activeGameObject == this.gameObject ? 1 : 0);
                mpb.SetInteger("_Tfrag", 1);
                //mpb.SetFloat("_DoubleSidedEnable", 1);
                renderer.SetPropertyBlock(mpb);
            }
        }

        //base.OnPreBake(uidColor);
    }

    public void OnPostBake()
    {

    }

    public void UpdateAsset()
    {
        var hidden = SceneVisibilityManager.instance.IsHidden(this.gameObject);
        var selected = Selection.activeGameObject == this.gameObject || Selection.gameObjects.Contains(this.gameObject);
        var picking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);

        var mpb = new MaterialPropertyBlock();
        var renderers = GetComponentsInChildren<MeshRenderer>();
        if (renderers != null)
        {
            foreach (var renderer in renderers)
            {
                renderer.GetPropertyBlock(mpb);
                mpb.SetInteger("_Faded2", hidden ? 1 : 0);
                mpb.SetInteger("_Picking", picking ? 1 : 0);
                mpb.SetInteger("_Selected", selected ? 1 : 0);
                mpb.SetColor("_Color", new Color(2, 2, 2, 0.5f));
                mpb.SetInteger("_VertexColors", 1);
                mpb.SetInteger("_Tfrag", 1);
                renderer.SetPropertyBlock(mpb);
            }
        }
    }

    public void GetData(List<Material> materials, out byte[] header, out byte[] data)
    {
        header = HeaderBytes?.ToArray();
        data = DataBytes?.ToArray();

        // add to materials list
        var renderer = GetComponent<MeshRenderer>();
        if (renderer)
        {
            var texIdxs = new int[renderer.sharedMaterials.Length];
            for (int m = 0; m < renderer.sharedMaterials.Length; ++m)
            {
                var mat = renderer.sharedMaterials[m];
                var idx = materials.IndexOf(mat);
                if (idx < 0)
                {
                    idx = materials.Count;
                    materials.Add(mat);
                }

                texIdxs[m] = idx;
            }

            TfragHelper.SetChunkTextureIndices(header, data, texIdxs);
        }

        // apply transformation
        TfragHelper.TransformChunk(header, data, transform.localToWorldMatrix.SwizzleXZY());

        // collapse
        ValidateManipulators();
        if (_manipulators != null)
        {
            foreach (var manipulator in _manipulators.Where(x => x && x.IsEnabled))
            {
                manipulator.Apply(ref header, ref data);
            }
        }
    }

    #region Manipulators

    public void UpdateManipulators()
    {
        if (_mpb == null) _mpb = new MaterialPropertyBlock();
        if (_renderers == null) _renderers = GetComponentsInChildren<MeshRenderer>();

        var manipulators = _manipulators?.Where(x => x && x.IsEnabled);
        if (_renderers != null)
        {
            foreach (var renderer in _renderers.Where(x => x))
            {
                renderer.GetPropertyBlock(_mpb);
                TfragManipulator.ApplyMaterial(_mpb, manipulators);
                renderer.SetPropertyBlock(_mpb);
            }
        }
    }

    public void RegisterManipulator(TfragManipulator manipulator)
    {
        if (_manipulators == null) _manipulators = new List<TfragManipulator>();
        if (!_manipulators.Contains(manipulator)) _manipulators.Add(manipulator);
    }

    public void UnregisterManipulator(TfragManipulator manipulator)
    {
        if (_manipulators == null) _manipulators = new List<TfragManipulator>();
        _manipulators.Remove(manipulator);
    }

    public void ValidateManipulators()
    {
        if (_manipulators == null) _manipulators = new List<TfragManipulator>();

        _manipulators.RemoveAll(x => !x || !x.IsEnabled || x.Tfrags == null || !x.Tfrags.Contains(this));
    }

    #endregion

}
