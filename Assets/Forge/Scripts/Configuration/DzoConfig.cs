using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class DzoConfig : MonoBehaviour
{
    [Header("World Settings")]
    public Transform DefaultCameraPosition;

    [Header("Export Static Geometry")]
    public bool Ties = true;
    public bool Tfrags = true;
    public bool Shrubs = true;
    public bool Sky = true;
    public GameObject[] IncludeInExport;

    [Header("Export Misc")]
    public bool Lights = true;

    [Header("Post Processing")]
    public float PostExposure = 0f;
    public Color PostColorFilter = Color.white;
}

[Serializable]
public class DzoMapMetadata
{
    [Serializable]
    public class SkymeshShellMetadata
    {
        public string ShellName;
        public Vector3 AngularVelocity;
        public bool Bloom;
        public bool Disabled;
        public Color Color;
    }

    [Serializable]
    public class LightMetadata
    {
        public string LightName;
        public bool CastShadows;
        public float ShadowStrength;
    }

    public string TieShrubTfragCombinedName;
    public string SkymeshName;
    public SkymeshShellMetadata[] SkymeshShells;
    public LightMetadata[] Lights;
    public Vector3 DefaultCameraPosition;
    public Vector3 DefaultCameraEuler;
    public Color BackgroundColor;
    public Color FogColor;
    public float FogNearDistance;
    public float FogFarDistance;
    public Color PostColorFilter;
    public float PostExposure;
}
