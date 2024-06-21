using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(InstancedMeshCollider)), CanEditMultipleObjects]
public class InstancedMeshColliderEditor : Editor
{
    private SerializedProperty m_MaterialIdProperty;
    private SerializedProperty m_NormalsProperty;
    private SerializedProperty m_RecalculateNormalsFactorProperty;
    private SerializedProperty m_RenderProperty;

    private void OnEnable()
    {
        m_MaterialIdProperty = serializedObject.FindProperty("m_MaterialId");
        m_NormalsProperty = serializedObject.FindProperty("m_Normals");
        m_RecalculateNormalsFactorProperty = serializedObject.FindProperty("m_RecalculateNormalsFactor");
        m_RenderProperty = serializedObject.FindProperty("m_Render");
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        // misc properties
        EditorGUILayout.PropertyField(m_MaterialIdProperty);
        EditorGUILayout.PropertyField(m_NormalsProperty);
        if (m_NormalsProperty.enumValueIndex == (int)CollisionRenderHandleNormalMode.RecalculateOutside
            || m_NormalsProperty.enumValueIndex == (int)CollisionRenderHandleNormalMode.RecalculateInside)
            EditorGUILayout.PropertyField(m_RecalculateNormalsFactorProperty);
        EditorGUILayout.PropertyField(m_RenderProperty);

        if (targets.Select(x => x as InstancedMeshCollider).Any(x => x.HasInstancedCollider() && !x.GetInstancedCollider()?.AssetInstance))
        {
            EditorGUILayout.HelpBox("One or more instances have no configured collider. No collision will be built for those instances.", MessageType.Warning);
        }

        var changed = serializedObject.hasModifiedProperties;
        serializedObject.ApplyModifiedProperties();

        // update asset on changes
        if (changed)
        {
            foreach (var target in targets)
                if (target is InstancedMeshCollider collider)
                    collider.UpdateAsset();
        }
    }
}
