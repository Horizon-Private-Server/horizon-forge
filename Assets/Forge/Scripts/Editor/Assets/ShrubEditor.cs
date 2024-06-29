using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(Shrub)), CanEditMultipleObjects]
public class ShrubEditor : Editor
{
    // We need to use and to call an instnace of the default MaterialEditor
    private List<(MaterialEditor matEditor, bool canEdit)> _materialEditors = new List<(MaterialEditor, bool)>();
    private SerializedProperty m_OClassProperty;
    private SerializedProperty m_ReflectionProperty;
    private SerializedProperty m_GroupIdProperty;
    private SerializedProperty m_RenderDistanceProperty;
    private SerializedProperty m_TintProperty;
    private SerializedProperty m_InstancedColliderProperty;
    private SerializedProperty m_RenderInstancedColliderProperty;
    private SerializedProperty m_InstancedColliderIdOverridesProperty;
    private SerializedProperty m_InstancedColliderOverrideProperty;

    private static Shrub _clipboardShrub = null;

    private bool HasOneTarget => targets == null || targets.Length == 1;
    private bool TargetsShareOClass => targets?.All(x => (x as Shrub).OClass == (target as Shrub).OClass) ?? false;

    private void OnEnable()
    {
        m_OClassProperty = serializedObject.FindProperty("OClass");
        m_ReflectionProperty = serializedObject.FindProperty("Reflection");
        m_GroupIdProperty = serializedObject.FindProperty("GroupId");
        m_RenderDistanceProperty = serializedObject.FindProperty("RenderDistance");
        m_TintProperty = serializedObject.FindProperty("Tint");
        m_InstancedColliderProperty = serializedObject.FindProperty("InstancedCollider");
        m_RenderInstancedColliderProperty = serializedObject.FindProperty("RenderInstancedCollider");
        m_InstancedColliderIdOverridesProperty = serializedObject.FindProperty("InstancedColliderIdOverrides");
        m_InstancedColliderOverrideProperty = serializedObject.FindProperty("InstancedColliderOverride");
        GetMaterialEditors();
    }

    public override void OnInspectorGUI()
    {
        if (HasOneTarget)
        {
            serializedObject.Update();
            EditorGUI.BeginDisabledGroup(true);
            EditorGUILayout.TextField("OClass", $"{m_OClassProperty?.intValue} ({m_OClassProperty?.intValue:X4})");
            EditorGUI.EndDisabledGroup();
            serializedObject.ApplyModifiedProperties();
        }

        serializedObject.Update();

        // misc properties
        EditorGUILayout.PropertyField(m_GroupIdProperty);
        EditorGUILayout.PropertyField(m_RenderDistanceProperty);
        EditorGUILayout.PropertyField(m_TintProperty);

        // reflection
        UnityHelper.Matrix4x4PropertyField(m_ReflectionProperty);

        // collision
        EditorGUILayout.PropertyField(m_InstancedColliderProperty);
        if (m_InstancedColliderProperty.boolValue)
        {
            // ensure reflection is empty if instanced collider is active
            if (targets.Select(x => x as Shrub).Any(x => x.InstancedCollider && !x.Reflection.isIdentity))
            {
                EditorGUILayout.HelpBox("One or more instances have non-empty Reflection matrices. Please clear the Reflection matrix to use instanced collision.", MessageType.Error);
            }
            else
            {
                // additional collision params
                EditorGUILayout.PropertyField(m_RenderInstancedColliderProperty);
                EditorGUILayout.PropertyField(m_InstancedColliderOverrideProperty);

                if (HasOneTarget && target is Shrub shrub)
                {
                    // draw collision id overrides if there isn't a model override already
                    if (!m_InstancedColliderOverrideProperty.objectReferenceValue)
                    {
                        DrawCollisionIdOverrides(shrub);
                    }
                }
            }

            if (targets.Select(x => x as Shrub).Any(x => x.HasInstancedCollider() && !x.GetInstancedCollider()?.AssetInstance))
            {
                EditorGUILayout.HelpBox("One or more instances have no configured collider. No collision will be built for those instances.", MessageType.Warning);
            }
        }

        var changed = serializedObject.hasModifiedProperties;
        serializedObject.ApplyModifiedProperties();

        // update asset on changes
        if (changed)
        {
            foreach (var target in targets)
            {
                if (target is Shrub shrub)
                {
                    shrub.UpdateAsset();
                    shrub.UpdateMaterials();
                }
            }
        }

        // materials
        if (HasOneTarget)
        {
            GUILayout.Space(20);

            // draw materials
            foreach (var matEditor in _materialEditors)
            {
                if (!matEditor.matEditor)
                {
                    GetMaterialEditors();
                    break;
                }

                // Draw the material's foldout and the material shader field
                // Required to call _materialEditor.OnInspectorGUI ();
                matEditor.matEditor.DrawHeader();

                using (new EditorGUI.DisabledGroupScope(!matEditor.canEdit))
                {
                    // Draw the material properties
                    // Works only if the foldout of _materialEditor.DrawHeader () is open
                    matEditor.matEditor.OnInspectorGUI();
                }
            }
        }
    }

