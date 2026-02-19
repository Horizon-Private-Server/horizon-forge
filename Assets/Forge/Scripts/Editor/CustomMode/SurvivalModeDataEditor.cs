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
    private SerializedProperty m_DebugEnabledProperty;
    private SerializedProperty m_DebugPathProperty;
    private SerializedProperty m_DebugMoveProperty;
    private SerializedProperty m_DebugManualSpawningProperty;
    private SerializedProperty m_DebugInfiniteHealthProperty;
    private SerializedProperty m_DebugInfiniteAmmoProperty;
    private SerializedProperty m_DebugPaydayProperty;
    private SerializedProperty m_DebugMoonjumpProperty;
    private SerializedProperty m_DebugStartRoundProperty;

    private SurvivalMobDef[] m_Mobs;
    
    private void OnEnable()
    {
        m_DebugEnabledProperty = serializedObject.FindProperty("DebugEnabled");
        m_DebugPathProperty = serializedObject.FindProperty("DebugPath");
        m_DebugMoveProperty = serializedObject.FindProperty("DebugMove");
        m_DebugManualSpawningProperty = serializedObject.FindProperty("DebugManualSpawning");
        m_DebugInfiniteHealthProperty = serializedObject.FindProperty("DebugInfiniteHealth");
        m_DebugInfiniteAmmoProperty = serializedObject.FindProperty("DebugInfiniteAmmo");
        m_DebugPaydayProperty = serializedObject.FindProperty("DebugPayday");
        m_DebugMoonjumpProperty = serializedObject.FindProperty("DebugMoonjump");
        m_DebugStartRoundProperty = serializedObject.FindProperty("DebugStartRound");

        m_Mobs = (target as SurvivalModeData).GetEnabledMobs();
    }

    public override void OnInspectorGUI()
    {
        var manager = (target as SurvivalModeData);

        // draw base
        base.OnInspectorGUI();

        // draw debug options
        serializedObject.Update();
        GUILayout.Space(20);
        EditorGUILayout.PropertyField(m_DebugEnabledProperty, new GUIContent("Debug"));
        if (m_DebugEnabledProperty.boolValue)
        {
            EditorGUI.indentLevel++;
            EditorGUILayout.PropertyField(m_DebugPathProperty, new GUIContent("Mob Pathfinding"));
            EditorGUILayout.PropertyField(m_DebugMoveProperty, new GUIContent("Mob Move"));
            EditorGUILayout.PropertyField(m_DebugInfiniteHealthProperty, new GUIContent("Infinite Health"));
            EditorGUILayout.PropertyField(m_DebugInfiniteAmmoProperty, new GUIContent("Infinite Ammo"));
            EditorGUILayout.PropertyField(m_DebugPaydayProperty, new GUIContent("Max Money/Tokens"));
            EditorGUILayout.PropertyField(m_DebugMoonjumpProperty, new GUIContent("Moonjump"));
            EditorGUILayout.PropertyField(m_DebugStartRoundProperty, new GUIContent("Start at Round"));
            EditorGUILayout.PropertyField(m_DebugManualSpawningProperty, new GUIContent("Manual Mob Spawning"));
            if (m_DebugManualSpawningProperty.boolValue)
            {
                EditorGUILayout.HelpBox("Use PAD LEFT/RIGHT to select a mob. Refer to the PCSX2 console for logging.\nUse PAD DOWN to spawn selected mob.\nUse PAD UP to destroy all mobs.", MessageType.Info, true);
            }
            EditorGUI.indentLevel--;
        }
        serializedObject.ApplyModifiedProperties();

        // draw asset functions
        CheckForMissingMobAssets(manager);
    }

    private void CheckForMissingMobAssets(SurvivalModeData survivalModeData)
    {
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var mapName = SceneManager.GetActiveScene().name;
        var racVersion = RCVER.DL;
        var mobyDir = $"{FolderNames.GetMapFolder(mapName)}/{FolderNames.GetMapMobyFolder(racVersion)}";
        var missingVariants = new List<(string, SurvivalMobsScriptableObject.SurvivalMobVariant)>();
        foreach (var mob in m_Mobs)
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
                    missingVariants.Add((mob.Name, variant));
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
                foreach (var item in missingVariants)
                {
                    var variant = item.Item2;
                    foreach (var dependency in variant.Dependencies)
                    {
                        PackerHelper.ExtractAndInstallMoby(mapName, racVersion, dependency.SourceMapId, dependency.SourceMissionId, dependency.OClass, name: $"{item.Item1}:{variant.Name}", overwrite: true);
                    }
                }
            }
        }

        if (GUILayout.Button("Reinstall All Mobs"))
        {
            foreach (var mob in m_Mobs)
            {
                var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == mob.Mob);
                var variant = mobDefaults.Variants.ElementAtOrDefault(mob.Variant);
                if (variant == null) continue;

                foreach (var dependency in variant.Dependencies)
                {
                    PackerHelper.ExtractAndInstallMoby(mapName, racVersion, dependency.SourceMapId, dependency.SourceMissionId, dependency.OClass, name: $"{mob.Name}:{variant.Name}", overwrite: true);
                }
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

        return EditorGUIUtility.singleLineHeight * 36;
    }

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
        if (!survivalData) survivalData = SurvivalMobsScriptableObject.Load();
        var mob = ((SurvivalMob[])Enum.GetValues(typeof(SurvivalMob)))[property.FindPropertyRelative("Mob").enumValueIndex];
        var mobData = survivalData.Mobs.FirstOrDefault(x => x.Mob == mob);
        if (mobData == null) return;

        EditorGUI.BeginProperty(position, label, property);

        var line = new Rect(position.x, position.y, position.width, EditorGUIUtility.singleLineHeight);
        property.isExpanded = EditorGUI.ToggleLeft(line, label, property.isExpanded);
        if (property.isExpanded)
        {
            var indent = EditorGUI.indentLevel;
            EditorGUI.indentLevel = 0;

            // Draw fields
            line.y += EditorGUIUtility.singleLineHeight;
            line = UnityHelper.PropertyField(line, property, "Name");
            line = UnityHelper.PropertyField(line, property, "Disabled");
            line = UnityHelper.PropertyField(line, property, "Mob");
            line = UnityHelper.PopupField(line, property, "Variant", mobData.Variants.Select(x => x.Name).ToArray());
            line = UnityHelper.PopupField(line, property, "Behavior", mobData.Behaviors.ToArray());
            line = UnityHelper.PropertyField(line, property, "Attributes");
            line = UnityHelper.EnumOverride(line, property, "BlipType", mobData.BlipType);

            line = UnityHelper.PropertyField(line, property, "SpecialRoundOnly");
            line = UnityHelper.PropertyField(line, property, "MinRound");
            line = UnityHelper.PropertyField(line, property, "MaxSpawnedAtOnce");
            line = UnityHelper.PropertyField(line, property, "MaxSpawnedPerRound");
            line = UnityHelper.PropertyField(line, property, "Probability");
            line = UnityHelper.PropertyField(line, property, "SpawnType");
            line = UnityHelper.PropertyField(line, property, "CooldownTicks");
            line = UnityHelper.PropertyField(line, property, "CooldownOffsetPerRoundFactor");

            line = UnityHelper.PropertyField(line, property, "SizeMultiplier");
            line = UnityHelper.PropertyField(line, property, "TurnSpeedMultiplier");
            line = UnityHelper.FloatOverride(line, property, "RangedAttackDistance", mobData.RangedAttackDistance);
            line = UnityHelper.FloatOverride(line, property, "Xp", mobData.Xp);
            line = UnityHelper.FloatOverride(line, property, "Bolts", mobData.Bolts);
            line = UnityHelper.FloatOverride(line, property, "Damage", mobData.Damage);
            line = UnityHelper.FloatOverride(line, property, "DamageMax", mobData.DamageMax);
            line = UnityHelper.FloatOverride(line, property, "DamageScale", mobData.DamageScale);
            line = UnityHelper.FloatOverride(line, property, "Speed", mobData.Speed);
            line = UnityHelper.FloatOverride(line, property, "SpeedMax", mobData.SpeedMax);
            line = UnityHelper.FloatOverride(line, property, "SpeedScale", mobData.SpeedScale);
            line = UnityHelper.FloatOverride(line, property, "Health", mobData.Health);
            line = UnityHelper.FloatOverride(line, property, "HealthMax", mobData.HealthMax);
            line = UnityHelper.FloatOverride(line, property, "HealthScale", mobData.HealthScale);
            line = UnityHelper.ColorOverride(line, property, "BaseColor", mobData.BaseColor);
            line = UnityHelper.ColorOverride(line, property, "GlowColor", mobData.GlowColor);
            line = UnityHelper.ColorOverride(line, property, "SpriteColor", mobData.SpriteColor);

            EditorGUI.indentLevel = indent;
        }

        //EditorGUI.EndFoldoutHeaderGroup();
        EditorGUI.EndProperty();
    }

}

