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
        m_CameraType = serializedObject.FindProperty("CameraType");
        m_PVars = serializedObject.FindProperty("PVars");
        m_PVarCuboidRefs = serializedObject.FindProperty("PVarCuboidRefs");
        m_PVarMobyRefs = serializedObject.FindProperty("PVarMobyRefs");
        m_PVarSplineRefs = serializedObject.FindProperty("PVarSplineRefs");
        m_PVarAreaRefs = serializedObject.FindProperty("PVarAreaRefs");
        m_PVarPathGraphRefs = serializedObject.FindProperty("PVarPathGraphRefs");
        m_PVarStrings = serializedObject.FindProperty("PVarStrings");

        m_PVarPropertiesContainer = new UnityHelper.PVarsPropertiesContainer()
        {
            PVars = m_PVars,
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
        EditorGUILayout.PropertyField(m_CameraType);
        UnityHelper.PVarsPropertyField(m_PVarPropertiesContainer, target as RatchetCamera, (target as RatchetCamera).RCVersion, cameraType: (target as RatchetCamera).CameraType);
        serializedObject.ApplyModifiedProperties();
    }
}
