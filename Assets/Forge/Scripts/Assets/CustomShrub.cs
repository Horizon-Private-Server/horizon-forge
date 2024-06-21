using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using UnityEngine;

[CreateAssetMenu(fileName = "CustomShrub", menuName = "Forge/ScriptableObjects/CustomShrub", order = 1)]
public class CustomShrub : ScriptableObject
{
    public enum CustomShrubTextureSize
    {
        _32,
        _64,
        _128,
        _256,
        _512,
        _1024
    }

    [Header("Shrub Information")]
    public int startingShrubClass = 30000;
    [HideInInspector] public int lastComputedShrubCount = 0;
    public int mipDistance = 64;

    [Header("Model Information")]
    public GameObject modelToConvert;
    public bool shrubPerRootLevelObject = false;
    public int maxMaterialsPerShrub = 16;

    [Header("Collider Information")]
    public bool generateColliders = false;
    public string generateCollidersDefaultMaterialId = "2f";

    [Header("Instance Information")]
    public bool createInstanceAfterImport;
    public int instanceRenderDistance = 64;

    [Header("Material Information")]
    public Color globalTintColor = Color.white;
    public bool globalCorrectForAlphaBloom = true;
    public CustomShrubTextureSize globalMaxTextureSize = CustomShrubTextureSize._512;
    public List<CustomShrubMaterialData> materials = new List<CustomShrubMaterialData>();


    [Serializable]
    public struct CustomShrubMaterialData
    {
        public string name;
        public bool useMaterialOverride;
        public CustomShrubTextureSize maxTextureSize;
        public Color tintColor;
        public bool correctForAlphaBloom;
    }
}
