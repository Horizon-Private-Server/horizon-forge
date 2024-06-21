using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;


public class ISOImporterWindow : EditorWindow
{
    static readonly List<string> Sources = new List<string>() { "DL ISO", "UYA ISO", "GC ISO" };
    static readonly List<int> SourceRACVersion = new List<int>() { 4, 3, 2 };
    static readonly List<string> AssetTypes = new List<string>() { FolderNames.TieFolder, FolderNames.ShrubFolder, FolderNames.MobyFolder };
    static object lockObject = new object();

    DropdownField srcDropdown;
    Toggle overwriteToggle;
    Toggle[] assetToggles;
    Button importButton;

    [MenuItem("Forge/Tools/Importers/ISO Importer")]
    public static void CreateNewWindow()
    {
        var wnd = GetWindow<ISOImporterWindow>();
        wnd.titleContent = new GUIContent("ISO Importer");
    }

    public void CreateGUI()
    {
        // Each editor window contains a root VisualElement object
        VisualElement root = rootVisualElement;

        root.Clear();

        // VisualElements objects can contain other VisualElement following a tree hierarchy
        var titleLabel = new Label("Import Assets from ISO");
        titleLabel.style.unityTextAlign = TextAnchor.MiddleCenter;
        titleLabel.style.fontSize = 18;
        root.BuildPadding();
        root.Add(titleLabel);
        root.BuildPadding();

        // Create destination dropdown
        root.BuildRow("Source", (container) =>
        {
            srcDropdown = new DropdownField();
            srcDropdown.choices = Sources;
            srcDropdown.index = 0;
            container.Add(srcDropdown);
        });

        // Create overwrite toggle
        root.BuildRow("Overwrite existing", (container) =>
        {
            overwriteToggle = new Toggle();
            overwriteToggle.value = true;
            container.Add(overwriteToggle);
        });

        // VisualElements objects can contain other VisualElement following a tree hierarchy
        root.BuildPadding();
        var importTypesLabel = new Label("Types");
        importTypesLabel.style.fontSize = 14;
        root.Add(importTypesLabel);

        // Create asset type toggles
        assetToggles = new Toggle[AssetTypes.Count];
        for (int i = 0; i < AssetTypes.Count; ++i)
        {
            root.BuildRow(AssetTypes[i], (container) =>
            {
                assetToggles[i] = new Toggle();
                assetToggles[i].value = true;
                container.Add(assetToggles[i]);
            });
        }

        root.BuildPadding();
        importButton = new Button(OnImport);
        importButton.text = "Import";
        root.Add(importButton);
    }

    private void OnImport()
    {
        var settings = ForgeSettings.Load();
        if (!settings)
        {
            EditorUtility.DisplayDialog("Cannot import iso", $"Missing ForgeSettings", "Ok");
            return;
        }

        string isoPath;
        switch (srcDropdown.index)
        {
            case 2: isoPath = settings.PathToCleanGcIso; break;
            case 1: isoPath = settings.PathToCleanUyaNtscIso; break;
            case 0: isoPath = settings.PathToCleanDeadlockedIso; break;
            default: throw new NotImplementedException();
        }

        if (String.IsNullOrEmpty(isoPath))
        {
            EditorUtility.DisplayDialog("Cannot import iso", $"Missing CleanIsoPath", "Ok");
            return;
        }

        ImportISO(isoPath, SourceRACVersion[srcDropdown.index], assetToggles[0].value, assetToggles[1].value, assetToggles[2].value, overwriteToggle.value);
    }

