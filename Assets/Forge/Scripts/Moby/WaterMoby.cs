using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode]
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
            var cuboidHide1 = m_Moby.PVarCuboidRefs[0x54 / 4];
            var cuboidHide2 = m_Moby.PVarCuboidRefs[0x58 / 4];
            var cuboidShow = m_Moby.PVarCuboidRefs[0x5C / 4];

            if (cuboidShow && cuboidShow.IsInCuboid(camera.transform.position))
            {
                // show
                m_Renderer.enabled = true;
            }
            else if (cuboidHide1 && cuboidHide1.IsInCuboid(camera.transform.position))
            {
                // hide
                m_Renderer.enabled = false;
            }
            else if (cuboidHide2 && cuboidHide2.IsInCuboid(camera.transform.position))
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
        if (m_Moby.PVars == null || m_Moby.PVars.Length != 112) return;

        var levelDir = FolderNames.GetMapBinFolder(SceneManager.GetActiveScene().name, Constants.GameVersion);
        var overlayTex = Texture2D.grayTexture;
        var underlayTex = Texture2D.whiteTexture;

        var overlayTexIdx = 98 + BitConverter.ToInt32(m_Moby.PVars, 0x00);
        var underlayTexIdx = 98 + BitConverter.ToInt32(m_Moby.PVars, 0x04);
        var overlayColor = UnityHelper.GetColor(BitConverter.ToUInt32(m_Moby.PVars, 0x30));
        var underlayColor = UnityHelper.GetColor(BitConverter.ToUInt32(m_Moby.PVars, 0x34));

        var fogColor = UnityHelper.GetColor(BitConverter.ToUInt32(m_Moby.PVars, 0x3A));
        var fogNearIntensity = m_Moby.PVars[0x3D] / 100f;
        var fogFarIntensity = m_Moby.PVars[0x3E] / 100f;
        var fogNearDistance = BitConverter.ToSingle(m_Moby.PVars, 0x40);
        var fogFarDistance = BitConverter.ToSingle(m_Moby.PVars, 0x44);

        var waveSpeed = BitConverter.ToSingle(m_Moby.PVars, 0x08);
        var waveHeight = BitConverter.ToSingle(m_Moby.PVars, 0x0C);
        var waveOverlayFactor = BitConverter.ToSingle(m_Moby.PVars, 0x10);
        var waveFrequency = BitConverter.ToSingle(m_Moby.PVars, 0x14);
        var waveDirectionDeg = BitConverter.ToSingle(m_Moby.PVars, 0x18);
        var waveIntersectFactor = BitConverter.ToSingle(m_Moby.PVars, 0x1C);
        var waveOverlayIntersectFactor = BitConverter.ToSingle(m_Moby.PVars, 0x20);
        var overlayTiling = BitConverter.ToSingle(m_Moby.PVars, 0x24);
        var overlayDirectionDeg = BitConverter.ToSingle(m_Moby.PVars, 0x28);
        var overlaySpeed = BitConverter.ToSingle(m_Moby.PVars, 0x2C);
        var overlayAdditive = m_Moby.PVars[0x39];
        m_Height = BitConverter.ToSingle(m_Moby.PVars, 0x4C);

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
        m_Mpb.SetFloat("_Overlay_Additive", overlayAdditive != 0 ? 1 : 0);
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
}
