using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[ExecuteInEditMode, SelectionBase]
public class PrefabRenderHandle : RenderSelectionBase
{
    public GameObject Prefab;

    RenderHandle renderHandle = new RenderHandle(null);

    public Renderer[] GetRenderers() => renderHandle?.AssetInstance?.GetComponentsInChildren<Renderer>();

    void Update()
    {
        renderHandle.IsHidden = SceneVisibilityManager.instance.IsHidden(this.gameObject);
        renderHandle.IsSelected = Selection.activeGameObject == this.gameObject;
        renderHandle.IsPicking = !SceneVisibilityManager.instance.IsPickingDisabled(this.gameObject);
        renderHandle.Update(this.gameObject, Prefab);
    }

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
                renderer.SetPropertyBlock(mpb);
            }
        }
    }

    public void OnPostBake()
    {

    }

}
