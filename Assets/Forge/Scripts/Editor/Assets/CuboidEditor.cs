using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Cuboid))]
public class CuboidEditor : Editor
{
    SerializedProperty m_Type;
    SerializedProperty m_Subtype;

    private void OnEnable()
    {
        m_Type = serializedObject.FindProperty("Type");
        m_Subtype = serializedObject.FindProperty("Subtype");
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        EditorGUILayout.PropertyField(m_Type);
        if (m_Type.enumValueIndex == (int)CuboidType.None || m_Type.enumValueIndex == (int)CuboidType.Player)
            EditorGUILayout.PropertyField(m_Subtype);

        serializedObject.ApplyModifiedProperties();
    }
}
