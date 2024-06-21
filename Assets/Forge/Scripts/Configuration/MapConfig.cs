using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode]
public class MapConfig : MonoBehaviour
{
    [Header("Map Build")]
    [Min(0)] public int MapVersion = 0;
    public string MapName;
    public string MapFilename;

    [Header("Deadlocked")]
    [ReadOnly] public DLMapIds DLBaseMap = DLMapIds.MP_Battledome;
    public DLCustomModeIds DLForceCustomMode = DLCustomModeIds.None;
    public Texture2D DLLoadingScreen;
    public Texture2D DLMinimap;
    public int[] DLMobysIncludedInExport;

    [Header("Render Settings")]
    [Min(0)] public int ShrubMinRenderDistance = 0;

    [Header("World Settings")]
    public float DeathPlane = 0;
    public bool RenderDeathPlane = false;

    [Header("Fog")]
    [ColorUsage(false)] public Color BackgroundColor = Color.white * 0.25f;
    [ColorUsage(false)] public Color FogColor = Color.white * 0.5f;
    [Min(0)] public float FogNearDistance = 60;
    [Min(0)] public float FogFarDistance = 200;
    [Range(0f, 0.99f)] public float FogNearIntensity = 0;
    [Range(0.01f, 1f)] public float FogFarIntensity = 0.75f;

    private int sceneCameraCount = -1;
    private ForgeSettings forgeSettings;

    private void OnEnable()
    {
        UpdateShaderGlobals();
    }

    private void Update()
    {
        var cameras = SceneView.GetAllSceneCameras();
        if (sceneCameraCount == cameras.Length) return;

        foreach (var c in cameras)
        {
            var component = c.GetComponent<CameraCullFix>();
            if (!component) c.gameObject.AddComponent<CameraCullFix>();
        }

        sceneCameraCount = cameras.Length;
        UpdateShaderGlobals();
    }

    private void OnValidate()
    {
        UpdateShaderGlobals();

        if (FogFarIntensity < FogNearIntensity)
            FogFarIntensity = FogNearIntensity + 0.01f;
    }

    public void UpdateShaderGlobals()
    {
        // fog
        BackgroundColor.a = 1;
        FogColor.a = 1;
        Shader.SetGlobalColor("_FORGE_FOG_COLOR", FogColor);
        Shader.SetGlobalFloat("_FORGE_FOG_NEAR_DISTANCE", FogNearDistance);
        Shader.SetGlobalFloat("_FORGE_FOG_FAR_DISTANCE", FogFarDistance);
        Shader.SetGlobalFloat("_FORGE_FOG_NEAR_INTENSITY", FogNearIntensity);
        Shader.SetGlobalFloat("_FORGE_FOG_FAR_INTENSITY", FogFarIntensity);

        // forge settings
        if (!forgeSettings) forgeSettings = ForgeSettings.Load();
        if (forgeSettings)
        {
            Shader.SetGlobalColor("_FORGE_SELECTION_COLOR", forgeSettings.SelectionColor);
        }

        // world lighting
        var worldLights = GetWorldLights();
        var worldLightRays = new Vector4[32];
        var worldLightColors = new Vector4[32];
        for (int i = 0; i < worldLights.Length && i < 16; ++i)
        {
            var idx = i * 2;

            worldLightRays[idx] = worldLights[i].GetRay(0);
            worldLightRays[idx+1] = worldLights[i].GetRay(1);
            worldLightColors[idx] = worldLights[i].GetColor(0) * worldLights[i].GetIntensity(0) * 2;
            worldLightColors[idx+1] = worldLights[i].GetColor(1) * worldLights[i].GetIntensity(1) * 2;
        }

        Shader.SetGlobalVectorArray("_WorldLightRays", worldLightRays);
        Shader.SetGlobalVectorArray("_WorldLightColors", worldLightColors);
    }

    public ConvertToShrubDatabase GetConvertToShrubDatabase()
    {
        var mapFolder = FolderNames.GetMapFolder(SceneManager.GetActiveScene().name);
        var dbFile = Path.Combine(mapFolder, "shrubdb.asset");
        var db = AssetDatabase.LoadAssetAtPath<ConvertToShrubDatabase>(dbFile);
        
        if (!db)
        {
            db = ScriptableObject.CreateInstance<ConvertToShrubDatabase>();
            AssetDatabase.CreateAsset(db, dbFile);
            AssetDatabase.SaveAssets();
        }

        return db;
    }

    public TieDatabase GetTieDatabase()
    {
        var mapFolder = FolderNames.GetMapFolder(SceneManager.GetActiveScene().name);
        var dbFile = Path.Combine(mapFolder, "tiedb.asset");
        var db = AssetDatabase.LoadAssetAtPath<TieDatabase>(dbFile);

        if (!db)
        {
            db = ScriptableObject.CreateInstance<TieDatabase>();
            AssetDatabase.CreateAsset(db, dbFile);
            AssetDatabase.SaveAssets();
        }

        return db;
    }

    private void OnDrawGizmos()
    {
        if (RenderDeathPlane)
        {
            Gizmos.color = Color.black * 0.75f;
            Gizmos.DrawCube(new Vector3(transform.position.x, DeathPlane, transform.position.z), new Vector3(10000, 0, 10000));
            Gizmos.color = Color.white * 1f;
            Gizmos.DrawWireCube(new Vector3(transform.position.x, DeathPlane, transform.position.z), new Vector3(10000, 0, 10000));
            Gizmos.matrix = Matrix4x4.identity;
        }
    }

    #region Cuboids

    public Cuboid[] GetCuboids()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Cuboid>());
    }

    public Cuboid GetCuboidAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Cuboid>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfCuboid(Cuboid cuboid)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<Cuboid>()), cuboid);
    }

    #endregion

    #region Splines

    public Spline[] GetSplines()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Spline>());
    }

    public Spline GetSplineAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Spline>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfSpline(Spline spline)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<Spline>()), spline);
    }

    #endregion

    #region Mobys

    public Moby[] GetMobys()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Moby>());
    }

    public Moby GetMobyAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Moby>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfMoby(Moby moby)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<Moby>()), moby);
    }

    #endregion

    #region Areas

    public Area[] GetAreas()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Area>());
    }

    public Area GetAreaAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Area>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfArea(Area area)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<Area>()), area);
    }

    #endregion

    #region World Lights

    public WorldLight[] GetWorldLights()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<WorldLight>());
    }

    public WorldLight GetWorldLightAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<WorldLight>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfWorldLight(WorldLight area)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<WorldLight>()), area);
    }

    #endregion

}
