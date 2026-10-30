using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(RaidsMobsScriptableObject))]
public class RaidsMobsScriptableObjectEditor : Editor
{
    private SerializedProperty m_MobsProperty;

    private void OnEnable()
    {
        m_MobsProperty = serializedObject.FindProperty("Mobs");
    }


    public override void OnInspectorGUI()
    {
        var manager = (target as RaidsMobsScriptableObject);
        //base.OnInspectorGUI();

        serializedObject.Update();

        foreach (RaidsMob mob in Enum.GetValues(typeof(RaidsMob)))
        {
            var idx = manager.Mobs.FindIndex(x => x.Mob == mob);
            if (idx < 0)
            {
                manager.Mobs.Add(new RaidsMobsScriptableObject.RaidsMobsConfig()
                {
                    Mob = mob,
                    Variants = new List<RaidsMobsScriptableObject.RaidsMobVariant>()
                    {
                        new RaidsMobsScriptableObject.RaidsMobVariant("Normal", 0, DLMapIds.SP_Battledome, 0)
                    }
                });
                EditorUtility.SetDirty(target);
                idx = manager.Mobs.Count - 1;
            }

            EditorGUILayout.PropertyField(m_MobsProperty.GetArrayElementAtIndex(idx), new GUIContent(mob.ToString()));
        }

        serializedObject.ApplyModifiedProperties();
    }

}

[CustomPropertyDrawer(typeof(RaidsMobsScriptableObject.RaidsMobAction))]
public class RaidsMobActionDrawer : PropertyDrawer
{
    private static readonly GUIContent MinCooldownLabel = new GUIContent("Min Cooldown (sec)");
    private static readonly GUIContent MaxCooldownLabel = new GUIContent("Max Cooldown (sec)");
    private static readonly GUIContent ProbabilityLabel = new GUIContent("Probability", "0 = never; 1 = always");
    private static readonly GUIContent QueuedTicksLabel = new GUIContent("Queued Ticks", "0 = unlimited");

    public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
    {
        var height = EditorGUIUtility.singleLineHeight;
        if (!property.isExpanded) return height;

        var spacing = EditorGUIUtility.standardVerticalSpacing;
        height += spacing + EditorGUI.GetPropertyHeight(property.FindPropertyRelative("Name"), true);
        height += spacing + EditorGUI.GetPropertyHeight(property.FindPropertyRelative("Description"), true);
        foreach (var difficulty in RaidsMobsScriptableObject.DifficultyFields)
        {
            height += spacing + EditorGUIUtility.singleLineHeight;
            if (property.FindPropertyRelative("MinCooldownSeconds").FindPropertyRelative(difficulty).isExpanded)
                height += 4 * (spacing + EditorGUIUtility.singleLineHeight);
        }
        return height;
    }

    public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
    {
        EditorGUI.BeginProperty(position, label, property);
        var line = new Rect(position.x, position.y, position.width, EditorGUIUtility.singleLineHeight);
        property.isExpanded = EditorGUI.Foldout(line, property.isExpanded, label, true);
        if (property.isExpanded)
        {
            EditorGUI.indentLevel++;
            NextLine(ref line);
            var name = property.FindPropertyRelative("Name");
            line.height = EditorGUI.GetPropertyHeight(name, true);
            EditorGUI.PropertyField(line, name, true);
            NextLine(ref line);
            var description = property.FindPropertyRelative("Description");
            line.height = EditorGUI.GetPropertyHeight(description, true);
            EditorGUI.PropertyField(line, description, true);

            for (var i = 0; i < RaidsMobsScriptableObject.DifficultyFields.Length; ++i)
            {
                var difficulty = RaidsMobsScriptableObject.DifficultyFields[i];
                var min = property.FindPropertyRelative("MinCooldownSeconds").FindPropertyRelative(difficulty);
                NextLine(ref line);
                min.isExpanded = EditorGUI.Foldout(line, min.isExpanded, RaidsMobsScriptableObject.DifficultyNames[i], true);
                if (!min.isExpanded) continue;

                EditorGUI.indentLevel++;
                NextLine(ref line);
                NonnegativeField(line, min, MinCooldownLabel);
                NextLine(ref line);
                NonnegativeField(line, property.FindPropertyRelative("MaxCooldownSeconds").FindPropertyRelative(difficulty), MaxCooldownLabel);
                NextLine(ref line);
                EditorGUI.Slider(line, property.FindPropertyRelative("Probability").FindPropertyRelative(difficulty), 0, 1, ProbabilityLabel);
                NextLine(ref line);
                NonnegativeField(line, property.FindPropertyRelative("QueuedForTicks").FindPropertyRelative(difficulty), QueuedTicksLabel);
                EditorGUI.indentLevel--;
            }
            EditorGUI.indentLevel--;
        }
        EditorGUI.EndProperty();
    }

    private static void NextLine(ref Rect line)
    {
        line.y += line.height + EditorGUIUtility.standardVerticalSpacing;
        line.height = EditorGUIUtility.singleLineHeight;
    }

    private static void NonnegativeField(Rect position, SerializedProperty property, GUIContent label)
    {
        EditorGUI.BeginChangeCheck();
        EditorGUI.PropertyField(position, property, label);
        if (EditorGUI.EndChangeCheck())
        {
            if (property.propertyType == SerializedPropertyType.Integer)
                property.intValue = Mathf.Max(0, property.intValue);
            else
                property.floatValue = Mathf.Max(0, property.floatValue);
        }
    }
}
