using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class RaidsMobSpawnerMoby : MonoBehaviour, IRenderHandlePrefab
{
    private Moby m_Moby;
    private float m_YawMin;
    private float m_YawMax;
    private Cuboid m_YawFaceCuboid;
    private bool m_YawInvert;

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
        if (m_Moby.OClass != RaidsModeData.MOB_SPAWNER_OCLASS) return;

        float.TryParse(m_Moby.PVarValues[".Emission.Random Rotation Min"], out m_YawMin);
        float.TryParse(m_Moby.PVarValues[".Emission.Random Rotation Max"], out m_YawMax);
        bool.TryParse(m_Moby.PVarValues[".Emission.Rotation Invert"], out m_YawInvert);
        m_YawFaceCuboid = m_Moby.PVarReferences[".Emission.Rotation Face Cuboid"] as Cuboid;
    }

    public void DrawGizmos()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != RaidsModeData.MOB_SPAWNER_OCLASS) return;

        var startYaw = (-m_YawMin * 180f) + (m_YawInvert ? 180f : 0);

        Gizmos.color = Color.red;
        //Handles.color = Color.red;
        //Handles.Label(this.transform.position + this.transform.up, "Mob Rotation");
        if (m_YawFaceCuboid)
        {
            var dir = (m_YawFaceCuboid.transform.position - this.transform.position).normalized * 5f;
            var startDir = Quaternion.AngleAxis(startYaw, m_Moby.transform.up) * dir;
            GizmosHelper.DrawWireArc(this.transform.position, startDir, (m_YawMax - m_YawMin) * 180f, 5f);
        }
        else
        {
            var startDir = Quaternion.AngleAxis(startYaw, m_Moby.transform.up) * this.transform.right;
            GizmosHelper.DrawWireArc(this.transform.position, startDir, (m_YawMax - m_YawMin) * 180f, 5f);
        }
    }
}
