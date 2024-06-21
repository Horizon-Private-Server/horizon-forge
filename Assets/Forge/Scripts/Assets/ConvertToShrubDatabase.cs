using GLTFast.Export;
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

public class ConvertToShrubDatabase : ScriptableObject
{
    const int SHRUB_CLASS_START = 50000;

    public List<ConvertToShrubData> Shrubs = new List<ConvertToShrubData>();

    #region Accessors

    public ConvertToShrubData Get(ConvertToShrub shrub)
    {
        var prefab = PrefabUtility.GetCorrespondingObjectFromSource(shrub.gameObject);
        if (prefab)
            return Shrubs.FirstOrDefault(x => x.Parent == prefab);

        return Shrubs.FirstOrDefault(x => x.Parent == shrub.gameObject);
    }

    public ConvertToShrubData Create(ConvertToShrub shrub)
    {
        var parent = shrub.gameObject;
        var prefab = PrefabUtility.GetCorrespondingObjectFromSource(shrub.gameObject);
        if (prefab)
            parent = prefab;

        var data = new ConvertToShrubData()
        {
            Parent = parent,
        };

        Shrubs.Add(data);
        return data;
    }

    public ConvertToShrubData Get(GameObject prefab)
    {
        return Shrubs.FirstOrDefault(x => x.Parent == prefab);
    }

    #endregion

    public void Clean()
    {
        Shrubs.RemoveAll(x => !x.Parent);
    }

    private int GetFreeShrubClass()
    {
        var s = SHRUB_CLASS_START;
        var classes = Shrubs.Where(x => x.Parent).SelectMany(x => x.ShrubClasses);
        while (classes.Contains(s))
            ++s;

        return s;
    }

    #region Convert

    public async Task<bool> ConvertMany(bool force = false, bool silent = false, params ConvertToShrub[] shrubs)
    {
        if (shrubs == null) return false;

        var shrubsToConvert = new List<ConvertToShrub>();
        var shrubsToConvertDatas = new List<ConvertToShrubData>();
        foreach (var shrub in shrubs)
        {
            var data = Get(shrub);
            if (data != null && shrubsToConvertDatas.Contains(data))
                continue;

            if (data != null && !force)
            {
                var hash = ComputeHash(data.Parent);
                if (hash.Equals(data.Hash))
                    continue;
            }

            // reset
            if (data != null)
                data.ShrubClasses.Clear();

            shrubsToConvert.Add(shrub);
            shrubsToConvertDatas.Add(data);
        }

        var successful = true;
        foreach (var shrub in shrubsToConvert)
            if (await Convert(shrub, silent: silent) == null)
                successful = false;

        return successful;
    }

    public async Task<ConvertToShrubData> Convert(ConvertToShrub shrub, bool silent = false)
    {
        if (shrub == null) return null;
        if (!shrub.GetGeometry(out var modelGo)) return null;

        // get data
        var data = Get(shrub);
        if (data == null)
            data = Create(shrub);

        var assetImports = new List<PackerImporterWindow.PackerAssetImport>();
        var postImportActions = new List<Action>();

        try
        {
            if (!await Convert(shrub, data, modelGo.name, assetImports, postImportActions, silent: silent))
            {
                data.ShrubClasses.Clear();
                return null;
            }

            // import
            PackerImporterWindow.Import(assetImports, true);

            // update hash
            data.Hash = ComputeHash(data.Parent);

            // find all existing instance of updated shrubs and refresh asset
            var shrubs = FindObjectsOfType<Shrub>();
            foreach (var s in shrubs)
                if (data.ShrubClasses.Contains(s.OClass))
                    s.ResetAsset();

            //
            postImportActions.ForEach(x => x());
        }
        finally
        {
        }

        return data;
    }

