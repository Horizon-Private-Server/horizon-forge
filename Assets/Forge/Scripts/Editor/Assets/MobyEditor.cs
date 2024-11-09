using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Moby)), CanEditMultipleObjects]
public class MobyEditor : Editor
{
    private byte[] _buffer = new byte[256];
    private List<(MaterialEditor matEditor, bool canEdit)> _materialEditors = new List<(MaterialEditor, bool)>();
    private HashSet<Material> _materialsWithEditors = new HashSet<Material>();
    private SerializedProperty _rcVersionProperty;
    private SerializedProperty _pvarData;
    private SerializedProperty _pvarValues;
    private SerializedProperty _pvarRefs;
    private SerializedProperty _pvarStrings;
    private UnityHelper.PVarsPropertiesContainer _pvarPropertiesContainer;
    private static Moby _clipboardMoby = null;

    private bool HasOneTarget => targets == null || targets.Length == 1;
    private bool TargetsShareOClass => targets?.All(x => (x as Moby).OClass == (target as Moby).OClass) ?? false;

    private void OnEnable()
    {
        var moby = (Moby)serializedObject.targetObject;

        _rcVersionProperty = serializedObject.FindProperty("RCVersion");
        _pvarData = serializedObject.FindProperty("PVars");
        _pvarValues = serializedObject.FindProperty("PVarValues");
        _pvarRefs = serializedObject.FindProperty("PVarReferences");
        _pvarStrings = serializedObject.FindProperty("PVarStrings");

        _pvarPropertiesContainer = new UnityHelper.PVarsPropertiesContainer()
        {
            PVars = _pvarData,
            PVarValues = _pvarValues,
            PVarRefs = _pvarRefs,
            Strings = _pvarStrings
        };

        _materialEditors.Clear();
        var materials = moby.GetComponentsInChildren<MeshRenderer>()?.SelectMany(x => x.sharedMaterials)?.ToArray();
        if (materials != null)
        {
            // Create an instance of the default MaterialEditor
            for (int i = 0; i < materials.Length; i++)
            {
                var mat = materials[i];
                if (!mat || mat.shader.name == "Horizon Forge/Collider") continue;
                if (_materialsWithEditors.Contains(mat)) continue;

                var matEditor = (MaterialEditor)CreateEditor(mat);
                var canEdit = AssetDatabase.GetAssetPath(mat).StartsWith("Assets");
                _materialEditors.Add((matEditor, canEdit));
                _materialsWithEditors.Add(mat);
            }
        }
    }

    public override void OnInspectorGUI()
    {
        var updateAsset = false;

        serializedObject.Update();

        // draw oclass
        if (TargetsShareOClass)
        {
            var moby = (Moby)target;
            EditorGUI.BeginDisabledGroup(true);
            EditorGUILayout.TextField("OClass", $"{moby?.OClass} ({moby?.OClass:X4})");
            EditorGUILayout.PropertyField(_rcVersionProperty);
            EditorGUI.EndDisabledGroup();
        }

        base.OnInspectorGUI();

        // detect out of bounds
        if (targets.Any(x => IsOutOfBounds((x as Moby).transform.position)))
        {
            EditorGUILayout.HelpBox("One or more mobys are out of bounds. Please ensure all mobys are within 0 and 1023 on each axis.", MessageType.Error);
        }

        // draw pvar overlay
        if (HasOneTarget)
        {
            var moby = target as Moby;
            var mapConfig = GameObject.FindObjectOfType<MapConfig>();
            if (mapConfig)
            {
                GUILayout.Space(20);
                UnityHelper.PVarsPropertyField(_pvarPropertiesContainer, target as Moby, moby.RCVersion, mobyClass: moby.OClass);

                if (_pvarData.isExpanded)
                {
                    // copy/paste
                    GUILayout.BeginHorizontal();
                    if (GUILayout.Button("Copy PVars"))
                    {
                        _clipboardMoby = moby;
                    }
                    EditorGUI.BeginDisabledGroup(!_clipboardMoby || _clipboardMoby.OClass != moby.OClass);
                    if (GUILayout.Button($"Paste PVars{(_clipboardMoby ? $" ({_clipboardMoby.name})" : "")}"))
                    {
                        // copy refs
                        Undo.RecordObject(moby, "Paste PVars");
                        moby.PVars = _clipboardMoby.PVars.ToArray();
                        moby.PVarReferences = new SerializableMonoBehaviourDictionary();
                        moby.PVarValues = new SerializableStringDictionary();
                        UnityHelper.InitializePVars(mapConfig, moby, useDefault: true);
                    }
                    EditorGUI.EndDisabledGroup();
                    if (GUILayout.Button("Reset PVars"))
                    {
                        Undo.RecordObject(moby, "Reset PVars");
                        moby.PVarReferences = new SerializableMonoBehaviourDictionary();
                        moby.PVarValues = new SerializableStringDictionary();
                        UnityHelper.InitializePVars(mapConfig, moby, useDefault: true);
                    }
                    if (GUILayout.Button(new GUIContent("Last Built", "Resets the PVars to the last built.")))
                    {
                        Undo.RecordObject(moby, "Reset Last Built PVars");
                        moby.PVarReferences = new SerializableMonoBehaviourDictionary();
                        moby.PVarValues = new SerializableStringDictionary();
                        UnityHelper.InitializePVars(mapConfig, moby, useDefault: false);
                    }
                    GUILayout.EndHorizontal();
                }
            }
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
            var asset = UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, (target as Moby).OClass.ToString(), (target as Moby).RCVersion);
            if (asset)
            {
                EditorGUIUtility.PingObject(asset);
            }
        }

