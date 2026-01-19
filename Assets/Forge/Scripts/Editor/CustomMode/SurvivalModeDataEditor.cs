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
            foreach (var mob in survivalModeData.Mobs)
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
            line = PropertyField(line, property, "Name");
            line = PropertyField(line, property, "Disabled");
            line = PropertyField(line, property, "Mob");
            line = PopupOverride(line, property, "Variant", mobData.Variants.Select(x => x.Name).ToArray());
            line = PopupOverride(line, property, "Behavior", mobData.Behaviors.ToArray());
            line = PropertyField(line, property, "Attributes");
            line = EnumOverride(line, property, "BlipType", mobData.BlipType);

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
            line = FloatOverride(line, property, "RangedAttackDistance", mobData.RangedAttackDistance);
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
            line = ColorOverride(line, property, "BaseColor", mobData.BaseColor);
            line = ColorOverride(line, property, "GlowColor", mobData.GlowColor);
            line = ColorOverride(line, property, "SpriteColor", mobData.SpriteColor);

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

    Rect EnumOverride<T>(Rect position, SerializedProperty property, string field, T defaultValue) where T : Enum
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
            EditorGUI.EnumPopup(valueRect, defaultValue);
            EditorGUI.EndDisabledGroup();
        }

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

    Rect ColorOverride(Rect position, SerializedProperty property, string field, Color defaultValue)
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

        // Draw color only if there's a value
        if (hasValueProp.boolValue)
        {
            EditorGUI.PropertyField(valueRect, valueProp, GUIContent.none);
        }
        else
        {
            EditorGUI.BeginDisabledGroup(true);
            EditorGUI.ColorField(valueRect, GUIContent.none, defaultValue, true, false, false);
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