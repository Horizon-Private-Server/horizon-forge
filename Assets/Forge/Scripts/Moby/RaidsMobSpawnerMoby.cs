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

        var m = Gizmos.matrix;
        Gizmos.color = Color.red;
        //Handles.color = Color.red;
        //Handles.Label(this.transform.position + this.transform.up, "Mob Rotation");

        var hasSpawnCuboid = false;
        for (int i = 0; i < 4; ++i)
        {
            var cuboid = m_Moby.PVarReferences[$".Spawn Zones[{i}]"] as Cuboid;
            if (cuboid)
            {
                Gizmos.matrix = cuboid.transform.localToWorldMatrix;
                Gizmos.DrawWireCube(Vector3.zero, Vector3.one * 2f);
                Gizmos.matrix = m;
                DrawSpawnZone(cuboid.transform.position, cuboid.transform.rotation);
                hasSpawnCuboid = true;
            }
        }

        if (!hasSpawnCuboid)
        {
            DrawSpawnZone(this.transform.position, this.transform.rotation);
        }
    }

    private void DrawSpawnZone(Vector3 position, Quaternion rotation)
    {
        var up = rotation * Vector3.up;
        var right = rotation * Vector3.right;
        var startYaw = (-m_YawMin * 180f) + (m_YawInvert ? 180f : 0);

        if (m_YawFaceCuboid)
        {
            var dir = (m_YawFaceCuboid.transform.position - position).normalized * 5f;
            var startDir = Quaternion.AngleAxis(startYaw, up) * dir;
            GizmosHelper.DrawWireArc(position, startDir, (m_YawMax - m_YawMin) * 180f, 5f);
        }
        else
        {
            var startDir = Quaternion.AngleAxis(startYaw, up) * right;
            GizmosHelper.DrawWireArc(position, startDir, (m_YawMax - m_YawMin) * 180f, 5f);
        }
    }
}
