using System;
using System.Collections;
using System.Collections.Generic;
using System.Dynamic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Unity.VisualScripting;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

public class LevelImporterWindow : EditorWindow
{
    enum ImportSource
    {
        DL_ISO = 0,
        DL_WAD = 1,
        UYA_ISO = 2,
        UYA_WAD = 3,
        GC_ISO = 4
    }

    enum ImportStage
    {
        Preparing_Map_Files,
        Preparing_Level_WAD,
        Unpacking_Level_WAD,
        Unpacking_Sounds,
        Unpacking_Assets,
        Unpacking_Chunk,
        Unpacking_Gameplay,
        Unpacking_World_Instances,
        Unpacking_World_Instance_Ties,
        Unpacking_World_Instance_Shrubs,
        Unpacking_Code,
        Unpacking_Occlusion,
        Unpacking_Collision,
        Unpacking_Sky,
        Importing_Code,
        Importing_Sky,
        Importing_Collision,
        Importing_Ties,
        Importing_Shrubs,
        Importing_Mobys,
        Importing_Cuboids,
        Importing_Splines,
        Importing_Areas,
        Importing_Tfrags,
        Importing_Occlusion,
        Importing_World_Config,
        COUNT
    }

    static readonly string WindowTitle = "Level Importer";
    static readonly List<int> ImportSourceGameVersions = new List<int>() { 4, 4, 3, 3, 2 };
    static readonly List<string> ImportSources = new List<string>() { "DL ISO", "DL WAD", "UYA ISO",  "UYA WAD", "GC ISO" };
    static readonly List<string> DLBaseMaps = ((DLMapIds[])Enum.GetValues(typeof(DLMapIds))).Where(x => (int)x > 40).Select(x => Enum.GetName(typeof(DLMapIds), x)).ToList();
    static readonly List<string> DLMaps = ((DLMapIds[])Enum.GetValues(typeof(DLMapIds))).Select(x => Enum.GetName(typeof(DLMapIds), x)).ToList();
    static readonly List<string> UYAMaps = Enum.GetNames(typeof(UYAMapIds)).ToList();
    static readonly List<string> GCMaps = Enum.GetNames(typeof(GCMapIds)).ToList();
    static readonly List<string> AssetLimitedImportOptions = new List<string>() { "Skip", "Replace" };
    static readonly List<string> AssetImportOptions = new List<string>() { "Skip", "Add" };
    static readonly List<string> AssetMobyImportOptions = new List<string>() { "Skip", "Add" };

    int importSource = 0;
    int importBaseLevelIdx = 0;
    int importLevelIdx = 0;
    int importChunkId = 0;
    bool importIntoExistingMap;
    int importSky = 1;
    int importCollision = 1;
    int importTfrags = 1;
    int importTies = 1;
    int importShrubs = 1;
    int importMobys = 1;
    int importMisc = 1;
    string mapName = "New Map";
    string wadPath = "";

    [MenuItem("Forge/Tools/Importers/Level Importer")]
    public static void CreateNewWindow()
    {
        var wnd = GetWindow<LevelImporterWindow>();
        wnd.titleContent = new GUIContent(WindowTitle);
    }

    #region Getters

    ImportSource GetSelectedImportSource() => (ImportSource)importSource;
    bool ImportSourceIsDL() => importSource == (int)ImportSource.DL_ISO || importSource == (int)ImportSource.DL_WAD;
    bool ImportSourceIsUYA() => importSource == (int)ImportSource.UYA_ISO || importSource == (int)ImportSource.UYA_WAD;
    bool ImportSourceIsGC() => importSource == (int)ImportSource.GC_ISO;
    bool ImportSourceIsIso() => importSource == (int)ImportSource.DL_ISO || importSource == (int)ImportSource.UYA_ISO || importSource == (int)ImportSource.GC_ISO;
    bool ImportSourceIsWad() => importSource == (int)ImportSource.DL_WAD || importSource == (int)ImportSource.UYA_WAD;
    int ImportSourceRacVersion() => ImportSourceGameVersions[importSource];
    List<string> GetSelectedImportSourceLevelNames()
    {
        switch (ImportSourceRacVersion())
        {
            case 4: return DLMaps;
            case 3: return UYAMaps;
            case 2: return GCMaps;
            default: throw new NotImplementedException();
        }
    }
    string GetSelectedLevelName()
    {
        switch (ImportSourceRacVersion())
        {
            case 4: return importIntoExistingMap ? DLMaps[importLevelIdx] : DLBaseMaps[importBaseLevelIdx];
            case 3: return importIntoExistingMap ? UYAMaps[importLevelIdx] : throw new InvalidOperationException();
            case 2: return importIntoExistingMap ? GCMaps[importLevelIdx] : throw new InvalidOperationException();
            default: throw new NotImplementedException();
        }
    }
    int GetLevelId()
    {
        switch (ImportSourceRacVersion())
        {
            case 4: return (int)Enum.Parse<DLMapIds>(GetSelectedLevelName());
            case 3: return (int)Enum.Parse<UYAMapIds>(GetSelectedLevelName());
            case 2: return (int)Enum.Parse<GCMapIds>(GetSelectedLevelName());
            default: throw new NotImplementedException();
        }
    }
    string GetSelectedIsoPath()
    {
        var settings = ForgeSettings.Load();

        switch (ImportSourceRacVersion())
        {
            case 4: return settings.PathToCleanDeadlockedIso;
            case 3: return settings.GetPathToCleanUyaIso();
            case 2: return settings.PathToCleanGcIso;
            default: throw new NotImplementedException();
        }
    }

    #endregion

    #region Create GUI

    public void CreateGUI()
    {
        VisualElement root = rootVisualElement;
        root.Clear();

        // VisualElements objects can contain other VisualElement following a tree hierarchy
        root.BuildPadding();
        root.BuildHeading("Import Level");
        root.BuildPadding();

        // create source dropdown
        root.BuildRow("Source", (container) =>
        {
            var importSourceDropdown = new DropdownField();
            importSourceDropdown.choices = ImportSources;
            importSourceDropdown.index = importSource;
            importSourceDropdown.RegisterValueChangedCallback(OnImportSourceChanged);
            container.Add(importSourceDropdown);
        });

        // create import destination
        root.BuildRow("Import Into Current Map", (container) =>
        {
            var field = new Toggle();
            field.SetValueWithoutNotify(importIntoExistingMap);
            field.RegisterValueChangedCallback(OnImportDestinationChanged);
            container.Add(field);
        });

        // create map name
        if (!importIntoExistingMap)
        {
            root.BuildRow("New Map Name", (container) =>
            {
                var field = new TextField();
                field.SetValueWithoutNotify(mapName);
                field.RegisterValueChangedCallback((e) => mapName = e.newValue);
                container.Add(field);
            });
        }

        // build by selected source
        root.BuildPadding();
        if (importSource == (int)ImportSource.DL_ISO) CreateImportISOLevelGUI();
        else if (importSource == (int)ImportSource.DL_WAD) CreateImportWADLevelGUI();
        else if (importSource == (int)ImportSource.UYA_ISO) CreateImportISOLevelGUI();
        else if (importSource == (int)ImportSource.UYA_WAD) CreateImportWADLevelGUI();
        else if (importSource == (int)ImportSource.GC_ISO) CreateImportISOLevelGUI();

        // build import filter
        if (importIntoExistingMap)
        {
            root.BuildPadding();
            CreateImportIntoLevelAssetFilterGUI();
        }

        if (!ImportSourceIsDL() && !importIntoExistingMap)
        {
            // cannot import non-dl map as base map
            root.BuildPadding();
            var errorMsg = new Label("To create a new map you must import a Deadlocked map first.");
            errorMsg.style.backgroundColor = Color.red * 0.5f;
            errorMsg.style.borderTopLeftRadius = errorMsg.style.borderBottomLeftRadius = errorMsg.style.borderBottomRightRadius = errorMsg.style.borderTopRightRadius = 5;
            errorMsg.style.borderBottomColor = errorMsg.style.borderLeftColor = errorMsg.style.borderRightColor = errorMsg.style.borderTopColor = Color.black;
            errorMsg.style.borderBottomWidth = errorMsg.style.borderLeftWidth = errorMsg.style.borderRightWidth = errorMsg.style.borderTopWidth = 1;
            errorMsg.style.unityFontStyleAndWeight = FontStyle.Bold;
            errorMsg.style.unityTextAlign = TextAnchor.MiddleCenter;
            errorMsg.style.paddingBottom = errorMsg.style.paddingLeft = errorMsg.style.paddingRight = errorMsg.style.paddingTop = 10;
            root.Add(errorMsg);
        }
        else
        {
            // create import button
            root.BuildPadding();
            var importButton = new Button(importIntoExistingMap ? OnImportMerge : OnImport);
            importButton.text = "Import";
            root.Add(importButton);
        }
    }

    void CreateImportISOLevelGUI()
    {
        VisualElement root = rootVisualElement;

        // create heading
        root.BuildHeading("ISO Import Settings");
        root.BuildPadding();

        // create level select dropdown
        if (importIntoExistingMap)
        {
            root.BuildRow("Level", (container) =>
            {
                var levels = GetSelectedImportSourceLevelNames();

                // validate importLevelId
                if (importLevelIdx < 0 || importLevelIdx >= levels.Count)
                    importLevelIdx = 0;

                var dropdown = new DropdownField();
                dropdown.choices = levels;
                dropdown.index = importLevelIdx;
                dropdown.RegisterValueChangedCallback((e) => importLevelIdx = levels.IndexOf(e.newValue));
                container.Add(dropdown);
            });
        }
        else
        {
            root.BuildRow("Base Level", (container) =>
            {
                var levels = importSource == (int)ImportSource.DL_ISO ? DLBaseMaps : new List<string>();

                // validate importLevelId
                if (importBaseLevelIdx < 0 || importBaseLevelIdx >= levels.Count)
                    importBaseLevelIdx = 0;

                var dropdown = new DropdownField();
                dropdown.choices = levels;
                dropdown.index = importBaseLevelIdx;
                dropdown.RegisterValueChangedCallback((e) => importBaseLevelIdx = levels.IndexOf(e.newValue));
                container.Add(dropdown);
            });
        }

        // create chunk select dropdown
        root.BuildRow("Chunk", (container) =>
        {
            var choices = new List<string>() { "Core Level", "Chunk 1" };

            var dropdown = new DropdownField();
            dropdown.choices = choices;
            dropdown.index = importChunkId;
            dropdown.RegisterValueChangedCallback((e) => importChunkId = choices.IndexOf(e.newValue));
            container.Add(dropdown);
        });

    }

    void CreateImportWADLevelGUI()
    {
        VisualElement root = rootVisualElement;

        // create heading
        root.BuildHeading("WAD Import Settings");
        root.BuildPadding();

        // create base level select dropdown
        if (!importIntoExistingMap)
        {
            root.BuildRow("Base Level", (container) =>
            {
                var levels = importSource == (int)ImportSource.DL_WAD ? DLBaseMaps : new List<string>();

                // validate importLevelId
                if (importBaseLevelIdx < 0 || importBaseLevelIdx >= levels.Count)
                    importBaseLevelIdx = 0;

                var dropdown = new DropdownField();
                dropdown.choices = levels;
                dropdown.index = importBaseLevelIdx;
                dropdown.RegisterValueChangedCallback((e) => importBaseLevelIdx = levels.IndexOf(e.newValue));
                container.Add(dropdown);
            });
        }

        // create browse wad file
        root.BuildFileBrowser("Level WAD", wadPath, (v) => wadPath = v, filters: new string[] { "Level WAD", "wad" });
    }

