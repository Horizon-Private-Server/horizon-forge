using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class WaterMoby : MonoBehaviour, IRenderHandlePrefab
{
    private MeshRenderer m_Renderer;
    private Moby m_Moby;
    private MaterialPropertyBlock m_Mpb;
    private float m_Height;

    void Start()
    {
        m_Renderer = GetComponentInChildren<MeshRenderer>();
        m_Moby = GetComponentInParent<Moby>();
        m_Mpb = new MaterialPropertyBlock();

        UpdateMaterial();
    }

    // Start is called before the first frame update
    void OnEnable()
    {
        m_Renderer = GetComponentInChildren<MeshRenderer>();
        m_Moby = GetComponentInParent<Moby>();
        m_Mpb = new MaterialPropertyBlock();

        Camera.onPreCull -= OnRender;
        Camera.onPreCull += OnRender;

        UpdateMaterial();
    }

    private void OnDisable()
    {
        Camera.onPreCull -= OnRender;
    }

    private void OnRender(Camera camera)
    {
        if (m_Moby)
        {
            var cuboidHide0 = m_Moby.PVarReferences[".Hide When Camera In Cuboid 0"] as Cuboid;
            var cuboidHide1 = m_Moby.PVarReferences[".Hide When Camera In Cuboid 1"] as Cuboid;
            var cuboidShow = m_Moby.PVarReferences[".Show When Camera In Cuboid"] as Cuboid;

            if (cuboidShow && cuboidShow.IsInCuboid(camera.transform.position))
            {
                // show
                m_Renderer.enabled = true;
            }
            else if (cuboidHide0 && cuboidHide0.IsInCuboid(camera.transform.position))
            {
                // hide
                m_Renderer.enabled = false;
            }
            else if (cuboidHide1 && cuboidHide1.IsInCuboid(camera.transform.position))
            {
                // hide
                m_Renderer.enabled = false;
            }
            else
            {
                m_Renderer.enabled = true;
            }
        }

        this.transform.position = new Vector3(camera.transform.position.x, m_Height, camera.transform.position.z);
    }

    public void UpdateMaterials()
    {
        UpdateMaterial();
    }

    private void UpdateMaterial()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != 0x0b37) return;

        var levelDir = FolderNames.GetMapBinFolder(SceneManager.GetActiveScene().name, m_Moby.RCVersion);
        var overlayTex = Texture2D.grayTexture;
        var underlayTex = Texture2D.whiteTexture;

        var overlayTexIdx = 98 + m_Moby.GetPVarValue<int>(".Overlay FX Texture");
        var underlayTexIdx = 98 + m_Moby.GetPVarValue<int>(".Underlay FX Texture");
        var overlayColor = m_Moby.GetPVarValue<Color32>(".Overlay Color");
        var underlayColor = m_Moby.GetPVarValue<Color32>(".Underlay Color");

        var fogColor = m_Moby.GetPVarValue<Color32>(".Fog Color");
        var fogNearIntensity = m_Moby.GetPVarValue<byte>(".Fog Intensity Near");
        var fogFarIntensity = m_Moby.GetPVarValue<byte>(".Fog Intensity Far");
        var fogNearDistance = m_Moby.GetPVarValue<float>(".Fog Near Distance");
        var fogFarDistance = m_Moby.GetPVarValue<float>(".Fog Far Distance");

        var waveSpeed = m_Moby.GetPVarValue<float>(".Wave Speed");
        var waveHeight = m_Moby.GetPVarValue<float>(".Wave Crest");
        var waveOverlayFactor = m_Moby.GetPVarValue<float>(".Wave Surge");
        var waveFrequency = m_Moby.GetPVarValue<float>(".Wave Ripple Size");
        var waveDirectionDeg = m_Moby.GetPVarValue<float>(".Wave Direction");
        var waveIntersectFactor = m_Moby.GetPVarValue<float>(".Wave Direction Variation");
        var waveOverlayIntersectFactor = m_Moby.GetPVarValue<float>(".Wave Shimmer Intensity");
        var overlayTiling = m_Moby.GetPVarValue<float>(".Overlay Tiling");
        var overlayDirectionDeg = m_Moby.GetPVarValue<float>(".Overlay Direction");
        var overlaySpeed = m_Moby.GetPVarValue<float>(".Overlay Speed");
        var overlayAdditive = m_Moby.GetPVarValue<bool>(".Overlay Additive");
        m_Height = m_Moby.GetPVarValue<float>(".Z Position");

        if (overlayTexIdx >= 98)
        {
            var overlayFxFile = Path.Combine(levelDir, FolderNames.AssetsFolder, "fx", $"tex.{overlayTexIdx:0000}.png");
            if (File.Exists(overlayFxFile))
            {
                var data = File.ReadAllBytes(overlayFxFile);
                var tex = new Texture2D(2, 2);
                tex.LoadImage(data);
                overlayTex = tex;
            }
        }

        if (underlayTexIdx >= 98)
        {
            var underlayFxFile = Path.Combine(levelDir, FolderNames.AssetsFolder, "fx", $"tex.{underlayTexIdx:0000}.png");
            if (File.Exists(underlayFxFile))
            {
                var data = File.ReadAllBytes(underlayFxFile);
                var tex = new Texture2D(2, 2);
                tex.LoadImage(data);
                underlayTex = tex;
            }
        }

        underlayColor.a = (byte)Mathf.Clamp(underlayColor.a * 2, 0, 255);
        overlayColor.a = (byte)Mathf.Clamp(overlayColor.a * 2, 0, 255);

        m_Renderer.GetPropertyBlock(m_Mpb);

        // overlay
        m_Mpb.SetTexture("_Overlay_Tex", overlayTex);
        m_Mpb.SetFloat("_Overlay_Additive", overlayAdditive ? 1 : 0);
        m_Mpb.SetFloat("_Overlay_Tiling", 250f / overlayTiling);
        m_Mpb.SetColor("_Overlay_Color", overlayColor);
        m_Mpb.SetVector("_Overlay_Direction", new Vector2(Mathf.Cos(overlayDirectionDeg * Mathf.Deg2Rad), Mathf.Sin(overlayDirectionDeg * Mathf.Deg2Rad)) * (-overlaySpeed / 16));

        // underlay
        m_Mpb.SetTexture("_Underlay_Tex", underlayTex);
        m_Mpb.SetColor("_Underlay_Color", underlayColor);

        // wave
        m_Mpb.SetFloat("_Wave_Speed", waveSpeed);
        m_Mpb.SetFloat("_Wave_Overlay", waveOverlayFactor);
        m_Mpb.SetFloat("_Wave_Overlay_Intersect_Angle", waveOverlayIntersectFactor);
        m_Mpb.SetFloat("_Wave_Height", waveHeight * 25f);
        m_Mpb.SetFloat("_Wave_Frequency", 5f / waveFrequency);
        m_Mpb.SetFloat("_Wave_Intersect_Angle", waveIntersectFactor);
        m_Mpb.SetFloat("_Wave_Angle", Mathf.DeltaAngle(0, -waveDirectionDeg - 90));

        // fog
        m_Mpb.SetColor("_Fog_Color", fogColor);
        m_Mpb.SetFloat("_Fog_Near_Intensity", Mathf.Clamp01(Mathf.Min(fogNearIntensity, fogFarIntensity)));
        m_Mpb.SetFloat("_Fog_Far_Intensity", Mathf.Clamp01(Mathf.Max(fogNearIntensity, fogFarIntensity)));
        m_Mpb.SetFloat("_Fog_Near_Distance", Mathf.Min(fogNearDistance, fogFarDistance));
        m_Mpb.SetFloat("_Fog_Far_Distance", Mathf.Max(fogNearDistance, fogFarDistance));

        m_Renderer.SetPropertyBlock(m_Mpb);
    }

    public void DrawGizmos()
    {

    }

}
