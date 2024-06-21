using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase]
public class TfragChunk : MonoBehaviour, IOcclusionData, IAsset
{
    public static bool RenderOctants;

    [SerializeField, HideInInspector] public byte[] HeaderBytes;
    [SerializeField, HideInInspector] public byte[] DataBytes;

    [HideInInspector, SerializeField] private Vector3[] _octants;
    [HideInInspector, SerializeField] private int _occlusionId;

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
                mpb.SetInteger("_VertexColors", 1);
                renderer.SetPropertyBlock(mpb);
            }
        }
    }
}