    private async Task<bool> Convert(ConvertToShrub shrub, ConvertToShrubData data, string name, List<PackerImporterWindow.PackerAssetImport> assetImports, List<Action> postImportActions, bool silent = false)
    {
        var shrubsCreated = 0;
        var mapConfig = FindObjectOfType<MapConfig>();
        if (!mapConfig) return false;

        // build workspace paths
        var tempFolder = Path.Combine(FolderNames.GetTempFolder());
        var mapFolder = FolderNames.GetMapFolder(SceneManager.GetActiveScene().name);
        var localShrubDir = FolderNames.GetLocalAssetFolder(FolderNames.ShrubFolder);
        var glbModelExportedPath = Path.Combine(tempFolder, "export.glb");
        var glbModelAssetPath = Path.Combine(mapFolder, "temp.glb");
        var cancel = false;

        try
        {
            if (!silent && CancelProgressBar(ref cancel, "Shrub converter", $"Processing model {name}...", 0.25f)) return false;

            // export gameobject to glb
            if (!await ExportGameObjectAsGlb(data.Parent, glbModelExportedPath))
            {
                Debug.LogError("Unable to convert GameObject to .glb");
                return false;
            }

            // convert blend file to shrub
            if (!BlenderHelper.PrepareFileForShrubConvert(glbModelExportedPath, glbModelAssetPath, null))
            {
                Debug.LogError("Unable to preprocess shrub .glb");
                return false;
            }

            if (!silent && CancelProgressBar(ref cancel, "Shrub converter", $"Processing model {name}...", 0.5f)) return false;

            AssetDatabase.Refresh();

            if (!silent && CancelProgressBar(ref cancel, "Shrub converter", $"Processing model {name}...", 0.75f)) return false;

            // move glb file into working dir
            //File.Copy(glbModelAssetPath, shrubGlbPath, true);

            var shrubGo = AssetDatabase.LoadAssetAtPath<GameObject>(glbModelAssetPath);
            if (!shrubGo) return false;
            var renderers = shrubGo.GetComponentsInChildren<Renderer>();
            var meshNames = new List<string>();
            var textures = new List<Texture2D>();
            var materials = new List<Material>();
            foreach (var renderer in renderers)
            {
                // ignore any inside _collider name
                //var skip = false;
                //var parent = renderer.transform;
                //while (parent)
                //{
                //    if (parent.name.EndsWith("_collider"))
                //    {
                //        skip = true;
                //        break;
                //    }

                //    parent = parent.parent;
                //}

                //if (skip) continue;

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
            var maxTexPerShrub = 16;
            var shrubsToCreate = Mathf.CeilToInt(materials.Count / (float)maxTexPerShrub);

            // reset shrub classes
            data.ShrubClasses.Clear();
            data.Materials.RemoveAll(x => !materials.Any(m => m.name == x.Name));

            for (int i = 0; i < shrubsToCreate; ++i)
            {
                var outShrubClass = GetFreeShrubClass();
                var workingDir = Path.Combine(FolderNames.GetTempFolder(), $"shrub-converter-{outShrubClass}");
                var shrubGlbPath = Path.Combine(workingDir, "shrub.glb");
                //var shrubColliderGlbPath = Path.Combine(workingDir, "shrub_col.glb");
                var shrubWrenchAssetPath = Path.Combine(workingDir, "shrub.asset");

                var texOffset = i * maxTexPerShrub;
                var outputShrubDir = Path.Combine(localShrubDir, outShrubClass.ToString());
                var convertedShrubDest = Path.Combine(workingDir, "shrub.bin");
                var meshes = new List<string>();
                Action onImportActions = () => { };

                if (!silent && CancelProgressBar(ref cancel, "Shrub Converter", $"Processing shrub chunk {i} (class {outShrubClass})...", i / (float)shrubsToCreate)) return false;

                // clear output shrub dir
                if (Directory.Exists(workingDir)) Directory.Delete(workingDir, true);
                if (Directory.Exists(outputShrubDir)) Directory.Delete(outputShrubDir, true);
                Directory.CreateDirectory(outputShrubDir);
                Directory.CreateDirectory(workingDir);

                if (File.Exists(convertedShrubDest)) File.Delete(convertedShrubDest);

                // add to class list
                data.ShrubClasses.Add(outShrubClass);

                // export textures
                for (int t = 0; t < maxTexPerShrub; ++t)
                {
                    var tIdx = texOffset + t;
                    if (tIdx >= materials.Count) break;

                    // get mat data
                    var mat = materials[tIdx];
                    var matData = data.Materials?.FirstOrDefault(x => x.Name == mat.name);
                    if (matData == null)
                    {
                        matData = new ConvertToShrubMaterialData()
                        {
                            Name = mat.name,
                        };

                        data.Materials.Add(matData);
                    }

                    // try and export texture
                    var texPath = Path.Combine(workingDir, $"tex.{t:D4}.0.png");
                    var tex = mat.mainTexture as Texture2D; //mat.GetTexture("baseColorTexture") as Texture2D;
                    if (matData.TextureOverride) tex = matData.TextureOverride;
                    if (!tex) tex = new Texture2D(32, 32, TextureFormat.ARGB32, false);
                    var textAssetPath = AssetDatabase.GetAssetPath(tex);

                    var tint = matData.TintColor * mat.color;
                    if (matData.CorrectForAlphaBloom)
                        tint.a *= 0.5f;

                    ExportTexture(tex, texPath, tint);
                    textures.Add(tex);

                    // reconfigure texture size per tex
                    if (matData != null)
                    {
                        var importTexPath = Path.Combine(outputShrubDir, "Textures", $"{outShrubClass}-{t}.png");
                        onImportActions += () =>
                        {
                            WrenchHelper.SetDefaultWrenchModelTextureImportSettings(importTexPath, (int)Mathf.Pow(2, 5 + (int)matData.MaxTextureSize));
                        };
                    }
                }

                // build shrub asset
                using (var writer = new StringWriter())
                {
                    writer.WriteLine("ShrubClass shrub {");
                    writer.WriteLine($"\tid: {outShrubClass}");
                    writer.WriteLine("");

                    // mesh
                    writer.WriteLine("\tShrubClassCore core {");
                    writer.WriteLine($"\t\tmip_distance: {data.MipDistance}");
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
                    return false;
                }

                // export collider from '_collider'
                // or use shrub mesh if none
                //if (meshes.Any(m => renderers.Any(r => r.name == $"{m}_collider")))
                //{
                //    if (!BlenderHelper.PrepareFileForShrubConvert(glbModelAssetPath, shrubColliderGlbPath, String.Join(";", meshes.Select(x => $"{x}_collider"))))
                //    {
                //        Debug.LogError("Unable to convert .blend to .glb");
                //        return false;
                //    }
                //}
                //else
                //{
                //    File.Copy(shrubGlbPath, shrubColliderGlbPath, true);
                //}

                // convert to shrub
                if (!WrenchHelper.ConvertToShrub(workingDir, convertedShrubDest, "shrub"))
                {
                    Debug.LogError("Failed to convert glb to shrub.bin");
                    return false;
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
                        MaxTexSize = 512,
                        OnImport = onImportActions
                    });

                ++shrubsCreated;
            }
        }
        finally
        {
            if (!silent) EditorUtility.ClearProgressBar();

            // delete temp shrub asset
            AssetDatabase.DeleteAsset(glbModelAssetPath);
        }

        return true;
    }

