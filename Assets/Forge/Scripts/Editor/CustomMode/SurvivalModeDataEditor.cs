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

[CustomEditor(typeof(SurvivalModeData))]
public class SurvivalModeDataEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var manager = (target as SurvivalModeData);

        base.OnInspectorGUI();

        CheckForMissingMobAssets(manager);
    }

    private void CheckForMissingMobAssets(SurvivalModeData survivalModeData)
    {
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var mobyDir = $"{FolderNames.GetMapFolder(SceneManager.GetActiveScene().name)}/{FolderNames.GetMapMobyFolder(RCVER.DL)}";
        var missingVariants = new List<SurvivalMobsScriptableObject.SurvivalMobVariant>();
        foreach (var mob in survivalModeData.Mobs)
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

        GUILayout.Space(20);
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
            RunInitialSetup();
            foreach (var mob in survivalModeData.Mobs)
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

    private void RunInitialSetup()
    {
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var mobyDir = $"{FolderNames.GetMapFolder(SceneManager.GetActiveScene().name)}/{FolderNames.GetMapMobyFolder(RCVER.DL)}";

        // install auxillary mobys
        ExtractAndInstallMoby(mobyDir, DLMapIds.SP_Battledome, 43, 9786, "Vendor");
        ExtractAndInstallMoby(mobyDir, DLMapIds.SP_Battledome, 43, 8484, "Big Al");
        ExtractAndInstallMoby(mobyDir, DLMapIds.SP_Battledome, 43, 9795, "Prestige Machine");
        ExtractAndInstallMoby(mobyDir, DLMapIds.SP_Torval, 6, 9781, "Mystery Box");
        ExtractAndInstallMoby(mobyDir, DLMapIds.SP_Torval, 6, 8323, "Stackables Vendor");
    }

    private void ExtractAndInstallMoby(string destMobyFolder, DLMapIds map, int mission, int oclass, string name = null)
    {
        try
        {
            var title = $"Install Moby {name ?? oclass.ToString()}";
            EditorUtility.DisplayProgressBar(title, "Preparing", 0);

            var forgeSettings = ForgeSettings.Load();
            var levelFolder = Path.Combine(FolderNames.GetTempFolder(), $"rc4-{(int)map}");
            var imports = new List<PackerImporterWindow.PackerAssetImport>();

            // reset temp folder
            if (Directory.Exists(levelFolder)) Directory.Delete(levelFolder, true);
            Directory.CreateDirectory(levelFolder);

            EditorUtility.DisplayProgressBar(title, $"Extracting {map}", 0.1f);
            if (PackerHelper.ExtractLevelWads(forgeSettings.PathToCleanDeadlockedIso, levelFolder, (int)map, RCVER.DL) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            EditorUtility.DisplayProgressBar(title, $"Unpacking {map}", 0.2f);
            if (PackerHelper.DecompressAndUnpackLevelWad(Path.Combine(levelFolder, "core_level.wad"), levelFolder) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            EditorUtility.DisplayProgressBar(title, $"Unpacking Sounds {map}", 0.3f);
            if (PackerHelper.UnpackSounds(Path.Combine(levelFolder, "sound.bnk"), Path.Combine(levelFolder, FolderNames.BinarySoundsFolder), RCVER.DL) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            EditorUtility.DisplayProgressBar(title, $"Unpacking Assets {map}", 0.4f);
            if (PackerHelper.UnpackAssets(levelFolder, Path.Combine(levelFolder, FolderNames.BinaryAssetsFolder), RCVER.DL) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                return;

            var mobyAssetPath = Path.Combine(levelFolder, FolderNames.BinaryMobyFolder, PackerHelper.GetAssetOClassFolderName(oclass));
            if (mission >= 0)
            {
                EditorUtility.DisplayProgressBar(title, $"Unpacking Mission #{mission}", 0.5f);
                var missionsPath = Path.Combine(levelFolder, FolderNames.BinaryMissionsFolder);
                PackerHelper.UnpackMission(missionsPath, mission);
                mobyAssetPath = Path.Combine(missionsPath, $"{mission:0000}", FolderNames.BinaryMobyFolder, PackerHelper.GetAssetOClassFolderName(oclass));
            }

            if (!Directory.Exists(mobyAssetPath))
            {
                Debug.LogError($"Unable to find moby class {oclass} in {map} (mission:{mission})");
                return;
            }

            EditorUtility.DisplayProgressBar(title, $"Preprocessing", 0.7f);
            PreprocessMoby(imports, levelFolder, mobyAssetPath, oclass, RCVER.DL);

            EditorUtility.DisplayProgressBar(title, $"Importing", 0.9f);
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

[CustomPropertyDrawer(typeof(SurvivalMobSpawnParam))]
public class SurvivalMobSpawnParamDrawer : PropertyDrawer
{
    SurvivalMobsScriptableObject survivalData;

    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        if (!property.isExpanded) return EditorGUIUtility.singleLineHeight;

        return EditorGUIUtility.singleLineHeight * 32;
    }

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
        if (!survivalData) survivalData = SurvivalMobsScriptableObject.Load();
        var mob = (SurvivalMob)property.FindPropertyRelative("Mob").enumValueIndex;
        var mobData = survivalData.Mobs.FirstOrDefault(x => x.Mob == mob);

        EditorGUI.BeginProperty(position, label, property);

        var line = new Rect(position.x, position.y, position.width, EditorGUIUtility.singleLineHeight);
        property.isExpanded = EditorGUI.ToggleLeft(line, label, property.isExpanded);
        if (property.isExpanded)
        {
            var indent = EditorGUI.indentLevel;
            EditorGUI.indentLevel = 0;

            // Draw fields
            line.y += EditorGUIUtility.singleLineHeight;
            line = PropertyField(line, property, "Name");
            line = PropertyField(line, property, "Disabled");
            line = PropertyField(line, property, "Mob");
            line = PopupOverride(line, property, "Variant", mobData.Variants.Select(x => x.Name).ToArray());
            line = PopupOverride(line, property, "Behavior", mobData.Behaviors.ToArray());
            line = PropertyField(line, property, "Attributes");

            line = PropertyField(line, property, "SpecialRoundOnly");
            line = PropertyField(line, property, "MinRound");
            line = PropertyField(line, property, "MaxSpawnedAtOnce");
            line = PropertyField(line, property, "MaxSpawnedPerRound");
            line = PropertyField(line, property, "Probability");
            line = PropertyField(line, property, "SpawnType");
            line = PropertyField(line, property, "CooldownTicks");
            line = PropertyField(line, property, "CooldownOffsetPerRoundFactor");

            line = PropertyField(line, property, "SizeMultiplier");
            line = PropertyField(line, property, "TurnSpeedMultiplier");
            line = PropertyField(line, property, "RangedAttackDistance");
            line = FloatOverride(line, property, "Xp", mobData.Xp);
            line = FloatOverride(line, property, "Bolts", mobData.Bolts);
            line = FloatOverride(line, property, "Damage", mobData.Damage);
            line = FloatOverride(line, property, "DamageMax", mobData.DamageMax);
            line = FloatOverride(line, property, "DamageScale", mobData.DamageScale);
            line = FloatOverride(line, property, "Speed", mobData.Speed);
            line = FloatOverride(line, property, "SpeedMax", mobData.SpeedMax);
            line = FloatOverride(line, property, "SpeedScale", mobData.SpeedScale);
            line = FloatOverride(line, property, "Health", mobData.Health);
            line = FloatOverride(line, property, "HealthMax", mobData.HealthMax);
            line = FloatOverride(line, property, "HealthScale", mobData.HealthScale);

            EditorGUI.indentLevel = indent;
        }

        EditorGUI.EndFoldoutHeaderGroup();
        EditorGUI.EndProperty();
    }

    Rect PropertyField(Rect position, SerializedProperty property, string field)
    {
        var prop = property.FindPropertyRelative(field);
        var height = EditorGUI.GetPropertyHeight(prop);
        position.height = height;
        EditorGUI.PropertyField(position, prop);

        position.y += height;
        return position;
    }

    Rect PopupOverride(Rect position, SerializedProperty property, string field, string[] options)
    {
        var prop = property.FindPropertyRelative(field);
        var height = EditorGUIUtility.singleLineHeight;
        position.height = height;

        var rect = EditorGUI.PrefixLabel(position, new GUIContent(prop.displayName));
        prop.intValue = EditorGUI.Popup(rect, prop.intValue, options);

        position.y += height;
        return position;
    }

    Rect FloatOverride(Rect position, SerializedProperty property, string field, float defaultValue)
    {
        var prop = property.FindPropertyRelative(field);
        var height = EditorGUIUtility.singleLineHeight;
        position.height = height;

        SerializedProperty hasValueProp = prop.FindPropertyRelative("HasOverride");
        SerializedProperty valueProp = prop.FindPropertyRelative("OverrideValue");

        var prefixRect = EditorGUI.PrefixLabel(position, new GUIContent("A"));
        var labelWidth = position.width - prefixRect.width;
        Rect toggleRect = new Rect(position.x, position.y, 20, position.height);
        Rect labelRect = new Rect(position.x + 22, position.y, labelWidth - 22, position.height);
        Rect valueRect = new Rect(position.x + labelWidth, position.y, position.width - labelWidth, position.height);

        // On/off toggle
        hasValueProp.boolValue = EditorGUI.Toggle(toggleRect, hasValueProp.boolValue);
        GUI.Label(labelRect, prop.displayName);

        // Draw float only if there's a value
        if (hasValueProp.boolValue)
        {
            EditorGUI.PropertyField(valueRect, valueProp, GUIContent.none);
        }
        else
        {
            EditorGUI.BeginDisabledGroup(true);
            EditorGUI.FloatField(valueRect, defaultValue);
            EditorGUI.EndDisabledGroup();
        }

        position.y += height;
        return position;
    }
}

[CustomPropertyDrawer(typeof(SurvivalFloatOverride), true)]
public class SurvivalFloatOverrideDrawer : PropertyDrawer
{
    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        return EditorGUIUtility.singleLineHeight;
    }

    public override void OnGUI(Rect rect, SerializedProperty property, GUIContent label)
    {
        SerializedProperty hasValueProp = property.FindPropertyRelative("HasOverride");
        SerializedProperty valueProp = property.FindPropertyRelative("OverrideValue");

        Rect toggleRect = new Rect(rect.x, rect.y, 20, rect.height);
        Rect valueRect = new Rect(rect.x + 22, rect.y, rect.width - 22, rect.height);

        // On/off toggle
        hasValueProp.boolValue = EditorGUI.Toggle(toggleRect, hasValueProp.boolValue);

        // Draw float only if there's a value
        if (hasValueProp.boolValue)
        {
            EditorGUI.PropertyField(valueRect, valueProp, label);
        }
        else
        {
            EditorGUI.LabelField(valueRect, label, new GUIContent(""));
        }
    }
}