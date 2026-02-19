using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(SurvivalItemDef))]
public class SurvivalItemDefEditor : Editor
{
    private SurvivalMobsScriptableObject m_SurvivalData;

    private SerializedProperty m_IsCustomItemProperty;
    private SerializedProperty m_DefaultItemOverridesProperty;
    private SerializedProperty m_CustomItemProperty;

    private void OnEnable()
    {
        m_IsCustomItemProperty = serializedObject.FindProperty("IsCustomItem");
        m_DefaultItemOverridesProperty = serializedObject.FindProperty("DefaultItemOverrides");
        m_CustomItemProperty = serializedObject.FindProperty("CustomItem");
    }

    public override void OnInspectorGUI()
    {
        if (!m_SurvivalData) m_SurvivalData = SurvivalMobsScriptableObject.Load();
        serializedObject.Update();

        EditorGUILayout.PropertyField(m_IsCustomItemProperty);

        if (m_IsCustomItemProperty.boolValue)
        {
            EditorGUILayout.PropertyField(m_CustomItemProperty);
        }
        else
        {
            EditorGUILayout.PropertyField(m_DefaultItemOverridesProperty);
        }

        serializedObject.ApplyModifiedProperties();
    }

}
