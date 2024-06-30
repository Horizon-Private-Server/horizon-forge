using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Remoting.Messaging;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Tie)), CanEditMultipleObjects]
public class TieEditor : Editor
{
    // We need to use and to call an instnace of the default MaterialEditor
    private List<(MaterialEditor matEditor, bool canEdit)> _materialEditors = new List<(MaterialEditor, bool)>();
    private SerializedProperty m_ColorProperty;
    private SerializedProperty m_ColorDataProperty;
    private SerializedProperty m_ReflectionProperty;
    private SerializedProperty m_GroupIdProperty;
    private SerializedProperty m_InstancedColliderProperty;
    private SerializedProperty m_RenderInstancedColliderProperty;
    private SerializedProperty m_InstancedColliderIdOverridesProperty;
    private SerializedProperty m_InstancedColliderOverrideProperty;

    private MapConfig m_MapConfig;
    private SerializedProperty m_TiesProperty;

    private static Tie _clipboardTie = null;

    private bool HasOneTarget => targets == null || targets.Length == 1;
    private bool TargetsShareOClass => targets?.All(x => (x as Tie).OClass == (target as Tie).OClass) ?? false;

    private void OnEnable()
    {
        var tie = (Tie)serializedObject.targetObject;

        m_ColorProperty = serializedObject.FindProperty("ColorDataValue");
        m_ColorDataProperty = serializedObject.FindProperty("ColorData");
        m_ReflectionProperty = serializedObject.FindProperty("Reflection");
        m_GroupIdProperty = serializedObject.FindProperty("GroupId");
        m_InstancedColliderProperty = serializedObject.FindProperty("InstancedCollider");
        m_RenderInstancedColliderProperty = serializedObject.FindProperty("RenderInstancedCollider");
        m_InstancedColliderIdOverridesProperty = serializedObject.FindProperty("InstancedColliderIdOverrides");
        m_InstancedColliderOverrideProperty = serializedObject.FindProperty("InstancedColliderOverride");

        if (HasOneTarget)
        {
            _materialEditors.Clear();
            var materials = tie.GetComponentsInChildren<MeshRenderer>()?.SelectMany(x => x.sharedMaterials)?.ToArray();
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

        m_MapConfig = FindObjectOfType<MapConfig>();
        if (m_MapConfig)
        {
            var db = m_MapConfig.GetTieDatabase();
            if (db)
            {
                var so = new SerializedObject(db);
                m_TiesProperty = so.FindProperty("Ties");
            }
        }
    }

    public override void OnInspectorGUI()
    {
        var updateAsset = false;
        var octantCount = targets.Sum(x => (x as Tie)?.Octants?.Length ?? 0);

        if (HasOneTarget)
        {
            var tie = (Tie)target;
            EditorGUI.BeginDisabledGroup(true);
            EditorGUILayout.TextField("OClass", $"{tie?.OClass} ({tie?.OClass:X4})");
            EditorGUILayout.TextField("Occlusion Id", tie?.OcclusionId.ToString());
            EditorGUI.EndDisabledGroup();

            // init db
            var db = m_MapConfig.GetTieDatabase();
            if (db.Get(tie.OClass) == null)
                db.Create(tie.OClass);

            // expose tie asset properties
            if (m_TiesProperty != null)
            {
                for (int i = 0; i < m_TiesProperty.arraySize; ++i)
                {
                    var elem = m_TiesProperty.GetArrayElementAtIndex(i);
                    if (elem != null)
                    {
                        var oclassProperty = elem.FindPropertyRelative("OClass");
                        if (oclassProperty != null && oclassProperty.intValue == tie.OClass)
                        {
                            var mipProperty = elem.FindPropertyRelative("MipDistanceMultiplier");
                            var lodProperty = elem.FindPropertyRelative("LODDistanceMultiplier");

                            m_TiesProperty.serializedObject.Update();
                            EditorGUILayout.PropertyField(elem, new GUIContent("Tie Properties"));
                            m_TiesProperty.serializedObject.ApplyModifiedProperties();
                            break;
                        }
                    }
                }
            }
        }

        //base.OnInspectorGUI();
        serializedObject.Update();

        // misc properties
        EditorGUILayout.PropertyField(m_GroupIdProperty);

        // reflection
        UnityHelper.Matrix4x4PropertyField(m_ReflectionProperty);

        // collision
        EditorGUILayout.PropertyField(m_InstancedColliderProperty);
        if (m_InstancedColliderProperty.boolValue)
        {
            // ensure reflection is empty if instanced collider is active
            if (targets.Select(x => x as Tie).Any(x => x.InstancedCollider && !x.Reflection.isIdentity))
            {
                EditorGUILayout.HelpBox("One or more instances have non-empty Reflection matrices. Please clear the Reflection matrix to use instanced collision.", MessageType.Error);
            }
            else
            {
                // additional collision params
                EditorGUILayout.PropertyField(m_RenderInstancedColliderProperty);
                EditorGUILayout.PropertyField(m_InstancedColliderOverrideProperty);

                if (HasOneTarget && target is Tie tie)
                {
                    // draw collision id overrides if there isn't a model override already
                    if (!m_InstancedColliderOverrideProperty.objectReferenceValue)
                    {
                        DrawCollisionIdOverrides(tie);
                    }
                }
            }

            if (targets.Select(x => x as Tie).Any(x => x.HasInstancedCollider() && !x.GetInstancedCollider()?.AssetInstance))
            {
                EditorGUILayout.HelpBox("One or more instances have no configured collider. No collision will be built for those instances.", MessageType.Warning);
            }
        }

        // color
        GUILayout.Space(20);
        GUILayout.Label("Vertex Color", EditorStyles.boldLabel);
        EditorGUILayout.PropertyField(m_ColorProperty, new GUIContent("Base Vertex Color"));
        if (GUILayout.Button("Apply Uniform Color"))
        {
            Undo.RecordObjects(targets, "Apply Instance Color");
            foreach (var target in targets)
            {
                if (target is Tie tie)
                {
                    tie.ValidateColors();
                    tie.ColorData = TieAsset.GenerateUniformColor(tie.ColorData.Length, tie.ColorDataValue * 0.5f).ToArray();
                    tie.UpdateMaterials();
                }
            }
            Undo.FlushUndoRecordObjects();
        }

        // inform user that selected tie(s) have non uniform vertex colors
        if (targets.Any(x => (x as Tie).HasNonUniformVertexColors()))
        {
            EditorGUILayout.HelpBox("One or more selected ties have non uniform vertex colors. These colors will render on PS2, but won't appear correctly in the editor.", MessageType.Info);
        }

        // occlusion
        GUILayout.Space(20);
        GUILayout.Label($"Occlusion ({octantCount} Total Octants)", EditorStyles.boldLabel);
        Tie.RenderOctants = GUILayout.Toggle(Tie.RenderOctants, "Render Octants");
        GUILayout.Space(20);

        // set to all octants
        if (GUILayout.Button($"Set To All Octants"))
        {
            var octants = UnityHelper.GetAllOctants();

            Undo.RecordObjects(targets, "Set To All Octants");
            foreach (var obj in targets)
            {
                if (obj is Tie tie)
                {
                    tie.Octants = octants.ToArray();
                }
            }
            Undo.FlushUndoRecordObjects();
        }

        // clear octants
        if (GUILayout.Button($"Clear Octants"))
        {
            Undo.RecordObjects(targets, "Clear Octants");
            foreach (var obj in targets)
            {
                if (obj is Tie tie)
                {
                    tie.Octants = new Vector3[0];
                }
            }
            Undo.FlushUndoRecordObjects();
        }

        updateAsset = serializedObject.hasModifiedProperties;
        serializedObject.ApplyModifiedProperties();

        // refresh asset
        GUILayout.Space(20);
        if (GUILayout.Button("Refresh Asset"))
        {
            updateAsset = true;
        }

        // view in project window
        if (HasOneTarget && GUILayout.Button("Select in Project Window"))
        {
            var asset = UnityHelper.GetAssetPrefab(FolderNames.TieFolder, (target as Tie).OClass.ToString());
            if (asset)
            {
                EditorGUIUtility.PingObject(asset);
            }
        }

        GUILayout.Space(20);

        // render materials if only one selected
        if (HasOneTarget)
        {
            // draw materials
            foreach (var matEditor in _materialEditors)
            {
                if (!matEditor.matEditor) continue;

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

        // update asset
        if (updateAsset)
        {
            foreach (var obj in targets)
            {
                if (obj is Tie tie)
                {
                    tie.UpdateAsset();
                }
            }
        }
    }

    private void DrawCollisionIdOverrides(Tie tie)
    {
        var changed = false;
        ValidateCollisionOverrideMaterials(tie);

        // editors
        m_InstancedColliderIdOverridesProperty.isExpanded = EditorGUILayout.BeginFoldoutHeaderGroup(m_InstancedColliderIdOverridesProperty.isExpanded, "Collider ID Overrides");
        if (m_InstancedColliderIdOverridesProperty.isExpanded)
        {
            var colIdOverrides = tie.InstancedColliderIdOverrides;
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
                _clipboardTie = tie;
            }
            EditorGUI.BeginDisabledGroup(!_clipboardTie || _clipboardTie.OClass != tie.OClass);
            if (GUILayout.Button($"Paste Overrides{(_clipboardTie ? $" ({_clipboardTie.name})" : "")}"))
            {
                // copy refs
                Undo.RecordObject(tie, "Paste Collision Id Overrides");
                tie.InstancedColliderIdOverrides = _clipboardTie.InstancedColliderIdOverrides.Select(x => new ColliderIdOverride(x)).ToArray();
                changed = true;
            }
            EditorGUI.EndDisabledGroup();
            GUILayout.EndHorizontal();

            if (changed)
            {
                tie.UpdateAsset();
                Undo.FlushUndoRecordObjects();
            }
        }
        EditorGUILayout.EndFoldoutHeaderGroup();
    }

    private void ValidateCollisionOverrideMaterials(Tie tie)
    {
        var colIdOverrides = tie.InstancedColliderIdOverrides;
        var validColIdOverrides = new List<ColliderIdOverride>();

        // init list
        if (tie.InstancedColliderIdOverrides == null)
            tie.InstancedColliderIdOverrides = new ColliderIdOverride[0];

        // validate materials
        var materials = tie.GetComponentsInChildren<MeshRenderer>()?.SelectMany(x => x.sharedMaterials)?.ToArray();
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
                tie.InstancedColliderIdOverrides = validColIdOverrides.ToArray();
            }
        }
        else
        {
            tie.InstancedColliderIdOverrides = new ColliderIdOverride[0];
        }
    }
}
