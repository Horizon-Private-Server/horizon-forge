using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

[CustomEditor(typeof(RaidsModeData))]
public class RaidsModeDataEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var manager = (target as RaidsModeData);
        base.OnInspectorGUI();
        CheckForMissingMobAssets(manager);
    }

    private void CheckForMissingMobAssets(RaidsModeData raidsModeData)
    {
        var mobConfig = RaidsMobsScriptableObject.Load();
        var mobyDir = $"{FolderNames.GetMapFolder(SceneManager.GetActiveScene().name)}/{FolderNames.GetMapMobyFolder(RCVER.DL)}";
        var missingVariants = new List<RaidsMobsScriptableObject.RaidsMobVariant>();
        foreach (var mob in raidsModeData.Mobs)
        {
            var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == mob.Mob);
            var variant = mobDefaults.Variants.ElementAtOrDefault(mob.Variant);
            if (variant == null)
            {
                EditorGUILayout.HelpBox($"Variant {mob.Variant} is not valid for {mob.Name} ({mob.Mob})", MessageType.Error);
                continue;
            }

            foreach (var dependency in variant.Dependencies)
            {
                var mobyAssetPath = Path.Combine(mobyDir, $"{dependency.OClass}", "core.bin");
                if (!File.Exists(mobyAssetPath))
                {
                    missingVariants.Add(variant);
                    continue;
                }
            }
        }

        GUILayout.Space(40);
        if (missingVariants.Any())
        {
            EditorGUILayout.HelpBox($"Some mob mobys are not in your Map yet. Please use the button below to install them.", MessageType.Error);
            if (GUILayout.Button("Extract and Install"))
            {
                foreach (var variant in missingVariants)
                {
                    foreach (var dependency in variant.Dependencies)
                    {
                        ExtractAndInstallMoby(mobyDir, dependency.SourceMapId, dependency.SourceMissionId, dependency.OClass);
                    }
                }
            }
        }

        if (GUILayout.Button("Reinstall All Mobs"))
        {
            foreach (var mob in raidsModeData.Mobs)
            {
                var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == mob.Mob);
                var variant = mobDefaults.Variants.ElementAtOrDefault(mob.Variant);
                if (variant == null) continue;

                foreach (var dependency in variant.Dependencies)
                {
                    ExtractAndInstallMoby(mobyDir, dependency.SourceMapId, dependency.SourceMissionId, dependency.OClass);
                }
            }
        }

    }

    private void ExtractAndInstallMoby(string destMobyFolder, DLMapIds map, int mission, int oclass)
    {
        try
        {
            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Preparing", 0);

            var forgeSettings = ForgeSettings.Load();
            var levelFolder = Path.Combine(FolderNames.GetTempFolder(), $"rc4-{(int)map}");
            var imports = new List<PackerImporterWindow.PackerAssetImport>();

            // reset temp folder
            if (Directory.Exists(levelFolder)) Directory.Delete(levelFolder, true);
            Directory.CreateDirectory(levelFolder);

            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Extracting {map}", 0.1f);
            if (PackerHelper.ExtractLevelWads(forgeSettings.PathToCleanDeadlockedIso, levelFolder, (int)map, RCVER.DL) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Unpacking {map}", 0.2f);
            if (PackerHelper.DecompressAndUnpackLevelWad(Path.Combine(levelFolder, "core_level.wad"), levelFolder) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Unpacking Sounds {map}", 0.3f);
            if (PackerHelper.UnpackSounds(Path.Combine(levelFolder, "sound.bnk"), Path.Combine(levelFolder, FolderNames.BinarySoundsFolder), RCVER.DL) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Unpacking Assets {map}", 0.4f);
            if (PackerHelper.UnpackAssets(levelFolder, Path.Combine(levelFolder, FolderNames.BinaryAssetsFolder), RCVER.DL) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            var mobyAssetPath = Path.Combine(levelFolder, FolderNames.BinaryMobyFolder, PackerHelper.GetAssetOClassFolderName(oclass));
            if (mission >= 0)
            {
                EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Unpacking Mission #{mission}", 0.5f);
                var missionsPath = Path.Combine(levelFolder, FolderNames.BinaryMissionsFolder);
                PackerHelper.UnpackMission(missionsPath, mission);
                mobyAssetPath = Path.Combine(missionsPath, $"{mission:0000}", FolderNames.BinaryMobyFolder, PackerHelper.GetAssetOClassFolderName(oclass));
            }

            if (!Directory.Exists(mobyAssetPath))
            {
                Debug.LogError($"Unable to find moby class {oclass} in {map} (mission:{mission})");
                return;
            }

            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Preprocessing", 0.7f);
            PreprocessMoby(imports, levelFolder, mobyAssetPath, oclass, RCVER.DL);

            EditorUtility.DisplayProgressBar($"Install Moby {oclass}", $"Importing", 0.9f);
            ImportMoby(imports, destMobyFolder, mobyAssetPath, oclass, RCVER.DL, true);

            if (imports.Any())
                PackerImporterWindow.Import(imports, true);
        }
        finally
        {
            EditorUtility.ClearProgressBar();
        }
    }

    private void ImportMoby(List<PackerImporterWindow.PackerAssetImport> imports, string destMobyFolder, string srcMobyFolder, int oClass, int racVersion, bool overwrite)
    {
        // recreate moby asset dir
        var localMobyAssetDir = Path.Combine(destMobyFolder, oClass.ToString());
        if (Directory.Exists(localMobyAssetDir))
        {
            if (!overwrite) return;

            Directory.Delete(localMobyAssetDir, true);
        }
        Directory.CreateDirectory(localMobyAssetDir);

        // import sounds
        var soundsFolder = Path.Combine(srcMobyFolder, FolderNames.BinarySoundsFolder);
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

        // add import
        imports.Add(new PackerImporterWindow.PackerAssetImport()
        {
            AssetFolder = srcMobyFolder,
            DestinationFolder = localMobyAssetDir,
            Name = oClass.ToString(),
            AssetType = FolderNames.MobyFolder,
            PrependModelNameToTextures = true,
            RacVersion = racVersion,
            AdditionalTags = new string[] { Constants.GameAssetTag[racVersion] }
        });
    }

    private void PreprocessMoby(List<PackerImporterWindow.PackerAssetImport> imports, string levelFolder, string srcMobyFolder, int oClass, int racVersion)
    {
        var parentMobyDir = Directory.GetParent(srcMobyFolder).FullName;

        // some mobys need to be tweaked before import
        // swarmers for example store animations in the Orange swarmer
        // so other swarmers need to be rebuilt with a copy of the Orange swarmer's animations
        switch (oClass)
        {
            // rebuild swarmer animations
            case 9877:
            case 9952:
                {
                    var orangeSwarmerOClass = 8273;
                    var orangeSwarmerMobyDir = Path.Combine(parentMobyDir, PackerHelper.GetAssetOClassFolderName(orangeSwarmerOClass));
                    if (!Directory.Exists(orangeSwarmerMobyDir)) orangeSwarmerMobyDir = Path.Combine(levelFolder, FolderNames.BinaryMobyFolder, PackerHelper.GetAssetOClassFolderName(orangeSwarmerOClass));
                    if (!Directory.Exists(orangeSwarmerMobyDir))
                    {
                        Debug.LogError($"Unable to find required swarmer {orangeSwarmerOClass} in {orangeSwarmerMobyDir}");
                        return;
                    }

                    var srcUnpackedDir = Path.Combine(orangeSwarmerMobyDir, "unpacked");
                    if (PackerHelper.UnpackMobyModel(Path.Combine(orangeSwarmerMobyDir, "moby.bin"), srcUnpackedDir, racVersion) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                        return;

                    var dstUnpackedDir = Path.Combine(srcMobyFolder, "unpacked");
                    if (PackerHelper.UnpackMobyModel(Path.Combine(srcMobyFolder, "moby.bin"), dstUnpackedDir, racVersion) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                        return;

                    var srcAnimationsDir = Path.Combine(srcUnpackedDir, FolderNames.BinaryMobyAnimationsFolder);
                    var dstAnimationsDir = Path.Combine(dstUnpackedDir, FolderNames.BinaryMobyAnimationsFolder);
                    if (Directory.Exists(dstAnimationsDir)) Directory.Delete(dstAnimationsDir, true);
                    if (!Directory.Exists(srcAnimationsDir))
                    {
                        Debug.LogError($"Required swarmer {orangeSwarmerOClass} missing animations");
                        return;
                    }

                    IOHelper.CopyDirectory(srcAnimationsDir, dstAnimationsDir);
                    if (PackerHelper.PackMobyModel(dstUnpackedDir, srcMobyFolder, racVersion) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                        return;

                    var srcSoundsDir = Path.Combine(orangeSwarmerMobyDir, FolderNames.BinarySoundsFolder);
                    var dstSoundsDir = Path.Combine(srcMobyFolder, FolderNames.BinarySoundsFolder);
                    if (Directory.Exists(srcSoundsDir))
                    {
                        if (Directory.Exists(dstSoundsDir)) Directory.Delete(dstSoundsDir, true);
                        IOHelper.CopyDirectory(srcSoundsDir, dstSoundsDir);
                    }

                    break;
                }
        }
    }


}