    void CreateImportIntoLevelAssetFilterGUI()
    {
        VisualElement root = rootVisualElement;

        // create heading
        root.BuildHeading("Asset Import Settings");
        root.BuildPadding();

        // 
        root.BuildRow("Sky", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetLimitedImportOptions;
            field.index = importSky;
            field.RegisterValueChangedCallback((e) => importSky = AssetLimitedImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

        // 
        root.BuildRow("Collision", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetLimitedImportOptions;
            field.index = importCollision;
            field.RegisterValueChangedCallback((e) => importCollision = AssetLimitedImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

        // 
        root.BuildRow("Tfrags", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetImportOptions;
            field.index = importTfrags;
            field.RegisterValueChangedCallback((e) => importTfrags = AssetImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

        // 
        root.BuildRow("Ties", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetImportOptions;
            field.index = importTies;
            field.RegisterValueChangedCallback((e) => importTies = AssetImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

        // 
        root.BuildRow("Shrubs", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetImportOptions;
            field.index = importShrubs;
            field.RegisterValueChangedCallback((e) => importShrubs = AssetImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

        // 
        root.BuildRow("Mobys", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetMobyImportOptions;
            field.index = importMobys;
            field.SetEnabled(ImportSourceIsDL());
            field.RegisterValueChangedCallback((e) => importMobys = AssetMobyImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

        // 
        root.BuildRow("Cuboids/Splines/Areas", (container) =>
        {
            var field = new DropdownField();
            field.choices = AssetImportOptions;
            field.index = importMisc;
            field.RegisterValueChangedCallback((e) => importMisc = AssetImportOptions.IndexOf(e.newValue));
            container.Add(field);
        });

    }

    void OnImportSourceChanged(ChangeEvent<string> e)
    {
        importSource = ImportSources.IndexOf(e.newValue);
        CreateGUI();
    }

    void OnImportDestinationChanged(ChangeEvent<bool> e)
    {
        importIntoExistingMap = e.newValue;
        CreateGUI();
    }

    #endregion

    #region Validation

    bool ValidateMapName()
    {
        // trim
        mapName = mapName.Trim();

        // validate map name
        if (String.IsNullOrEmpty(mapName))
        {
            EditorUtility.DisplayDialog(WindowTitle, "Please enter a map name.", "Ok");
            return false;
        }

        // validate map name
        Regex r = new Regex("^([A-Z]|[a-z]|_|\\s|\\d)*$");
        if (!r.IsMatch(mapName))
        {
            EditorUtility.DisplayDialog(WindowTitle, $"{mapName} is not a valid map name. Please use only letters, numbers, underscores, and spaces.", "Ok");
            return false;
        }

        return true;
    }

    bool ValidateISO()
    {
        if (importSource == (int)ImportSource.DL_ISO)
        {
            // DL
            var settings = ForgeSettings.Load();
            if (!File.Exists(settings.PathToCleanDeadlockedIso))
            {
                EditorUtility.DisplayDialog(WindowTitle, $"Configured clean deadlocked iso path does not point to a valid iso file.\n\nPlease configure the correct iso path in {ForgeSettings.FORGE_SETTINGS_PATH}.", "Ok");
                return false;
            }    
        }
        else if (importSource == (int)ImportSource.UYA_ISO)
        {
            // UYA
            var settings = ForgeSettings.Load();
            var uyaIsoPath = settings.GetPathToCleanUyaIso();
            if (!File.Exists(uyaIsoPath))
            {
                EditorUtility.DisplayDialog(WindowTitle, $"Configured clean uya iso path does not point to a valid iso file.\n\nPlease configure the correct iso path in {ForgeSettings.FORGE_SETTINGS_PATH}.", "Ok");
                return false;
            }
        }

        return true;
    }

    bool ValidateWAD()
    {
        if (ImportSourceIsWad())
        {
            if (!File.Exists(wadPath))
            {
                EditorUtility.DisplayDialog(WindowTitle, $"Configured WAD file does not exist.", "Ok");
                return false;
            }
        }

        return true;
    }

    bool ValidateImportDestination()
    {
        var destSceneFile = FolderNames.GetScenePath(mapName);

        // check if scene already exists
        if (File.Exists(destSceneFile))
        {
            if (!EditorUtility.DisplayDialog(this.titleContent.text, $"Scene already exists for {mapName}.\nWould you like to overwrite it?", "Yes", "Cancel"))
            {
                return false;
            }
        }

        return true;
    }

    bool ValidateImportDestinationForMerge()
    {
        var scene = SceneManager.GetActiveScene();
        var destSceneFile = FolderNames.GetScenePath(scene.name);

        // check if scene already exists
        if (!File.Exists(destSceneFile))
        {
            EditorUtility.DisplayDialog(this.titleContent.text, $"You must first open a map before you can import into it.", "Okay");
            return false;
        }

        return true;
    }

    #endregion


    void OnImport()
    {
        GameObject rootGo = null;

        if (!ValidateMapName()) return;
        if (!ValidateISO()) return;
        if (!ValidateWAD()) return;
        if (!ValidateImportDestination()) return;
        if (!ImportSourceIsDL()) return;

        // get dest paths
        var destSceneFile = FolderNames.GetScenePath(mapName);
        var destMapFolder = FolderNames.GetMapFolder(mapName);
        var destMapHUDFolder = Path.Combine(destMapFolder, FolderNames.HUDFolder);
        var destMapBinFolder = FolderNames.GetMapBinFolder(mapName, Constants.GameVersion);
        var destMapWadFile = Path.Combine(destMapBinFolder, $"{mapName}.wad");
        var chunkId = importChunkId;
        var assetImports = new List<PackerImporterWindow.PackerAssetImport>();
        var postActions = new List<Action>();
        var destMinimapPath = Path.Combine(destMapHUDFolder, "minimap.png");
        var destLoadingScreenPath = Path.Combine(destMapHUDFolder, "loadingscreen.png");

        try
        {
            //ImportWorldConfig(destMapBinFolder, destMapFolder, assetImports);
            //ImportSky(destMapBinFolder, destMapFolder, assetImports);
            //ImportMobys(destMapBinFolder, destMapFolder, assetImports);
            //ImportCollision(destMapBinFolder, destMapFolder, assetImports);
            //ImportCuboids(destMapBinFolder, destMapFolder, assetImports);
            //PackerImporterWindow.Import(assetImports, true);
            //return;

            // clear map assets folder on fresh import
            if (Directory.Exists(destMapFolder)) Directory.Delete(destMapFolder, true);

            // prepare
            UpdateImportProgressBar(ImportStage.Preparing_Map_Files);
            PrepareMapResourceFolder(destMapFolder, destMapBinFolder);
            PrepareNewScene(destSceneFile);

            rootGo = new GameObject(GetSelectedLevelName());
            rootGo.transform.SetAsFirstSibling();

            var mapConfig = FindObjectOfType<MapConfig>();
            if (!mapConfig) return;

            // extract and copy wad to map bin directory
            UpdateImportProgressBar(ImportStage.Preparing_Level_WAD);
            if (ImportSourceIsIso())
            {
                PackerHelper.ExtractMinimap(GetSelectedIsoPath(), destMinimapPath, GetLevelId(), ImportSourceRacVersion());
                PackerHelper.ExtractTransitionBackground(GetSelectedIsoPath(), destLoadingScreenPath, GetLevelId(), ImportSourceRacVersion());
                ExtractWadFromISO(GetSelectedIsoPath(), GetLevelId(), destMapWadFile);
            }
            else if (ImportSourceIsWad())
            {
                // include matching .sound file (sound.bnk)
                var soundWadPath = Path.Combine(Path.GetDirectoryName(wadPath), Path.GetFileNameWithoutExtension(wadPath) + ".sound");

                chunkId = 0;
                PackerHelper.ExtractMinimap(GetSelectedIsoPath(), destMinimapPath, GetLevelId(), ImportSourceRacVersion());
                PackerHelper.ExtractTransitionBackground(GetSelectedIsoPath(), destLoadingScreenPath, GetLevelId(), ImportSourceRacVersion());
                ExtractWadFromISO(GetSelectedIsoPath(), GetLevelId(), destMapWadFile);
                File.Copy(wadPath, destMapWadFile, true);
                if (File.Exists(soundWadPath)) File.Copy(soundWadPath, Path.Combine(destMapFolder, "sound.bnk"), true);
            }

            // unpack level wad
            if (!DecompressAndUnpackLevelWad(destMapWadFile, chunkId)) return;

            // import assets
            ImportCode(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportSky(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportCollision(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportTies(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportTieInstances(destMapBinFolder, destMapFolder, postActions, rootGo);
            ImportShrubs(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportShrubInstances(destMapBinFolder, destMapFolder, postActions, rootGo);
            ImportMobys(destMapBinFolder, destMapFolder, true, assetImports, rootGo);
            ImportMobyInstances(destMapBinFolder, destMapFolder, postActions, rootGo);
            ImportCuboids(destMapBinFolder, destMapFolder, postActions, rootGo);
            ImportSplines(destMapBinFolder, destMapFolder, postActions, rootGo);
            ImportAreas(destMapBinFolder, destMapFolder, postActions, rootGo);
            ImportTfrags(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportOcclusion(destMapBinFolder, destMapFolder, assetImports, rootGo);
            ImportWorldConfig(destMapBinFolder, destMapFolder, assetImports, rootGo);

            // postprocess hill moby cuboids
            FindAndSetHillCuboidTypes(destMapBinFolder, destMapFolder, assetImports, rootGo);

            // create map render object
            CreateMapRenderObject(destMapBinFolder, destMapFolder);

            // create dzo object
            CreateDZOObject(destMapBinFolder, destMapFolder);

            // import final asset imports
            PackerImporterWindow.Import(assetImports, true);

            // set minimap/loadingscreen
            mapConfig.DLMinimap = AssetDatabase.LoadAssetAtPath<Texture2D>(destMinimapPath);
            mapConfig.DLLoadingScreen = AssetDatabase.LoadAssetAtPath<Texture2D>(destLoadingScreenPath);

            // configure always export mobys to all imported mobys that don't have instances
            // since the final exported moby assets will be all classes in this list + all classes from active moby instances
            var mobyInstanceClasses = mapConfig.GetMobys().Select(x => x.OClass).ToArray();
            mapConfig.DLMobysIncludedInExport = assetImports
                .Where(x => x.AssetType == FolderNames.MobyFolder)
                .Select(x => int.TryParse(x.Name, out var oclass) ? oclass : -1)
                .Where(x => x > 0)
                .Where(x => !mobyInstanceClasses.Contains(x))
                .OrderBy(x => x)
                .ToArray();

            // move map to top of hierarchy
            mapConfig.transform.SetAsFirstSibling();

            // refresh assets
            var assets = GameObject.FindObjectsOfType<MonoBehaviour>().Where(x => x is IAsset).Select(x => x as IAsset);
            foreach (var asset in assets)
                asset.UpdateAsset();
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
        }
        finally
        {
            foreach (var postAction in postActions)
                postAction?.Invoke();

            if (rootGo) rootGo.transform.SetAsLastSibling();
            FinalizeImport();
        }
    }

    void OnImportMerge()
    {
        if (!ValidateMapName()) return;
        if (!ValidateISO()) return;
        if (!ValidateWAD()) return;
        if (!ValidateImportDestinationForMerge()) return;

        // get dest paths
        var scene = SceneManager.GetActiveScene();
        var destMapName = scene.name;
        var destMapFolder = FolderNames.GetMapFolder(destMapName);
        var tempMapBinFolder = Path.Combine(FolderNames.GetTempFolder(), "import-level-merge");
        var destMapBinFolder = FolderNames.GetMapBinFolder(destMapName, Constants.GameVersion);
        var destMapWadFile = Path.Combine(tempMapBinFolder, $"{destMapName}.wad");
        var chunkId = importChunkId;
        var assetImports = new List<PackerImporterWindow.PackerAssetImport>();
        var reimportOcclusion = (importTfrags == 1) || (importTies > 0) || (importMobys > 0);
        var postActions = new List<Action>();
        var mapConfig = FindObjectOfType<MapConfig>();
        var rootGo = new GameObject(GetSelectedLevelName());
        rootGo.transform.SetAsFirstSibling();

        // base map isn't set when importing into current map
        // force it to the map's base map
        if (ImportSourceIsWad())
            importLevelIdx = DLMaps.IndexOf(mapConfig.DLBaseMap.ToString());

        // cannot import mobys from other games
        if (ImportSourceRacVersion() != Constants.GameVersion)
            importMobys = 0;

        try
        {
            // prepare
            UpdateImportProgressBar(ImportStage.Preparing_Map_Files);
            PrepareMapResourceFolder(destMapFolder, tempMapBinFolder);

            // extract and copy wad to map bin directory
            UpdateImportProgressBar(ImportStage.Preparing_Level_WAD);
            if (ImportSourceIsIso())
            {
                ExtractWadFromISO(GetSelectedIsoPath(), GetLevelId(), destMapWadFile);
            }
            else if (ImportSourceIsWad())
            {
                // include matching .sound file (sound.bnk)
                var soundWadPath = Path.Combine(Path.GetDirectoryName(wadPath), Path.GetFileNameWithoutExtension(wadPath) + ".sound");

                chunkId = 0;
                ExtractWadFromISO(GetSelectedIsoPath(), GetLevelId(), destMapWadFile);
                File.Copy(wadPath, destMapWadFile, true);
                if (File.Exists(soundWadPath)) File.Copy(soundWadPath, Path.Combine(destMapFolder, "sound.bnk"), true);
            }

            // unpack level wad
            if (!DecompressAndUnpackLevelWad(destMapWadFile, chunkId)) return;

            // move assets over
            if (importSky == 1) CopySky(tempMapBinFolder, destMapBinFolder);
            if (importCollision == 1) CopyCollision(tempMapBinFolder, destMapBinFolder);
            if (importTfrags == 1) CopyTfrags(tempMapBinFolder, destMapBinFolder);

            // import assets
            if (importSky == 1) ImportSky(destMapBinFolder, destMapFolder, assetImports, rootGo);
            if (importCollision == 1) ImportCollision(destMapBinFolder, destMapFolder, assetImports, rootGo);
            if (importTies > 0) ImportTies(tempMapBinFolder, destMapFolder, assetImports, rootGo);
            if (importTies > 0) ImportTieInstances(tempMapBinFolder, destMapFolder, postActions, rootGo);
            if (importShrubs > 0) ImportShrubs(tempMapBinFolder, destMapFolder, assetImports, rootGo);
            if (importShrubs > 0) ImportShrubInstances(tempMapBinFolder, destMapFolder, postActions, rootGo);
            if (importMobys > 0) ImportMobys(tempMapBinFolder, destMapFolder, true, assetImports, rootGo);
            if (importMobys > 0) ImportMobyInstances(tempMapBinFolder, destMapFolder, postActions, rootGo);
            if (importMisc == 1) ImportCuboids(tempMapBinFolder, destMapFolder, postActions, rootGo);
            if (importMisc == 1) ImportSplines(tempMapBinFolder, destMapFolder, postActions, rootGo);
            if (importMisc == 1 && ImportSourceIsDL()) ImportAreas(tempMapBinFolder, destMapFolder, postActions, rootGo);
            if (importTfrags == 1) ImportTfrags(destMapBinFolder, destMapFolder, assetImports, rootGo);
            if (reimportOcclusion) ImportOcclusion(destMapBinFolder, destMapFolder, assetImports, rootGo);
            //ImportWorldConfig(destMapBinFolder, destMapFolder, assetImports);

            // postprocess hill moby cuboids
            if (importMisc == 1) FindAndSetHillCuboidTypes(destMapBinFolder, destMapFolder, assetImports, rootGo);

            // import final asset imports
            PackerImporterWindow.Import(assetImports, true);

            // refresh assets
            var assets = GameObject.FindObjectsOfType<MonoBehaviour>().Where(x => x is IAsset).Select(x => x as IAsset);
            foreach (var asset in assets)
                asset.UpdateAsset();
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
        }
        finally
        {
            foreach (var postAction in postActions)
                postAction?.Invoke();

            if (rootGo) rootGo.transform.SetAsLastSibling();
            FinalizeImport();
        }
    }

    void PrepareMapResourceFolder(string destMapFolder, string destMapBinFolder)
    {
        if (!Directory.Exists(destMapFolder)) Directory.CreateDirectory(destMapFolder);
        if (Directory.Exists(destMapBinFolder)) Directory.Delete(destMapBinFolder, true);

        Directory.CreateDirectory(destMapBinFolder);
    }

    void PrepareNewScene(string destSceneFile)
    {
        // create scene directory
        var destSceneDir = Path.GetDirectoryName(destSceneFile);
        if (!Directory.Exists(destSceneDir)) Directory.CreateDirectory(destSceneDir);

        // create scene
        var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        scene.name = mapName;
        EditorSceneManager.SaveScene(scene, destSceneFile);

        // open scene
        EditorSceneManager.OpenScene(destSceneFile, OpenSceneMode.Single);

        // create map object
        var mapGameObject = new GameObject("Map");
        var map = mapGameObject.AddComponent<MapConfig>();
        map.DLBaseMap = Enum.Parse<DLMapIds>(DLBaseMaps[importBaseLevelIdx]);
        map.MapVersion = 0;
        map.MapName = map.MapFilename = mapName;

        var occBakeSettings = mapGameObject.AddComponent<OcclusionBakeSettings>();
        occBakeSettings.CullingMask = LayerMask.GetMask("OCCLUSION_BAKE");

        RenderSettings.skybox = null;
        RenderSettings.ambientMode = UnityEngine.Rendering.AmbientMode.Trilight;
        RenderSettings.ambientSkyColor = Color.white;
        RenderSettings.ambientEquatorColor = Color.white * 0.8f;
        RenderSettings.ambientGroundColor = Color.white * 0.5f;
    }

    void FinalizeImport()
    {
        EditorSceneManager.MarkAllScenesDirty();
        //EditorSceneManager.SaveOpenScenes();
        AssetDatabase.Refresh();
        EditorUtility.ClearProgressBar();
    }

    #region Unpack WAD

    void ExtractWadFromISO(string isoPath, int levelId, string outWadFilePath)
    {
        var racVersion = ImportSourceRacVersion();
        var outDir = FolderNames.GetTempFolder();
        var result = PackerHelper.ExtractLevelWads(isoPath, outDir, levelId, racVersion);
        var dstDir = Path.GetDirectoryName(outWadFilePath);

        if (racVersion == 4)
        {
            var addFilesToCopy = new[]
            {
                "chunk0.wad",
                "chunk1.wad",
                "sound.bnk"
            };

            var expectedWad = Path.Combine(outDir, $"core_level.wad");
            if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS || !File.Exists(expectedWad))
            {
                EditorUtility.DisplayDialog(WindowTitle, $"Failed to extract wad from ISO: {result}.", "Ok");
                return;
            }

            File.Copy(expectedWad, outWadFilePath, true);
            foreach (var file in addFilesToCopy)
            {
                var srcFile = Path.Combine(outDir, file);
                if (File.Exists(srcFile))
                    File.Copy(srcFile, Path.Combine(dstDir, file), true);
            }
        }
        else
        {
            var addFilesToCopy = new Dictionary<string, string>()
            {
                { $"level{levelId}.1.wad", $"sound.bnk" },
                { $"level{levelId}.2.wad", $"level{levelId}.2.wad" }, // gameplay
            };

            var expectedWad = Path.Combine(outDir, $"level{levelId}.0.wad");
            if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS || !File.Exists(expectedWad))
            {
                EditorUtility.DisplayDialog(WindowTitle, $"Failed to extract wad from ISO: {result}.", "Ok");
                return;
            }

            File.Copy(expectedWad, outWadFilePath, true);
            foreach (var file in addFilesToCopy)
            {
                var srcFile = Path.Combine(outDir, file.Key);
                if (File.Exists(srcFile))
                    File.Copy(srcFile, Path.Combine(dstDir, file.Value), true);
            }
        }
    }

    bool DecompressAndUnpackLevelWad(string wadPath, int chunkId)
    {
        var racVersion = ImportSourceRacVersion();
        var workingDir = Path.GetDirectoryName(wadPath);
        var assetsFolder = Path.Combine(workingDir, FolderNames.BinaryAssetsFolder);
        var soundsFolder = Path.Combine(workingDir, FolderNames.BinarySoundsFolder);
        var skybinFile = Path.Combine(Environment.CurrentDirectory, workingDir, FolderNames.BinarySkyBinFile);
        var worldInstanceFolder = racVersion == 4 ? Path.Combine(workingDir, FolderNames.BinaryWorldInstancesFolder) : Path.Combine(workingDir, FolderNames.BinaryGameplayFolder);
        var levelId = GetLevelId();

        // decompress and unpack level wad
        UpdateImportProgressBar(ImportStage.Unpacking_Level_WAD);
        var scResult = PackerHelper.DecompressAndUnpackLevelWad(wadPath, workingDir);
        if (!CheckResult(scResult, $"Failed to unpack level wad: {scResult}.")) return false;

        // unpack sounds -- DL only supported atm
        if (racVersion == 4)
        {
            UpdateImportProgressBar(ImportStage.Unpacking_Sounds);
            scResult = PackerHelper.UnpackSounds(Path.Combine(workingDir, "sound.bnk"), soundsFolder, racVersion);
            if (!CheckResult(scResult, $"Failed to unpack sounds: {scResult}.")) return false;
        }

        // unpack assets
        UpdateImportProgressBar(ImportStage.Unpacking_Assets);
        scResult = PackerHelper.UnpackAssets(workingDir, assetsFolder, racVersion);
        if (!CheckResult(scResult, $"Failed to unpack assets: {scResult}.")) return false;

        // unpack chunk
        if (chunkId > 0)
        {
            UpdateImportProgressBar(ImportStage.Unpacking_Chunk);
            scResult = PackerHelper.UnpackChunk(Path.Combine(workingDir, $"chunk{chunkId}.wad"), assetsFolder);
            if (!CheckResult(scResult, $"Failed to unpack chunk: {scResult}.")) return false;

            // move unpacked chunk terrain.bin into terrain folder
            var outTerrainBin = Path.Combine(workingDir, FolderNames.BinaryTerrainBinFile);
            var chunkTerrainBin = Path.Combine(assetsFolder, FolderNames.BinaryChunkTerrainBinFile);
            if (File.Exists(outTerrainBin)) File.Delete(outTerrainBin);
            if (File.Exists(chunkTerrainBin)) File.Move(chunkTerrainBin, outTerrainBin);
        }

        // unpack sky
        UpdateImportProgressBar(ImportStage.Unpacking_Sky);
        var bResult = WrenchHelper.ExportSky(Path.Combine(Environment.CurrentDirectory, workingDir, FolderNames.BinarySkyBinFile), Path.Combine(Environment.CurrentDirectory, workingDir, FolderNames.BinarySkyFolder), racVersion);
        if (!CheckResult(bResult, $"Failed to unpack sky.")) return false;
        scResult = PackerHelper.ConvertSky(skybinFile, skybinFile, racVersion, Constants.GameVersion);
        if (!CheckResult(scResult, $"Failed to convert skybox: {scResult}.")) return false;

        // unpack gameplay
        UpdateImportProgressBar(ImportStage.Unpacking_Gameplay);
        scResult = PackerHelper.UnpackGameplay(workingDir, Path.Combine(workingDir, FolderNames.BinaryGameplayFolder), levelId, racVersion);
        if (!CheckResult(scResult, $"Failed to unpack gameplay: {scResult}.")) return false;

        // unpack world instances
        if (racVersion == 4)
        {
            UpdateImportProgressBar(ImportStage.Unpacking_World_Instances);
            scResult = PackerHelper.UnpackWorldInstances(workingDir, worldInstanceFolder, racVersion);
            if (!CheckResult(scResult, $"Failed to unpack world instances: {scResult}.")) return false;

            // unpack world instances ties
            UpdateImportProgressBar(ImportStage.Unpacking_World_Instance_Ties);
            scResult = PackerHelper.UnpackWorldInstanceTies(worldInstanceFolder, Path.Combine(workingDir, FolderNames.BinaryWorldInstanceTieFolder), racVersion);
            if (!CheckResult(scResult, $"Failed to unpack world instance ties: {scResult}.")) return false;

            // unpack world instances shrubs
            UpdateImportProgressBar(ImportStage.Unpacking_World_Instance_Shrubs);
            scResult = PackerHelper.UnpackWorldInstanceShrubs(worldInstanceFolder, Path.Combine(workingDir, FolderNames.BinaryWorldInstanceShrubFolder), racVersion);
            if (!CheckResult(scResult, $"Failed to unpack world instance shrubs: {scResult}.")) return false;
        }
        else
        {
            // unpack world instances ties
            UpdateImportProgressBar(ImportStage.Unpacking_World_Instance_Ties);
            scResult = PackerHelper.UnpackWorldInstanceTies(worldInstanceFolder, Path.Combine(workingDir, FolderNames.BinaryWorldInstanceTieFolder), racVersion);
            if (!CheckResult(scResult, $"Failed to unpack world instance ties: {scResult}.")) return false;

            // unpack world instances shrubs
            UpdateImportProgressBar(ImportStage.Unpacking_World_Instance_Shrubs);
            scResult = PackerHelper.UnpackWorldInstanceShrubs(worldInstanceFolder, Path.Combine(workingDir, FolderNames.BinaryWorldInstanceShrubFolder), racVersion);
            if (!CheckResult(scResult, $"Failed to unpack world instance shrubs: {scResult}.")) return false;
        }

        // unpack code
        UpdateImportProgressBar(ImportStage.Unpacking_Code);
        scResult = PackerHelper.UnpackCode(workingDir, Path.Combine(workingDir, FolderNames.BinaryCodeFolder), racVersion);
        if (!CheckResult(scResult, $"Failed to unpack code: {scResult}.")) return false;

        // unpack occlusion
        UpdateImportProgressBar(ImportStage.Unpacking_Occlusion);
        scResult = PackerHelper.UnpackOcclusion(Path.Combine(workingDir, FolderNames.BinaryOcclusionFile), worldInstanceFolder, Path.Combine(workingDir, FolderNames.BinaryWorldInstanceOcclusionFolder), racVersion);
        if (!CheckResult(scResult, $"Failed to unpack occlusion: {scResult}.")) return false;

        // unpack collision
        UpdateImportProgressBar(ImportStage.Unpacking_Collision);
        var collisionBinFile = Path.Combine(Environment.CurrentDirectory, workingDir, FolderNames.BinaryCollisionBinFile);
        scResult = PackerHelper.ConvertCollision(collisionBinFile, collisionBinFile, racVersion, Constants.GameVersion);
        if (!CheckResult(scResult, $"Failed to convert collision: {scResult}.")) return false;
        bResult = WrenchHelper.ExportCollision(collisionBinFile, Path.Combine(Environment.CurrentDirectory, workingDir, FolderNames.BinaryAssetsFolder));
        if (!CheckResult(bResult, $"Failed to unpack collision.")) return false;

        return true;
    }

    #endregion

    #region Import Code

    void ImportCode(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var racVersion = ImportSourceRacVersion();
        var racRegion = GameRegion.NTSC;
        var binCodeFolder = Path.Combine(mapBinFolder, FolderNames.CodeFolder);
        var resourcesCodeFolder = Path.Combine(mapResourcesFolder, FolderNames.GetMapCodeFolder(racVersion, racRegion));
        if (Directory.Exists(resourcesCodeFolder)) Directory.Delete(resourcesCodeFolder, true);
        Directory.CreateDirectory(resourcesCodeFolder);

        // import
        UpdateImportProgressBar(ImportStage.Importing_Code);

        // import code segments
        if (Directory.Exists(binCodeFolder))
        {
            IOHelper.CopyDirectory(binCodeFolder, resourcesCodeFolder);
        }
    }

    #endregion

    #region Import Sky

    void ImportSky(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var resourcesSkyFolder = Path.Combine(mapResourcesFolder, FolderNames.SkyFolder);
        var resourcesSkyMaterialsFolder = Path.Combine(mapResourcesFolder, FolderNames.SkyFolder, "Materials");
        var meshFile = Path.Combine(mapBinFolder, FolderNames.BinarySkyMeshFile);
        var skyFolder = Path.Combine(mapBinFolder, FolderNames.BinarySkyFolder);
        var skyAssetFile = Path.Combine(skyFolder, "sky.asset");
        var worldConfigBinFile = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayFolder, "0.bin");
        var racVersion = ImportSourceRacVersion();
        if (!File.Exists(meshFile)) return;
        if (Directory.Exists(resourcesSkyFolder)) Directory.Delete(resourcesSkyFolder, true);
        Directory.CreateDirectory(resourcesSkyFolder);
        Directory.CreateDirectory(resourcesSkyMaterialsFolder);

        // import
        UpdateImportProgressBar(ImportStage.Importing_Sky);

        // import background color
        var backgroundColor = Color.black;
        if (File.Exists(worldConfigBinFile))
        {
            using (var fs = File.OpenRead(worldConfigBinFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    backgroundColor = new Color32((byte)((reader.ReadInt32() + 1) / 2), (byte)((reader.ReadInt32() + 1) / 2), (byte)((reader.ReadInt32() + 1) / 2), 255);
                }
            }
        }

        // parse sky asset for layer information
        if (File.Exists(skyAssetFile))
        {
            var skyAsset = WrenchHelper.ParseWrenchAssetFile(skyAssetFile);

            // create gouraud material
            var gouraudMat = new Material(Shader.Find("Horizon Forge/Skymesh"));
            //gouraudMat.SetColor("_Color", new Color32((byte)skyAsset.sky.colour[0], (byte)skyAsset.sky.colour[1], (byte)skyAsset.sky.colour[2], 255));
            gouraudMat.SetColor("_Color", backgroundColor);
            AssetDatabase.CreateAsset(gouraudMat, Path.Combine(resourcesSkyMaterialsFolder, "sky-gouraud.mat"));

            assetImports.Add(new PackerImporterWindow.PackerAssetImport()
            {
                AssetFolder = skyFolder,
                DestinationFolder = resourcesSkyFolder,
                Name = "sky",
                AssetType = FolderNames.SkyFolder,
                PrependModelNameToTextures = true,
                GetTexName = (texPath) =>
                {
                    // find fx def with tex
                    var fxTexs = skyAsset.sky.fx as IDictionary<string, System.Object>;
                    if (fxTexs != null)
                    {
                        var fxTex = fxTexs.FirstOrDefault(x => (x.Value as dynamic).src == texPath);
                        if (fxTex.Value != null)
                        {
                            return $"fx_{Path.GetFileNameWithoutExtension(texPath)}";
                        }
                    }

                    // find material with tex
                    var materials = skyAsset.sky.materials as IDictionary<string, System.Object>;
                    if (materials != null)
                    {
                        var mat = materials.FirstOrDefault(x => (x.Value as dynamic).diffuse.src == texPath);
                        if (mat.Value != null)
                        {
                            return (string)(mat.Value as dynamic).name;
                        }
                    }

                    return null;
                },
                OnImport = () =>
                {
                    var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(UnityHelper.GetProjectRelativePath(Path.Combine(resourcesSkyFolder, "sky.blend")));
                    if (prefab)
                    {
                        var go = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
                        if (go)
                        {
                            go.transform.SetParent(rootGo.transform, false);
                            foreach (var shellObj in skyAsset.sky.shells)
                            {
                                var shellDict = shellObj as IDictionary<string, System.Object>;
                                var bloom = false;
                                var angularVel = Vector3.zero;
                                var rot = Quaternion.identity;
                                var shell = shellObj.Value;
                                var name = (string)shell.mesh.name;

                                //
                                if (racVersion >= 3)
                                {
                                    bloom = (bool)shell.bloom;
                                    rot = Quaternion.Euler(new Vector3(shell.starting_rotation[1], shell.starting_rotation[0], shell.starting_rotation[2]) * Mathf.Rad2Deg);
                                    angularVel = new Vector3(shell.angular_velocity[1], shell.angular_velocity[0], shell.angular_velocity[2]) * Mathf.Rad2Deg;
                                }

                                if (!String.IsNullOrEmpty(name))
                                {
                                    var child = go.transform.Find(name);
                                    if (child)
                                    {
                                        var layer = child.gameObject.AddComponent<SkyLayer>();
                                        layer.Bloom = bloom;
                                        layer.transform.rotation = rot;
                                        layer.AngularRotation = angularVel;
                                    }
                                }
                            }

                            var sky = go.AddComponent<Sky>();
                            sky.MaxSpriteCount = (int)skyAsset.sky.maximum_sprite_count;
                        }
                    }
                }
            });
        }
    }

    void CopySky(string srcMapBinFolder, string destMapBinFolder)
    {
        var srcSkyBinFile = Path.Combine(srcMapBinFolder, FolderNames.BinarySkyBinFile);
        var srcSkyFolder = Path.Combine(srcMapBinFolder, FolderNames.BinarySkyFolder);
        var destSkyFolder = Path.Combine(destMapBinFolder, FolderNames.BinarySkyFolder);

        // copy sky bin
        if (File.Exists(srcSkyBinFile))
            File.Copy(srcSkyBinFile, Path.Combine(destMapBinFolder, FolderNames.BinarySkyBinFile), true);

        // move sky folder
        if (Directory.Exists(srcSkyFolder))
        {
            if (Directory.Exists(destSkyFolder)) Directory.Delete(destSkyFolder, true);
            IOHelper.CopyDirectory(srcSkyFolder, destSkyFolder);
        }
    }

    #endregion

    #region Import Collision

    void ImportCollision(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var collisionDaeFile = Path.Combine(mapBinFolder, FolderNames.BinaryCollisionColladaFile);
        if (!File.Exists(collisionDaeFile)) return;

        // import
        UpdateImportProgressBar(ImportStage.Importing_Collision);
        BlenderHelper.ImportMeshAsBlend(collisionDaeFile, mapResourcesFolder, "collision", true, out var outMeshFile);
        AssetDatabase.ImportAsset(UnityHelper.GetProjectRelativePath(outMeshFile));
        SetModelImportSettings(outMeshFile, addCollider: true);

        // spawn instance
        var collPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(UnityHelper.GetProjectRelativePath(outMeshFile));
        if (collPrefab)
        {
            var go = (GameObject)PrefabUtility.InstantiatePrefab(collPrefab);
            if (go)
            {
                go.transform.SetParent(rootGo.transform, true);
                go.layer = LayerMask.NameToLayer("COLLISION");
            }
        }
    }

    void CopyCollision(string srcMapBinFolder, string destMapBinFolder)
    {
        var srcCollisionBinFile = Path.Combine(srcMapBinFolder, FolderNames.BinaryCollisionBinFile);
        var srcCollisionColladaFile = Path.Combine(srcMapBinFolder, FolderNames.BinaryCollisionColladaFile);

        // copy files
        if (File.Exists(srcCollisionBinFile))
            File.Copy(srcCollisionBinFile, Path.Combine(destMapBinFolder, FolderNames.BinaryCollisionBinFile), true);

        if (File.Exists(srcCollisionColladaFile))
            File.Copy(srcCollisionColladaFile, Path.Combine(destMapBinFolder, FolderNames.BinaryCollisionColladaFile), true);
    }

    #endregion

    #region Import Occlusion

    void ImportOcclusion(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var occlusionFiles = new[] { "tfrag.bin", "tie.bin", "moby.bin" };
        var worldInstanceOcclusionFolder = Path.Combine(mapBinFolder, FolderNames.BinaryWorldInstanceOcclusionFolder);
        if (!Directory.Exists(worldInstanceOcclusionFolder)) return;

        var occlusionRootGo = new GameObject("Occlusion Octants");
        var occlusionOctantsComponent = occlusionRootGo.AddComponent<OcclusionOctant>();
        occlusionRootGo.transform.SetParent(rootGo.transform, true);

        // import
        UpdateImportProgressBar(ImportStage.Importing_Occlusion);

        var octantsFast = new Dictionary<Vector3, bool>();
        foreach (var occlusionFile in occlusionFiles)
        {
            var occlusionFilePath = Path.Combine(worldInstanceOcclusionFolder, occlusionFile);
            if (File.Exists(occlusionFilePath))
            {
                using (var fs = File.OpenRead(occlusionFilePath))
                {
                    using (var reader = new BinaryReader(fs))
                    {
                        while (reader.BaseStream.Position < reader.BaseStream.Length)
                        {
                            var octants = PackerHelper.ReadOcclusionBlock(reader, out _, out _);
                            foreach (var octant in octants)
                                octantsFast[octant] = true;
                        }
                    }
                }
            }
        }

        occlusionOctantsComponent.Octants = octantsFast.Keys.ToList();
    }

    #endregion

    #region Import Tie

    void ImportTies(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var tieAssetFolder = Path.Combine(mapBinFolder, FolderNames.BinaryTieFolder);
        var tieAssetDirs = Directory.EnumerateDirectories(tieAssetFolder).OrderBy(x => int.Parse(Path.GetFileName(x).Split('_')[0])).ToList();
        var localTieDir = Path.Combine(mapResourcesFolder, FolderNames.TieFolder);
        var racVersion = ImportSourceRacVersion();

        UpdateImportProgressBar(ImportStage.Importing_Ties);

        // convert ties -- convert on export now, not on import
        //if (PackerHelper.ConvertTies(tieAssetFolder, tieAssetFolder, ImportSourceRacVersion(), Constants.GameVersion) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
        //{
        //    Debug.LogError($"Unable to convert ties to rc{Constants.GameVersion}");
        //    return;
        //}
        
        // import ties
        foreach (var tieAssetDir in tieAssetDirs)
        {
            var folderName = Path.GetFileName(tieAssetDir);
            var tieClassStr = folderName.Split('_')[0];
            var tieOClass = int.Parse(tieClassStr);
            var tieClass = $"{tieOClass}";

            // recreate tie asset dir
            var localTieAssetDir = Path.Combine(localTieDir, tieClass);
            if (Directory.Exists(localTieAssetDir)) Directory.Delete(localTieAssetDir, true);
            Directory.CreateDirectory(localTieAssetDir);

            assetImports.Add(new PackerImporterWindow.PackerAssetImport()
            {
                AssetFolder = tieAssetDir,
                DestinationFolder = localTieAssetDir,
                Name = tieClass,
                AssetType = FolderNames.TieFolder,
                GenerateCollisionId = 0x2F,
                PrependModelNameToTextures = true,
                RacVersion = racVersion,
                AdditionalTags = new string[] { Constants.GameAssetTag[racVersion] }
            });
        }
    }

    void ImportTieInstances(string mapBinFolder, string mapResourcesFolder, List<Action> postActions, GameObject rootGo)
    {
        var racVersion = ImportSourceRacVersion();
        var worldInstanceTiesFolder = Path.Combine(mapBinFolder, FolderNames.BinaryWorldInstanceTieFolder);
        var worldInstanceOcclusionFolder = Path.Combine(mapBinFolder, FolderNames.BinaryWorldInstanceOcclusionFolder);
        var tieWorldInstanceDirs = Directory.EnumerateDirectories(worldInstanceTiesFolder).OrderBy(x => int.Parse(Path.GetFileName(x).Split('_')[0])).ToList();
        var tieOcclusion = File.ReadAllBytes(Path.Combine(worldInstanceOcclusionFolder, "tie.bin"));

        var tieRootGo = new GameObject("Ties");
        tieRootGo.transform.SetParent(rootGo.transform, true);
        tieRootGo.layer = LayerMask.NameToLayer("OCCLUSION_BAKE");

        UpdateImportProgressBar(ImportStage.Importing_Ties);

        // import instances
        var instancesByOcclId = new Dictionary<int, Tie>();
        foreach (var tieWorldInstanceDir in tieWorldInstanceDirs)
        {
            var folderName = Path.GetFileName(tieWorldInstanceDir);
            var tieClassStr = folderName.Split('_')[0];
            var tieOClass = int.Parse(tieClassStr);
            var tieClass = $"{tieOClass}";
            var tieDirs = Directory.EnumerateDirectories(tieWorldInstanceDir).ToList();

            foreach (var tieDir in tieDirs)
            {
                var tie = ImportTieInstance(tieDir, tieRootGo, tieClass, racVersion);
                if (tie)
                    instancesByOcclId[tie.OcclusionId] = tie;
            }
        }

        // read occlusion
        using (var ms = new MemoryStream(tieOcclusion))
        {
            using (var occlusionReader = new BinaryReader(ms))
            {
                while (occlusionReader != null && occlusionReader.BaseStream.Position < occlusionReader.BaseStream.Length)
                {
                    var octants = PackerHelper.ReadOcclusionBlock(occlusionReader, out var instanceIdx, out var occlusionId).ToArray();
                    if (instancesByOcclId.TryGetValue(occlusionId, out var tie))
                        tie.Octants = octants;
                    else
                        Debug.LogWarning($"Occlusion block with no matching tie!! instance:{instanceIdx} id:{occlusionId} octants:{octants.Length}");
                }
            }
        }
    }

    void CopyTies(string srcMapBinFolder, string destMapBinFolder)
    {
        var srcTieAssetFolder = Path.Combine(srcMapBinFolder, FolderNames.BinaryTieFolder);
        var destTieAssetFolder = Path.Combine(destMapBinFolder, FolderNames.BinaryTieFolder);
        if (!Directory.Exists(destTieAssetFolder)) Directory.CreateDirectory(destTieAssetFolder);

        // iterate ties and copy
        var tieFolders = Directory.EnumerateDirectories(srcTieAssetFolder);
        foreach (var tieFolder in tieFolders)
        {
            var tieClassStr = Path.GetFileName(tieFolder);
            var destTieFolder = Path.Combine(destTieAssetFolder, tieClassStr);

            // replace exist tie
            if (Directory.Exists(destTieFolder)) Directory.Delete(destTieFolder, true);

            IOHelper.CopyDirectory(tieFolder, destTieFolder);
        }
    }

    Tie ImportTieInstance(string tieDir, GameObject tieRootGo, string tieClass, int racVersion)
    {
        // read
        var groupFilePath = Path.Combine(tieDir, "group.bin");
        var group = -1;

        if (File.Exists(groupFilePath)) group = BitConverter.ToInt32(File.ReadAllBytes(groupFilePath));
        var colors = File.ReadAllBytes(Path.Combine(tieDir, "colors.bin"));
        var tie = File.ReadAllBytes(Path.Combine(tieDir, "tie.bin"));

        // create instance
        var tieGo = new GameObject(tieClass);
        tieGo.layer = tieRootGo.layer;
        var tieComponent = tieGo.AddComponent<Tie>();
        tieGo.transform.SetParent(tieRootGo.transform, true);

        // parse data
        using (var ms = new MemoryStream(tie))
        {
            using (var reader = new BinaryReader(ms))
            {
                tieComponent.Read(reader, racVersion);
                tieComponent.ColorData = colors;
                tieComponent.GroupId = group;

                if (colors.Length > 3)
                {
                    tieComponent.ColorDataValue = new Color(colors[0] / 128f, colors[1] / 128f, colors[2] / 128f, 1);
                }
            }
        }

        return tieComponent;
    }

    #endregion

    #region Import Shrub

    void ImportShrubs(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var shrubAssetFolder = Path.Combine(mapBinFolder, FolderNames.BinaryShrubFolder);
        var shrubAssetDirs = Directory.EnumerateDirectories(shrubAssetFolder).OrderBy(x => int.Parse(Path.GetFileName(x).Split('_')[0])).ToList();
        var localShrubDir = Path.Combine(mapResourcesFolder, FolderNames.ShrubFolder);
        var racVersion = ImportSourceRacVersion();

        UpdateImportProgressBar(ImportStage.Importing_Shrubs);

        // import shrubs
        foreach (var shrubAssetDir in shrubAssetDirs)
        {
            var folderName = Path.GetFileName(shrubAssetDir);
            var shrubClassStr = folderName.Split('_')[0];
            var shrubOClass = int.Parse(shrubClassStr);
            var shrubClass = $"{shrubOClass}";

            // recreate shrub asset dir
            var localShrubAssetDir = Path.Combine(localShrubDir, shrubClass);
            if (Directory.Exists(localShrubAssetDir)) Directory.Delete(localShrubAssetDir, true);
            Directory.CreateDirectory(localShrubAssetDir);

            assetImports.Add(new PackerImporterWindow.PackerAssetImport()
            {
                AssetFolder = shrubAssetDir,
                DestinationFolder = localShrubAssetDir,
                Name = shrubClass,
                AssetType = FolderNames.ShrubFolder,
                GenerateCollisionId = 0x2F,
                PrependModelNameToTextures = true,
                RacVersion = racVersion,
                AdditionalTags = new string[] { Constants.GameAssetTag[racVersion] }
            });
        }
    }

    void ImportShrubInstances(string mapBinFolder, string mapResourcesFolder, List<Action> postActions, GameObject rootGo)
    {
        var racVersion = ImportSourceRacVersion();
        var worldInstanceShrubsFolder = Path.Combine(mapBinFolder, FolderNames.BinaryWorldInstanceShrubFolder);
        var shrubWorldInstanceDirs = Directory.EnumerateDirectories(worldInstanceShrubsFolder).OrderBy(x => int.Parse(Path.GetFileName(x).Split('_')[0])).ToList();

        var shrubRootGo = new GameObject("Shrubs");
        shrubRootGo.transform.SetParent(rootGo.transform, true);
        shrubRootGo.layer = LayerMask.NameToLayer("SHRUB");

        UpdateImportProgressBar(ImportStage.Importing_Shrubs);

        // import instances
        foreach (var shrubWorldInstanceDir in shrubWorldInstanceDirs)
        {
            var folderName = Path.GetFileName(shrubWorldInstanceDir);
            var shrubClassStr = folderName.Split('_')[0];
            var shrubOClass = int.Parse(shrubClassStr);
            var shrubClass = $"{shrubOClass}";
            var shrubDirs = Directory.EnumerateDirectories(shrubWorldInstanceDir).ToList();

            foreach (var shrubDir in shrubDirs)
            {
                ImportShrubInstance(shrubDir, shrubRootGo, shrubClass, racVersion);
            }
        }
    }

    void CopyShrubs(string srcMapBinFolder, string destMapBinFolder)
    {
        var srcShrubAssetFolder = Path.Combine(srcMapBinFolder, FolderNames.BinaryShrubFolder);
        var destShrubAssetFolder = Path.Combine(destMapBinFolder, FolderNames.BinaryShrubFolder);
        if (!Directory.Exists(destShrubAssetFolder)) Directory.CreateDirectory(destShrubAssetFolder);

        // iterate ties and copy
        var shrubFolders = Directory.EnumerateDirectories(srcShrubAssetFolder);
        foreach (var shrubFolder in shrubFolders)
        {
            var tieClassStr = Path.GetFileName(shrubFolder);
            var destShrubFolder = Path.Combine(destShrubAssetFolder, tieClassStr);

            // replace exist shrub
            if (Directory.Exists(destShrubFolder)) Directory.Delete(destShrubFolder, true);

            IOHelper.CopyDirectory(shrubFolder, destShrubFolder);
        }
    }

    void ImportShrubInstance(string shrubDir, GameObject shrubRootGo, string shrubClass, int racVersion)
    {
        var groupFilePath = Path.Combine(shrubDir, "group.bin");
        var group = -1;

        // read
        if (File.Exists(groupFilePath)) group = BitConverter.ToInt32(File.ReadAllBytes(groupFilePath));
        var shrub = File.ReadAllBytes(Path.Combine(shrubDir, "shrub.bin"));

        // create instance
        var shrubGo = new GameObject(shrubClass);
        var shrubComponent = shrubGo.AddComponent<Shrub>();
        shrubGo.transform.SetParent(shrubRootGo.transform, true);
        shrubGo.layer = shrubRootGo.layer;

        // parse data
        using (var ms = new MemoryStream(shrub))
        {
            using (var reader = new BinaryReader(ms))
            {
                shrubComponent.Read(reader, racVersion);
                shrubComponent.GroupId = group;
            }
        }
    }

    #endregion

    #region Import Moby

    void ImportMobys(string mapBinFolder, string mapResourcesFolder, bool overwrite, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var mobyAssetFolder = Path.Combine(mapBinFolder, FolderNames.BinaryMobyFolder);
        var mobyDirs = Directory.EnumerateDirectories(mobyAssetFolder).ToList();
        var localMobyDir = Path.Combine(mapResourcesFolder, FolderNames.MobyFolder);
        var racVersion = ImportSourceRacVersion();

        UpdateImportProgressBar(ImportStage.Importing_Mobys);
        foreach (var mobyDir in mobyDirs)
        {
            var folderName = Path.GetFileName(mobyDir);
            var mobyClassStr = folderName.Split('_')[0];
            var mobyOClass = int.Parse(mobyClassStr);
            var mobyClass = $"{mobyOClass}";

            // recreate moby asset dir
            var localMobyAssetDir = Path.Combine(localMobyDir, mobyClass);
            if (Directory.Exists(localMobyAssetDir))
            {
                if (!overwrite) continue;

                Directory.Delete(localMobyAssetDir, true);
            }
            Directory.CreateDirectory(localMobyAssetDir);

            // import sounds
            var soundsFolder = Path.Combine(mobyDir, FolderNames.BinarySoundsFolder);
            if (Directory.Exists(soundsFolder))
            {
                var mobyAssetSoundsFolder = Path.Combine(localMobyAssetDir, FolderNames.SoundsFolder);
                if (!Directory.Exists(mobyAssetSoundsFolder)) Directory.CreateDirectory(mobyAssetSoundsFolder);

                var soundsInFolder = Directory.GetDirectories(soundsFolder);
                foreach (var soundFolder in soundsInFolder)
                {
                    var idxStr = Path.GetFileName(soundFolder);
                    if (Directory.Exists(soundFolder))
                    {
                        PackerHelper.PackSound(soundFolder, Path.Combine(mobyAssetSoundsFolder, $"{idxStr}.sound"));
                    }
                }
            }

            // add asset import
            assetImports.Add(new PackerImporterWindow.PackerAssetImport()
            {
                AssetFolder = mobyDir,
                DestinationFolder = localMobyAssetDir,
                Name = mobyClass,
                AssetType = FolderNames.MobyFolder,
                PrependModelNameToTextures = true,
                RacVersion = racVersion,
                AdditionalTags = new string[] { Constants.GameAssetTag[racVersion] }
            });
        }
    }

    void ImportMobyInstances(string mapBinFolder, string mapResourcesFolder, List<Action> postActions, GameObject rootGo)
    {
        var pvarOverlays = PvarOverlay.GetPvarOverlays(true);
        var gameplayMobysFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayMobyFolder);
        var mobyDirs = Directory.EnumerateDirectories(gameplayMobysFolder).ToList();
        var racVersion = ImportSourceRacVersion();
        var mobyRootGo = new GameObject("Mobys");
        mobyRootGo.transform.SetParent(rootGo.transform, true);

        UpdateImportProgressBar(ImportStage.Importing_Mobys);
        foreach (var mobyDir in mobyDirs)
        {
            var folderName = Path.GetFileName(mobyDir);
            var folderNameParts = folderName.Split('_');
            var idx = int.Parse(folderNameParts[0]);
            var mobyClassStr = folderNameParts[1];
            var mobyOClass = int.Parse(mobyClassStr);
            var mobyClass = $"{mobyOClass}";
            var overlay = pvarOverlays?.FirstOrDefault(x => x.RCVersion == racVersion && x.OClass == mobyOClass);
            var gameObjectName = overlay?.Name ?? mobyClass;

            if (ImportSourceIsDL()) ImportDLMobyInstance(mobyDir, mobyRootGo, gameObjectName, postActions);
            else if (ImportSourceIsUYA()) ImportUYAMobyInstance(mobyDir, mobyRootGo, gameObjectName, postActions);
        }
    }

    void ImportDLMobyInstance(string mobyDir, GameObject mobyRootGo, string name, List<Action> postActions)
    {
        // read
        var pvarsFile = Path.Combine(mobyDir, "pvar.bin");
        var pvarPtrFile = Path.Combine(mobyDir, "pvar_ptr.bin");
        var shrub = File.ReadAllBytes(Path.Combine(mobyDir, "moby.bin"));

        // create instance
        var mobyGo = new GameObject(name);
        var mobyComponent = mobyGo.AddComponent<Moby>();
        mobyGo.transform.SetParent(mobyRootGo.transform, true);

        // parse data
        using (var ms = new MemoryStream(shrub))
        {
            using (var reader = new BinaryReader(ms))
            {
                mobyComponent.RCVersion = 4;
                mobyComponent.Read(reader);
                if (File.Exists(pvarsFile)) mobyComponent.PVars = File.ReadAllBytes(pvarsFile);
                if (File.Exists(pvarPtrFile))
                {
                    var pvarPtrBytes = File.ReadAllBytes(pvarPtrFile);
                    var pvarPtrs = new List<int>();
                    while (pvarPtrs.Count < (pvarPtrBytes.Length / 4))
                        pvarPtrs.Add(BitConverter.ToInt32(pvarPtrBytes, 4 * pvarPtrs.Count));

                    mobyComponent.PVarPointers = pvarPtrs.ToArray();
                }
            }
        }

        postActions.Add(() => mobyComponent.InitializePVarReferences());
    }

    void ImportUYAMobyInstance(string mobyDir, GameObject mobyRootGo, string name, List<Action> postActions)
    {
        throw new NotImplementedException();
    }

    #endregion

    #region Import Splines

    void ImportSplines(string mapBinFolder, string mapResourcesFolder, List<Action> postActions, GameObject rootGo)
    {
        var gameplaySplinesFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplaySplineFolder);
        var idx = 0;
        var splineRootGo = new GameObject("Splines");
        splineRootGo.transform.SetParent(rootGo.transform, true);

        UpdateImportProgressBar(ImportStage.Importing_Splines);
        while (true)
        {
            var splineFilePath = Path.Combine(gameplaySplinesFolder, $"{idx:D4}.bin");
            if (!File.Exists(splineFilePath)) break;

            var splineGo = new GameObject(idx.ToString());
            splineGo.transform.SetParent(splineRootGo.transform, false);
            var spline = splineGo.AddComponent<Spline>();
            spline.Vertices = new List<SplineVertex>();

            using (var fs = File.OpenRead(splineFilePath))
            {
                using (var reader = new BinaryReader(fs))
                {
                    var vertices = spline.ReadSpline(reader);
                    if (vertices.Any()) splineGo.transform.position = vertices.Average();

                    for (int j = 0; j < vertices.Count; ++j)
                    {
                        var vertexGo = new GameObject(j.ToString());
                        var splineVertex = vertexGo.AddComponent<SplineVertex>();
                        vertexGo.transform.SetParent(splineGo.transform, false);
                        vertexGo.transform.position = vertices[j];

                        spline.Vertices.Add(splineVertex);
                    }
                }
            }

            ++idx;
        }
    }

    #endregion

    #region Import Areas

    void ImportAreas(string mapBinFolder, string mapResourcesFolder, List<Action> postActions, GameObject rootGo)
    {
        var gameplayAreaFile = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayAreaFile);
        var areasRootGo = new GameObject("Areas");
        var mapConfig = FindObjectOfType<MapConfig>();
        areasRootGo.transform.SetParent(rootGo.transform, true);

        UpdateImportProgressBar(ImportStage.Importing_Areas);
        using (var fs = File.OpenRead(gameplayAreaFile))
        {
            using (var reader = new BinaryReader(fs))
            {
                reader.ReadInt32();

                var count = reader.ReadInt32();
                var splineOffset = reader.ReadInt32();
                var cuboidOffset = reader.ReadInt32();
                var unkOffset = reader.ReadInt32();
                var cylinderOffset = reader.ReadInt32();

                for (int i = 0; i < count; ++i)
                {
                    var offset = reader.BaseStream.Position = 0x20 + 4 + (i * 0x30);
                    var areaGo = new GameObject(i.ToString());
                    var area = areaGo.AddComponent<Area>();
                    areaGo.transform.SetParent(areasRootGo.transform, false);

                    var pos = new Vector3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle()).SwizzleXZY();
                    var radius = reader.ReadSingle();
                    var splineCount = reader.ReadInt16();
                    var cuboidCount = reader.ReadInt16();
                    var unkCount = reader.ReadInt16();
                    var cylinderCount = reader.ReadInt16();

                    reader.BaseStream.Position = offset + 0x1C;
                    var splineStart = reader.ReadInt32();
                    var cuboidStart = reader.ReadInt32();
                    var unkStart = reader.ReadInt32();
                    var cylinderStart = reader.ReadInt32();

                    // set bsphere
                    area.BSphereRadius = radius;
                    area.transform.position = pos;

                    // add splines
                    reader.BaseStream.Position = splineOffset + splineStart + 4;
                    area.Splines = new List<Spline>();
                    for (int splineIdx = 0; splineIdx < splineCount; ++splineIdx)
                    {
                        var spline = mapConfig.GetSplineAtIndex(reader.ReadInt32());
                        area.Splines.Add(spline);
                    }

                    // add cuboids
                    reader.BaseStream.Position = cuboidOffset + cuboidStart + 4;
                    area.Cuboids = new List<Cuboid>();
                    for (int cuboidIdx = 0; cuboidIdx < cuboidCount; ++cuboidIdx)
                    {
                        var cuboid = mapConfig.GetCuboidAtIndex(reader.ReadInt32());
                        area.Cuboids.Add(cuboid);
                    }
                }
            }
        }
    }

    #endregion

    #region Import Tfrags

    void ImportTfrags(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var terrainBinFile = Path.Combine(Environment.CurrentDirectory, mapBinFolder, FolderNames.BinaryTerrainBinFile);
        var terrainBinFolder = Path.Combine(Environment.CurrentDirectory, mapBinFolder, FolderNames.BinaryTerrainFolder);
        var terrainOutColladaFile = Path.Combine(Environment.CurrentDirectory, mapBinFolder, FolderNames.BinaryTerrainFolder, "terrain.dae");
        var terrainMapResourcesFolder = Path.Combine(mapResourcesFolder, FolderNames.TfragFolder, $"rc{ImportSourceRacVersion()}_level{GetLevelId()}");
        var terrainTexturesMapResourcesFolder = Path.Combine(terrainMapResourcesFolder, "Textures");
        var terrainMaterialsMapResourcesFolder = Path.Combine(terrainMapResourcesFolder, "Materials");
        var shader = Shader.Find("Horizon Forge/Universal");
        if (!File.Exists(terrainBinFile)) return;

        // recreate tfrag asset dir
        if (Directory.Exists(terrainMapResourcesFolder)) Directory.Delete(terrainMapResourcesFolder, true);
        Directory.CreateDirectory(terrainMapResourcesFolder);

        if (!Directory.Exists(terrainTexturesMapResourcesFolder)) Directory.CreateDirectory(terrainTexturesMapResourcesFolder);
        if (!Directory.Exists(terrainMaterialsMapResourcesFolder)) Directory.CreateDirectory(terrainMaterialsMapResourcesFolder);

        UpdateImportProgressBar(ImportStage.Importing_Tfrags);

        // convert tfrags bin to collada and import
        WrenchHelper.ExportTfrags(terrainBinFile, terrainOutColladaFile, ImportSourceRacVersion());
        if (!File.Exists(terrainOutColladaFile)) return;

        // parse wrapping
        var wrappings = WrenchHelper.GetColladaTextureWraps(terrainOutColladaFile);

        // import textures
        var texFiles = Directory.GetFiles(terrainBinFolder, "*.0.png");
        foreach (var texFile in texFiles)
        {
            var texFileName = Path.GetFileNameWithoutExtension(texFile);
            var texIdx = int.Parse(texFileName.Split('.')[1]);

            var wrap = wrappings?.GetValueOrDefault(texIdx);

            // import texture
            var outTexFile = Path.Combine(terrainTexturesMapResourcesFolder, $"tfrags-{texIdx}.png");
            File.Copy(texFile, outTexFile, true);
            UnityHelper.ImportTexture(outTexFile, wrapu: wrap?.Item1, wrapv: wrap?.Item2);

            // create material
            var outMatFile = Path.Combine(terrainMaterialsMapResourcesFolder, $"tfrags-{texIdx}.mat");
            var mat = new Material(shader);
            mat.SetTexture("_MainTex", AssetDatabase.LoadAssetAtPath<Texture2D>(outTexFile));
            AssetDatabase.CreateAsset(mat, outMatFile);
        }

        BlenderHelper.ImportMesh(terrainOutColladaFile, terrainMapResourcesFolder, "tfrags", overwrite: true, out var outMeshFile, fixNormals: false);
        AssetDatabase.ImportAsset(UnityHelper.GetProjectRelativePath(outMeshFile));
        SetModelImportSettings(outMeshFile, addCollider: false, remapMaterials: true);

        // spawn instance
        var tfragPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(UnityHelper.GetProjectRelativePath(outMeshFile));
        if (tfragPrefab)
        {
            var tfragGo = (GameObject)PrefabUtility.InstantiatePrefab(tfragPrefab);
            if (tfragGo)
            {
                tfragGo.transform.SetParent(rootGo.transform, true);
                var tfrag = tfragGo.AddComponent<Tfrag>();
                tfragGo.layer = LayerMask.NameToLayer("OCCLUSION_BAKE");
                UnityHelper.RecurseHierarchy(tfragGo.transform, (t) => t.gameObject.layer = tfragGo.layer);

                // populate tfrag chunks
                ReadTfragChunks(mapBinFolder, tfrag);
            }
        }
    }

    void ReadTfragChunks(string mapBinFolder, Tfrag tfrag)
    {
        var terrainBinFile = Path.Combine(Environment.CurrentDirectory, mapBinFolder, FolderNames.BinaryTerrainBinFile);
        var worldInstanceOcclusionFolder = Path.Combine(mapBinFolder, FolderNames.BinaryWorldInstanceOcclusionFolder);
        var tfragOcclusion = File.ReadAllBytes(Path.Combine(worldInstanceOcclusionFolder, "tfrag.bin"));

        if (!File.Exists(terrainBinFile))
            return;

        // import instances
        var instancesById = new Dictionary<int, TfragChunk>();
        using (var fs = File.OpenRead(terrainBinFile))
        {
            using (var reader = new BinaryReader(fs))
            {
                var packetStart = reader.ReadInt32();
                var packetCount = reader.ReadInt32();

                for (int i = 0; i < packetCount; i++)
                {
                    // find chunk gameobject
                    var chunkTransform = tfrag.transform.Find($"tfrag_{i}");
                    if (!chunkTransform)
                    {
                        Debug.LogError($"Unable to find matching tfrag chunk for chunk {i}. Tfrag rebuilding may be broken.");
                        continue;
                    }

                    var tfragChunk = chunkTransform.gameObject.AddComponent<TfragChunk>();

                    // read chunk def
                    fs.Position = packetStart + (i * 0x40);
                    tfragChunk.HeaderBytes = reader.ReadBytes(0x40);
                    tfragChunk.OcclusionId = BitConverter.ToInt16(tfragChunk.HeaderBytes, 0x3a);
                    instancesById[i] = tfragChunk;

                    // read chunk data
                    fs.Position = packetStart + (i * 0x40) + 0x10;
                    var dataOff = reader.ReadInt32();
                    var dataLen = 0;

                    // compute data len from location of next chunk data buffer
                    // or end of file if last chunk
                    if ((i + 1) < packetCount)
                    {
                        fs.Position = packetStart + ((i + 1) * 0x40) + 0x10;
                        var nextDataOff = reader.ReadInt32();
                        dataLen = nextDataOff - dataOff;
                    }
                    else
                    {
                        dataLen = (int)(fs.Length - dataOff - packetStart);
                    }

                    // read data
                    fs.Position = dataOff + packetStart;
                    tfragChunk.DataBytes = reader.ReadBytes(dataLen);
                }
            }
        }

        // read occlusion
        using (var ms = new MemoryStream(tfragOcclusion))
        {
            using (var occlusionReader = new BinaryReader(ms))
            {
                while (occlusionReader != null && occlusionReader.BaseStream.Position < occlusionReader.BaseStream.Length)
                {
                    var octants = PackerHelper.ReadOcclusionBlock(occlusionReader, out var instanceIdx, out var occlusionId).ToArray();
                    if (instancesById.TryGetValue(instanceIdx, out var chunk))
                        chunk.Octants = octants;
                    else
                        Debug.LogWarning($"Occlusion block with no matching tfrag!! instance:{instanceIdx} id:{occlusionId} octants:{octants.Length}");
                }
            }
        }
    }

    void CopyTfrags(string srcMapBinFolder, string destMapBinFolder)
    {
        var srcFolder = Path.Combine(srcMapBinFolder, FolderNames.BinaryTerrainFolder);
        var destFolder = Path.Combine(destMapBinFolder, FolderNames.BinaryTerrainFolder);
        var srcTfragOcclusionFile = Path.Combine(srcMapBinFolder, FolderNames.BinaryWorldInstanceOcclusionFolder, "tfrag.bin");
        var destTfragOcclusionFile = Path.Combine(destMapBinFolder, FolderNames.BinaryWorldInstanceOcclusionFolder, "tfrag.bin");

        // copy folder
        if (Directory.Exists(srcFolder))
        {
            if (Directory.Exists(destFolder)) Directory.Delete(destFolder, true);
            IOHelper.CopyDirectory(srcFolder, destFolder);
        }

        // copy occlusion
        if (File.Exists(srcTfragOcclusionFile))
            File.Copy(srcTfragOcclusionFile, destTfragOcclusionFile, true);
    }

    #endregion

    #region Import Cuboids

    void ImportCuboids(string mapBinFolder, string mapResourcesFolder, List<Action> postActions, GameObject rootGo)
    {
        var gameplayMobysFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayMobyFolder);
        var gameplayCuboidsFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayCuboidFolder);
        var mobyDirs = Directory.EnumerateDirectories(gameplayMobysFolder).ToList();
        var mpInitMobyDir = mobyDirs.FirstOrDefault(x => x.EndsWith("_106A"));
        var cuboids = new List<Cuboid>();

        UpdateImportProgressBar(ImportStage.Importing_Cuboids);

        var cuboidRootGo = new GameObject("Cuboids");
        cuboidRootGo.transform.SetParent(rootGo.transform, true);

        var i = 0;
        while (true)
        {
            var cuboidFile = Path.Combine(gameplayCuboidsFolder, $"{i:0000}.bin");
            if (!File.Exists(cuboidFile)) break;

            using (var fs = File.OpenRead(cuboidFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    var cuboidGo = new GameObject(i.ToString());
                    var cuboid = cuboidGo.AddComponent<Cuboid>();
                    cuboid.Read(reader);

                    reader.BaseStream.Position = 0x28;
                    var isCircular = reader.ReadSingle() >= 2;

                    cuboidGo.transform.SetParent(cuboidRootGo.transform, true);
                    cuboids.Add(cuboid);
                }
            }

            ++i;
        }

        // read multiplayer init moby pvars
        // contains cuboid assignments (player, hill, ctf spawn)
        if (mpInitMobyDir != null && Directory.Exists(mpInitMobyDir))
        {
            var mpInitPvarsFilePath = Path.Combine(mpInitMobyDir, "pvar.bin");
            if (File.Exists(mpInitPvarsFilePath))
            {
                using (var fs = File.OpenRead(mpInitPvarsFilePath))
                {
                    using (var reader = new BinaryReader(fs))
                    {
                        try
                        {
                            // read deathmatch spawns
                            reader.BaseStream.Position = 0x1F8;
                            var idx = reader.ReadInt32();
                            i = 0;
                            while (idx >= 0 && i < 64)
                            {
                                // recompute rotation based off euler rotation
                                // we don't need this for vanilla maps because the euler is always the inverse of the rotation matrix
                                // but for custom maps we historically haven't recomputed the rotation matrix for player spawns
                                // so to be compatible with importing old custom maps we need to recompute it here
                                var cuboidFile = Path.Combine(gameplayCuboidsFolder, $"{idx:0000}.bin");
                                if (File.Exists(cuboidFile))
                                {
                                    using (var cfs = File.OpenRead(cuboidFile))
                                    {
                                        using (var creader = new BinaryReader(cfs))
                                        {
                                            var cuboid = cuboids?.ElementAtOrDefault(idx);
                                            if (cuboid)
                                            {
                                                creader.BaseStream.Position = 0x78;
                                                var yaw = creader.ReadSingle();
                                                cuboid.transform.rotation = Quaternion.Euler(0, -(yaw * Mathf.Rad2Deg) + 90, 0);
                                            }
                                        }
                                    }
                                }

                                SetCuboidType(cuboids, idx, CuboidType.Player);
                                idx = reader.ReadInt32();

                                ++i;
                            }

                            // read flag spawns
                            reader.BaseStream.Position = 0;
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.BlueFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.BlueFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.BlueFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.RedFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.RedFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.RedFlagSpawn);
                            reader.BaseStream.Position = 0x168;
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.GreenFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.GreenFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.GreenFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.OrangeFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.OrangeFlagSpawn);
                            SetCuboidSubType(cuboids, reader.ReadInt32(), CuboidSubType.OrangeFlagSpawn);
                        }
                        catch { }
                    }
                }
            }
        }
    }

    void SetCuboidType(List<Cuboid> cuboids, int idx, CuboidType type)
    {
        if (idx < 0 || idx >= cuboids.Count) return;
        cuboids[idx].Type = type;
    }

    void SetCuboidSubType(List<Cuboid> cuboids, int idx, CuboidSubType subtype)
    {
        if (idx < 0 || idx >= cuboids.Count) return;
        cuboids[idx].Subtype = subtype;
    }

    void FindAndSetHillCuboidTypes(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var gameplayCuboidsFolder = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayCuboidFolder);
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var hillMoby = FindObjectsOfType<Moby>()?.FirstOrDefault(x => x.OClass == 0x2604);
        if (!hillMoby || hillMoby.PVars == null || hillMoby.PVars.Length < 0x60) return;


        var areaIdx = BitConverter.ToInt32(hillMoby.PVars, 0x58);
        if (areaIdx >= 0)
        {
            var area = mapConfig.GetAreaAtIndex(areaIdx);
            if (area)
            {
                foreach (var cuboid in area.Cuboids)
                {
                    cuboid.Type = CuboidType.HillSquare;
                    var cuboidIdx = mapConfig.GetIndexOfCuboid(cuboid);
                    if (cuboidIdx < 0) continue;

                    var cuboidFile = Path.Combine(gameplayCuboidsFolder, $"{cuboidIdx:0000}.bin");
                    if (!File.Exists(cuboidFile)) continue;

                    using (var fs = File.OpenRead(cuboidFile))
                    {
                        using (var reader = new BinaryReader(fs))
                        {
                            reader.BaseStream.Position = 0x28;
                            var isCircular = reader.ReadSingle() >= 1.99;
                            cuboid.Type = isCircular ? CuboidType.HillCircle : CuboidType.HillSquare;
                        }
                    }
                }
            }
        }
    }

    #endregion

    #region Import World Config

    void ImportWorldConfig(string mapBinFolder, string mapResourcesFolder, List<PackerImporterWindow.PackerAssetImport> assetImports, GameObject rootGo)
    {
        var worldConfigBinFile = Path.Combine(mapBinFolder, FolderNames.BinaryGameplayFolder, "0.bin");
        var worldLightingBinFile = Path.Combine(mapBinFolder, FolderNames.BinaryWorldInstancesFolder, "0.bin");
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        UpdateImportProgressBar(ImportStage.Importing_World_Config);

        // import world config
        if (File.Exists(worldConfigBinFile))
        {
            using (var fs = File.OpenRead(worldConfigBinFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    fs.Position = 0x00;
                    mapConfig.BackgroundColor = new Color32((byte)reader.ReadInt32(), (byte)reader.ReadInt32(), (byte)reader.ReadInt32(), 255);
                    mapConfig.FogColor = new Color32((byte)reader.ReadInt32(), (byte)reader.ReadInt32(), (byte)reader.ReadInt32(), 255);
                    mapConfig.FogNearDistance = reader.ReadSingle() / 1024.0f;
                    mapConfig.FogFarDistance = reader.ReadSingle() / 1024.0f;
                    mapConfig.FogNearIntensity = 1 - (reader.ReadSingle() / 255.0f);
                    mapConfig.FogFarIntensity = 1 - (reader.ReadSingle() / 255.0f);

                    fs.Position = 0x28;
                    mapConfig.DeathPlane = reader.ReadSingle();
                }
            }
        }

        // import world lighting
        if (File.Exists(worldLightingBinFile))
        {
            using (var fs = File.OpenRead(worldLightingBinFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    fs.Position = 0x00;
                    var count = reader.ReadInt32();

                    for (int i = 0; i < count; ++i)
                    {
                        fs.Position = 0x10 + (i * 0x40);

                        var worldLightGo = new GameObject($"WorldLight {i}");
                        var worldLight = worldLightGo.AddComponent<WorldLight>();
                        worldLightGo.transform.SetParent(mapConfig.transform, false);

                        var colorR1 = reader.ReadSingle();
                        var colorG1 = reader.ReadSingle();
                        var colorB1 = reader.ReadSingle();
                        _ = reader.ReadInt32();
                        var dirX1 = reader.ReadSingle();
                        var dirY1 = reader.ReadSingle();
                        var dirZ1 = reader.ReadSingle();
                        _ = reader.ReadInt32();

                        var dir = new Vector3(dirX1, dirZ1, dirY1);
                        var ray1 = new GameObject("Ray 1");
                        ray1.transform.SetParent(worldLight.transform, false);
                        ray1.transform.rotation = Quaternion.LookRotation(dir);
                        worldLight.Ray1 = ray1.transform;
                        worldLight.Intensity1 = dir.magnitude;
                        worldLight.Color1 = new Color(colorR1, colorG1, colorB1);

                        var colorR2 = reader.ReadSingle();
                        var colorG2 = reader.ReadSingle();
                        var colorB2 = reader.ReadSingle();
                        _ = reader.ReadInt32();
                        var dirX2 = reader.ReadSingle();
                        var dirY2 = reader.ReadSingle();
                        var dirZ2 = reader.ReadSingle();
                        _ = reader.ReadInt32();

                        dir = new Vector3(dirX2, dirZ2, dirY2);
                        var ray2 = new GameObject("Ray 2");
                        ray2.transform.SetParent(worldLight.transform, false);
                        ray2.transform.rotation = Quaternion.LookRotation(dir);
                        worldLight.Ray2 = ray2.transform;
                        worldLight.Intensity2 = dir.magnitude;
                        worldLight.Color2 = new Color(colorR2, colorG2, colorB2);
                    }
                }
            }
        }

        mapConfig.UpdateShaderGlobals();
    }

    #endregion

    void CreateMapRenderObject(string mapBinFolder, string mapResourcesFolder)
    {
        var prefab = UnityHelper.GetMiscPrefab("Map Render");
        if (!prefab) return;

        var mapRender = Instantiate(prefab);
        if (!mapRender) return;

        mapRender.name = "Map Render";
        mapRender.transform.SetSiblingIndex(2);
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        // read pos/scale from map
        var codeSegBinFile = Path.Combine(mapBinFolder, FolderNames.BinaryCodeFolder, "code.0002.bin");
        if (File.Exists(codeSegBinFile))
        {
            using (var fs = File.OpenRead(codeSegBinFile))
            {
                using (var reader = new BinaryReader(fs))
                {
                    var cuboids = mapConfig.GetCuboids();
                    var mapIdx = (int)mapConfig.DLBaseMap - 41;
                    var yMin = cuboids.Min(x => x.transform.position.y);
                    var yMax = cuboids.Max(x => x.transform.position.y);

                    fs.Position = 0x175C8 + (0x10 * mapIdx);
                    mapRender.transform.position = new Vector3(reader.ReadSingle(), yMax + 50, reader.ReadSingle());
                    mapRender.transform.localScale = new Vector3(reader.ReadSingle(), (yMax - yMin) + 100, reader.ReadSingle());

                    var mapRenderComponent = mapRender.GetComponent<MapRender>();
                    if (mapRenderComponent)
                        mapRenderComponent.UpdateCamera();
                }
            }
        }
    }

    void CreateDZOObject(string mapBinFolder, string mapResourcesFolder)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var prefab = UnityHelper.GetMiscPrefab("DZO");
        if (!prefab) return;

        var dzo = Instantiate(prefab);
        if (!dzo) return;

        dzo.name = "DZO";
        dzo.transform.SetSiblingIndex(2);
        var dzoConfig = FindObjectOfType<DzoConfig>();
        if (!dzoConfig) return;

        // get default camera position
        if (!mapConfig) return;

        var cuboids = mapConfig.GetCuboids();
        var center = Vector3.zero;
        var count = 0;
        foreach (var cuboid in cuboids.Where(x => x.Type == CuboidType.Player))
        {
            center += cuboid.transform.position;
            count++;
        }

        if (count > 0)
            dzoConfig.DefaultCameraPosition.transform.position = (center / count) + Vector3.up * 15f;
    }

    void SetModelImportSettings(string path, bool addCollider, bool remapMaterials = false)
    {
        var assetPath = UnityHelper.GetProjectRelativePath(path);
        ModelImporter importer = (ModelImporter)ModelImporter.GetAtPath(assetPath);
        if (!importer) return;

        importer.preserveHierarchy = true;
        importer.bakeAxisConversion = true;
        importer.importNormals = ModelImporterNormals.Calculate;
        importer.addCollider = addCollider;

        if (remapMaterials) importer.SearchAndRemapMaterials(ModelImporterMaterialName.BasedOnModelNameAndMaterialName, ModelImporterMaterialSearch.Local);
        importer.SaveAndReimport();
    }

    bool CheckResult(PackerHelper.PACKER_STATUS_CODES result, string displayOnErrorMessage)
    {
        if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
        {
            EditorUtility.DisplayDialog(WindowTitle, displayOnErrorMessage, "Ok");
            return false;
        }

        return true;
    }

    bool CheckResult(bool result, string displayOnErrorMessage)
    {
        if (!result)
        {
            EditorUtility.DisplayDialog(WindowTitle, displayOnErrorMessage, "Ok");
            return false;
        }

        return true;
    }

    void UpdateImportProgressBar(ImportStage stage)
    {
        float p = (float)stage / (float)ImportStage.COUNT;
        string name = stage.ToString().Replace("_", " ");
        EditorUtility.DisplayProgressBar($"Importing Level", name, p);
    }

    void UpdateImportProgressBar(ImportStage stage, int idx, int count)
    {
        float p = (float)stage / (float)ImportStage.COUNT;
        float max = 1 / (float)ImportStage.COUNT;
        float offset = (float)idx / (float)count;

        string name = stage.ToString().Replace("_", " ");
        EditorUtility.DisplayProgressBar($"Importing Level", name + $" ({idx}/{count})", p + (offset * max));
    }
}