    static async Task<bool> ExportGameObjectAsGlb(GameObject go, string outGlbFile)
    {
        var exportSettings = new ExportSettings
        {
            Format = GltfFormat.Binary,
            ImageDestination = ImageDestination.MainBuffer,
            FileConflictResolution = FileConflictResolution.Overwrite,
            ComponentMask = GLTFast.ComponentType.Mesh,
            //Deterministic = true,
        };

        var gameObjectExportSettings = new GameObjectExportSettings
        {
            OnlyActiveInHierarchy = true,
            DisabledComponents = true,
        };

        // copy renderers
        var rootCopyGo = new GameObject("copy");
        UnityHelper.CloneHierarchy(go.transform, rootCopyGo.transform, (srcT, dstT) =>
        {
            // skip inactive or hidden objects
            if (!srcT.gameObject.activeSelf || srcT.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy))
                return false;

            // copy renderers/meshfilters
            var renderer = srcT.GetComponent<Renderer>();
            var meshFilter = srcT.GetComponent<MeshFilter>();
            if (!renderer || !meshFilter || !meshFilter.sharedMesh) return true;
            if (meshFilter.sharedMesh.triangles.Length == 0) return true;

            var dRenderer = dstT.gameObject.AddComponent<MeshRenderer>();
            var dMeshFilter = dstT.gameObject.AddComponent<MeshFilter>();

            dRenderer.sharedMaterials = renderer.sharedMaterials;
            dMeshFilter.sharedMesh = meshFilter.sharedMesh;
            return true;
        });