    private void GetMaterialEditors()
    {
        if (HasOneTarget)
        {
            var shrub = (Shrub)serializedObject.targetObject;
            _materialEditors.Clear();
            var materials = shrub.GetComponentsInChildren<MeshRenderer>()?.SelectMany(x => x.sharedMaterials)?.ToArray();
            if (materials != null)
            {
                // Create an instance of the default MaterialEditor
                for (int i = 0; i < materials.Length; i++)
                {
                    var mat = materials[i];
                    if (!mat || mat.shader.name == "Horizon Forge/Collider") continue;

                    var matEditor = (MaterialEditor)CreateEditor(mat);
                    var canEdit = AssetDatabase.GetAssetPath(mat).StartsWith("Assets");
                    _materialEditors.Add((matEditor, canEdit));
                }
            }
        }
    }

    private void DrawCollisionIdOverrides(Shrub shrub)
    {
        var changed = false;
        ValidateCollisionOverrideMaterials(shrub);

        // editors
        m_InstancedColliderIdOverridesProperty.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(m_InstancedColliderIdOverridesProperty.isExpanded, "Collider ID Overrides");
        if (m_InstancedColliderIdOverridesProperty.isExpanded)
        {
            var colIdOverrides = shrub.InstancedColliderIdOverrides;
            foreach (var colIdOverride in colIdOverrides)
            {
                var newColId = EditorGUILayout.TextField(colIdOverride.MaterialName, colIdOverride.OverrideId);
                if (newColId != colIdOverride.OverrideId)
                {
                    if (!changed) Undo.RecordObjects(targets, "Update Collision Id Overrides");
                    colIdOverride.OverrideId = newColId;
                    changed = true;
                }
            }

            // copy/paste
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Copy Overrides"))
            {
                _clipboardShrub = shrub;
            }
            EditorGUI.BeginDisabledGroup(!_clipboardShrub || _clipboardShrub.OClass != shrub.OClass);
            if (GUILayout.Button($"Paste Overrides{(_clipboardShrub ? $" ({_clipboardShrub.name})" : "")}"))
            {
                // copy refs
                Undo.RecordObject(shrub, "Paste Collision Id Overrides");
                shrub.InstancedColliderIdOverrides = _clipboardShrub.InstancedColliderIdOverrides.Select(x => new ColliderIdOverride(x)).ToArray();
                changed = true;
            }
            EditorGUI.EndDisabledGroup();
            GUILayout.EndHorizontal();

            if (changed)
            {
                shrub.UpdateAsset();
                Undo.FlushUndoRecordObjects();
            }
        }
        EditorGUILayout.EndFoldoutHeaderGroup();
    }

    private void ValidateCollisionOverrideMaterials(Shrub shrub)
    {
        var colIdOverrides = shrub.InstancedColliderIdOverrides;
        var validColIdOverrides = new List<ColliderIdOverride>();

        // init list
        if (shrub.InstancedColliderIdOverrides == null)
            shrub.InstancedColliderIdOverrides = new ColliderIdOverride[0];

        // validate materials
        var materials = shrub.GetComponentsInChildren<MeshRenderer>()?.SelectMany(x => x.sharedMaterials)?.ToArray();
        if (materials != null)
        {
            var changed = false;
            for (int i = 0; i < materials.Length; i++)
            {
                var mat = materials[i];
                if (!mat || mat.shader.name != "Horizon Forge/Collider") continue;

                var existingOverride = colIdOverrides.FirstOrDefault(x => x.MaterialName == mat.name && !validColIdOverrides.Contains(x));
                if (existingOverride != null)
                {
                    validColIdOverrides.Add(existingOverride);
                }
                else
                {
                    changed = true;
                    validColIdOverrides.Add(new ColliderIdOverride()
                    {
                        MaterialName = mat.name,
                        OverrideId = mat.GetInteger("_ColId").ToString("x")
                    });
                }
            }

            if (changed)
            {
                shrub.InstancedColliderIdOverrides = validColIdOverrides.ToArray();
            }
        }
        else
        {
            shrub.InstancedColliderIdOverrides = new ColliderIdOverride[0];
        }
    }
}
