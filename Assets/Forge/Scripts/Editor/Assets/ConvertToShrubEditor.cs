using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(ConvertToShrub)), CanEditMultipleObjects]
public class ConvertToShrubEditor : Editor
{
    private MapConfig m_MapConfig;
    private SerializedProperty m_ShrubsProperty;
    private SerializedProperty m_MaterialsProperty;

    private void OnEnable()
    {
        m_MapConfig = FindObjectOfType<MapConfig>();
        if (m_MapConfig)
        {
            var db = m_MapConfig.GetConvertToShrubDatabase();
            if (db)
            {
                var so = new SerializedObject(db);
                m_ShrubsProperty = so.FindProperty("Shrubs");
                m_MaterialsProperty = m_ShrubsProperty.FindPropertyRelative("Materials");
            }
        }
    }

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        // render materials
        if (targets.Length == 1)
        {
            var shrub = target as ConvertToShrub;
            if (shrub.GetGeometry(out var parentGo))
            {
                for (int i = 0; i < m_ShrubsProperty.arraySize; ++i)
                {
                    var elem = m_ShrubsProperty.GetArrayElementAtIndex(i);
                    if (elem != null)
                    {
                        var parentProperty = elem.FindPropertyRelative("Parent");
                        if (parentProperty != null && parentProperty.objectReferenceValue == parentGo)
                        {
                            m_ShrubsProperty.serializedObject.Update();
                            var materialsProperty = elem.FindPropertyRelative("Materials");
                            EditorGUILayout.PropertyField(materialsProperty);
                            m_ShrubsProperty.serializedObject.ApplyModifiedProperties();
                            break;
                        }
                    }
                }
            }
        }

        var invalid = new List<ConvertToShrub>();
        foreach (var target in targets)
        {
            if (!(target as ConvertToShrub).Validate())
            {
                invalid.Add(target as ConvertToShrub);
            }
        }

        if (invalid.Any())
        {
            EditorGUILayout.HelpBox("One or more meshes are not readable. Please enable 'Read/Write' in the model import settings.", MessageType.Error);

            if (GUILayout.Button("Fix"))
            {
                var errorMsg = "";
                foreach (var shrub in invalid)
                {
                    var mfs = shrub.GetComponentsInChildren<MeshFilter>();
                    foreach (var mf in mfs)
                    {
                        if (mf.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy)) continue;
                        if (!mf.sharedMesh || mf.sharedMesh.isReadable) continue;

                        var assetPath = AssetDatabase.GetAssetPath(mf.sharedMesh);
                        if (!string.IsNullOrEmpty(assetPath))
                        {
                            ModelImporter importer = (ModelImporter)ModelImporter.GetAtPath(assetPath);
                            if (!importer)
                            {
                                errorMsg += $"Unable to find model for mesh {mf.sharedMesh.name} ({mf.gameObject.name})";
                                continue;
                            }

                            importer.isReadable = true;
                            importer.SaveAndReimport();
                        }
                    }
                }

                if (!string.IsNullOrEmpty(errorMsg))
                {
                    EditorUtility.DisplayDialog("Unable to fix mesh", errorMsg, "Ok");
                }
            }

        }

        if (!invalid.Any() && m_MapConfig && GUILayout.Button("Reimport"))
        {
            var db = m_MapConfig.GetConvertToShrubDatabase();
            if (db)
            {
                _ = db.ConvertMany(force: true, silent: false, targets.Select(x => x as ConvertToShrub).ToArray());
            }
        }
    }
}
