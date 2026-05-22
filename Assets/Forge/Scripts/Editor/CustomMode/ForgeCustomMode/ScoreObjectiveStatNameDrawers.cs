using System;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomPropertyDrawer(typeof(ForgeCustomModeScoreModule.ScoreObjective))]
public class ScoreObjectivePropertyDrawer : PropertyDrawer
{
	const float Spacing = 2;
	const int LineCount = 5;

	public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
	{
		EditorGUI.BeginProperty(position, label, property);

		var line = new Rect(position.x, position.y, position.width, EditorGUIUtility.singleLineHeight);
		property.isExpanded = EditorGUI.Foldout(line, property.isExpanded, label, true);
		if (property.isExpanded)
		{
			EditorGUI.indentLevel++;
			line.y += EditorGUIUtility.singleLineHeight + Spacing;
			EditorGUI.PropertyField(line, property.FindPropertyRelative("Sort"));

			line.y += EditorGUIUtility.singleLineHeight + Spacing;
			DrawStatNamePopup(line, property.FindPropertyRelative("StatName"), GetScoreModule(property));

			line.y += EditorGUIUtility.singleLineHeight + Spacing;
			EditorGUI.PropertyField(line, property.FindPropertyRelative("Target"));

			line.y += EditorGUIUtility.singleLineHeight + Spacing;
			EditorGUI.PropertyField(line, property.FindPropertyRelative("Scoreboard"));

			line.y += EditorGUIUtility.singleLineHeight + Spacing;
			EditorGUI.PropertyField(line, property.FindPropertyRelative("CustomTarget"));
			EditorGUI.indentLevel--;
		}

		EditorGUI.EndProperty();
	}

	public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
	{
		if (!property.isExpanded)
			return EditorGUIUtility.singleLineHeight;

		return (EditorGUIUtility.singleLineHeight * (LineCount + 1)) + (Spacing * LineCount);
	}

	static ForgeCustomModeScoreModule GetScoreModule(SerializedProperty property)
	{
		return property.serializedObject.targetObject as ForgeCustomModeScoreModule;
	}

	public static void DrawStatNamePopup(Rect rect, SerializedProperty statNameProperty, ForgeCustomModeScoreModule scoreModule)
	{
		var options = scoreModule ? scoreModule.GetStatNames() : Array.Empty<string>();
		if (options.Length <= 0)
		{
			EditorGUI.PropertyField(rect, statNameProperty, new GUIContent("Stat Index"));
			return;
		}

		var currentIndex = Array.IndexOf(options, statNameProperty.stringValue);
		if (currentIndex < 0)
		{
			options = options.Concat(new[] { $"{statNameProperty.stringValue} (Missing)" }).ToArray();
			currentIndex = options.Length - 1;
		}

		var optionContents = options.Select(x => new GUIContent(x)).ToArray();
		var selectedIndex = EditorGUI.Popup(rect, new GUIContent("Stat Index", statNameProperty.tooltip), currentIndex, optionContents);
		if (selectedIndex >= 0 && selectedIndex < options.Length && selectedIndex != currentIndex)
			statNameProperty.stringValue = options[selectedIndex].Replace(" (Missing)", "");
	}
}

[CustomEditor(typeof(ForgeCustomModeRoundsModule))]
public class ForgeCustomModeRoundsModuleEditor : Editor
{
	SerializedProperty m_RoundObjectiveStatNameProperty;

	void OnEnable()
	{
		m_RoundObjectiveStatNameProperty = serializedObject.FindProperty("RoundObjectiveStatName");
	}

	public override void OnInspectorGUI()
	{
		serializedObject.Update();

		var iterator = serializedObject.GetIterator();
		var enterChildren = true;
		while (iterator.NextVisible(enterChildren))
		{
			enterChildren = false;
			using (new EditorGUI.DisabledScope(iterator.propertyPath == "m_Script"))
			{
				if (iterator.propertyPath == m_RoundObjectiveStatNameProperty.propertyPath)
					ScoreObjectivePropertyDrawer.DrawStatNamePopup(EditorGUILayout.GetControlRect(), iterator, GetScoreModule());
				else
					EditorGUILayout.PropertyField(iterator, true);
			}
		}

		serializedObject.ApplyModifiedProperties();
	}

	ForgeCustomModeScoreModule GetScoreModule()
	{
		var roundsModule = target as ForgeCustomModeRoundsModule;
		if (!roundsModule)
			return null;

		var modeData = roundsModule.GetComponentInParent<ForgeCustomModeData>(true);
		return modeData ? modeData.GetComponentInChildren<ForgeCustomModeScoreModule>(true) : FindObjectOfType<ForgeCustomModeScoreModule>(true);
	}
}
