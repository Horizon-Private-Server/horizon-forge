using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(ConvertToShrub)), CanEditMultipleObjects]
public class ConvertToShrubEditor : Editor
{
    private TextureSize m_BulkTextureSize = TextureSize._128;
    private MapConfig m_MapConfig;
    private SerializedProperty m_ShrubsProperty;
    private SerializedProperty m_MaterialsProperty;
    private GameObject m_LastMergedParent;
    private int m_LastMergedMatchCount;

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
        if (targets.Length == 1 && m_ShrubsProperty != null)
        {
            var shrub = target as ConvertToShrub;
            if (shrub.GetGeometry(out var parentGo))
            {
                var matchedMaterials = new List<SerializedProperty>();

                m_ShrubsProperty.serializedObject.Update();
                for (int i = 0; i < m_ShrubsProperty.arraySize; ++i)
                {
                    var elem = m_ShrubsProperty.GetArrayElementAtIndex(i);
                    if (elem != null)
                    {
                        var parentProperty = elem.FindPropertyRelative("Parent");
                        if (parentProperty != null && parentProperty.objectReferenceValue && (parentProperty.objectReferenceValue == parentGo || PrefabUtility.GetOriginalSourceRootWhereGameObjectIsAdded(parentProperty.objectReferenceValue as GameObject) == parentGo))
                        {
                            var materialsProperty = elem.FindPropertyRelative("Materials");
                            if (materialsProperty != null)
                                matchedMaterials.Add(materialsProperty);
                        }
                    }
                }

                if (matchedMaterials.Count > 0)
                {
                    var primaryMaterials = matchedMaterials[0];
                    var needsInitialMerge = m_LastMergedParent != parentGo || m_LastMergedMatchCount != matchedMaterials.Count;

                    if (needsInitialMerge)
                    {
                        for (int i = 1; i < matchedMaterials.Count; ++i)
                            MergeMaterialsByName(primaryMaterials, matchedMaterials[i]);

                        for (int i = 1; i < matchedMaterials.Count; ++i)
                            CopyMaterials(primaryMaterials, matchedMaterials[i]);
                    }

                    EditorGUI.BeginChangeCheck();
                    EditorGUILayout.PropertyField(primaryMaterials, includeChildren: true);
                    var changed = EditorGUI.EndChangeCheck();

                    if (changed)
                    {
                        for (int i = 1; i < matchedMaterials.Count; ++i)
                            CopyMaterials(primaryMaterials, matchedMaterials[i]);
                    }

                    if (needsInitialMerge || changed)
                        m_ShrubsProperty.serializedObject.ApplyModifiedProperties();

                    m_LastMergedParent = parentGo;
                    m_LastMergedMatchCount = matchedMaterials.Count;
                }
                else
                {
                    m_LastMergedParent = null;
                    m_LastMergedMatchCount = 0;
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

        if (!invalid.Any() && m_MapConfig)
        {
            var selectedParents = GetSelectedParents();

            GUILayout.Space(20);

            // bulk remove alpha
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Bulk Alpha: Toggle Off"))
            {
                var db = m_MapConfig.GetConvertToShrubDatabase();
                if (db)
                {
                    foreach (var shrub in db.Shrubs)
                    {
                        if (!selectedParents.Any(selectedParent => IsParentMatch(shrub.Parent, selectedParent)))
                            continue;

                        foreach (var material in shrub.Materials)
                            material.RemoveAlpha = true;
                    }
                }
            }
            if (GUILayout.Button("Bulk Alpha: Toggle On"))
            {
                var db = m_MapConfig.GetConvertToShrubDatabase();
                if (db)
                {
                    foreach (var shrub in db.Shrubs)
                    {
                        if (!selectedParents.Any(selectedParent => IsParentMatch(shrub.Parent, selectedParent)))
                            continue;

                        foreach (var material in shrub.Materials)
                            material.RemoveAlpha = false;
                    }
                }
            }
            GUILayout.EndHorizontal();

            // bulk set texture size
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Bulk Set Texture Size"))
            {
                var db = m_MapConfig.GetConvertToShrubDatabase();
                if (db)
                {
                    foreach (var shrub in db.Shrubs)
                    {
                        if (!selectedParents.Any(selectedParent => IsParentMatch(shrub.Parent, selectedParent)))
                            continue;

                        foreach (var material in shrub.Materials)
                            material.MaxTextureSize = m_BulkTextureSize;
                    }
                }
            }
            m_BulkTextureSize = (TextureSize)EditorGUILayout.EnumPopup(m_BulkTextureSize);
            GUILayout.EndHorizontal();

            // reimport
            GUILayout.Space(20);
            if (GUILayout.Button("Reimport"))
            {
                var db = m_MapConfig.GetConvertToShrubDatabase();
                if (db)
                {
                    _ = db.ConvertMany(force: true, silent: false, targets.Select(x => x as ConvertToShrub).ToArray());
                }
            }
        }

        //var terrain = (target as ConvertToShrub).GetComponent<Terrain>();
        //if (!invalid.Any() && m_MapConfig && terrain && GUILayout.Button("Test Terrain"))
        //{
        //    var mesh = terrain.transform.Find("terrain_mesh");
        //    if (!mesh)
        //    {
        //        mesh = new GameObject("terrain_mesh").transform;
        //        mesh.SetParent(terrain.transform, false);
        //    }

        //    terrain.ToMesh(mesh.gameObject);
        //}
    }

    private static void MergeMaterialsByName(SerializedProperty destination, SerializedProperty source)
    {
        if (destination == null || source == null)
            return;

        var existingNames = new HashSet<string>();
        for (int i = 0; i < destination.arraySize; ++i)
        {
            var material = destination.GetArrayElementAtIndex(i);
            var materialName = material.FindPropertyRelative("Name")?.stringValue;
            if (!string.IsNullOrEmpty(materialName))
                existingNames.Add(materialName);
        }

        for (int i = 0; i < source.arraySize; ++i)
        {
            var srcMaterial = source.GetArrayElementAtIndex(i);
            var srcName = srcMaterial.FindPropertyRelative("Name")?.stringValue;

            if (!string.IsNullOrEmpty(srcName) && existingNames.Contains(srcName))
                continue;

            destination.InsertArrayElementAtIndex(destination.arraySize);
            var dstMaterial = destination.GetArrayElementAtIndex(destination.arraySize - 1);
            CopyMaterial(srcMaterial, dstMaterial);

            if (!string.IsNullOrEmpty(srcName))
                existingNames.Add(srcName);
        }
    }

    private static void CopyMaterials(SerializedProperty source, SerializedProperty destination)
    {
        if (source == null || destination == null)
            return;

        destination.arraySize = source.arraySize;
        for (int i = 0; i < source.arraySize; ++i)
            CopyMaterial(source.GetArrayElementAtIndex(i), destination.GetArrayElementAtIndex(i));
    }

    private static void CopyMaterial(SerializedProperty source, SerializedProperty destination)
    {
        if (source == null || destination == null)
            return;

        destination.FindPropertyRelative("Name").stringValue = source.FindPropertyRelative("Name").stringValue;
        destination.FindPropertyRelative("MaxTextureSize").enumValueIndex = source.FindPropertyRelative("MaxTextureSize").enumValueIndex;
        destination.FindPropertyRelative("TintColor").colorValue = source.FindPropertyRelative("TintColor").colorValue;
        destination.FindPropertyRelative("CorrectForAlphaBloom").boolValue = source.FindPropertyRelative("CorrectForAlphaBloom").boolValue;
        destination.FindPropertyRelative("RemoveAlpha").boolValue = source.FindPropertyRelative("RemoveAlpha").boolValue;
        destination.FindPropertyRelative("TextureOverride").objectReferenceValue = source.FindPropertyRelative("TextureOverride").objectReferenceValue;
    }

    private HashSet<GameObject> GetSelectedParents()
    {
        var selectedParents = new HashSet<GameObject>();
        foreach (var targetObject in targets)
        {
            var shrub = targetObject as ConvertToShrub;
            if (shrub != null && shrub.GetGeometry(out var parentGo) && parentGo)
                selectedParents.Add(parentGo);
        }

        return selectedParents;
    }

    private static bool IsParentMatch(GameObject shrubParent, GameObject selectedParent)
    {
        if (!shrubParent || !selectedParent)
            return false;

        return shrubParent == selectedParent || PrefabUtility.GetOriginalSourceRootWhereGameObjectIsAdded(shrubParent) == selectedParent;
    }
}
