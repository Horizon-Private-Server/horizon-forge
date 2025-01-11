using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

[ExecuteInEditMode, AddComponentMenu("")]
public class MapConfig : MonoBehaviour
{
    public static bool HideFog = false;

    const int MAP_CONFIG_VERSION = 2;

    [Header("Map Build")]
    [Min(0)] public int MapVersion = 0;
    public string MapName;
    public string MapFilename;

    [Header("Deadlocked")]
    [ReadOnly] public DLMapIds DLBaseMap = DLMapIds.SP_Battledome;
    public DLCustomModeIds DLForceCustomMode = DLCustomModeIds.None;
    public Texture2D DLLoadingScreen;
    public Texture2D DLMinimap;
    public int[] DLMobysIncludedInExport;

    [Header("UYA")]
    [ReadOnly] public UYAMapIds UYABaseMap = UYAMapIds.SP_Veldin;
    public Texture2D UYAMinimap;
    public int[] UYAMobysIncludedInExport;

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

    [SerializeField, HideInInspector] private int _version = 0;

    private int sceneCameraCount = -1;
    private ForgeSettings forgeSettings;

    public bool HasDeadlockedBaseMap() => DLBaseMap >= DLMapIds.MP_Battledome;
    public bool HasUYABaseMap() => UYABaseMap >= UYAMapIds.MP_Bakisi_Isles;
    public int FirstRacVersion => HasDeadlockedBaseMap() ? 4 : (HasUYABaseMap() ? 3 : -1);
    public int SecondRacVersion => (HasDeadlockedBaseMap() && HasUYABaseMap()) ? 3 : -1;
    public int ImportDLBaseMapIdx { get; set; } = 0;
    public int ImportUYABaseMapIdx { get; set; } = 0;

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
        Upgrade();
        UpdateShaderGlobals();

        if (MapName != null && MapName.Length > 32) MapName = MapName.Substring(0, 32);
        if (MapFilename != null && MapFilename.Length > 48) MapFilename = MapFilename.Substring(0, 48);

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
        Shader.SetGlobalFloat("_FORGE_FOG_NEAR_INTENSITY", HideFog ? 0 : FogNearIntensity);
        Shader.SetGlobalFloat("_FORGE_FOG_FAR_INTENSITY", HideFog ? 0 : FogFarIntensity);

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
            worldLightColors[idx] = worldLights[i].GetColor(0) * worldLights[i].GetIntensity(0);
            worldLightColors[idx+1] = worldLights[i].GetColor(1) * worldLights[i].GetIntensity(1);
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

    #region PathGraphs

