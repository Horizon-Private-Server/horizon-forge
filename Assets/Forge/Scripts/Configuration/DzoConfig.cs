using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class DzoConfig : MonoBehaviour
{
    [Header("World Settings")]
    public Transform DefaultCameraPosition;
    public bool UseBackgroundColorOverride = false;
    [ColorUsage(false)] public Color BackgroundColorOverride = Color.white;

    [Header("Export Static Geometry")]
    public bool Ties = true;
    public bool Tfrags = true;
    public bool Shrubs = true;
    public bool Sky = true;
    public GameObject[] IncludeInExport;

    [Header("Export Brightness")]
    public float TieBrightness = 1f;
    public float ShrubBrightness = 1f;
    public float TfragBrightness = 1f;

    [Header("Export Misc")]
    public bool Lights = true;
    public Texture2D MinimapTextureOverride;
    public int MinimapResolution = 512;

    [Header("Post Processing")]
    public float PostExposure = 0f;
    public Color PostColorFilter = Color.white;

    [Header("Export Fog")]
    public bool FogOverride = false;
    [ColorUsage(false)] public Color FogOverrideColor = Color.gray;
    public float FogOverrideNearDistance = 0f;
    public float FogOverrideFarDistance = 1000f;
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

    [Serializable]
    public class StaticMeshMetadata
    {
        public string Name;
        public float EmissiveIntensity;
    }

    public string TieShrubTfragCombinedName;
    public string SkymeshName;
    public SkymeshShellMetadata[] SkymeshShells;
    public string MinimapMeshName;
    public LightMetadata[] Lights;
    public Vector3 DefaultCameraPosition;
    public Vector3 DefaultCameraEuler;
    public Color BackgroundColor;
    public Color FogColor;
    public float FogNearDistance;
    public float FogFarDistance;
    public Color PostColorFilter;
    public float PostExposure;
    public List<StaticMeshMetadata> Meshes = new List<StaticMeshMetadata>();
}
