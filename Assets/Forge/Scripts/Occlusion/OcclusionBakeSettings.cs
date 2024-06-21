using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using UnityEngine;

public class OcclusionBakeSettings : MonoBehaviour
{
    public enum OcclusionBakeResolution
    {
        _32,
        _64,
        _128,
        _256,
        _512,
        _1024,
        _2048,
        _4096
    }

    public OcclusionBakeResolution Resolution = OcclusionBakeResolution._256;
    public float RenderDistance = 1000f;
    public LayerMask CullingMask = -1;
    [Range(0f, 179f), Tooltip("Default is 90. Increasing will increase the number of objects included in each octant.")] public float RenderFov = 90f;
    [Range(0, 4)] public int FeatherOctantRadius = 0;
    //[Range(0f, 1f)] public float ClipPercent = 0f;
    //[Min(1)] public int ClipPixelCount = 1;
    public Vector3 OctantOffset = Vector3.zero;
}