    public PathGraph[] GetPathGraphs()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<PathGraph>());
    }

    public PathGraph GetPathGraphAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<PathGraph>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfPathGraph(PathGraph graph)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<PathGraph>()), graph);
    }

    #endregion

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

    #region Cameras

    public RatchetCamera[] GetCameras(int racVersion)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<RatchetCamera>().Where(x => x.RCVersion == racVersion).ToArray());
    }

    public RatchetCamera GetCameraAtIndex(int racVersion, int idx)
    {
        return GetCameras(racVersion)?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfCamera(RatchetCamera camera)
    {
        return Array.IndexOf(GetCameras(camera.RCVersion), camera);
    }

    #endregion

    #region Ambient Sounds

    public AmbientSound[] GetAmbientSounds(int racVersion)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<AmbientSound>().Where(x => x.RCVersion == racVersion).ToArray());
    }

    public AmbientSound GetAmbientSoundAtIndex(int racVersion, int idx)
    {
        return GetAmbientSounds(racVersion)?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfAmbientSound(AmbientSound ambientSound)
    {
        return Array.IndexOf(GetAmbientSounds(ambientSound.RCVersion), ambientSound);
    }

    #endregion

    #region Mobys

    public Moby[] GetMobys(int racVersion)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Moby>().Where(x => x.RCVersion == racVersion).ToArray());
    }

    public Moby GetMobyAtIndex(int racVersion, int idx)
    {
        return GetMobys(racVersion)?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfMoby(Moby moby)
    {
        return Array.IndexOf(GetMobys(moby.RCVersion), moby);
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

    #region Ties

    public Tie[] GetTies()
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Tie>().ToArray());
    }

    public Tie GetTieAtIndex(int idx)
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<Tie>())?.ElementAtOrDefault(idx);
    }

    public int GetIndexOfTie(Tie tie)
    {
        return Array.IndexOf(HierarchicalSorting.Sort(FindObjectsOfType<Tie>()), tie);
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

    #region Versioning

    public void InitializeVersion()
    {
        _version = MAP_CONFIG_VERSION;
    }

    private void Upgrade()
    {
        // wait for import to finish before upgrading
        if (LevelImporterWindow.IsImporting) return;

        // upgrade
        if (_version < MAP_CONFIG_VERSION)
        {
            while (_version < MAP_CONFIG_VERSION)
            {
                RunMigration(_version + 1);
                ++_version;
            }

            Debug.Log($"Map Config upgraded to v{_version}");
            UnityHelper.MarkActiveSceneDirty();
        }
        else if (_version > MAP_CONFIG_VERSION)
        {
            _version = MAP_CONFIG_VERSION;
        }
    }

    private void RunMigration(int version)
    {
        switch (version)
        {
            case 1: // USE Moby/rc# folder for map mobys
                {
                    var srcMobyDir = $"{FolderNames.GetMapFolder(SceneManager.GetActiveScene().name)}/{FolderNames.MobyFolder}";
                    var dstMobyDir = $"{FolderNames.GetMapFolder(SceneManager.GetActiveScene().name)}/{FolderNames.GetMapMobyFolder(RCVER.DL)}";
                    if (!Directory.Exists(dstMobyDir)) Directory.CreateDirectory(dstMobyDir);

                    // check if moby folder has mobys
                    var dirsToMove = new List<string>();
                    var subdirs = Directory.EnumerateDirectories(srcMobyDir);
                    foreach (var subdir in subdirs)
                    {
                        // moby folder is a whole number (base 10)
                        var subdirName = Path.GetFileName(subdir);
                        if (int.TryParse(subdirName, out _))
                        {
                            dirsToMove.Add(Path.GetFullPath(subdir));
                        }
                    }

                    // prompt user and migrate data
                    if (dirsToMove.Any())
                    {
                        if (!EditorUtility.DisplayDialog("Map Upgrade", $"Forge needs to migrate {dirsToMove.Count} mobys in {srcMobyDir} to {dstMobyDir}.\n", "Okay"))
                            throw new InvalidOperationException();

                        // migrate
                        foreach (var dir in dirsToMove)
                        {
                            IOHelper.CopyDirectory(dir, Path.Combine(dstMobyDir, Path.GetFileName(dir)));
                            Directory.Delete(dir, true);
                            File.Delete(dir + ".meta");
                        }

                        // refresh assets
                        AssetDatabase.Refresh();
                        var assets = GameObject.FindObjectsOfType<MonoBehaviour>().Where(x => x is IAsset).Select(x => x as IAsset);
                        foreach (var asset in assets)
                            asset.UpdateAsset();
                    }

                    break;
                }
            case 2: // Import Cameras & Ambient Sounds
                {
                    GameObject rootGo = new GameObject($"MAPCONFIG MIGRATION V{version} DATA");

                    if (this.HasDeadlockedBaseMap() && !GetCameras(RCVER.DL).Any())
                        RunMigration_ImportCameras(rootGo, RCVER.DL);
                    if (this.HasDeadlockedBaseMap() && !GetAmbientSounds(RCVER.DL).Any())
                        RunMigration_ImportAmbientSounds(rootGo, RCVER.DL);

                    if (this.HasUYABaseMap() && !GetCameras(RCVER.UYA).Any())
                        RunMigration_ImportCameras(rootGo, RCVER.UYA);
                    if (this.HasUYABaseMap() && !GetAmbientSounds(RCVER.UYA).Any())
                        RunMigration_ImportAmbientSounds(rootGo, RCVER.UYA);
                    break;
                }
        }
    }

    private void RunMigration_ImportCameras(GameObject rootGo, int racVersion)
    {
        var mapBinFolder = FolderNames.GetMapBinFolder(EditorSceneManager.GetActiveScene().name, racVersion);
        var camerasFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayCameraFolder);
        var camerasGo = new GameObject($"Cameras rc{racVersion}");
        camerasGo.transform.SetParent(rootGo.transform, false);

        int i = 0;
        while (true)
        {
            var cameraFile = Path.Combine(camerasFolder, $"{i:D4}", "camera.bin");
            if (!File.Exists(cameraFile)) break;

            // import camera
            var cameraGo = new GameObject(i.ToString());
            cameraGo.transform.SetParent(camerasGo.transform, false);
            var camera = cameraGo.AddComponent<RatchetCamera>();
            camera.RCVersion = racVersion;

            using (var fs = File.OpenRead(cameraFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    camera.Read(reader);
                }
            }

            // import pvars
            var cameraPVarFile = Path.Combine(camerasFolder, $"{i:D4}", "pvar.bin");
            if (File.Exists(cameraPVarFile))
                camera.PVars = File.ReadAllBytes(cameraPVarFile);

            ++i;
        }
    }

    private void RunMigration_ImportAmbientSounds(GameObject rootGo, int racVersion)
    {
        var mapBinFolder = FolderNames.GetMapBinFolder(EditorSceneManager.GetActiveScene().name, racVersion);
        var ambientSoundsFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayAmbientSoundFolder);
        var soundsGo = new GameObject($"Ambient Sounds rc{racVersion}");
        soundsGo.transform.SetParent(rootGo.transform, false);

        int i = 0;
        while (true)
        {
            var ambientSoundFile = Path.Combine(ambientSoundsFolder, $"{i:D4}", "sound.bin");
            if (!File.Exists(ambientSoundFile)) break;

            // import camera
            var ambientSoundGo = new GameObject(i.ToString());
            ambientSoundGo.transform.SetParent(soundsGo.transform, false);
            var ambientSound = ambientSoundGo.AddComponent<AmbientSound>();
            ambientSound.RCVersion = racVersion;

            using (var fs = File.OpenRead(ambientSoundFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    ambientSound.Read(reader);
                }
            }

            // import pvars
            var ambientSoundPVarFile = Path.Combine(ambientSoundsFolder, $"{i:D4}", "pvar.bin");
            if (File.Exists(ambientSoundPVarFile))
                ambientSound.PVars = File.ReadAllBytes(ambientSoundPVarFile);

            ++i;
        }
    }

    #endregion

}
