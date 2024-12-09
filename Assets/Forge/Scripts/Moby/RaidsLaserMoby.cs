using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class RaidsLaserMoby : MonoBehaviour, IRenderHandlePrefab
{
    public LineRenderer m_BeamLineRenderer;
    public LineRenderer m_GlowLineRenderer;

    private Moby m_Moby;
    private MaterialPropertyBlock m_Mpb;
    private float m_Width;
    private float m_Length;
    private DLTeamIds m_TeamColor;
    private DLFXTextureIds m_BeamTextureId;
    private DLFXTextureIds m_LastBeamTextureId;
    private Texture2D m_BeamTexture;
    private DLFXTextureIds m_GlowTextureId;
    private DLFXTextureIds m_LastGlowTextureId;
    private Texture2D m_GlowTexture;

    void Start()
    {
        m_Moby = GetComponentInParent<Moby>();

        UpdateMaterial();
    }

    // Start is called before the first frame update
    void OnEnable()
    {
        m_Moby = GetComponentInParent<Moby>();

        UpdateMaterial();
    }

    public void UpdateMaterials()
    {
        UpdateMaterial();
    }

    private void UpdateMaterial()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != RaidsModeData.LASER_OCLASS) return;
        if (m_Mpb == null) m_Mpb = new MaterialPropertyBlock();

        var levelDir = FolderNames.GetMapBinFolder(SceneManager.GetActiveScene().name, m_Moby.RCVersion);

        float.TryParse(m_Moby.PVarValues[".Length"], out m_Length);
        float.TryParse(m_Moby.PVarValues[".Width"], out m_Width);
        Enum.TryParse<DLTeamIds>(m_Moby.PVarValues[".Color"], out m_TeamColor);
        Enum.TryParse<DLFXTextureIds>(m_Moby.PVarValues[".Beam Texture"], out m_BeamTextureId);
        Enum.TryParse<DLFXTextureIds>(m_Moby.PVarValues[".Glow Texture"], out m_GlowTextureId);

        var beamColor = m_TeamColor.ToColor(0);
        beamColor *= 1f / beamColor.maxColorComponent;
        var glowColor = m_TeamColor.ToColor(1);
        var beamWidth = m_Width * 0.5f;
        var glowWidth = m_Width * 1.0f;
        var linePositions = new Vector3[]
        {
            Vector3.zero,
            Vector3.right * m_Length
        };

        // read beam texture
        if (m_LastBeamTextureId != m_BeamTextureId)
        {
            var beamTexFile = Path.Combine(levelDir, FolderNames.AssetsFolder, "fx", $"tex.{(int)m_BeamTextureId:0000}.png");
            if (File.Exists(beamTexFile))
            {
                var data = File.ReadAllBytes(beamTexFile);
                var tex = new Texture2D(2, 2);
                tex.LoadImage(data);
                m_BeamTexture = tex;
            }
            m_LastBeamTextureId = m_BeamTextureId;
        }

        // read glow texture
        if (m_LastGlowTextureId != m_GlowTextureId)
        {
            var glowTexFile = Path.Combine(levelDir, FolderNames.AssetsFolder, "fx", $"tex.{(int)m_GlowTextureId:0000}.png");
            if (File.Exists(glowTexFile))
            {
                var data = File.ReadAllBytes(glowTexFile);
                var tex = new Texture2D(2, 2);
                tex.LoadImage(data);
                m_GlowTexture = tex;
            }
            m_LastGlowTextureId = m_GlowTextureId;
        }

        // beam
        m_BeamLineRenderer.SetPositions(linePositions);
        m_BeamLineRenderer.startWidth = beamWidth;
        m_BeamLineRenderer.endWidth = beamWidth;
        m_BeamLineRenderer.startColor = beamColor;
        m_BeamLineRenderer.endColor = beamColor;
        m_BeamLineRenderer.textureMode = LineTextureMode.Tile;
        m_BeamLineRenderer.textureScale = new Vector2(m_Length, 1);

        m_BeamLineRenderer.GetPropertyBlock(m_Mpb);
        m_Mpb.SetTexture("_MainTex", m_BeamTexture ?? Texture2D.whiteTexture);
        m_BeamLineRenderer.SetPropertyBlock(m_Mpb);

        // glow
        m_GlowLineRenderer.SetPositions(linePositions);
        m_GlowLineRenderer.startWidth = glowWidth;
        m_GlowLineRenderer.endWidth = glowWidth;
        m_GlowLineRenderer.startColor = glowColor;
        m_GlowLineRenderer.endColor = glowColor;
        m_GlowLineRenderer.textureMode = LineTextureMode.Tile;
        m_GlowLineRenderer.textureScale = new Vector2(m_Length, 1);

        m_GlowLineRenderer.GetPropertyBlock(m_Mpb);
        m_Mpb.SetTexture("_MainTex", m_GlowTexture ?? Texture2D.whiteTexture);
        m_GlowLineRenderer.SetPropertyBlock(m_Mpb);

        m_Moby.transform.localScale = Vector3.one;
        this.transform.localScale = Vector3.one;
    }
}