[CustomPropertyDrawer(typeof(SurvivalDefaultItemOverrideEntry))]
public class SurvivalTemplateItemEntryDrawer : PropertyDrawer
{
    SurvivalMobsScriptableObject survivalConfig;
    SurvivalModeData survivalModeData;
    string[] itemOptions;
    int[] itemOptionMapping;

    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        var height = EditorGUIUtility.singleLineHeight;

        //if (property.isExpanded)
            height += EditorGUIUtility.singleLineHeight * 17;

        return height;
    }

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
        if (!survivalConfig) survivalConfig = SurvivalMobsScriptableObject.Load();
        if (!survivalModeData) survivalModeData = GameObject.FindObjectOfType<SurvivalModeData>();
        if (itemOptions == null) BuildItemOptions();

        var defaultItemId = ((SurvivalDefaultItems[])Enum.GetValues(typeof(SurvivalDefaultItems)))[property.FindPropertyRelative("Item").enumValueIndex];
        var defaultItemDef = survivalConfig.SurvivalDefaultItems.FirstOrDefault(x => x.Item == defaultItemId);
        if (defaultItemDef == null) return;

        // compute sum total mbox weights
        var mboxSumWeights = Math.Max(survivalModeData?.GetMysteryboxSumWeights() ?? 1, 0.001f);
        var dropSumWeights = Math.Max(survivalModeData?.GetDropsSumWeights() ?? 1, 0.001f);
        var wepUpgradeSumWeights = Math.Max(survivalModeData?.GetVendorWeaponUpgradeSumWeights() ?? 1, 0.001f);

        EditorGUI.BeginProperty(position, label, property);

        var line = new Rect(position.x, position.y, position.width, EditorGUIUtility.singleLineHeight);
        //property.isExpanded = EditorGUI.ToggleLeft(line, labelWithType, property.isExpanded);
        if (property.isExpanded || true)
        {
            var indent = EditorGUI.indentLevel;
            EditorGUI.indentLevel = 0;

            // draw item selection
            line.y += EditorGUIUtility.singleLineHeight;
            line = UnityHelper.PopupField(line, property, "Item", itemOptions, itemOptionMapping);

            // draw selected item description
            EditorGUI.HelpBox(line, defaultItemDef.Def.Description, MessageType.Info);
            line.y += EditorGUIUtility.singleLineHeight;

            // draw override fields
            line = UnityHelper.Int32Override(line, property, "MaxHeldAtOnce", defaultItemDef.Def.MaxHeldAtOnce);
            line = UnityHelper.BoolOverride(line, property, "AppearOnWall", defaultItemDef.Def.AppearOnWall);
            line = UnityHelper.FloatOverride(line, property, "ConsumeCooldown", defaultItemDef.Def.ConsumeCooldown);
            line = UnityHelper.EnumOverride(line, property, "StoreCostType", defaultItemDef.Def.StoreCostType);
            line = UnityHelper.UInt32Override(line, property, "StoreCost", defaultItemDef.Def.StoreCost);
            line = UnityHelper.DoubleOverride(line, property, "StoreCostIncrease", defaultItemDef.Def.StoreCostIncrease);
            line = UnityHelper.FloatOverride(line, property, "MysteryboxChanceWeight", defaultItemDef.Def.MysteryboxChanceWeight);
            line = UnityHelper.BoolOverride(line, property, "MysteryboxForceAcquire", defaultItemDef.Def.MysteryboxForceAcquire);
            line = UnityHelper.FloatOverride(line, property, "DropChanceWeight", defaultItemDef.Def.DropChanceWeight);
            line = UnityHelper.FloatOverride(line, property, "VendorRewardChanceWeight", defaultItemDef.Def.VendorRewardChanceWeight);
            
            // draw probabilities
            if (survivalModeData)
            {
                // mbox
                var chanceProp = property.FindPropertyRelative("MysteryboxChanceWeight");
                var chanceHasOverride = chanceProp.FindPropertyRelative("HasOverride").boolValue;
                var chanceOverride = chanceProp.FindPropertyRelative("OverrideValue").floatValue;
                var chance = chanceHasOverride ? chanceOverride : defaultItemDef.Def.MysteryboxChanceWeight;
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"Computed Mystery Box Probability: {(chance / mboxSumWeights):P2}", MessageType.Info);

                // drop
                chanceProp = property.FindPropertyRelative("DropChanceWeight");
                chanceHasOverride = chanceProp.FindPropertyRelative("HasOverride").boolValue;
                chanceOverride = chanceProp.FindPropertyRelative("OverrideValue").floatValue;
                chance = chanceHasOverride ? chanceOverride : defaultItemDef.Def.DropChanceWeight;
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"Computed Mob Drop Probability: {(chance / dropSumWeights):P2}", MessageType.Info);

                // vendor weapon upgrade
                chanceProp = property.FindPropertyRelative("VendorRewardChanceWeight");
                chanceHasOverride = chanceProp.FindPropertyRelative("HasOverride").boolValue;
                chanceOverride = chanceProp.FindPropertyRelative("OverrideValue").floatValue;
                chance = chanceHasOverride ? chanceOverride : defaultItemDef.Def.VendorRewardChanceWeight;
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"Computed Vendor Reward Probability: {(chance / wepUpgradeSumWeights):P2}", MessageType.Info);

                // item idx define
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"-D{(property.boxedValue as SurvivalDefaultItemOverrideEntry)?.DefineName}=#", MessageType.Info);
            }

            EditorGUI.indentLevel = indent;
        }

        EditorGUI.EndProperty();
    }

    private void BuildItemOptions()
    {
        var dict = ((SurvivalDefaultItems[])Enum.GetValues(typeof(SurvivalDefaultItems)))
            .ToDictionary(x =>
            {
                if (survivalConfig)
                {
                    var def = survivalConfig.SurvivalDefaultItems.FirstOrDefault(def => def.Item == x);
                    if (def != null)
                    {
                        return $"{def.Def.Type.GetInspectorName()}/{def.Def.Name}";
                    }
                }

                return ObjectNames.NicifyVariableName(x.ToString());
            }, x => (int)x);

        itemOptions = dict.Keys.OrderBy(x => x).ToArray();
        itemOptionMapping = itemOptions.Select(x => dict[x]).ToArray();
    }

}

