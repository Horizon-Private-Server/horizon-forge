using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(CustomShrub))]
public class CustomShrubEditor : Editor
{

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        var canConvert = true;
        var customShrub = target as CustomShrub;
        if (!customShrub) return;

        // path
        var modelAssetPath = AssetDatabase.GetAssetPath(customShrub.modelToConvert);
        if (!String.IsNullOrEmpty(modelAssetPath) && !modelAssetPath.EndsWith(".blend"))
        {
            canConvert = false;
            EditorGUILayout.HelpBox("The model must be a .blend file", MessageType.Error);
        }

        // validate max materials per shrub
        if (customShrub.maxMaterialsPerShrub > 16) customShrub.maxMaterialsPerShrub = 16;
        if (customShrub.maxMaterialsPerShrub < 1) customShrub.maxMaterialsPerShrub = 1;

        // validate materials
        if (customShrub.modelToConvert)
        {
            var materials = customShrub.modelToConvert.GetComponentsInChildren<Renderer>().SelectMany(x => x.sharedMaterials).Distinct().ToArray();
            foreach (var material in materials)
            {
                var mat = customShrub.materials.FirstOrDefault(x => x.name == material.name);
                if (mat.name == null)
                {
                    mat = new CustomShrub.CustomShrubMaterialData();
                    mat.name = material.name;
                    mat.correctForAlphaBloom = customShrub.globalCorrectForAlphaBloom;
                    mat.tintColor = customShrub.globalTintColor;
                    mat.maxTextureSize = customShrub.globalMaxTextureSize;
                    customShrub.materials.Add(mat);
                }
            }
        }

