using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode]
public class WaterTriStripMoby : MonoBehaviour, IRenderHandlePrefab
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
        if (m_Moby.OClass != 0x19b0) return;
        if (m_Moby.PVars == null || m_Moby.PVars.Length != 112) return;

        var overlayTexIdx = 98 + BitConverter.ToInt32(m_Moby.PVars, 4);
        var overlayTex = Texture2D.grayTexture;
        var underlayColor = UnityHelper.GetColor(BitConverter.ToUInt32(m_Moby.PVars, 8));
        var overlayColor = UnityHelper.GetColor(BitConverter.ToUInt32(m_Moby.PVars, 12));
        var invert = BitConverter.ToInt32(m_Moby.PVars, 16) != 0;
        var overlaySpeed = BitConverter.ToSingle(m_Moby.PVars, 20);
        var overlayDirX = BitConverter.ToSingle(m_Moby.PVars, 24);
        var overlayDirY = BitConverter.ToSingle(m_Moby.PVars, 28);

        var levelDir = FolderNames.GetMapBinFolder(SceneManager.GetActiveScene().name, Constants.GameVersion);
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
        m_Mpb.SetTexture("_Overlay_Tex", overlayTex);
        m_Mpb.SetFloat("_Overlay_Negate", invert ? 1 : 0);
        m_Mpb.SetFloat("_Overlay_Cross_Speed", overlaySpeed / 100f);
        m_Mpb.SetColor("_Overlay_Color", overlayColor);
        m_Mpb.SetColor("_Underlay_Color", underlayColor);
        m_Mpb.SetVector("_Overlay_Direction", new Vector2(overlayDirX, overlayDirY) * 1);
        m_Renderer.SetPropertyBlock(m_Mpb);
    }

    private void UpdateMesh()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != 0x19b0) return;

        var spline = m_Moby.PVarSplineRefs[0];
        if (!spline) return;

        var mesh = new Mesh();
        var vertexCount = spline.Vertices.Count;
        var vertices = new Vector3[vertexCount];
        var indices = new int[(vertexCount - 2) * 3];
        vertices[0] = this.transform.worldToLocalMatrix.MultiplyPoint(spline.Vertices[0].transform.position);
        vertices[1] = this.transform.worldToLocalMatrix.MultiplyPoint(spline.Vertices[1].transform.position);
        for (int i = 2; i < vertexCount; i++)
        {
            var idx = (i - 2) * 3;

            vertices[i] = this.transform.worldToLocalMatrix.MultiplyPoint(spline.Vertices[i].transform.position);

            if (i % 2 == 0)
            {
                indices[idx + 0] = i - 0;
                indices[idx + 1] = i - 1;
                indices[idx + 2] = i - 2;
            }
            else
            {
                indices[idx + 0] = i - 2;
                indices[idx + 1] = i - 1;
                indices[idx + 2] = i - 0;
            }
        }
        mesh.SetVertices(vertices);
        mesh.SetIndices(indices, MeshTopology.Triangles, 0);
        mesh.RecalculateNormals();
        mesh.RecalculateBounds();
        mesh.RecalculateTangents();

        m_Filter.sharedMesh = mesh;
    }
}