        try
        {
            rootCopyGo.transform.position = Vector3.zero;
            rootCopyGo.transform.rotation = Quaternion.identity; // Quaternion.AngleAxis(-90, Vector3.right);
            rootCopyGo.transform.localScale = Vector3.one;
            rootCopyGo.hideFlags = HideFlags.DontSave | HideFlags.HideInHierarchy;

            var export = new GameObjectExport(exportSettings, gameObjectExportSettings);
            export.AddScene(new GameObject[] { rootCopyGo });

            // save glb file
            using (var fs = File.Create(outGlbFile))
            {
                if (!await export.SaveToStreamAndDispose(fs))
                    return false;
            }
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
            return false;
        }
        finally
        {
            if (rootCopyGo) GameObject.DestroyImmediate(rootCopyGo);

            EditorUtility.ClearProgressBar();
        }

        return true;
    }

    static bool CancelProgressBar(ref bool cancel, string title, string info, float progress)
    {
        EditorUtility.DisplayProgressBar(title, info, progress);
        return false;

        //cancel |= EditorUtility.DisplayCancelableProgressBar(title, info, progress);
        //System.Threading.Thread.Sleep(1);
        //return cancel;
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
                if (RenderTexture.active == rt)
                    RenderTexture.active = null;

                rt.Release();
            }
        }

        return false;
    }

    #endregion

    #region Hash

    Hash128 ComputeHash(GameObject parent)
    {
        Hash128 hash = new Hash128();

        var data = Get(parent);

        // hash is computed on textures, meshes and material data
        var renderers = parent.GetComponentsInChildren<Renderer>();
        foreach (var renderer in renderers)
        {
            if (!renderer.enabled || renderer.gameObject.hideFlags.HasFlag(HideFlags.HideInHierarchy))
                continue;

            var mf = renderer.GetComponent<MeshFilter>();
            var mesh = mf ? mf.sharedMesh : null;

            // include mesh hash
            if (mf.sharedMesh)
                hash.Append(mf.sharedMesh.ComputeHash());

            // include texture hash
            if (renderer.sharedMaterials != null)
            {
                foreach (var mat in renderer.sharedMaterials)
                {
                    if (mat.mainTexture)
                        hash.Append(mat.mainTexture.imageContentsHash.ToString());
                    hash.Append(mat.color.GetHashCode());
                }
            }

            // include metadata
            if (data != null)
            {
                hash.Append(data.MipDistance);
                if (data.Materials != null)
                {
                    foreach (var mat in data.Materials)
                    {
                        hash.Append($"{mat.Name}-{mat.CorrectForAlphaBloom}-{mat.TintColor}-{(int)mat.MaxTextureSize}");
                        if (mat.TextureOverride)
                            hash.Append(mat.TextureOverride.imageContentsHash.ToString());
                    }
                }
            }
        }

        return hash;
    }

    #endregion

}

[Serializable]
public class ConvertToShrubData
{
    public GameObject Parent;
    public Hash128 Hash;

    public int MipDistance = 64;

    public List<int> ShrubClasses = new List<int>();
    public List<ConvertToShrubMaterialData> Materials = new List<ConvertToShrubMaterialData>();
}

[Serializable]
public class ConvertToShrubMaterialData
{
    public enum TextureSize
    {
        _32,
        _64,
        _128,
        _256,
        _512,
        _1024
    }


    [SerializeField, HideInInspector] public string Name;
    public TextureSize MaxTextureSize = TextureSize._128;
    public Color TintColor = Color.white;
    public bool CorrectForAlphaBloom = true;
    public Texture2D TextureOverride;
}
