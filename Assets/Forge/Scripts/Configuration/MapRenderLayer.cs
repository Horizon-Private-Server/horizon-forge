using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MapRenderLayer : MonoBehaviour
{
    public Color Color = Color.white;
    public bool AppearInRender = true;

    public void OnPreMapRender()
    {
        var mpb = new MaterialPropertyBlock();
        var outline = new MaterialPropertyBlock();
        var renderers = this.GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            renderer.GetPropertyBlock(mpb);
            mpb.SetColor("_LayerColor", AppearInRender ? Color : Color.clear);
            mpb.SetInt("_RenderIgnore", AppearInRender ? 0 : 1);
            mpb.SetFloat("_Outline", 1f);
            mpb.SetColor("_OutlineColor", Color.red);
            renderer.SetPropertyBlock(mpb);
        }
    }

    public void OnPostMapRender()
    {
        var mpb = new MaterialPropertyBlock();
        var outline = new MaterialPropertyBlock();
        var renderers = this.GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            renderer.GetPropertyBlock(mpb);
            mpb.SetInt("_RenderIgnore", 0);
            renderer.SetPropertyBlock(mpb);
        }
    }
}