[CustomPropertyDrawer(typeof(SurvivalItemEntry))]
public class SurvivalItemEntryDrawer : PropertyDrawer
{
    SurvivalModeData survivalModeData;

    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        var height = EditorGUI.GetPropertyHeight(property, label, includeChildren: true);

        if (property.isExpanded)
            height += EditorGUIUtility.singleLineHeight * 3;

        return height;
    }

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
        if (!survivalModeData) survivalModeData = GameObject.FindObjectOfType<SurvivalModeData>();

        // compute sum total mbox weights
        var mboxSumWeights = Math.Max(survivalModeData?.GetMysteryboxSumWeights() ?? 1, 0.001f);
        var dropSumWeights = Math.Max(survivalModeData?.GetDropsSumWeights() ?? 1, 0.001f);
        var wepUpgradeSumWeights = Math.Max(survivalModeData?.GetVendorWeaponUpgradeSumWeights() ?? 1, 0.001f);

        EditorGUI.BeginProperty(position, label, property);

        var line = new Rect(position.x, position.y, Math.Max(0, position.width), EditorGUIUtility.singleLineHeight);
        property.isExpanded = EditorGUI.ToggleLeft(line, label, property.isExpanded);
        if (property.isExpanded)
        {
            var indent = EditorGUI.indentLevel;
            EditorGUI.indentLevel = 0;

            // Draw fields
            line.y += EditorGUIUtility.singleLineHeight;
            line = UnityHelper.PropertyField(line, property, "Name");
            line = UnityHelper.PropertyField(line, property, "Description");

            line = UnityHelper.PropertyField(line, property, "Type");
            line = UnityHelper.PropertyField(line, property, "MaxHeldAtOnce");
            line = UnityHelper.PropertyField(line, property, "AppearOnWall");
            line = UnityHelper.PropertyField(line, property, "ConsumeCooldown");

            line = UnityHelper.PropertyField(line, property, "TexId");
            line = UnityHelper.PropertyField(line, property, "TexColor");
            line = UnityHelper.PropertyField(line, property, "SpriteDefOverride");

            line = UnityHelper.PropertyField(line, property, "StoreCostType");
            line = UnityHelper.PropertyField(line, property, "StoreCost");
            line = UnityHelper.PropertyField(line, property, "StoreCostIncrease");
            line = UnityHelper.PropertyField(line, property, "MysteryboxChanceWeight");
            line = UnityHelper.PropertyField(line, property, "MysteryboxForceAcquire");
            line = UnityHelper.PropertyField(line, property, "DropChanceWeight");
            line = UnityHelper.PropertyField(line, property, "VendorRewardChanceWeight");
            
            line = UnityHelper.PropertyField(line, property, "CustomInitFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomTickUpdateFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomDrawUpdateFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomOnAcquiredFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomOnConsumedFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomHasRoomForMoreFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomGetConsumeCooldownTicksFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomCanBuyInStoreFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomGetStoreCostFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomGetMysteryboxChanceFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomGetDropChanceFunctionName");
            line = UnityHelper.PropertyField(line, property, "CustomGetVendorRewardChanceFunctionName");

            // draw probabilities
            if (survivalModeData)
            {
                // mbox
                var chance = property.FindPropertyRelative("MysteryboxChanceWeight").floatValue;
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"Computed Mystery Box Probability: {(chance / mboxSumWeights):P2}", MessageType.Info);

                // drop
                chance = property.FindPropertyRelative("DropChanceWeight").floatValue;
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"Computed Mob Drop Probability: {(chance / dropSumWeights):P2}", MessageType.Info);

                // vendor weapon upgrade
                chance = property.FindPropertyRelative("VendorRewardChanceWeight").floatValue;
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"Computed Vendor Reward Probability: {(chance / wepUpgradeSumWeights):P2}", MessageType.Info);

                // item idx define
                line.y += EditorGUIUtility.singleLineHeight;
                EditorGUI.HelpBox(line, $"-D{(property.boxedValue as SurvivalItemEntry)?.DefineName}=#", MessageType.Info);
            }

            EditorGUI.indentLevel = indent;
        }

        EditorGUI.EndProperty();
    }

}
