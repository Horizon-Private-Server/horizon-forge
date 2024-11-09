using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(RatchetCamera))]
public class RatchetCameraEditor : Editor
{
    SerializedProperty m_RCVersion;
    SerializedProperty m_CameraType;
    SerializedProperty m_PVars;
    SerializedProperty m_PVarValues;
    SerializedProperty m_PVarRefs;
    SerializedProperty m_PVarStrings;
    UnityHelper.PVarsPropertiesContainer m_PVarPropertiesContainer;

    private void OnEnable()
    {
        m_RCVersion = serializedObject.FindProperty("RCVersion");
        m_CameraType = serializedObject.FindProperty("CameraType");
        m_PVars = serializedObject.FindProperty("PVars");
        m_PVarValues = serializedObject.FindProperty("PVarValues");
        m_PVarRefs = serializedObject.FindProperty("PVarReferences");
        m_PVarStrings = serializedObject.FindProperty("PVarStrings");

        m_PVarPropertiesContainer = new UnityHelper.PVarsPropertiesContainer()
        {
            PVars = m_PVars,
            PVarValues = m_PVarValues,
            PVarRefs = m_PVarRefs,
            Strings = m_PVarStrings
        };
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();
        EditorGUILayout.PropertyField(m_RCVersion);
        EditorGUILayout.PropertyField(m_CameraType);
        UnityHelper.PVarsPropertyField(m_PVarPropertiesContainer, target as RatchetCamera, (target as RatchetCamera).RCVersion, cameraType: (target as RatchetCamera).CameraType);
        serializedObject.ApplyModifiedProperties();
    }
}
