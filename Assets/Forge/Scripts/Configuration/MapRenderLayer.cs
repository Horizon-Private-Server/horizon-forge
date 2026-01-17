using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MapRenderLayer : MonoBehaviour
{
    public static bool ForceRender = false;

    public Color Color = Color.white;
    public bool AppearInRender = true;

    [Header("Depth")]
    public bool RenderAsDepth = false;
    [Range(0f, 1f)] public float DepthLowClamp = 0f;
    [Range(0f, 1f)] public float DepthMidpoint = 0.5f;
    [Range(0f, 1f)] public float DepthHighClamp = 1f;
    public float DepthRamp = 1f;
    public Color DepthLowColor = Color.black;
    public Color DepthHighColor = Color.white;
    [Min(2)] public int DepthQuantizeCount = 100;

    [Header("Clip")]
    [Range(0f, 1f)] public float ClipLow = 0f;
    [Range(0f, 1f)] public float ClipHigh = 1f;

    private void OnValidate()
    {
        if (UnityHelper.IsObjectPrefabFile(this.gameObject)) return;

        if (ForceRender)
            OnPreMapRender();
    }

    public void OnPreMapRender()
    {
        var mpb = new MaterialPropertyBlock();
        var outline = new MaterialPropertyBlock();
        var renderers = this.GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            // only affect children under this layer
            var layer = renderer.GetComponentInParent<MapRenderLayer>();
            if (layer != this) continue;

            renderer.GetPropertyBlock(mpb);
            mpb.SetColor("_LayerColor", AppearInRender ? Color : Color.clear);
            mpb.SetFloat("_RenderIgnore", AppearInRender ? 0 : 1);
            mpb.SetInteger("_MapRenderDepth", RenderAsDepth ? 1 : 0);
            mpb.SetColor("_MapRenderDepthNearColor", DepthHighColor);
            mpb.SetColor("_MapRenderDepthFarColor", DepthLowColor);
            mpb.SetVector("_MapRenderDepthRange", new Vector4(DepthLowClamp, DepthHighClamp, DepthMidpoint, DepthRamp));
            mpb.SetInteger("_MapRenderDepthQuantizeCount", DepthQuantizeCount);
            mpb.SetVector("_MapRenderClip", new Vector2(ClipLow, ClipHigh));
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
            mpb.SetFloat("_RenderIgnore", 0);
            renderer.SetPropertyBlock(mpb);
        }
    }
}