        if (TargetsShareOClass)
        { 
            // selection
            EditorGUILayout.Space(20);
            if (GUILayout.Button("Select children"))
            {
                var selected = Selection.objects ?? new UnityEngine.Object[0];
                foreach (var targetMoby in targets) SelectChildren(targetMoby as Moby, ref selected);
                Selection.objects = selected;
            }
            if (GUILayout.Button("Select moby children"))
            {
                var selected = Selection.objects ?? new UnityEngine.Object[0];
                foreach (var targetMoby in targets) SelectMobyChildren(targetMoby as Moby, ref selected);
                Selection.objects = selected;
            }

            // draw materials
            foreach (var matEditor in _materialEditors)
            {
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
                if (obj is Moby moby)
                {
                    moby.UpdateAsset();
                    moby.UpdateMaterials();
                }
            }
        }
    }

    private void SelectChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarReferences != null)
        {
            foreach (var childMoby in moby.PVarReferences.Select(x => x.Value as Moby).Where(x => x))
            {
                if (!childMoby) continue;
                if (!selected.Contains(childMoby.gameObject))
                {
                    Array.Resize(ref selected, selected.Length + 1);
                    selected[selected.Length - 1] = childMoby.gameObject;
                    SelectChildren(childMoby, ref selected);
                }
            }
        }

        SelectCuboidChildren(moby, ref selected);
        SelectSplineChildren(moby, ref selected);
        SelectAreaChildren(moby, ref selected);
    }

    private void SelectMobyChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarReferences != null)
        {
            foreach (var childMoby in moby.PVarReferences.Select(x => x.Value as Moby).Where(x => x))
            {
                if (!childMoby) continue;
                if (!selected.Contains(childMoby.gameObject))
                {
                    Array.Resize(ref selected, selected.Length + 1);
                    selected[selected.Length - 1] = childMoby.gameObject;
                    SelectMobyChildren(childMoby, ref selected);
                }
            }
        }
    }

    private void SelectCuboidChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarReferences != null)
        {
            foreach (var childCuboid in moby.PVarReferences.Select(x => x.Value as Cuboid).Where(x => x))
            {
                if (!childCuboid) continue;
                if (!selected.Contains(childCuboid.gameObject))
                {
                    Array.Resize(ref selected, selected.Length + 1);
                    selected[selected.Length - 1] = childCuboid.gameObject;
                }
            }
        }
    }

    private void SelectSplineChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarReferences != null)
        {
            foreach (var childSpline in moby.PVarReferences.Select(x => x.Value as Spline).Where(x => x))
            {
                if (!childSpline) continue;
                if (!selected.Contains(childSpline.gameObject))
                {
                    Array.Resize(ref selected, selected.Length + 1);
                    selected[selected.Length - 1] = childSpline.gameObject;
                }
            }
        }
    }

    private void SelectAreaChildren(Moby moby, ref UnityEngine.Object[] selected)
    {
        if (moby.PVarReferences != null)
        {
            foreach (var childArea in moby.PVarReferences.Select(x => x.Value as Area).Where(x => x))
            {
                if (!childArea) continue;
                if (!selected.Contains(childArea.gameObject))
                {
                    Array.Resize(ref selected, selected.Length + 1);
                    selected[selected.Length - 1] = childArea.gameObject;
                }
            }
        }
    }

    private bool IsOutOfBounds(Vector3 position)
    {
        if (Mathf.Max(position.x, position.y, position.z) > 1023) return true;
        if (Mathf.Min(position.x, position.y, position.z) < 0) return true;

        return false;
    }
}
