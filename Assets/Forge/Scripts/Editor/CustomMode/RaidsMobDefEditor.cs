using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(RaidsMobDef))]
public class RaidsMobDefEditor : Editor
{
    private int _difficulty;
    private SerializedProperty _mob, _variant, _actions, _parameters, _texturePalette;

    private void OnEnable()
    {
        _mob = serializedObject.FindProperty("Mob");
        _variant = serializedObject.FindProperty("Variant");
        _actions = serializedObject.FindProperty("Actions");
        _parameters = serializedObject.FindProperty("Parameters");
        _texturePalette = serializedObject.FindProperty("TexturePalette");
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();
        EditorGUILayout.PropertyField(_mob);
        var defaults = RaidsMobsScriptableObject.Load()?.Mobs.FirstOrDefault(x => (int)x.Mob == _mob.intValue);
        if (defaults == null)
        {
            EditorGUILayout.HelpBox("The selected mob has no raids config.", MessageType.Error);
            serializedObject.ApplyModifiedProperties();
            return;
        }
        Help(defaults.Description);
        _variant.intValue = EditorGUILayout.Popup("Variant", _variant.intValue, defaults.Variants.Select(x => x.Name).ToArray());
        var variant = defaults.Variants.ElementAtOrDefault(_variant.intValue);
        Help(variant?.Description);
        EditorGUILayout.PropertyField(serializedObject.FindProperty("IsBoss"));
        UnityHelper.EnumOverride(serializedObject.FindProperty("BlipType"), null, defaults.BlipType);

        Section("Mob Settings");
        EditorGUILayout.PropertyField(serializedObject.FindProperty("SizeMultiplier"));

        Section("Perception");
        UnityHelper.FloatOverride(serializedObject.FindProperty("VisionRange"), null, defaults.VisionRange);
        var visionRect = EditorGUILayout.GetControlRect(false, EditorGUIUtility.singleLineHeight);
        UnityHelper.FloatOverride(visionRect, serializedObject.FindProperty("PeripheralVisionDegrees"), null, defaults.PeripheralVisionDegrees, rangeMin: 0, rangeMax: 360);
        UnityHelper.FloatOverride(serializedObject.FindProperty("ForceAggroRange"), null, defaults.ForceAggroRange);
        UnityHelper.FloatOverride(serializedObject.FindProperty("OutOfSightDeAggroTime"), null, defaults.OutOfSightDeAggroTime);

        Section("Colors");
        if (variant?.SupportsTeamPalette == true)
            EditorGUILayout.PropertyField(_texturePalette);
        UnityHelper.ColorOverride(serializedObject.FindProperty("BaseColor"), null, defaults.BaseColor);
        UnityHelper.ColorOverride(serializedObject.FindProperty("GlowColor"), null, defaults.GlowColor);
        UnityHelper.ColorOverride(serializedObject.FindProperty("SpriteColor"), null, defaults.SpriteColor);

        if (_actions.arraySize > defaults.Actions.Count || _parameters.arraySize > defaults.Parameters.Count)
        {
            EditorGUILayout.HelpBox("Overrides from another mob remain. Reset overrides before building this definition.", MessageType.Error);
            if (GUILayout.Button("Reset Action and Parameter Overrides"))
            {
                _actions.ClearArray();
                _parameters.ClearArray();
            }
        }
        Section("Difficulty Overrides");
        _difficulty = GUILayout.Toolbar(_difficulty, RaidsMobsScriptableObject.DifficultyNames, GUILayout.Height(24));
        EditorGUILayout.LabelField("Fields below apply to the selected difficulty.", EditorStyles.wordWrappedMiniLabel);
        var difficultyField = RaidsMobsScriptableObject.DifficultyFields[_difficulty];
        if (defaults.Actions.Count > 0) Section($"{defaults.Mob} Actions");
        Grow(_actions, defaults.Actions.Count, new[] { "MinCooldownSeconds", "MaxCooldownSeconds", "Probability", "QueuedForTicks" }, perDifficulty: true);
        foreach (var i in Enumerable.Range(0, defaults.Actions.Count).OrderBy(i => defaults.Actions[i].Name))
        {
            var config = defaults.Actions[i];
            var property = _actions.GetArrayElementAtIndex(i);
            property.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(property.isExpanded, new GUIContent(config.Name, config.Description));
            if (property.isExpanded)
            {
                Help(config.Description);
                UnityHelper.FloatOverride(property.FindPropertyRelative("MinCooldownSeconds"), difficultyField, config.MinCooldownSeconds.Get(_difficulty), label: "Min Cooldown (sec)");
                UnityHelper.FloatOverride(property.FindPropertyRelative("MaxCooldownSeconds"), difficultyField, config.MaxCooldownSeconds.Get(_difficulty), label: "Max Cooldown (sec)");
                Probability(property.FindPropertyRelative("Probability").FindPropertyRelative(difficultyField), config.Probability.Get(_difficulty), "Probability", "0 = never; 1 = always");
                UnityHelper.Int32Override(property.FindPropertyRelative("QueuedForTicks"), difficultyField, config.QueuedForTicks.Get(_difficulty), label: "Queued Ticks", tooltip: "0 = unlimited");
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
        }

        if (defaults.Parameters.Count > 0) Section($"{defaults.Mob} Parameters");
        Grow(_parameters, defaults.Parameters.Count, new[] { "FloatValue", "IntValue", "BoolValue" }, perDifficulty: true);
        foreach (var i in Enumerable.Range(0, defaults.Parameters.Count).OrderBy(i => defaults.Parameters[i].Name))
        {
            var config = defaults.Parameters[i];
            var property = _parameters.GetArrayElementAtIndex(i);
            var defaultValue = config.DefaultValue.Get(_difficulty);
            var tooltip = $"{config.Name}\n{config.Description}".Trim();
            switch (config.ValueType)
            {
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Boolean:
                    var boolean = (bool.TryParse(defaultValue, out var b) && b) || (int.TryParse(defaultValue, out var n) && n != 0);
                    UnityHelper.BoolOverride(property.FindPropertyRelative("BoolValue"), difficultyField, boolean, label: config.Name, tooltip: tooltip);
                    break;
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Integer:
                    int.TryParse(defaultValue, out var integer);
                    UnityHelper.Int32Override(property.FindPropertyRelative("IntValue"), difficultyField, integer, label: config.Name, tooltip: tooltip);
                    break;
                case RaidsMobsScriptableObject.RaidsMobParameter.InputType.Probability:
                    Probability(property.FindPropertyRelative("FloatValue").FindPropertyRelative(difficultyField), NumberHelper.ParseFloatInvariant(defaultValue) ?? 0, config.Name, tooltip);
                    break;
                default:
                    var number = NumberHelper.ParseFloatInvariant(defaultValue) ?? 0;
                    UnityHelper.FloatOverride(property.FindPropertyRelative("FloatValue"), difficultyField, number, label: config.Name, tooltip: tooltip);
                    break;
            }
        }
        Section("Stats");
        UnityHelper.FloatOverride(serializedObject.FindProperty("HealthOverrides"), difficultyField, defaults.Health.Get(_difficulty), label: "Health");
        UnityHelper.FloatOverride(serializedObject.FindProperty("DamageOverrides"), difficultyField, defaults.Damage.Get(_difficulty), label: "Damage");
        UnityHelper.FloatOverride(serializedObject.FindProperty("SpeedOverrides"), difficultyField, defaults.Speed.Get(_difficulty), label: "Speed");
        UnityHelper.FloatOverride(serializedObject.FindProperty("TurnSpeed"), difficultyField, defaults.TurnSpeed.Get(_difficulty), label: "Turn Speed");

        Section("Rewards");
        UnityHelper.Int32Override(serializedObject.FindProperty("XpOverrides"), difficultyField, defaults.Xp.Get(_difficulty), label: "Hero / Weapon XP",
            tooltip: "Base kill XP used for both Hero XP and Weapon XP rewards.");
        UnityHelper.Int32Override(serializedObject.FindProperty("BoltsOverrides"), difficultyField, defaults.Bolts.Get(_difficulty), label: "Bolts");

        serializedObject.ApplyModifiedProperties();
    }

    private static void Section(string title)
    {
        EditorGUILayout.Space();
        EditorGUILayout.LabelField(title, EditorStyles.boldLabel);
    }

    private static void Help(string text)
    {
        if (!string.IsNullOrEmpty(text)) EditorGUILayout.HelpBox(text, MessageType.Info);
    }

    private static void Probability(SerializedProperty property, float defaultValue, string label, string description)
    {
        var rect = EditorGUILayout.GetControlRect(false, EditorGUIUtility.singleLineHeight);
        UnityHelper.FloatOverride(rect, property, null, defaultValue, label: label, tooltip: description, rangeMin: 0, rangeMax: 1);
    }

    private static void Grow(SerializedProperty array, int count, string[] fields, bool perDifficulty = false)
    {
        while (array.arraySize < count)
        {
            var i = array.arraySize++;
            foreach (var field in fields)
            {
                var property = array.GetArrayElementAtIndex(i).FindPropertyRelative(field);
                if (perDifficulty)
                {
                    foreach (var difficulty in RaidsMobsScriptableObject.DifficultyFields)
                        property.FindPropertyRelative(difficulty).FindPropertyRelative("HasOverride").boolValue = false;
                }
                else property.FindPropertyRelative("HasOverride").boolValue = false;
            }
        }
    }
}
