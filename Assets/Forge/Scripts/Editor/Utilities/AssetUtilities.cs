using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class AssetUtilities
{
    [MenuItem("Forge/Utilities/Remap Materials for Selected Models")]
    public static void RemapMaterials()
    {
        foreach (var assetGuid in Selection.assetGUIDs)
        {
            var assetPath = AssetDatabase.GUIDToAssetPath(assetGuid);
            ModelImporter importer = (ModelImporter)ModelImporter.GetAtPath(assetPath);
            if (!importer) continue;
            importer.materialName = ModelImporterMaterialName.BasedOnModelNameAndMaterialName;
            importer.materialSearch = ModelImporterMaterialSearch.Local;
            importer.SearchAndRemapMaterials(ModelImporterMaterialName.BasedOnModelNameAndMaterialName, ModelImporterMaterialSearch.Local);
            importer.SaveAndReimport();
        }
    }

    [MenuItem("Forge/Utilities/Create Materials for Selected Textures")]
    public static void CreateUniversalMaterials()
    {
        var shader = Shader.Find("Horizon Forge/Universal");

        foreach (var assetGuid in Selection.assetGUIDs)
        {
            var assetPath = AssetDatabase.GUIDToAssetPath(assetGuid);
            var texture = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
            if (!texture) continue;

            var fi = new FileInfo(assetPath);
            var matDir = Path.Combine(fi.Directory.Parent.FullName, "Materials");
            if (!Directory.Exists(matDir)) Directory.CreateDirectory(matDir);

            var matAssetPath = UnityHelper.GetProjectRelativePath(Path.Combine(matDir, Path.GetFileNameWithoutExtension(fi.Name) + ".mat"));
            var mat = new Material(shader);
            mat.SetTexture("_MainTex", texture);
            AssetDatabase.CreateAsset(mat, matAssetPath);
        }
    }

    [MenuItem("Forge/Utilities/Snap Selected To Ground")]
    public static void SnapToGround()
    {
        Undo.RecordObjects(Selection.transforms, "Snap To Ground");
        foreach (var go in Selection.gameObjects)
        {
            if (Physics.Raycast(go.transform.position + Vector3.up, Vector3.down, out var hitInfo, 1000, -1))
            {
                go.transform.position = hitInfo.point;
            }
        }
    }

    [MenuItem("Forge/Utilities/Screenshot")]
    public static void Screenshot()
    {
        var scene = SceneManager.GetActiveScene();
        if (scene == null) return;
        var sceneView = SceneView.lastActiveSceneView;
        if (!sceneView) return;

        // hide hidden
        var gameObjectsTempDisabled = new List<GameObject>();
        var allGameObjects = GameObject.FindObjectsOfType<GameObject>();
        foreach (var go in allGameObjects)
        {
            if (go && go.activeInHierarchy && SceneVisibilityManager.instance.IsHidden(go))
            {
                go.SetActive(false);
                gameObjectsTempDisabled.Add(go);
            }
        }

        try
        {
            // render forward
            var sceneRt = sceneView.camera.targetTexture;
            var savePath = EditorUtility.SaveFilePanel("Save Screenshot", FolderNames.GetMapFolder(scene.name), scene.name, "png");
            if (!String.IsNullOrEmpty(savePath))
            {
                sceneView.camera.Render();
                UnityHelper.SaveRenderTexture(sceneRt, savePath);
                AssetDatabase.Refresh();
            }
        }
        catch (Exception ex)
        {
            UnityEngine.Debug.LogError(ex);
        }
        finally
        {
            foreach (var go in gameObjectsTempDisabled)
                go.SetActive(true);
        }
    }
}