        if (!String.IsNullOrEmpty(modelAssetPath) && canConvert)
        {
            EditorGUILayout.Space(20);
            if (GUILayout.Button("Import"))
            {
                if (customShrub.lastComputedShrubCount <= 0)
                {
                    customShrub.lastComputedShrubCount = Count(customShrub);
                
                    // nothing to import
                    if (customShrub.lastComputedShrubCount <= 0)
                        return;
                }

                if (EditorUtility.DisplayDialog("Custom Shrub Import", $"This import will overwrite any shrubs in the range of [{customShrub.startingShrubClass},{customShrub.startingShrubClass+customShrub.lastComputedShrubCount-1}]", "Continue", "Cancel"))
                {
                    customShrub.lastComputedShrubCount = OnConvert(customShrub);
                }
            }

            EditorGUILayout.Space(20);
            EditorGUILayout.HelpBox($"This CustomShrub will use {customShrub.lastComputedShrubCount} shrubs", MessageType.Info);
            if (GUILayout.Button("Compute # of Shrubs"))
            {
                customShrub.lastComputedShrubCount = Count(customShrub);
            }
        }
    }



    public static int OnConvert(CustomShrub customShrub)
    {
        var modelGo = customShrub.modelToConvert;
        var blenderModelAssetPath = AssetDatabase.GetAssetPath(modelGo);
        if (String.IsNullOrEmpty(blenderModelAssetPath)) return 0;
        var glbModelAssetPath = Path.Combine(Path.GetDirectoryName(blenderModelAssetPath), "split.glb");
        var assetImports = new List<PackerImporterWindow.PackerAssetImport>();
        var postImportActions = new List<Action>();
        var cancel = false;
        var sClass = customShrub.startingShrubClass;

        try
        {
            if (customShrub.shrubPerRootLevelObject && modelGo.transform.childCount > 0)
            {
                var childCount = modelGo.transform.childCount;
                for (int i = 0; i < childCount; ++i)
                {
                    var child = modelGo.transform.GetChild(i);
                    if (!child) continue;

                    if (CancelProgressBar(ref cancel, "Shrub Converter", $"Processing shrub root object {child.name}...", i / (float)childCount)) break;

                    // export child
                    if (!BlenderHelper.PrepareFileForShrubConvert(blenderModelAssetPath, glbModelAssetPath, $"{child.name}"))
                    {
                        Debug.LogError("Unable to convert .blend to .glb");
                        break;
                    }
                    AssetDatabase.Refresh();

                    // convert
                    var count = Convert(glbModelAssetPath, child.name, sClass, customShrub, null, assetImports, postImportActions);
                    if (count < 0) break;

                    sClass += count;
                }
            }
            else
            {
                var count = Convert(blenderModelAssetPath, modelGo.name, customShrub.startingShrubClass, customShrub, null, assetImports, postImportActions);
                if (count > 0)
                    sClass += count;
            }

            // import
            PackerImporterWindow.Import(assetImports, true);

            // find all existing instance of updated shrubs and refresh asset
            var shrubs = FindObjectsOfType<Shrub>();
            foreach (var shrub in shrubs)
                if (shrub.OClass >= customShrub.startingShrubClass && shrub.OClass <= sClass)
                    shrub.ResetAsset();

            //
            postImportActions.ForEach(x => x());
        }
        finally
        {
            // delete temp shrub asset
            if (File.Exists(glbModelAssetPath))
                AssetDatabase.DeleteAsset(glbModelAssetPath);
        }

        return sClass - customShrub.startingShrubClass;
    }

    public static int Count(CustomShrub customShrub)
    {
        var modelGo = customShrub.modelToConvert;
        var blenderModelAssetPath = AssetDatabase.GetAssetPath(modelGo);
        if (String.IsNullOrEmpty(blenderModelAssetPath)) return 0;
        var glbModelAssetPath = Path.Combine(Path.GetDirectoryName(blenderModelAssetPath), "split.glb");
        var assetImports = new List<PackerImporterWindow.PackerAssetImport>();
        var postImportActions = new List<Action>();
        var cancel = false;
        var sClass = customShrub.startingShrubClass;

        try
        {
            if (customShrub.shrubPerRootLevelObject && modelGo.transform.childCount > 0)
            {
                var childCount = modelGo.transform.childCount;
                for (int i = 0; i < childCount; ++i)
                {
                    var child = modelGo.transform.GetChild(i);
                    if (!child) continue;

                    if (CancelProgressBar(ref cancel, "Shrub Converter", $"Processing shrub root object {child.name}...", i / (float)childCount)) break;

                    // export child
                    if (!BlenderHelper.PrepareFileForShrubConvert(blenderModelAssetPath, glbModelAssetPath, child.name))
                    {
                        Debug.LogError("Unable to convert .blend to .glb");
                        break;
                    }
                    AssetDatabase.Refresh();

                    // convert
                    var count = Convert(glbModelAssetPath, child.name, sClass, customShrub, null, assetImports, postImportActions, count: true);
                    if (count < 0) break;

                    sClass += count;
                }
            }
            else
            {
                var count = Convert(blenderModelAssetPath, modelGo.name, customShrub.startingShrubClass, customShrub, null, assetImports, postImportActions, count: true);
                if (count > 0)
                    sClass += count;
            }
        }
        finally
        {
            // delete temp shrub asset
            if (File.Exists(glbModelAssetPath))
                AssetDatabase.DeleteAsset(glbModelAssetPath);
        }

        return sClass - customShrub.startingShrubClass;
    }

    private static int Convert(string modelPath, string name, int startingShrubClass, CustomShrub customShrub, Transform rootTransform, List<PackerImporterWindow.PackerAssetImport> assetImports, List<Action> postImportActions, bool count = false)
    {
        var shrubsCreated = 0;
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return 0;

        // build workspace paths
        var localShrubDir = FolderNames.GetLocalAssetFolder(FolderNames.ShrubFolder);
        if (String.IsNullOrEmpty(modelPath)) return 0;
        var glbModelAssetPath = Path.Combine(Path.GetDirectoryName(modelPath), "temp.glb");
        var cancel = false;
        var texSize = (int)Mathf.Pow(2, 5 + (int)customShrub.globalMaxTextureSize);

        try
        {
            if (CancelProgressBar(ref cancel, "Shrub converter", $"Processing model {name}...", 0.25f)) return 0;

            // convert blend file to shrub
            if (!BlenderHelper.PrepareFileForShrubConvert(modelPath, glbModelAssetPath, null))
            {
                Debug.LogError("Unable to convert .blend to .glb");
                return 0;
            }

            if (CancelProgressBar(ref cancel, "Shrub converter", $"Processing model {name}...", 0.5f)) return 0;

            AssetDatabase.Refresh();

            if (CancelProgressBar(ref cancel, "Shrub converter", $"Processing model {name}...", 0.75f)) return 0;

            // move glb file into working dir
            //File.Copy(glbModelAssetPath, shrubGlbPath, true);

            var shrubGo = AssetDatabase.LoadAssetAtPath<GameObject>(glbModelAssetPath);
            if (!shrubGo) return 0;
            var renderers = shrubGo.GetComponentsInChildren<Renderer>();
            var meshNames = new List<string>();
            var textures = new List<Texture2D>();
            var materials = new List<Material>();
            foreach (var renderer in renderers)
            {
                // ignore any inside _collider name
                var skip = false;
                var parent = renderer.transform;
                while (parent)
                {
                    if (parent.name.EndsWith("_collider"))
                    {
                        skip = true;
                        break;
                    }

                    parent = parent.parent;
                }

                if (skip) continue;

                foreach (var material in renderer.sharedMaterials)
                {
                    var mf = renderer.GetComponent<MeshFilter>();
                    string mesh = null;
                    if (mf) mesh = mf.sharedMesh.name;

                    materials.Add(material);
                    meshNames.Add(mesh);
                }
            }

            // determine how many shrubs we need to create
            var maxTexPerShrub = customShrub.maxMaterialsPerShrub;
            var shrubsToCreate = Mathf.CeilToInt(materials.Count / (float)maxTexPerShrub);

            // if count, return # of shrubs to create
            if (count) return shrubsToCreate;

            for (int i = 0; i < shrubsToCreate; ++i)
            {
                var workingDir = Path.Combine(FolderNames.GetTempFolder(), $"shrub-converter-{startingShrubClass + i}");
                var shrubGlbPath = Path.Combine(workingDir, "shrub.glb");
                var shrubColliderGlbPath = Path.Combine(workingDir, "shrub_col.glb");
                var shrubWrenchAssetPath = Path.Combine(workingDir, "shrub.asset");

                var texOffset = i * maxTexPerShrub;
                var outShrubClass = startingShrubClass + i;
                var outputShrubDir = Path.Combine(localShrubDir, outShrubClass.ToString());
                var convertedShrubDest = Path.Combine(workingDir, "shrub.bin");
                var meshes = new List<string>();

                if (CancelProgressBar(ref cancel, "Shrub Converter", $"Processing shrub chunk {i} (class {outShrubClass})...", i / (float)shrubsToCreate)) return -1;

                // clear output shrub dir
                if (Directory.Exists(workingDir)) Directory.Delete(workingDir, true);
                if (Directory.Exists(outputShrubDir)) Directory.Delete(outputShrubDir, true);
                Directory.CreateDirectory(outputShrubDir);
                Directory.CreateDirectory(workingDir);

                if (File.Exists(convertedShrubDest)) File.Delete(convertedShrubDest);

                // export textures
                for (int t = 0; t < maxTexPerShrub; ++t)
                {
                    var tIdx = texOffset + t;
                    if (tIdx >= materials.Count) break;

                    // try and export texture
                    var mat = materials[tIdx];
                    var texPath = Path.Combine(workingDir, $"tex.{t:D4}.0.png");
                    var tex = mat.GetTexture("baseColorTexture") as Texture2D;
                    if (!tex) tex = new Texture2D(32, 32, TextureFormat.ARGB32, false);
                    var textAssetPath = AssetDatabase.GetAssetPath(tex);
                    var tint = customShrub.globalTintColor;
                    var matData = customShrub.materials?.FirstOrDefault(x => x.name == mat.name);
                    if (matData.HasValue && matData.Value.useMaterialOverride)
                        tint = matData.Value.tintColor;

                    if ((matData.HasValue && matData.Value.useMaterialOverride && matData.Value.correctForAlphaBloom) || ((!matData.HasValue || !matData.Value.useMaterialOverride) && customShrub.globalCorrectForAlphaBloom))
                        tint.a *= 0.5f;

                    ExportTexture(tex, texPath, tint);
                    textures.Add(tex);
                }

                // build shrub asset
                using (var writer = new StringWriter())
                {
                    writer.WriteLine("ShrubClass shrub {");
                    writer.WriteLine($"\tid: {outShrubClass}");
                    writer.WriteLine("");

                    // mesh
                    writer.WriteLine("\tShrubClassCore core {");
                    writer.WriteLine($"\t\tmip_distance: {customShrub.mipDistance}");
                    writer.WriteLine("");
                    writer.WriteLine("\t\tMesh mesh {");
                    writer.WriteLine("\t\t\tsrc: \"shrub.glb\"");
                    writer.WriteLine("\t\t\tname: \"shrub\"");
                    writer.WriteLine("\t\t}");
                    //for (int m = 0; m < maxTexPerShrub; ++m)
                    //{
                    //    var mIdx = texOffset + m;
                    //    if (mIdx >= meshNames.Count) break;
                    //    if (meshes.Contains(meshNames[mIdx])) continue;

                    //    writer.WriteLine("");
                    //    writer.WriteLine("\t\tMesh mesh {");
                    //    writer.WriteLine($"\t\t\tsrc: \"shrub.glb\"");
                    //    writer.WriteLine($"\t\t\tname: \"{meshNames[mIdx]}\"");
                    //    writer.WriteLine("\t\t}");

                    //    meshes.Add(meshNames[mIdx]);
                    //}
                    writer.WriteLine("\t}");
                    writer.WriteLine("");

                    // materials
                    writer.WriteLine("\tmaterials {");
                    for (int m = 0; m < maxTexPerShrub; ++m)
                    {
                        var mIdx = texOffset + m;
                        if (mIdx >= materials.Count) break;

                        writer.WriteLine("");
                        writer.WriteLine($"\t\tMaterial {m} {{");
                        writer.WriteLine($"\t\t\tname: \"{materials[mIdx].name}\"");
                        writer.WriteLine("\t\t\tTexture diffuse {");
                        writer.WriteLine($"\t\t\t\tsrc: \"tex.{m:D4}.0.png\"");
                        writer.WriteLine("\t\t\t}");
                        writer.WriteLine("\t\t}");
                    }
                    writer.WriteLine("\t}");
                    writer.WriteLine("}");
                    writer.WriteLine("");

                    // reference
                    writer.WriteLine($"Reference dl.shrub_classes.{outShrubClass} {{");
                    writer.WriteLine("\tasset: \"shrub\"");
                    writer.WriteLine("}");

                    File.WriteAllText(shrubWrenchAssetPath, writer.ToString());
                }

                // build list of meshes to use
                for (int m = 0; m < maxTexPerShrub; ++m)
                {
                    var mIdx = texOffset + m;
                    if (mIdx >= meshNames.Count) break;
                    if (meshes.Contains(meshNames[mIdx])) continue;
                    meshes.Add(meshNames[mIdx]);
                }

                // export selected meshes to glb
                if (File.Exists(shrubGlbPath)) File.Delete(shrubGlbPath);
                if (!BlenderHelper.PrepareFileForShrubConvert(glbModelAssetPath, shrubGlbPath, String.Join(";", meshes)))
                {
                    Debug.LogError("Unable to convert .blend to .glb");
                    return -1;
                }

                // export collider from '_collider'
                // or use shrub mesh if none
                if (meshes.Any(m => renderers.Any(r => r.name == $"{m}_collider")))
                {
                    if (!BlenderHelper.PrepareFileForShrubConvert(glbModelAssetPath, shrubColliderGlbPath, String.Join(";", meshes.Select(x => $"{x}_collider"))))
                    {
                        Debug.LogError("Unable to convert .blend to .glb");
                        return -1;
                    }
                }
                else
                {
                    File.Copy(shrubGlbPath, shrubColliderGlbPath, true);
                }

                // convert to shrub
                if (!WrenchHelper.ConvertToShrub(workingDir, convertedShrubDest, "shrub"))
                {
                    Debug.LogError("Failed to convert glb to shrub.bin");
                    return -1;
                }

                // convert packed shrub to asset
                assetImports.Add(
                    new PackerImporterWindow.PackerAssetImport()
                    {
                        AssetFolder = workingDir,
                        DestinationFolder = outputShrubDir,
                        AssetType = FolderNames.ShrubFolder,
                        Name = outShrubClass.ToString(),
                        PrependModelNameToTextures = true,
                        MaxTexSize = texSize,
                    });

                if (customShrub.generateColliders)
                {
                    postImportActions.Add(() =>
                    {
                        var colFile = Path.Combine(outputShrubDir, $"{outShrubClass}_col.fbx");
                        var defaultMatId = "col_2f";

                        if (int.TryParse(customShrub.generateCollidersDefaultMaterialId, System.Globalization.NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var id))
                            defaultMatId = $"col_{id:x}";

                        if (BlenderHelper.PrepareMeshFileForCollider(shrubColliderGlbPath, colFile, defaultMatId))
                        {
                            AssetDatabase.ImportAsset(colFile);
                            AssetDatabase.Refresh();
                            WrenchHelper.SetDefaultWrenchModelImportSettings(colFile, "Collider", false);
                        }
                    });
                }

                ++shrubsCreated;
            }

            postImportActions.Add(() =>
            {
                if (customShrub.createInstanceAfterImport)
                {
                    // create instances
                    if (shrubsToCreate == 1)
                    {
                        var go = new GameObject(name);
                        var shrub = go.AddComponent<Shrub>();
                        shrub.OClass = startingShrubClass;
                        shrub.RenderDistance = customShrub.instanceRenderDistance;
                        go.transform.localRotation = Quaternion.Euler(0, 0, -90);

                        if (rootTransform)
                            go.transform.SetParent(rootTransform, false);
                    }
                    else
                    {
                        var rootGo = new GameObject(name);
                        for (int i = 0; i < shrubsToCreate; ++i)
                        {
                            var sClass = startingShrubClass + i;
                            var go = new GameObject(sClass.ToString());
                            var shrub = go.AddComponent<Shrub>();
                            shrub.OClass = sClass;
                            shrub.RenderDistance = customShrub.instanceRenderDistance;
                            go.transform.SetParent(rootGo.transform, false);
                            go.transform.localRotation = Quaternion.Euler(0, 0, -90);
                        }

                        if (rootTransform)
                            rootGo.transform.SetParent(rootTransform, false);
                    }

                }
            });
        }
        finally
        {
            EditorUtility.ClearProgressBar();

            // delete temp shrub asset
            AssetDatabase.DeleteAsset(glbModelAssetPath);
        }

        return shrubsCreated;
    }

    static bool CancelProgressBar(ref bool cancel, string title, string info, float progress)
    {
        cancel |= EditorUtility.DisplayCancelableProgressBar(title, info, progress);
        System.Threading.Thread.Sleep(1);
        return cancel;
    }

    static bool ExportTexture(Texture2D tex, string texPath, Color tint)
    {
        if (tex)
        {
            if (tex.isReadable)
            {
                var bytes = tex.EncodeToPNG();
                File.WriteAllBytes(texPath, bytes);
                return true;
            }

            var width = tex.width;
            var height = tex.height;
            var rt = new RenderTexture(width, height, 0, RenderTextureFormat.ARGB32);
            rt.Create();
            try
            {
                var mat = new Material(AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(FolderNames.ForgeFolder, "Shaders", "TintBlit.mat")));
                mat.SetColor("_Color", tint);
                mat.SetTexture("_In", tex);
                mat.SetTexture("_Out", rt);
                Graphics.Blit(tex, rt, mat);

                var oldRt = RenderTexture.active;
                RenderTexture.active = rt;
                var tex2 = new Texture2D(width, height, TextureFormat.ARGB32, false);
                tex2.ReadPixels(new Rect(0, 0, width, height), 0, 0);
                tex2.Apply();
                RenderTexture.active = oldRt;

                var bytes = tex2.EncodeToPNG();
                File.WriteAllBytes(texPath, bytes);
                return true;
            }
            finally
            {
                rt.Release();
            }
        }

        return false;
    }
}