    public static bool ImportISO(string isoPath, int isoRacVersion, bool importTies, bool importShrubs, bool importMobys, bool overwrite)
    {
        var assetImports = new List<PackerImporterWindow.PackerAssetImport>(10000);
        var racVersion = isoRacVersion;
        var tempFolder = FolderNames.GetTempFolder();
        var additionalTags = new string[] { Constants.GameAssetTag[isoRacVersion] };
        var soundImportTasks = new List<Task>(10000);
        var mobyOClasses = new HashSet<int>();
        var isoLabelStr = Constants.GameAssetTag[isoRacVersion] + " ISO";
        object[] levelsToImport = null;
        var cancel = false;
        var i = 0;

        // get list of levels to import
        // prioritize mp levels first (since moby sounds might be different in SP)
        switch (racVersion)
        {
            case 4: levelsToImport = ((DLMapIds[])Enum.GetValues(typeof(DLMapIds))).Select(x => (object)x).Reverse().ToArray(); break;
            case 3: levelsToImport = ((UYAMapIds[])Enum.GetValues(typeof(UYAMapIds))).Select(x => (object)x).Reverse().ToArray(); break;
            case 2: levelsToImport = ((GCMapIds[])Enum.GetValues(typeof(GCMapIds))).Select(x => (object)x).Reverse().ToArray(); break;
            default: throw new NotImplementedException();
        }

        try
        {
            // extract and unpack levels first
            foreach (var level in levelsToImport)
            {
                if (CancelProgressBar(ref cancel, $"Extracting {isoLabelStr} Levels", level.ToString(), i / (float)levelsToImport.Length))
                    return false;

                var levelFolder = Path.Combine(tempFolder, $"rc{racVersion}-{(int)level}");
                if (Directory.Exists(levelFolder)) Directory.Delete(levelFolder, true);
                Directory.CreateDirectory(levelFolder);

                // extract wad
                var result = PackerHelper.ExtractLevelWads(isoPath, levelFolder, (int)level, racVersion);
                if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                {
                    Debug.LogError($"Error extracting {level}: {result}");
                    return false;
                }

                // unpack wad
                var wadPath = racVersion == 4 ? Path.Combine(levelFolder, "core_level.wad") : Path.Combine(levelFolder, $"level{(int)level}.0.wad");
                result = PackerHelper.DecompressAndUnpackLevelWad(wadPath, levelFolder);
                if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                {
                    Debug.LogError($"Error unpacking assets {level}: {result}");
                    return false;
                }

                ++i;
            }

            // then gather assets
            i = 0;
            foreach (var level in levelsToImport)
            {
                if (CancelProgressBar(ref cancel, $"Gathering {isoLabelStr} Level Assets ({assetImports.Count} total assets to import)", level.ToString(), i / (float)levelsToImport.Length))
                    return false;

                var result = PackerHelper.PACKER_STATUS_CODES.SUCCESS;
                var levelFolder = Path.Combine(tempFolder, $"rc{racVersion}-{(int)level}");
                var assetsFolder = Path.Combine(levelFolder, FolderNames.AssetsFolder);

                // unpack sounds -- DL only supported atm
                if (racVersion == 4)
                {
                    var soundPath = racVersion == 4 ? Path.Combine(levelFolder, "sound.bnk") : Path.Combine(levelFolder, $"level{(int)level}.1.wad");
                    var soundsFolder = Path.Combine(levelFolder, FolderNames.BinarySoundsFolder);
                    result = PackerHelper.UnpackSounds(soundPath, soundsFolder, racVersion);
                    if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                    {
                        Debug.LogError($"Error unpacking sounds {level}: {result}");
                        cancel = true;
                        continue;
                    }
                }

                if (CancelProgressBar(ref cancel, $"Gathering {isoLabelStr} Level Assets ({assetImports.Count} total assets to import)", level.ToString(), i / (float)levelsToImport.Length))
                    return false;

                // unpack assets
                result = PackerHelper.UnpackAssets(levelFolder, assetsFolder, racVersion);
                if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                {
                    Debug.LogError($"Error unpacking assets {level}: {result}");
                    cancel = true;
                    continue;
                }

                // unpack missions
                var missionsPath = Path.Combine(levelFolder, FolderNames.BinaryMissionsFolder);
                if (Directory.Exists(missionsPath))
                {
                    result = PackerHelper.UnpackMissions(missionsPath, racVersion);
                    if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                    {
                        Debug.LogError($"Error unpacking missions {level}: {result}");
                        cancel = true;
                        continue;
                    }
                }

                if (CancelProgressBar(ref cancel, $"Gathering {isoLabelStr} Level Assets ({assetImports.Count} total assets to import)", level.ToString(), i / (float)levelsToImport.Length))
                    return false;

                // ties
                if (importTies)
                {
                    var tieAssetDir = Path.Combine(assetsFolder, FolderNames.TieFolder);
                    var tieGlobalDir = FolderNames.GetGlobalAssetFolder(FolderNames.TieFolder);
                    var tieDirs = Directory.EnumerateDirectories(tieAssetDir).ToList();

                    // convert ties -- convert on export now, not on import
                    //if (PackerHelper.ConvertTies(tieAssetDir, tieAssetDir, rcVersion, Constants.GameVersion) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                    //{
                    //    Debug.LogError($"Unable to convert rc{rcVersion} ties for {level}. Skipping.");
                    //}

                    foreach (var tieDir in tieDirs)
                    {
                        var oclass = FolderNames.GetOClassFromAssetFolderName(Path.GetFileName(tieDir));

                        assetImports.Add(new PackerImporterWindow.PackerAssetImport()
                        {
                            AssetFolder = tieDir,
                            AssetType = FolderNames.TieFolder,
                            DestinationFolder = Path.Combine(tieGlobalDir, $"{oclass}"),
                            Name = $"{oclass}",
                            PrependModelNameToTextures = true,
                            GenerateCollisionId = 0x2f,
                            RacVersion = racVersion,
                            AdditionalTags = additionalTags
                        });
                    }
                }

                if (CancelProgressBar(ref cancel, $"Gathering {isoLabelStr} Level Assets ({assetImports.Count} total assets to import)", level.ToString(), i / (float)levelsToImport.Length))
                    return false;

                // shrubs
                if (importShrubs)
                {
                    var shrubAssetDir = Path.Combine(assetsFolder, FolderNames.ShrubFolder);
                    var shrubGlobalDir = FolderNames.GetGlobalAssetFolder(FolderNames.ShrubFolder);
                    var shrubDirs = Directory.EnumerateDirectories(shrubAssetDir).ToList();

                    foreach (var shrubDir in shrubDirs)
                    {
                        var oclass = FolderNames.GetOClassFromAssetFolderName(Path.GetFileName(shrubDir));

                        assetImports.Add(new PackerImporterWindow.PackerAssetImport()
                        {
                            AssetFolder = shrubDir,
                            AssetType = FolderNames.ShrubFolder,
                            DestinationFolder = Path.Combine(shrubGlobalDir, $"{oclass}"),
                            Name = $"{oclass}",
                            PrependModelNameToTextures = true,
                            GenerateCollisionId = 0x2f,
                            RacVersion = racVersion,
                            AdditionalTags = additionalTags
                        });
                    }
                }

                if (CancelProgressBar(ref cancel, $"Gathering {isoLabelStr} Level Assets ({assetImports.Count} total assets to import)", level.ToString(), i / (float)levelsToImport.Length))
                    return false;

                // moby conversion not yet supported between games
                if (racVersion == Constants.GameVersion && importMobys)
                {
                    var mobyAssetDir = Path.Combine(assetsFolder, FolderNames.MobyFolder);
                    var mobyGlobalDir = FolderNames.GetGlobalAssetFolder(FolderNames.MobyFolder);
                    var mobyDirs = Directory.EnumerateDirectories(mobyAssetDir).ToList();

                    // missions may have more mobys
                    if (Directory.Exists(missionsPath))
                    {
                        var missionDirs = Directory.EnumerateDirectories(missionsPath);
                        foreach (var missionDir in missionDirs)
                        {
                            var missionMobyAssetDir = Path.Combine(missionDir, FolderNames.BinaryMobyFolder);
                            if (Directory.Exists(missionMobyAssetDir))
                            {
                                var missionMobyDirs = Directory.EnumerateDirectories(missionMobyAssetDir)?.ToArray();
                                if (missionMobyDirs != null)
                                    mobyDirs.AddRange(missionMobyDirs);
                            }
                        }
                    }

                    foreach (var mobyDir in mobyDirs)
                    {
                        var oclass = FolderNames.GetOClassFromAssetFolderName(Path.GetFileName(mobyDir));

                        // import if not already imported
                        if (!mobyOClasses.Contains(oclass))
                        {
                            // ensure moby has model
                            // don't import runtime mobys
                            var mobyBinFile = Path.Combine(mobyDir, "moby.bin");
                            if (!File.Exists(mobyBinFile)) continue;

                            var outMobyDir = Path.Combine(mobyGlobalDir, $"{oclass}");
                            if (overwrite || !Directory.Exists(outMobyDir))
                            {
                                var mobySoundsFolder = Path.Combine(mobyDir, FolderNames.BinarySoundsFolder);
                                if (Directory.Exists(mobySoundsFolder))
                                {
                                    var mobyAssetSoundsFolder = Path.Combine(outMobyDir, FolderNames.SoundsFolder);
                                    if (Directory.Exists(mobyAssetSoundsFolder)) Directory.Delete(mobyAssetSoundsFolder, true);
                                    Directory.CreateDirectory(mobyAssetSoundsFolder);

                                    var soundsInFolder = Directory.GetDirectories(mobySoundsFolder);
                                    foreach (var soundFolder in soundsInFolder)
                                    {
                                        var idxStr = Path.GetFileName(soundFolder);
                                        if (Directory.Exists(soundFolder))
                                            PackerHelper.PackSound(soundFolder, Path.Combine(mobyAssetSoundsFolder, $"{idxStr}.sound"));
                                    }
                                }
                            }

                            mobyOClasses.Add(oclass);
                            assetImports.Add(new PackerImporterWindow.PackerAssetImport()
                            {
                                AssetFolder = mobyDir,
                                AssetType = FolderNames.MobyFolder,
                                DestinationFolder = outMobyDir,
                                Name = $"{oclass}",
                                PrependModelNameToTextures = true,
                                RacVersion = racVersion,
                                AdditionalTags = additionalTags
                            });
                        }
                    }
                }

                ++i;
            }

            if (cancel) return false;
        }
        finally
        {
            EditorUtility.ClearProgressBar();
        }

        // import
        PackerImporterWindow.Import(assetImports, overwrite);
        return true;
    }

    static bool CancelProgressBar(ref bool cancel, string title, string info, float progress)
    {
        cancel |= EditorUtility.DisplayCancelableProgressBar(title, info, progress);
        //System.Threading.Thread.Sleep(1);
        return cancel;
    }

}
