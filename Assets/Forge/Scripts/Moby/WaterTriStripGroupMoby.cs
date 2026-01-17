using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class WaterTriStripGroupMoby : MonoBehaviour, IRenderHandlePrefab
{
    private MeshRenderer m_Renderer;
    private MeshFilter m_Filter;
    private Moby m_Moby;
    private MaterialPropertyBlock m_Mpb;

    // Start is called before the first frame update
    void Start()
    {
        m_Renderer = GetComponent<MeshRenderer>();
        m_Filter = GetComponent<MeshFilter>();
        m_Moby = GetComponentInParent<Moby>();
        m_Mpb = new MaterialPropertyBlock();

        UpdateMesh();
        UpdateMaterial();
    }

    public void UpdateMaterials()
    {
        UpdateMesh();
        UpdateMaterial();
    }

    private void UpdateMaterial()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != 0x24ed) return;

        var overlayTex = Texture2D.grayTexture;
        var emission = m_Moby.GetPVarValue<byte>(".Emission");
        var overlayTexIdx = 98 + m_Moby.GetPVarValue<int>(".Overlay FX Texture");
        var underlayColor = m_Moby.GetPVarValue<Color32>(".Underlay Color");
        var overlayColor = m_Moby.GetPVarValue<Color32>(".Overlay Color");
        var invert = m_Moby.GetPVarValue<bool>(".Invert Overlay Color");
        var overlaySpeed = m_Moby.GetPVarValue<float>(".Overlay Speed");
        var overlayDir = m_Moby.GetPVarValue<Vector2>(".Overlay Direction");

        var levelDir = FolderNames.GetMapBinFolder(SceneManager.GetActiveScene().name, m_Moby.RCVersion);
        var overlayFxFile = Path.Combine(levelDir, FolderNames.AssetsFolder, "fx", $"tex.{overlayTexIdx:0000}.png");
        if (File.Exists(overlayFxFile))
        {
            var data = File.ReadAllBytes(overlayFxFile);
            var tex = new Texture2D(2, 2);
            tex.LoadImage(data);
            overlayTex = tex;
        }

        underlayColor.a = (byte)Mathf.Clamp(underlayColor.a * 2, 0, 255);
        overlayColor.a = (byte)Mathf.Clamp(overlayColor.a * 2, 0, 255);

        m_Renderer.GetPropertyBlock(m_Mpb);
        m_Mpb.SetFloat("_Underlay_Post_Bloom", Mathf.Clamp01((emission / 128f) - 1) * 5);
        m_Mpb.SetTexture("_Overlay_Tex", overlayTex);
        m_Mpb.SetFloat("_Overlay_Negate", invert ? 1 : 0);
        m_Mpb.SetFloat("_Overlay_Cross_Speed", overlaySpeed / 100f);
        m_Mpb.SetColor("_Overlay_Color", overlayColor);
        m_Mpb.SetColor("_Underlay_Color", underlayColor);
        m_Mpb.SetVector("_Overlay_Direction", overlayDir * 1);
        m_Renderer.SetPropertyBlock(m_Mpb);
    }

    private void UpdateMesh()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != 0x24ed) return;

        var area = m_Moby.PVarReferences[".Area"] as Area;
        if (!area) return;

        var mesh = new Mesh();
        var splineCount = area.Splines.Count;
        var vertices = new List<Vector3>();
        var indices = new List<int>();
        for (int i = 0; i < splineCount; ++i)
        {
            var spline = area.Splines[i];
            if (spline.Vertices != null && spline.Vertices.Count > 2)
            {
                // build mesh from tristrip
                var offset = vertices.Count;
                vertices.Add(this.transform.worldToLocalMatrix.MultiplyPoint(spline.Vertices[0].transform.position));
                vertices.Add(this.transform.worldToLocalMatrix.MultiplyPoint(spline.Vertices[1].transform.position));
                for (int j = 2; j < spline.Vertices.Count; j++)
                {
                    var idx = j + offset;
                    vertices.Add(this.transform.worldToLocalMatrix.MultiplyPoint(spline.Vertices[j].transform.position));
                    if (idx % 2 == 0)
                    {
                        indices.Add(idx - 0);
                        indices.Add(idx - 1);
                        indices.Add(idx - 2);
                    }
                    else
                    {
                        indices.Add(idx - 2);
                        indices.Add(idx - 1);
                        indices.Add(idx - 0);
                    }
                }
            }
        }

        mesh.SetVertices(vertices);
        mesh.SetIndices(indices, MeshTopology.Triangles, 0);
        mesh.RecalculateNormals();
        mesh.RecalculateBounds();
        mesh.RecalculateTangents();

        m_Filter.sharedMesh = mesh;
    }

    public void DrawGizmos()
    {

    }

}
