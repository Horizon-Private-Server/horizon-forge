using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(UnityColliderToInstancedCollider)), CanEditMultipleObjects]
public class UnityColliderToInstancedColliderEditor : Editor
{
    private SerializedProperty m_ColliderProperty;
    private SerializedProperty m_MaterialIdProperty;
    private SerializedProperty m_NormalsProperty;
    private SerializedProperty m_RecalculateNormalsFactorProperty;
    private SerializedProperty m_ResolutionProperty;
    private SerializedProperty m_TfragSizeProperty;
    private SerializedProperty m_RenderProperty;
    private SerializedProperty m_TerrainLayerCollisionIdsProperty;

    private void OnEnable()
    {
        m_ColliderProperty = serializedObject.FindProperty("m_Collider");
        m_MaterialIdProperty = serializedObject.FindProperty("m_MaterialId");
        m_NormalsProperty = serializedObject.FindProperty("m_Normals");
        m_RecalculateNormalsFactorProperty = serializedObject.FindProperty("m_RecalculateNormalsFactor");
        m_ResolutionProperty = serializedObject.FindProperty("m_Resolution");
        m_TfragSizeProperty = serializedObject.FindProperty("m_TfragSize");
        m_RenderProperty = serializedObject.FindProperty("m_Render");
        m_TerrainLayerCollisionIdsProperty = serializedObject.FindProperty("m_TerrainLayerCollisionIds");
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        // disabled
        EditorGUI.BeginDisabledGroup(true);
        EditorGUILayout.PropertyField(m_ColliderProperty);
        EditorGUI.EndDisabledGroup();

        // collision id
        EditorGUILayout.PropertyField(m_MaterialIdProperty);

        // terrain
        if (targets.Length == 1 && (target as UnityColliderToInstancedCollider).GetComponent<Collider>() is TerrainCollider terrainCollider)
        {
            var count = m_TerrainLayerCollisionIdsProperty.arraySize;
            if (count > 0)
            {
                m_TerrainLayerCollisionIdsProperty.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(m_TerrainLayerCollisionIdsProperty.isExpanded, "Terrain Layer Collision Id Overrides");
                if (m_TerrainLayerCollisionIdsProperty.isExpanded)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        var elem = m_TerrainLayerCollisionIdsProperty.GetArrayElementAtIndex(i);
                        var layerProperty = elem.FindPropertyRelative("Layer");
                        var collisionIdProperty = elem.FindPropertyRelative("CollisionId");
                        var layer = layerProperty.objectReferenceValue as TerrainLayer;

                        EditorGUI.indentLevel++;
                        EditorGUILayout.PropertyField(collisionIdProperty, new GUIContent(layer ? layer.name : "NONE"));
                        EditorGUI.indentLevel--;
                    }
                }
                EditorGUILayout.EndFoldoutHeaderGroup();
            }
        }

        // misc properties
        EditorGUILayout.PropertyField(m_NormalsProperty);
        if (m_NormalsProperty.enumValueIndex == (int)CollisionRenderHandleNormalMode.RecalculateOutside
            || m_NormalsProperty.enumValueIndex == (int)CollisionRenderHandleNormalMode.RecalculateInside)
            EditorGUILayout.PropertyField(m_RecalculateNormalsFactorProperty);

        if (m_ColliderProperty.objectReferenceValue is SphereCollider)
            EditorGUILayout.PropertyField(m_ResolutionProperty);
        else if (m_ColliderProperty.objectReferenceValue is TerrainCollider)
            EditorGUILayout.PropertyField(m_TfragSizeProperty);

        EditorGUILayout.PropertyField(m_RenderProperty);

        if (targets.Select(x => x as UnityColliderToInstancedCollider).Any(x => x.HasInstancedCollider() && !x.GetInstancedCollider()?.AssetInstance))
        {
            EditorGUILayout.HelpBox("One or more instances have no configured collider. No collision will be built for those instances.", MessageType.Warning);
        }

        var changed = serializedObject.hasModifiedProperties;
        serializedObject.ApplyModifiedProperties();

        // refresh
        GUILayout.Space(20);
        if (GUILayout.Button("Refresh Collider"))
            changed = true;

        // update asset on changes
        if (changed)
        {
            foreach (var target in targets)
                if (target is UnityColliderToInstancedCollider collider)
                    collider.ForceRegenerateCollider();
        }
    }
}
