using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(AmbientSound))]
public class AmbientSoundEditor : Editor
{
    SerializedProperty m_RCVersion;
    SerializedProperty m_AmbientSoundType;
    SerializedProperty m_Unknown_0C;
    SerializedProperty m_PVars;
    SerializedProperty m_PVarValues;
    SerializedProperty m_PVarCuboidRefs;
    SerializedProperty m_PVarMobyRefs;
    SerializedProperty m_PVarSplineRefs;
    SerializedProperty m_PVarAreaRefs;
    SerializedProperty m_PVarPathGraphRefs;
    SerializedProperty m_PVarStrings;
    UnityHelper.PVarsPropertiesContainer m_PVarPropertiesContainer;

    private void OnEnable()
    {
        m_RCVersion = serializedObject.FindProperty("RCVersion");
        m_AmbientSoundType = serializedObject.FindProperty("AmbientSoundType");
        m_Unknown_0C = serializedObject.FindProperty("Unknown_0C");
        m_PVars = serializedObject.FindProperty("PVars");
        m_PVarValues = serializedObject.FindProperty("PVarValues");
        m_PVarCuboidRefs = serializedObject.FindProperty("PVarCuboidRefs");
        m_PVarMobyRefs = serializedObject.FindProperty("PVarMobyRefs");
        m_PVarSplineRefs = serializedObject.FindProperty("PVarSplineRefs");
        m_PVarAreaRefs = serializedObject.FindProperty("PVarAreaRefs");
        m_PVarPathGraphRefs = serializedObject.FindProperty("PVarPathGraphRefs");
        m_PVarStrings = serializedObject.FindProperty("PVarStrings");

        m_PVarPropertiesContainer = new UnityHelper.PVarsPropertiesContainer()
        {
            PVars = m_PVars,
            PVarValues = m_PVarValues,
            CuboidRefs = m_PVarCuboidRefs,
            AreaRefs = m_PVarAreaRefs,
            MobyRefs = m_PVarMobyRefs,
            SplineRefs = m_PVarSplineRefs,
            PathGraphRefs = m_PVarPathGraphRefs,
            Strings = m_PVarStrings
        };
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();
        EditorGUILayout.PropertyField(m_RCVersion);
        EditorGUILayout.PropertyField(m_AmbientSoundType);
        EditorGUILayout.PropertyField(m_Unknown_0C);
        UnityHelper.PVarsPropertyField(m_PVarPropertiesContainer, target as AmbientSound, (target as AmbientSound).RCVersion, ambientSoundType: (target as AmbientSound).AmbientSoundType);
        serializedObject.ApplyModifiedProperties();
    }
}
