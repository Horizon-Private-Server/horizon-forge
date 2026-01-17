using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
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

    [MenuItem("Forge/Utilities/Fix Texture Clamping for Selected Objects")]
    public static void FixTextureClampSelectedObjects()
    {
        Dictionary<Texture2D, List<(Mesh, int) >> texMeshes = new Dictionary<Texture2D, List<(Mesh, int)>>();
        List<(Texture2D, bool, bool) > clampedTextures = new List<(Texture2D, bool, bool)>();

        // collect
        foreach (var go in Selection.gameObjects)
        {
            var meshRenderers = go.GetComponentsInChildren<MeshRenderer>(includeInactive: true);

            foreach (var mr in meshRenderers)
            {
                var mf = mr.GetComponent<MeshFilter>();
                if (mf)
                {
                    for (int submesh = 0; submesh < mf.sharedMesh.subMeshCount; ++submesh)
                    {
                        var m = submesh % mr.sharedMaterials.Length;
                        var mat = mr.sharedMaterials[m];
                        if (!mat || mat.shader.name != "Horizon Forge/Universal") continue;

                        var tex = mat.mainTexture as Texture2D;
                        if (tex)
                        {
                            if (!texMeshes.TryGetValue(tex, out var list))
                                texMeshes[tex] = list = new List<(Mesh, int)>();

                            var pair = (mf.sharedMesh, submesh);
                            if (!list.Any(x => x.Item1 == pair.Item1 && x.Item2 == pair.Item2))
                                list.Add(pair);
                        }
                    }
                }
            }
        }

        // check
        foreach (var item in texMeshes)
        {
            var tex = item.Key;
            var meshes = item.Value;
            var clampX = true;
            var clampY = true;

            foreach (var meshSubmesh in meshes)
            {
                var mesh = meshSubmesh.Item1;
                var idx = meshSubmesh.Item2;

                var submesh = mesh.GetSubMesh(idx);
                var indices = mesh.GetIndices(idx);

                for (int i = 0; i < indices.Length; ++i)
                {
                    var vIdx = submesh.baseVertex + indices[i];
                    var uv = mesh.uv[vIdx];

                    if (uv.x < 0 || uv.x > 1)
                        clampX = false;
                    if (uv.y < 0 || uv.y > 1)
                        clampY = false;

                    if (!clampX && !clampY) break;
                }
            }

            //if (clampX || clampY)
            {
                //Debug.Log($"CLAMP X:{clampX} Y:{clampY} {tex.name}");
                clampedTextures.Add((tex, clampX, clampY));
            }
        }

        if (clampedTextures.Any())
        {
            Undo.RecordObjects(clampedTextures.Select(x => x.Item1).ToArray(), "Clamp Textures");
            foreach (var texClamp in clampedTextures)
            {
                var tex = texClamp.Item1;
                var clampX = texClamp.Item2;
                var clampY = texClamp.Item3;

                tex.wrapModeU = clampX ? TextureWrapMode.Clamp : TextureWrapMode.Repeat;
                tex.wrapModeV = clampY ? TextureWrapMode.Clamp : TextureWrapMode.Repeat;

                EditorUtility.SetDirty(tex);

                var assetPath = AssetDatabase.GetAssetPath(tex);
                if (!string.IsNullOrEmpty(assetPath))
                {
                    var assetImporter = AssetImporter.GetAtPath(assetPath);
                    if (assetImporter is TextureImporter textureImporter)
                    {
                        textureImporter.wrapModeU = clampX ? TextureWrapMode.Clamp : TextureWrapMode.Repeat;
                        textureImporter.wrapModeV = clampY ? TextureWrapMode.Clamp : TextureWrapMode.Repeat;
                    }
                }
            }
            Undo.FlushUndoRecordObjects();
        }
    }

    [MenuItem("Forge/Utilities/Delete Unused Occlusion Data")]
    public static void DeleteUnusedOcclusionData()
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig)
        {
            EditorUtility.DisplayDialog("Missing scene", "Please open a map scene first.", "Okay");
            return;
        }

        var db = mapConfig.GetOcclusionDatabase();
        if (!db) return;

        try
        {
            AssetDatabase.StartAssetEditing();

            var occlusionFolder = Path.Combine(FolderNames.GetMapFolder(EditorSceneManager.GetActiveScene().name), FolderNames.OcclusionFolder);
            var files = Directory.GetFiles(occlusionFolder, "*.asset");
            foreach (var file in files)
            {
                var metaFile = file + ".meta";
                var key = Path.GetFileNameWithoutExtension(file);
                if (!db.Occlusion.ContainsKey(key))
                {
                    File.Delete(file);
                    if (File.Exists(metaFile)) File.Delete(metaFile);
                }
            }
        }
        finally
        {
            AssetDatabase.StopAssetEditing();
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

    [MenuItem("Forge/Utilities/Align Selected With Ground")]
    public static void AlignWithGround()
    {
        Undo.RecordObjects(Selection.transforms, "Align With Ground");
        foreach (var go in Selection.gameObjects)
        {
            if (Physics.Raycast(go.transform.position + Vector3.up, Vector3.down, out var hitInfo, 1000, -1))
            {
                go.transform.rotation = Quaternion.LookRotation(Vector3.Cross(go.transform.right, hitInfo.normal), hitInfo.normal);
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
