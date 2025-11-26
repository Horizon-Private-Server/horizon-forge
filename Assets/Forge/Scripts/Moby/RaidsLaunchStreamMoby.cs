using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class RaidsLaunchStreamMoby : MonoBehaviour, IRenderHandlePrefab
{
    private Moby m_Moby;

    public float Gravity = 9.81f;
    public float MaxSpeed = 1f;
    [Min(0.001f)] public float DeltaTime = 1 / 60f;
    public float MaxFallSpeed = 0.833333f;
    public float MaxHorSpeed = 0.1166666f;

    void Start()
    {
        m_Moby = GetComponentInParent<Moby>();
    }

    void OnEnable()
    {
        m_Moby = GetComponentInParent<Moby>();
    }

    public void UpdateMaterials()
    {

    }

    private void OnDrawGizmosSelected()
    {
        DrawTrajectory(70, 20);
    }

    public void DrawGizmos()
    {
        if (!m_Moby) return;
        if (m_Moby.OClass != CommonCodeGen.LAUNCHSTREAM_OCLASS) return;

        float.TryParse(m_Moby.PVarValues[".Launch Angle"], out var angle);
        float.TryParse(m_Moby.PVarValues[".Launch Power"], out var power);
        DrawTrajectory(angle, power);
    }


    private void DrawTrajectory(float angle, float power)
    {
        var initialVelocity = GetInitialVelocity(angle * Mathf.Deg2Rad, power);

        var lastPos = transform.position;
        while ((lastPos.y - transform.position.y) > -50)
        {
            var nextPos = lastPos;
            for (int t = 0; t < 60; ++t)
            {
                initialVelocity *= 1 - (0.1f * DeltaTime); // drag
                initialVelocity += Vector3.down * Gravity * DeltaTime;
                var appliedVelocity = Vector3.ClampMagnitude(initialVelocity, MaxSpeed);
                var appliedVelocityXZ = Vector3.ClampMagnitude(new Vector3(appliedVelocity.x, 0, appliedVelocity.z), MaxHorSpeed);

                appliedVelocity.y = Mathf.Clamp(appliedVelocity.y, -MaxFallSpeed, MaxFallSpeed);
                appliedVelocity.x = appliedVelocityXZ.x;
                appliedVelocity.z = appliedVelocityXZ.z;

                nextPos += appliedVelocity;
                if (Vector3.Distance(nextPos, lastPos) > 5) break;
            }

            Gizmos.DrawLine(lastPos, nextPos);

            lastPos = nextPos;
        }
    }

    private Vector3 GetInitialVelocity(float pitch, float power)
    {
        float yaw = 0;
        float sin2Pitch = Mathf.Sin(2f * pitch);
        if (Mathf.Abs(sin2Pitch) < 1e-6)
        {
            return Vector3.zero;
        }

        float speed = Mathf.Sqrt((power * Gravity) / sin2Pitch);
        float cosPitch = Mathf.Cos(pitch);

        var velocity = new Vector3(cosPitch * Mathf.Sin(yaw), Mathf.Sin(pitch), cosPitch * Mathf.Cos(yaw)) * speed;
        return Quaternion.LookRotation(Vector3.ProjectOnPlane(transform.forward, Vector3.up).normalized, Vector3.up) * velocity;
    }
}
