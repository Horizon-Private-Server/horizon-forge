using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class WorldLight : MonoBehaviour
{
    [Header("Vector 1")]
    public Transform Ray1;
    public float Intensity1;
    [ColorUsage(false)] public Color Color1;

    [Header("Vector 2")]
    public Transform Ray2;
    public float Intensity2;
    [ColorUsage(false)] public Color Color2;

    private Quaternion _lastRayDir1;
    private Quaternion _lastRayDir2;

    private void OnDrawGizmosSelected()
    {
        if (!Ray1 || !Ray2) return;

        Gizmos.color = Color1 * Intensity1;
        Gizmos.DrawLine(this.transform.position - (Ray1.transform.forward * 100), this.transform.position + (Ray1.transform.forward * 1000));
        Gizmos.color = Color2 * Intensity2;
        Gizmos.DrawLine(this.transform.position - (Ray2.transform.forward * 100), this.transform.position + (Ray2.transform.forward * 1000));
    }

    private void Update()
    {
        if (Ray1)
        {
            if (_lastRayDir1 != Ray1.rotation)
            {
                _lastRayDir1 = Ray1.rotation;
                _lastRayDir2 = Ray2.rotation;
                OnValidate();
            }
        }

        if (Ray2)
        {
            if (_lastRayDir2 != Ray2.rotation)
            {
                _lastRayDir2 = Ray2.rotation;
                OnValidate();
            }
        }
    }

    public Vector3 GetRay(int idx)
    {
        if (idx == 0 && Ray1) return Ray1.forward;
        else if (idx == 1 && Ray2) return Ray2.forward;

        return Vector3.zero;
    }

    public float GetIntensity(int idx)
    {
        if (idx == 0) return Intensity1;
        else if (idx == 1) return Intensity2;

        return 0f;
    }

    public Color GetColor(int idx)
    {
        if (idx == 0 && Ray1) return Color1;
        else if (idx == 1 && Ray2) return Color2;

        return Color.black;
    }

    private void OnValidate()
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (mapConfig) mapConfig.UpdateShaderGlobals();
    }
}
