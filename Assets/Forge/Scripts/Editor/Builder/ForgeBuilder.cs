using GLTFast.Export;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEditor.SearchService;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class ForgeBuilder
{
    [MenuItem("Forge/Builder/Rebuild")]
    public static async void Rebuild()
    {
        if (await RebuildLevel(EditorSceneManager.GetActiveScene()))
            CopyToBuildFolders(EditorSceneManager.GetActiveScene());
    }

    [MenuItem("Forge/Builder/Rebuild and Patch")]
    public static async void RebuildAndPatch()
    {
        if (await RebuildLevel(EditorSceneManager.GetActiveScene()))
        {
            CopyToBuildFolders(EditorSceneManager.GetActiveScene());
            Patch();
        }
    }

    [MenuItem("Forge/Builder/Patch")]
    public static void Patch()
    {
        PatchLevel(EditorSceneManager.GetActiveScene());
    }

    [MenuItem("Forge/Builder/Build DZO Files")]
    public static void CommandBuildDZOFiles()
    {
        BuildDZOFiles(EditorSceneManager.GetActiveScene());
    }

    static bool RebuildLevelProgress(ref bool cancel, string info, float progress)
    {
        cancel |= EditorUtility.DisplayCancelableProgressBar($"Rebuilding Level", info, progress);
        System.Threading.Thread.Sleep(1);
        return cancel;
    }

    static void PatchLevel(UnityEngine.SceneManagement.Scene scene)
    {
        if (scene == null) return;

        // validate level folder
        var binFolder = FolderNames.GetMapBinFolder(scene.name, Constants.GameVersion);
        if (!Directory.Exists(binFolder))
        {
            EditorUtility.DisplayDialog("Cannot patch", $"Scene does not have matching level folder \"{scene.name}\"", "Ok");
            return;
        }

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig)
        {
            EditorUtility.DisplayDialog("Cannot patch", $"Scene does not have a map config", "Ok");
            return;
        }

        var settings = ForgeSettings.Load();
        if (settings == null)
        {
            EditorUtility.DisplayDialog("Cannot patch", $"Missing ForgeSettings", "Ok");
            return;
        }

        var isoPath = settings.PathToOutputDeadlockedIso;
        var cleanIsoPath = settings.PathToCleanDeadlockedIso;

        if (String.IsNullOrEmpty(isoPath))
        {
            EditorUtility.DisplayDialog("Cannot patch", $"Missing ForgeSettings PathToModdedIso", "Ok");
            return;
        }

        if (String.IsNullOrEmpty(cleanIsoPath))
        {
            EditorUtility.DisplayDialog("Cannot patch", $"Missing ForgeSettings PathToCleanIso", "Ok");
            return;
        }

        // ensure modded iso exists
        if (!File.Exists(isoPath))
        {
            // clean iso also doesn't exist, exit
            if (!File.Exists(cleanIsoPath))
            {
                EditorUtility.DisplayDialog("Cannot patch", $"Clean iso \"{cleanIsoPath}\" does not exist.", "Okay");
                return;
            }

            // clean iso exists, give option to create modded iso by copy
            if (!EditorUtility.DisplayDialog("Cannot patch", $"Output iso \"{isoPath}\" does not exist.\n\nWould you like to create it?", "Create", "Cancel"))
                return;

            try
            {
                // copy
                EditorUtility.DisplayProgressBar("Copying iso", $"{cleanIsoPath} => {isoPath}...\n\nThis may take awhile...", 0.5f);
                File.Copy(cleanIsoPath, isoPath, true);
            }
            finally
            {
                EditorUtility.ClearProgressBar();
            }
        }

        if (PackerHelper.Patch(binFolder, isoPath, cleanIsoPath, (int)mapConfig.DLBaseMap) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
            Debug.LogError($"Unable to patch {isoPath}. Please make sure PCSX2 is paused/closed.");
        else
            Debug.Log($"{isoPath} patched!");

        // patch minimap
        var mapPath = Path.Combine(FolderNames.GetMapBuildFolder(scene.name, Constants.GameVersion), $"{mapConfig.MapFilename}.map");
        if (File.Exists(mapPath))
            PackerHelper.PatchMinimap(isoPath, cleanIsoPath, mapPath, (int)mapConfig.DLBaseMap, Constants.GameVersion);

        // patch transition
        var bgPath = Path.Combine(FolderNames.GetMapBuildFolder(scene.name, Constants.GameVersion), $"{mapConfig.MapFilename}.bg");
        if (File.Exists(bgPath))
            PackerHelper.PatchTransitionBackground(isoPath, cleanIsoPath, bgPath, (int)mapConfig.DLBaseMap, Constants.GameVersion);
    }

    static async Task<bool> RebuildLevel(UnityEngine.SceneManagement.Scene scene)
    {
        if (scene == null) return false;

        var resourcesFolder = FolderNames.GetMapFolder(scene.name);
        var binFolder = FolderNames.GetMapBinFolder(scene.name, Constants.GameVersion);
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();

        // validate resources folder
        if (!Directory.Exists(resourcesFolder))
        {
            EditorUtility.DisplayDialog("Cannot build", $"Scene does not have matching resources folder \"{scene.name}\"", "Ok");
            return false;
        }

        // validate level folder
        if (!Directory.Exists(binFolder))
        {
            EditorUtility.DisplayDialog("Cannot build", $"Scene does not have matching level folder \"{scene.name}\"", "Ok");
            return false;
        }

        if (!scene.isLoaded || !mapConfig)
            return false;

        try
        {
            var ctx = new RebuildContext()
            {
                MapSceneName = scene.name
            };

            //RebuildSky(ref ctx.Cancel, resourcesFolder, binFolder); if (cancel) return;
            //await RebuildCollision(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            //return false;

            await RebuildCollision(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildTfrags(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildTies(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildTieInstances(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            await RebuildShrubs(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildShrubInstances(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildMobys(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildMobyInstances(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildCuboids(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildSplines(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildAreas(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildCode(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;
            RebuildPostProcess(ctx, resourcesFolder, binFolder); if (ctx.Cancel) return false;

            EditorUtility.ClearProgressBar();
            EditorUtility.DisplayProgressBar($"Rebuilding Level", "Packing", 0);

            var result = PackerHelper.Pack(binFolder, (int)mapConfig.DLBaseMap, Constants.GameVersion, PackerHelper.PACKER_PACK_OPS.PACK_WORLD_INSTANCES
                                                                                    | PackerHelper.PACKER_PACK_OPS.PACK_OCCLUSION
                                                                                    | PackerHelper.PACKER_PACK_OPS.PACK_GAMEPLAY
                                                                                    | PackerHelper.PACKER_PACK_OPS.PACK_CODE
                                                                                    | PackerHelper.PACKER_PACK_OPS.PACK_ASSETS
                                                                                    | PackerHelper.PACKER_PACK_OPS.PACK_LEVEL_WAD
                                                                                    | PackerHelper.PACKER_PACK_OPS.PACK_SOUND_WAD
                                                                                    , (p) => EditorUtility.DisplayProgressBar("Rebuilding Level", "Packing", p));

            if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
            {
                Debug.LogError($"Pack returned {result}");
                return false;
            }

            RebuildMapFiles(ctx, resourcesFolder, binFolder);
            Debug.Log("Rebuild complete");
        }
        catch (Exception ex)
        {
            Debug.LogError(ex);
            return false;
        }
        finally
        {
            EditorUtility.ClearProgressBar();
        }

        return true;
    }

    static void CopyToBuildFolders(UnityEngine.SceneManagement.Scene scene)
    {
        var binFolder = FolderNames.GetMapBinFolder(scene.name, Constants.GameVersion);
        var mapBuildFolder = FolderNames.GetMapBuildFolder(scene.name, Constants.GameVersion);
        if (!Directory.Exists(mapBuildFolder)) return;

        var settings = ForgeSettings.Load();
        if (settings == null)
        {
            EditorUtility.DisplayDialog("Cannot copy build files", $"Missing ForgeSettings", "Ok");
            return;
        }

        if (settings.DLBuildFolders == null) return;

        foreach (var buildFolder in settings.DLBuildFolders)
        {
            if (Directory.Exists(buildFolder))
            {
                IOHelper.CopyDirectory(mapBuildFolder, buildFolder);
            }
        }
    }

    static void RebuildMapFiles(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var buildPath = FolderNames.GetMapBuildFolder(ctx.MapSceneName, Constants.GameVersion);
        var wadPath = Path.Combine(binFolder, FolderNames.GetLevelWadFilename((int)mapConfig.DLBaseMap));
        var soundPath = Path.Combine(binFolder, FolderNames.GetSoundWadFilename((int)mapConfig.DLBaseMap));
        var bgPngPath = AssetDatabase.GetAssetPath(mapConfig.DLLoadingScreen);
        var minimapPngPath = AssetDatabase.GetAssetPath(mapConfig.DLMinimap);
        var customModeDatas = GameObject.FindObjectsOfType<CustomModeData>()?.Where(x => x.IsEnabled)?.ToArray();

        if (!Directory.Exists(buildPath)) Directory.CreateDirectory(buildPath);

        // copy files
        if (File.Exists(wadPath)) File.Copy(wadPath, Path.Combine(buildPath, $"{mapConfig.MapFilename}.wad"), true);
        if (File.Exists(soundPath)) File.Copy(soundPath, Path.Combine(buildPath, $"{mapConfig.MapFilename}.sound"), true);

        // build version file
        using (var fs = File.Create(Path.Combine(buildPath, $"{mapConfig.MapFilename}.version")))
        {
            using (var writer = new BinaryWriter(fs))
            {
                // write header
                writer.Write(mapConfig.MapVersion);
                writer.Write((int)mapConfig.DLBaseMap);
                writer.Write((int)mapConfig.DLForceCustomMode); // forced custom mode id
                writer.Write((short)(customModeDatas?.Length ?? 0)); // extra data count
                writer.Write((short)mapConfig.ShrubMinRenderDistance); // shrub min render distance
                writer.WriteString(mapConfig.MapName, 32);

                // write extra data
                if (customModeDatas != null)
                {
                    // write table entries
                    foreach (var customModeData in customModeDatas)
                    {
                        writer.Write((short)customModeData.CustomMode);
                        writer.Write((short)0); // size
                        writer.Write(0); // offset
                    }

                    // write data
                    for (int i = 0; i < customModeDatas.Length; i++)
                    {
                        var offset = writer.BaseStream.Position;
                        customModeDatas[0].Write(writer);
                        var endOffset = writer.BaseStream.Position;
                        writer.BaseStream.Position = 0x30 + (8 * i) + 2;
                        writer.Write((short)(endOffset - offset));
                        writer.Write((int)offset);
                        writer.BaseStream.Position = endOffset;
                    }
                }
            }
        }

        // build minimap
        if (File.Exists(minimapPngPath))
        {
            var tempPngPath = Path.Combine(FolderNames.GetTempFolder(), $"{mapConfig.MapFilename}.png");
            var minimap = AssetDatabase.LoadAssetAtPath<Texture2D>(minimapPngPath);
            if (minimap)
            {
                File.Copy(AssetDatabase.GetAssetPath(minimap), tempPngPath, true);

                var result = PackerHelper.ConvertPngToPif4bpp(tempPngPath, buildPath, half_alpha: true, outSwizzle: true);
                if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                {
                    Debug.LogError($"Failed to pack minimap. {result}");
                    return;
                }

                var outPif2File = Path.Combine(buildPath, $"minimap.map");
                var outMapFile = Path.Combine(buildPath, $"{mapConfig.MapFilename}.map");
                if (File.Exists(outMapFile)) File.Delete(outMapFile);
                if (File.Exists(outPif2File)) File.Move(outPif2File, outMapFile);
            }
        }
    
        // build loading screen
        if (File.Exists(bgPngPath))
        {
            var tempPngPath = Path.Combine(FolderNames.GetTempFolder(), $"{mapConfig.MapFilename}.png");
            var bg = UnityHelper.ResizeTexture(AssetDatabase.LoadAssetAtPath<Texture2D>(bgPngPath), 512, 512);
            if (bg)
            {
                byte[] bytes = bg.EncodeToPNG();
                System.IO.File.WriteAllBytes(tempPngPath, bytes);

                var result = PackerHelper.ConvertPngToLoadingScreen(tempPngPath, buildPath);
                if (result != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
                {
                    Debug.LogError($"Failed to pack loading screen. {result}");
                    return;
                }

                var outPif2File = Path.Combine(buildPath, $"converted.bg");
                var outBgFile = Path.Combine(buildPath, $"{mapConfig.MapFilename}.bg");
                if (File.Exists(outBgFile)) File.Delete(outBgFile);
                if (File.Exists(outPif2File)) File.Move(outPif2File, outBgFile);
            }
        }
    }

    static async Task RebuildCollision(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        if (!await CollisionBaker.BakeCollision())
        {
            ctx.Cancel = true;
        }
    }

    static void RebuildSky(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var skyMeshFile = Path.Combine(binFolder, FolderNames.BinarySkyMeshFile);
        var skyBinFolder = Path.Combine(binFolder, FolderNames.BinarySkyFolder + "2");
        var skyBinFile = Path.Combine(binFolder, FolderNames.BinarySkyBinFile);
        var skyBlendFile = Path.Combine(resourcesFolder, FolderNames.SkyFolder, "sky.blend");
        var sky = GameObject.FindObjectOfType<Sky>();

        if (!Directory.Exists(skyBinFolder)) Directory.CreateDirectory(skyBinFolder);

        if (!sky)
        {
            Debug.LogWarning($"No sky object found. No sky will be built.");
            return;
        }

        if (!File.Exists(skyBlendFile))
        {
            Debug.LogWarning($"No sky file found at {skyBlendFile}. No sky will be built.");
            return;
        }

        if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Sky", 0))
            return;

        // export as glb
        // there is a bug in blender 4.1 that removes vertex color alpha when exporting to glb
        // the bug is fixed in 4.2 (will be released July 2024)
        if (false && !BlenderHelper.ImportMeshAsGlb(skyBlendFile, skyBinFolder, "mesh", true, out var outSkyMeshFile))
        {
            Debug.LogError($"Failed to export sky blend as gltf {skyBlendFile}");
            return;
        }

        if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Sky", 0.25f))
            return;

        // export textures
        var layers = sky.GetComponentsInChildren<SkyLayer>();
        var materials = layers.SelectMany(x => x.GetRenderer().sharedMaterials).ToList();
        var matToTex = new Dictionary<string, string>();
        var fxTextures = new List<string>();
        var texIdx = 0;
        foreach (var mat in materials)
        {
            var matName = mat.name.Substring(4);
            if (matToTex.ContainsKey(matName)) continue;

            var tex = mat.GetTexture("_MainTex") as Texture2D;
            var outTexFileName = $"{texIdx++}.png";
            var outTexPath = Path.Combine(skyBinFolder, outTexFileName);
            UnityHelper.SaveTexture(tex, outTexPath);
            matToTex.Add(matName, outTexFileName);
        }

        // export fx textures
        var fxTexs = Directory.EnumerateFiles(Path.Combine(resourcesFolder, FolderNames.SkyFolder, "Textures"), "sky-fx_*.png");
        foreach (var fxTex in fxTexs)
        {
            var tex = AssetDatabase.LoadAssetAtPath<Texture2D>(fxTex);
            var outTexFileName = $"{texIdx++}.png";
            var outTexPath = Path.Combine(skyBinFolder, outTexFileName);
            UnityHelper.SaveTexture(tex, outTexPath);
            fxTextures.Add(outTexFileName);
        }

        if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Sky", 0.5f))
            return;

        // build sky.asset
        using (var fs = File.Create(Path.Combine(skyBinFolder, "sky.asset")))
        {
            using (var writer = new StreamWriter(fs))
            {
                writer.WriteLine("Sky sky {");
                writer.WriteLine($"\tcolour: [0 0 0 0]");
                writer.WriteLine($"\tclear_screen: false");
                writer.WriteLine($"\tmaximum_sprite_count: {sky.MaxSpriteCount}");

                // write shells
                writer.WriteLine("\tshells {");
                for (int i = 0; i < layers.Length; ++i)
                {
                    var layer = layers[i];
                    var mesh = layer.GetMeshFilter().sharedMesh;
                    var euler = layer.transform.eulerAngles * Mathf.Deg2Rad;
                    var angvel = layer.AngularRotation * Mathf.Deg2Rad;

                    writer.WriteLine($"\t\tSkyShell {i} {{");
                    writer.WriteLine($"\t\t\tbloom: {(layer.Bloom ? "true" : "false")}");
                    writer.WriteLine($"\t\t\tstarting_rotation: [{euler.y} {euler.x} {euler.z}]");
                    writer.WriteLine($"\t\t\tangular_velocity: [{angvel.y} {angvel.x} {angvel.z}]");
                    writer.WriteLine($"");
                    writer.WriteLine("\t\t\tMesh mesh {");
                    writer.WriteLine($"\t\t\t\tname: \"{mesh.name}\"");
                    writer.WriteLine("\t\t\t\tsrc: \"mesh.glb\"");
                    writer.WriteLine("\t\t\t}");
                    writer.WriteLine("\t\t}");
                }
                writer.WriteLine("\t}");

                // write materials
                writer.WriteLine("\tmaterials {");
                for (int i = 0; i < materials.Count; ++i)
                {
                    var mat = materials[i];
                    var matName = mat.name.Substring(4);
                    var tex = matToTex.GetValueOrDefault(matName);

                    writer.WriteLine($"\t\tMaterial {i} {{");
                    writer.WriteLine($"\t\t\tname: \"{matName}\"");
                    writer.WriteLine($"");
                    writer.WriteLine("\t\t\tTexture diffuse {");
                    writer.WriteLine($"\t\t\t\tsrc: \"{tex}\"");
                    writer.WriteLine("\t\t\t}");
                    writer.WriteLine("\t\t}");
                }
                writer.WriteLine("\t}");

                // write fx
                writer.WriteLine("\tfx {");
                for (int i = 0; i < fxTextures.Count; ++i)
                {
                    var tex = fxTextures[i];

                    writer.WriteLine($"\t\tTexture {i} {{");
                    writer.WriteLine($"\t\t\tsrc: \"{tex}\"");
                    writer.WriteLine("\t\t}");
                }
                writer.WriteLine("\t}");

                writer.WriteLine("}");
            }
        }

        if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Sky", 0.75f))
            return;

        // build
        if (!WrenchHelper.ConvertToSky(skyBinFolder, skyBinFile, Constants.GameVersion))
        {
            Debug.LogError($"Failed to pack sky.");
            return;
        }
    }

    static void RebuildTfrags(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var terrainAssetsFolder = Path.Combine(binFolder, FolderNames.BinaryTerrainFolder);
        var terrainBinFile = Path.Combine(binFolder, FolderNames.BinaryTerrainBinFile);
        var occlusionFolder = Path.Combine(binFolder, FolderNames.BinaryWorldInstanceOcclusionFolder);
        var tfragOcclusionBinFile = Path.Combine(occlusionFolder, "tfrag.bin");
        var chunks = GameObject.FindObjectsOfType<TfragChunk>();
        var materials = new List<Material>();

        if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Tfrags", 0.5f))
            return;

        // clear tfrag assets dir
        if (Directory.Exists(terrainAssetsFolder)) Directory.Delete(terrainAssetsFolder, true);
        Directory.CreateDirectory(terrainAssetsFolder);

        using (var fs = File.Create(terrainBinFile))
        {
            using (var writer = new BinaryWriter(fs))
            {
                int packetStart = 0x40;

                // write header
                writer.Write(packetStart); // chunk defs offset
                writer.Write(chunks.Length);
                writer.Write(18f); // unknown
                writer.Write(chunks.Length);
                writer.Write(new byte[0x30]);

                // build chunks
                for (int i = 0; i < chunks.Length; ++i)
                {
                    var chunk = chunks[i];
                    if (chunk.HeaderBytes == null || chunk.HeaderBytes.Length != 0x40)
                    {
                        Debug.LogError($"Unable to build tfrags. Chunk {chunk.name} has an invalid def. Please reimport the terrain to fix.");
                        ctx.Cancel = true;
                        return;
                    }

                    writer.Write(chunk.HeaderBytes);
                }

                // build data
                for (int i = 0; i < chunks.Length; ++i)
                {
                    var chunk = chunks[i];
                    var defCopy = chunk.HeaderBytes.ToArray();
                    var dataCopy = chunk.DataBytes.ToArray();
                    int dataOff = (int)fs.Position;

                    // add to materials list
                    var renderer = chunk.GetComponent<MeshRenderer>();
                    if (renderer)
                    {
                        var texIdxs = new int[renderer.sharedMaterials.Length];
                        for (int m = 0; m < renderer.sharedMaterials.Length; ++m)
                        {
                            var mat = renderer.sharedMaterials[m];
                            var idx = materials.IndexOf(mat);
                            if (idx < 0)
                            {
                                idx = materials.Count;
                                materials.Add(mat);
                            }

                            texIdxs[m] = idx;
                        }

                        TfragHelper.SetChunkTextureIndices(defCopy, dataCopy, texIdxs);
                    }

                    // apply transformation
                    TfragHelper.TransformChunk(defCopy, dataCopy, chunk.transform.localToWorldMatrix.SwizzleXZY());

                    // write header
                    fs.Position = packetStart + (0x40 * i);
                    writer.Write(defCopy);
                    fs.Position = packetStart + (0x40 * i) + 0x3a;
                    writer.Write((short)chunk.OcclusionId);

                    // write data buffer and save start/end positions
                    fs.Position = dataOff;
                    writer.Write(dataCopy);
                    var endDataOff = fs.Position;

                    // go back to header and write data offset
                    fs.Position = packetStart + (i * 0x40) + 0x10;
                    writer.Write(dataOff - packetStart);

                    // return to end of data
                    fs.Position = endDataOff;
                }

            }
        }

        // write textures
        if (materials.Any())
        {
            for (int i = 0; i < materials.Count; ++i)
            {
                var mat = materials[i];
                if (!mat || mat.shader.name != "Horizon Forge/Universal")
                {
                    Debug.LogError($"Tfrag material {mat.name} must use the 'Horizon Forge/Universal' shader.");
                    ctx.Cancel = true;
                    return;
                }

                var maxDimension = Constants.MAX_TEXTURE_SIZE;
                var baseTex = mat ? (mat.GetTexture("_MainTex") as Texture2D) : null;
                if (!baseTex) baseTex = UnityHelper.DefaultTexture;

                // determine max size of texture
                var texAssetPath = AssetDatabase.GetAssetPath(baseTex);
                if (!String.IsNullOrEmpty(texAssetPath))
                {
                    var importer = TextureImporter.GetAtPath(texAssetPath) as TextureImporter;
                    if (importer && importer.maxTextureSize < maxDimension)
                        maxDimension = Mathf.Max(Constants.MIN_TEXTURE_SIZE, importer.maxTextureSize);
                }

                var outTexFilePath = Path.Combine(terrainAssetsFolder, $"tex.{i:D4}.0.png");
                UnityHelper.SaveTexture(baseTex, outTexFilePath, forcePowerOfTwo: true, maxTexSize: maxDimension);
            }
        }

        // delete generated textures
        //foreach (var file in Directory.EnumerateFiles(terrainAssetsFolder, "tex.*.bin")) File.Delete(file);
        //foreach (var file in Directory.EnumerateFiles(terrainAssetsFolder, "tex.*.def")) File.Delete(file);
        //foreach (var file in Directory.EnumerateFiles(terrainAssetsFolder, "tex.*.palette")) File.Delete(file);

        // convert textures to asset textures
        if (PackerHelper.ConvertAssetTextures(terrainAssetsFolder, mipmaps: true, outSwizzle: true) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
        {
            Debug.LogError($"Failed to convert tfrag textures");
            ctx.Cancel = true;
        }


        // build tfrag occlusion
        if (chunks.Any())
        {
            using (var fs = File.Create(tfragOcclusionBinFile))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    var i = 0;
                    foreach (var chunk in chunks)
                    {
                        PackerHelper.WriteOcclusionBlock(writer, i, chunk.OcclusionId, chunk.Octants);
                        ++i;
                    }
                }
            }
        }
        else if (File.Exists(tfragOcclusionBinFile))
        {
            File.Delete(tfragOcclusionBinFile);
        }
    }

    static void RebuildTies(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var tieDb = mapConfig.GetTieDatabase();
        var tieAssetsFolder = Path.Combine(binFolder, FolderNames.BinaryTieFolder);

        // build list of ties in scene
        var ties = GameObject.FindObjectsOfType<Tie>(includeInactive: false) ?? new Tie[0];
        var tieClasses = ties.Select(x => x.OClass).Distinct().OrderBy(x => x).ToArray();

        // clear tie assets dir
        if (Directory.Exists(tieAssetsFolder)) Directory.Delete(tieAssetsFolder, true);
        Directory.CreateDirectory(tieAssetsFolder);

        // build ties
        int i = 0;
        foreach (var tieClass in tieClasses)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Ties", (float)i / tieClasses.Length))
                return;

            var srcTieDir = Path.Combine(resourcesFolder, FolderNames.TieFolder, tieClass.ToString());
            var srcTieGo = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(srcTieDir, tieClass.ToString() + ".fbx"));
            var srcTieLabels = AssetDatabase.GetLabels(srcTieGo);
            var tieDir = Path.Combine(tieAssetsFolder, $"{tieClass:00000}_{tieClass:X4}");
            if (!Directory.Exists(tieDir)) Directory.CreateDirectory(tieDir);

            // copy core.bin
            var coreBinFilePath = Path.Combine(srcTieDir, "core.bin");
            var coreBinBytes = File.ReadAllBytes(coreBinFilePath);
            var texCnt = coreBinBytes[15];

            // convert tie
            var sourceRacVersion = Constants.GameAssetTag.FirstOrDefault(x => Array.IndexOf(srcTieLabels, x.Value) >= 0).Key;
            if (sourceRacVersion == 0)
            {
                Debug.LogError($"Failed to determine source RC version for tie {tieClass}.");
                ctx.Cancel = true;
                return;
            }

            coreBinBytes = TieHelper.ConvertTie(sourceRacVersion, Constants.GameVersion, coreBinBytes);

            // update LOD distances
            if (tieDb)
            {
                var tieData = tieDb.Get(tieClass);
                if (tieData != null)
                {
                    using (var ms = new MemoryStream(coreBinBytes, true))
                    {
                        using (var writer = new BinaryWriter(ms))
                        {
                            var lodlow = BitConverter.ToSingle(coreBinBytes, 0x10);
                            var lodmid = BitConverter.ToSingle(coreBinBytes, 0x14) * tieData.LODDistanceMultiplier;
                            var lodhigh = BitConverter.ToSingle(coreBinBytes, 0x18) * tieData.LODDistanceMultiplier;

                            if (lodmid < 1)
                                lodmid = 2;
                            if (lodmid < lodlow)
                                lodlow = lodmid - 1;

                            ms.Position = 0x10;
                            writer.Write(lodlow);
                            writer.Write(lodmid);
                            writer.Write(lodhigh);

                            ms.Position = 0x48;
                            writer.Write(BitConverter.ToSingle(coreBinBytes, (int)ms.Position) * tieData.MipDistanceMultiplier);
                        }
                    }
                }
            }

            File.WriteAllBytes(Path.Combine(tieDir, "tie.bin"), coreBinBytes);

            // write textures
            if (srcTieGo)
            {
                var renderers = srcTieGo.GetComponentsInChildren<Renderer>();
                var materials = renderers.SelectMany(x => x.sharedMaterials).Distinct().ToArray();
                for (int j = 0; j < texCnt; ++j)
                {
                    var mat = materials.FirstOrDefault(x => x.name == $"{tieClass}-{j}");
                    if (!mat)
                        mat = AssetDatabase.LoadAssetAtPath<Material>(Path.Combine(srcTieDir, "Materials", $"{tieClass}-{j}.mat"));

                    if (!mat || mat.shader.name != "Horizon Forge/Universal") break;

                    var maxDimension = Constants.MAX_TEXTURE_SIZE;
                    var isGsStash = false;
                    var baseTex = mat ? (mat.GetTexture("_MainTex") as Texture2D) : null;
                    if (!baseTex) baseTex = UnityHelper.DefaultTexture;

                    // determine max size of texture
                    var texAssetPath = AssetDatabase.GetAssetPath(baseTex);
                    if (!String.IsNullOrEmpty(texAssetPath))
                    {
                        var importer = TextureImporter.GetAtPath(texAssetPath) as TextureImporter;
                        if (importer && importer.maxTextureSize < maxDimension)
                            maxDimension = Mathf.Max(Constants.MIN_TEXTURE_SIZE, importer.maxTextureSize);

                        isGsStash = Path.GetFileNameWithoutExtension(texAssetPath).EndsWith(".gs");
                    }

                    var outTexFilePath = Path.Combine(tieDir, $"tex.{j:D4}.{(isGsStash ? "3" : "0")}.png");
                    UnityHelper.SaveTexture(baseTex, outTexFilePath, forcePowerOfTwo: true, maxTexSize: maxDimension);
                }
            }

            // create tie.def
            using (var fs = File.Create(Path.Combine(tieDir, "tie.def")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    writer.Write(0);
                    writer.Write(tieClass);
                    writer.Write(0);
                    writer.Write(0);

                    for (int texIdx = 0; texIdx < 16; ++texIdx)
                    {
                        if (texIdx < texCnt)
                        {
                            // possible that a model reuses the same texture for multiple slots
                            // todo: handle that case
                            writer.Write((byte)texIdx);
                        }
                        else
                        {
                            writer.Write((byte)255);
                        }
                    }
                }
            }

            ++i;
        }

        // convert textures to asset textures
        if (PackerHelper.ConvertAssetTextures(tieAssetsFolder, mipmaps: true) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
        {
            Debug.LogError($"Failed to convert tie textures");
            ctx.Cancel = true;
        }
    }

    static async Task RebuildShrubs(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var shrubAssetsFolder = Path.Combine(binFolder, FolderNames.BinaryShrubFolder);

        // build list of shrubs in scene
        var shrubs = GameObject.FindObjectsOfType<Shrub>(includeInactive: false) ?? new Shrub[0];
        var shrubClasses = shrubs.Select(x => x.OClass).Distinct().OrderBy(x => x).ToList();

        // build dynamic shrubs
        var db = mapConfig.GetConvertToShrubDatabase();
        var dynamicShrubs = GameObject.FindObjectsOfType<ConvertToShrub>(includeInactive: false) ?? new ConvertToShrub[0];
        if (db && dynamicShrubs.Any())
        {
            await db.ConvertMany(force: false, silent: false, dynamicShrubs);

            foreach (var shrub in dynamicShrubs)
            {
                var data = db.Get(shrub);
                if (data != null)
                {
                    shrubClasses.AddRange(data.ShrubClasses);
                }
            }
        }

        // clear shrub assets dir
        if (Directory.Exists(shrubAssetsFolder)) Directory.Delete(shrubAssetsFolder, true);
        Directory.CreateDirectory(shrubAssetsFolder);

        // build shrubs
        int i = 0;
        foreach (var shrubClass in shrubClasses)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Shrubs", (float)i / shrubClasses.Count))
                return;

            var srcShrubDir = Path.Combine(resourcesFolder, FolderNames.ShrubFolder, shrubClass.ToString());
            var srcShrubGo = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(srcShrubDir, shrubClass.ToString() + ".fbx"));
            var shrubDir = Path.Combine(shrubAssetsFolder, $"{shrubClass:00000}_{shrubClass:X4}");
            if (!Directory.Exists(shrubDir)) Directory.CreateDirectory(shrubDir);

            // copy core.bin
            var coreBinFilePath = Path.Combine(srcShrubDir, "core.bin");
            var coreBinBytes = File.ReadAllBytes(coreBinFilePath);
            var hasBillboard = BitConverter.ToInt32(coreBinBytes, 0x1C) != 0;
            var texCnt = 0;
            File.Copy(coreBinFilePath, Path.Combine(shrubDir, "shrub.bin"), true);

            // write textures
            if (srcShrubGo)
            {
                var renderers = srcShrubGo.GetComponentsInChildren<Renderer>();
                var materials = renderers.SelectMany(x => x.sharedMaterials).Distinct().ToArray();
                for (int j = 0; j < 16; ++j)
                {
                    var mat = materials.FirstOrDefault(x => x.name == $"{shrubClass}-{j}");
                    if (!mat || mat.shader.name != "Horizon Forge/Universal") break;

                    var maxDimension = Constants.MAX_TEXTURE_SIZE;
                    var isGsStash = false;
                    var baseTex = mat.GetTexture("_MainTex") as Texture2D;
                    if (!baseTex) baseTex = UnityHelper.DefaultTexture;

                    // determine max size of texture
                    var texAssetPath = AssetDatabase.GetAssetPath(baseTex);
                    if (!String.IsNullOrEmpty(texAssetPath))
                    {
                        var importer = TextureImporter.GetAtPath(texAssetPath) as TextureImporter;
                        if (importer && importer.maxTextureSize < maxDimension)
                            maxDimension = Mathf.Max(Constants.MIN_TEXTURE_SIZE, importer.maxTextureSize);

                        isGsStash = Path.GetFileNameWithoutExtension(texAssetPath).EndsWith(".gs");
                    }

                    var outTexFilePath = Path.Combine(shrubDir, $"tex.{j:D4}.{(isGsStash ? "3" : "0")}.png");
                    UnityHelper.SaveTexture(baseTex, outTexFilePath, forcePowerOfTwo: true, maxTexSize: maxDimension);
                    ++texCnt;
                }
            }

            // write billboard texture
            if (hasBillboard)
            {
                var billboardFilePath = Path.Combine(srcShrubDir, "Textures", $"billboard-{shrubClass}-0.png");
                if (File.Exists(billboardFilePath))
                {
                    var billboardDir = Path.Combine(shrubDir, "billboard");
                    if (!Directory.Exists(billboardDir)) Directory.CreateDirectory(billboardDir);
                    File.Copy(billboardFilePath, Path.Combine(billboardDir, "tex.0000.0.png"), true);
                }
                else
                {
                    Debug.LogWarning($"Billboard configured for shrub {shrubClass} but no billboard texture exists at {billboardFilePath}");

                    // remove billboard ptr
                    coreBinBytes[0x1C] = coreBinBytes[0x1D] = coreBinBytes[0x1E] = coreBinBytes[0x1F] = 0;
                    File.WriteAllBytes(coreBinFilePath, coreBinBytes);
                }
            }

            // create shrub.def
            using (var fs = File.Create(Path.Combine(shrubDir, "shrub.def")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    writer.Write(0);
                    writer.Write(shrubClass);
                    writer.Write(0);
                    writer.Write(0);

                    for (int texIdx = 0; texIdx < 16; ++texIdx)
                    {
                        if (texIdx < texCnt)
                        {
                            // possible that a model reuses the same texture for multiple slots
                            // todo: handle that case
                            writer.Write((byte)texIdx);
                        }
                        else
                        {
                            writer.Write((byte)255);
                        }
                    }

                    writer.Write(new byte[0x10]);
                }
            }

            ++i;
        }

        // convert textures to asset textures
        if (PackerHelper.ConvertAssetTextures(shrubAssetsFolder, mipmaps: true) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
        {
            Debug.LogError($"Failed to convert shrub textures");
            ctx.Cancel = true;
        }
    }

    static void RebuildMobys(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mobyAssetsFolder = Path.Combine(binFolder, FolderNames.BinaryMobyFolder);
        var mobyResourcesFolder = Path.Combine(resourcesFolder, FolderNames.MobyFolder);
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();

        // build list of mobys to export
        // start with default list of mobys
        // then add the mobys in the scene that aren't already in the list
        var mobysToExport = mapConfig.DLMobysIncludedInExport.ToList();
        var mobys = mapConfig.GetMobys();
        foreach (var moby in mobys)
            if (!mobysToExport.Contains(moby.OClass))
                mobysToExport.Add(moby.OClass);

        var mobyClasses = mobysToExport.Distinct().OrderBy(x => x).ToArray();

        // clear moby assets dir
        if (Directory.Exists(mobyAssetsFolder)) Directory.Delete(mobyAssetsFolder, true);
        Directory.CreateDirectory(mobyAssetsFolder);

        // build mobys
        int i = 0;
        foreach (var mobyClass in mobyClasses)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Mobys", (float)i / mobyClasses.Length))
                return;

            var srcMobyDir = Path.Combine(resourcesFolder, FolderNames.MobyFolder, mobyClass.ToString());
            var srcMobyGo = AssetDatabase.LoadAssetAtPath<GameObject>(Path.Combine(srcMobyDir, mobyClass.ToString() + ".fbx"));
            var mobyDir = Path.Combine(mobyAssetsFolder, $"{mobyClass:00000}_{mobyClass:X4}");
            var texCnt = 0;
            var teamTexCnt = 0;
            if (!Directory.Exists(mobyDir)) Directory.CreateDirectory(mobyDir);

            // copy core.bin
            var coreBinFilePath = Path.Combine(srcMobyDir, "core.bin");
            if (File.Exists(coreBinFilePath))
            {
                var coreBinBytes = File.ReadAllBytes(coreBinFilePath);
                teamTexCnt = coreBinBytes[0xb];
                File.Copy(coreBinFilePath, Path.Combine(mobyDir, "moby.bin"), true);
            }

            // unpack sounds
            var soundsDir = Path.Combine(srcMobyDir, FolderNames.SoundsFolder);
            if (Directory.Exists(soundsDir))
            {
                var soundFiles = Directory.GetFiles(soundsDir, "*.sound");
                foreach (var soundFile in soundFiles)
                {
                    var soundIdxStr = Path.GetFileNameWithoutExtension(soundFile);
                    var outSoundFolder = Path.Combine(mobyDir, FolderNames.BinarySoundsFolder, soundIdxStr);
                    if (!Directory.Exists(outSoundFolder)) Directory.CreateDirectory(outSoundFolder);
                    PackerHelper.UnpackSound(soundFile, outSoundFolder);
                }
            }

            // write textures
            var packedTexFile = Path.Combine(srcMobyDir, "tex.bin");
            if (File.Exists(packedTexFile))
            {
                texCnt = PackerHelper.UnpackAssetTextures(packedTexFile, mobyDir);
            }
            else if (srcMobyGo)
            {
                if (teamTexCnt > 0)
                    Debug.LogWarning($"Moby {mobyClass} uses team palettes but does not have a packed tex.bin. Textures will not render correctly.");

                var renderers = srcMobyGo.GetComponentsInChildren<Renderer>();
                var materials = renderers.SelectMany(x => x.sharedMaterials).Distinct().ToArray();
                for (int j = 0; j < 16; ++j)
                {
                    var mat = materials.FirstOrDefault(x => x.name == $"{mobyClass}-{j}");
                    if (!mat || mat.shader.name != "Horizon Forge/Universal") break;

                    var baseTex = mat.GetTexture("_MainTex") as Texture2D;
                    if (!baseTex) baseTex = UnityHelper.DefaultTexture;

                    // determine max size of texture
                    var maxDimension = Constants.MAX_TEXTURE_SIZE;
                    var isGsStash = false;
                    var texAssetPath = AssetDatabase.GetAssetPath(baseTex);
                    if (!String.IsNullOrEmpty(texAssetPath))
                    {
                        var importer = TextureImporter.GetAtPath(texAssetPath) as TextureImporter;
                        if (importer && importer.maxTextureSize < maxDimension)
                            maxDimension = Mathf.Max(Constants.MIN_TEXTURE_SIZE, importer.maxTextureSize);

                        isGsStash = Path.GetFileNameWithoutExtension(texAssetPath).EndsWith(".gs");
                    }

                    var outTexFilePath = Path.Combine(mobyDir, $"tex.{j:D4}.{(isGsStash ? "3" : "0")}.png");
                    UnityHelper.SaveTexture(baseTex, outTexFilePath, forcePowerOfTwo: true, maxTexSize: maxDimension);
                    ++texCnt;
                }
            }

            // create moby.def
            using (var fs = File.Create(Path.Combine(mobyDir, "moby.def")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    writer.Write(0);
                    writer.Write(mobyClass);
                    writer.Write(0);
                    writer.Write(0);

                    for (int texIdx = 0; texIdx < 16; ++texIdx)
                    {
                        if (texIdx < texCnt)
                        {
                            // possible that a model reuses the same texture for multiple slots
                            // todo: handle that case
                            writer.Write((byte)texIdx);
                        }
                        else
                        {
                            writer.Write((byte)255);
                        }
                    }
                }
            }

            ++i;
        }

        // convert textures to asset textures
        if (PackerHelper.ConvertAssetTextures(mobyAssetsFolder, mipmaps: true) != PackerHelper.PACKER_STATUS_CODES.SUCCESS)
        {
            Debug.LogError($"Failed to convert moby textures");
            ctx.Cancel = true;
        }
    }

    static void RebuildTieInstances(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var tieInstancesFolder = Path.Combine(binFolder, FolderNames.BinaryWorldInstanceTieFolder);
        var occlusionFolder = Path.Combine(binFolder, FolderNames.BinaryWorldInstanceOcclusionFolder);
        var tieOcclusionBinFile = Path.Combine(occlusionFolder, "tie.bin");

        // build list of ties in scene
        var ties = HierarchicalSorting.Sort(GameObject.FindObjectsOfType<Tie>(includeInactive: false) ?? new Tie[0]).OrderBy(x => x.OClass).ToArray();
        var tieClasses = ties.Select(x => x.OClass).Distinct().ToArray();
        var tieClassCount = new Dictionary<int, int>();

        // clear tie assets dir
        if (Directory.Exists(tieInstancesFolder)) Directory.Delete(tieInstancesFolder, true);
        Directory.CreateDirectory(tieInstancesFolder);

        // build ties
        int i = 0;
        foreach (var tie in ties)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Ties  ({i + 1}/{ties.Length})", (float)i / ties.Length))
                return;

            var classIdx = Array.IndexOf(tieClasses, tie.OClass);
            if (!tieClassCount.ContainsKey(classIdx)) tieClassCount[classIdx] = 0;
            var classCountIdx = tieClassCount[classIdx];
            tieClassCount[classIdx]++;

            var tieDir = Path.Combine(tieInstancesFolder, $"{tie.OClass:00000}_{tie.OClass:X4}/{classCountIdx:D4}");
            if (!Directory.Exists(tieDir)) Directory.CreateDirectory(tieDir);

            // write colors
            File.WriteAllBytes(Path.Combine(tieDir, "colors.bin"), tie.ColorData ?? new byte[0]);

            // write group
            File.WriteAllBytes(Path.Combine(tieDir, "group.bin"), BitConverter.GetBytes(tie.GroupId));

            // create tie.bin
            using (var fs = File.Create(Path.Combine(tieDir, "tie.bin")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    writer.Write((int)tie.OClass);
                    writer.Write((int)0xFA0);
                    writer.Write((int)0);
                    writer.Write((int)tie.OcclusionId);

                    // write matrix back in respective order
                    var m = tie.Reflection * tie.transform.localToWorldMatrix;
                    writer.Write(m[0 + 0]);
                    writer.Write(m[0 + 2]);
                    writer.Write(m[0 + 1]);
                    writer.Write(m[0 + 3]);
                    writer.Write(m[8 + 0]);
                    writer.Write(m[8 + 2]);
                    writer.Write(m[8 + 1]);
                    writer.Write(m[8 + 3]);
                    writer.Write(m[4 + 0]);
                    writer.Write(m[4 + 2]);
                    writer.Write(m[4 + 1]);
                    writer.Write(m[4 + 3]);
                    writer.Write(m[12 + 0]);
                    writer.Write(m[12 + 2]);
                    writer.Write(m[12 + 1]);
                    writer.Write(m[12 + 3]);

                    writer.Write((int)0);
                    writer.Write((int)tie.OcclusionId);
                    writer.Write((int)0);
                    writer.Write((int)0);
                }
            }

            ++i;
        }

        // build tie occlusion
        if (ties.Any())
        {
            var allOctants = UnityHelper.GetAllOctants();
            using (var fs = File.Create(tieOcclusionBinFile))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    i = 0;
                    foreach (var tie in ties)
                    {
                        PackerHelper.WriteOcclusionBlock(writer, i, tie.OcclusionId, tie.Octants ?? new Vector3[0]);
                        ++i;
                    }
                }
            }
        }
        else if (File.Exists(tieOcclusionBinFile))
        {
            File.Delete(tieOcclusionBinFile);
        }
    }

    static void RebuildShrubInstances(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var shrubInstancesFolder = Path.Combine(binFolder, FolderNames.BinaryWorldInstanceShrubFolder);
        var db = mapConfig.GetConvertToShrubDatabase();

        // build list of shrubs in scene
        var shrubs = HierarchicalSorting.Sort(GameObject.FindObjectsOfType<Shrub>(includeInactive: false) ?? new Shrub[0]).OrderBy(x => x.OClass).ToArray();
        var dynamicShrubs = GameObject.FindObjectsOfType<ConvertToShrub>(includeInactive: false) ?? new ConvertToShrub[0];

        var totalShrubInstances = shrubs.Length + dynamicShrubs.Length;
        var shrubClasses = shrubs.Select(x => x.OClass).Union(dynamicShrubs.SelectMany(x => db.Get(x)?.ShrubClasses) ?? new List<int>()).Distinct().OrderBy(x => x).ToList();
        var shrubClassCount = new Dictionary<int, int>();

        // clear shrub instances dir
        if (Directory.Exists(shrubInstancesFolder)) Directory.Delete(shrubInstancesFolder, true);
        Directory.CreateDirectory(shrubInstancesFolder);

        // build shrubs
        int i = 0;
        foreach (var shrub in shrubs)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Shrubs ({i + 1}/{totalShrubInstances})", (float)i / totalShrubInstances))
                return;

            var classIdx = shrubClasses.IndexOf(shrub.OClass);
            if (!shrubClassCount.ContainsKey(classIdx)) shrubClassCount[classIdx] = 0;
            var classCountIdx = shrubClassCount[classIdx];
            shrubClassCount[classIdx]++;

            var shrubDir = Path.Combine(shrubInstancesFolder, $"{shrub.OClass:00000}_{shrub.OClass:X4}/{classCountIdx:D4}");
            if (!Directory.Exists(shrubDir)) Directory.CreateDirectory(shrubDir);

            // write group
            File.WriteAllBytes(Path.Combine(shrubDir, "group.bin"), BitConverter.GetBytes(shrub.GroupId));

            // create shrub.bin
            using (var fs = File.Create(Path.Combine(shrubDir, "shrub.bin")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    writer.Write((int)shrub.OClass);
                    writer.Write((float)shrub.RenderDistance);
                    writer.Write((int)0);
                    writer.Write((int)0);

                    // write matrix back in respective order
                    var m = shrub.Reflection * shrub.transform.localToWorldMatrix;
                    writer.Write(m[0 + 0]);
                    writer.Write(m[0 + 2]);
                    writer.Write(m[0 + 1]);
                    writer.Write(m[0 + 3]);
                    writer.Write(m[8 + 0]);
                    writer.Write(m[8 + 2]);
                    writer.Write(m[8 + 1]);
                    writer.Write(m[8 + 3]);
                    writer.Write(m[4 + 0]);
                    writer.Write(m[4 + 2]);
                    writer.Write(m[4 + 1]);
                    writer.Write(m[4 + 3]);
                    writer.Write(m[12 + 0]);
                    writer.Write(m[12 + 2]);
                    writer.Write(m[12 + 1]);
                    writer.Write(m[12 + 3]);

                    // write rgb
                    writer.Write((int)(shrub.Tint.r * 128));
                    writer.Write((int)(shrub.Tint.g * 128));
                    writer.Write((int)(shrub.Tint.b * 128));

                    writer.Write((int)0);
                    writer.Write((int)0);
                    writer.Write((int)0);
                    writer.Write((int)0);
                    writer.Write((int)0);
                }
            }

            ++i;
        }
    
        // build dynamic shrubs
        foreach (var shrub in dynamicShrubs)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Shrubs ({i + 1}/{totalShrubInstances})", (float)i / totalShrubInstances))
                return;

            var data = db.Get(shrub);
            if (data == null) continue;

            foreach (var shrubSubClass in data.ShrubClasses)
            {
                var classIdx = shrubClasses.IndexOf(shrubSubClass);
                if (!shrubClassCount.ContainsKey(classIdx)) shrubClassCount[classIdx] = 0;
                var classCountIdx = shrubClassCount[classIdx];
                shrubClassCount[classIdx]++;

                var shrubDir = Path.Combine(shrubInstancesFolder, $"{shrubSubClass:00000}_{shrubSubClass:X4}/{classCountIdx:D4}");
                if (!Directory.Exists(shrubDir)) Directory.CreateDirectory(shrubDir);

                // write group
                File.WriteAllBytes(Path.Combine(shrubDir, "group.bin"), BitConverter.GetBytes(shrub.GroupId));

                // create shrub.bin
                using (var fs = File.Create(Path.Combine(shrubDir, "shrub.bin")))
                {
                    using (var writer = new BinaryWriter(fs))
                    {
                        writer.Write((int)shrubSubClass);
                        writer.Write((float)shrub.RenderDistance);
                        writer.Write((int)0);
                        writer.Write((int)0);

                        // write matrix back in respective order
                        var m = shrub.transform.localToWorldMatrix * Matrix4x4.Rotate(Quaternion.Euler(0, -90f, 0));
                        writer.Write(m[0 + 0]);
                        writer.Write(m[0 + 2]);
                        writer.Write(m[0 + 1]);
                        writer.Write(m[0 + 3]);
                        writer.Write(m[8 + 0]);
                        writer.Write(m[8 + 2]);
                        writer.Write(m[8 + 1]);
                        writer.Write(m[8 + 3]);
                        writer.Write(m[4 + 0]);
                        writer.Write(m[4 + 2]);
                        writer.Write(m[4 + 1]);
                        writer.Write(m[4 + 3]);
                        writer.Write(m[12 + 0]);
                        writer.Write(m[12 + 2]);
                        writer.Write(m[12 + 1]);
                        writer.Write(m[12 + 3]);

                        // write rgb
                        writer.Write((int)(shrub.Tint.r * 255));
                        writer.Write((int)(shrub.Tint.g * 255));
                        writer.Write((int)(shrub.Tint.b * 255));

                        writer.Write((int)0);
                        writer.Write((int)0);
                        writer.Write((int)0);
                        writer.Write((int)0);
                        writer.Write((int)0);
                    }
                }
            }

            ++i;
        }
    }

    static void RebuildMobyInstances(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var mobyInstancesFolder = Path.Combine(binFolder, FolderNames.BinaryGameplayMobyFolder);
        var occlusionFolder = Path.Combine(binFolder, FolderNames.BinaryWorldInstanceOcclusionFolder);
        var mobyOcclusionFile = Path.Combine(occlusionFolder, "moby.bin");

        // build list of mobys in scene
        var mobys = mapConfig.GetMobys();

        // clear moby instance dir
        if (Directory.Exists(mobyInstancesFolder)) Directory.Delete(mobyInstancesFolder, true);
        Directory.CreateDirectory(mobyInstancesFolder);

        // build mobys
        int i = 0;
        foreach (var moby in mobys)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Mobys ({i+1}/{mobys.Length})", (float)i / mobys.Length))
                return;

            var mobyDir = Path.Combine(mobyInstancesFolder, $"{i:D4}_{moby.OClass:00000}_{moby.OClass:X4}");
            if (!Directory.Exists(mobyDir)) Directory.CreateDirectory(mobyDir);

            // create moby.bin
            using (var fs = File.Create(Path.Combine(mobyDir, "moby.bin")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    moby.MakeUidUnique();
                    moby.Write(writer);
                }
            }

            // create pvar.bin
            if (moby.PVars != null && moby.PVars.Length > 0)
            {
                moby.UpdatePVars();
                File.WriteAllBytes(Path.Combine(mobyDir, "pvar.bin"), moby.PVars);
            }

            // create pvar_ptr.bin
            if (moby.PVarPointers != null && moby.PVarPointers.Length > 0)
            {
                using (var fs = File.Create(Path.Combine(mobyDir, "pvar_ptr.bin")))
                {
                    using (var writer = new BinaryWriter(fs))
                    {
                        foreach (var ptr in moby.PVarPointers)
                            writer.Write(ptr);
                    }
                }
            }

            ++i;
        }

        // remove moby occlusion
        // not yet supported
        // removing forces all mobys to render regardless of octant
        if (File.Exists(mobyOcclusionFile)) File.Delete(mobyOcclusionFile);
    }

    static void RebuildCuboids(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var cuboidsFolder = Path.Combine(binFolder, FolderNames.BinaryGameplayCuboidFolder);

        // build list of cuboids in scene
        var cuboids = mapConfig.GetCuboids();

        // clear cuboids instance dir
        if (Directory.Exists(cuboidsFolder)) Directory.Delete(cuboidsFolder, true);
        Directory.CreateDirectory(cuboidsFolder);

        // build cuboids
        int i = 0;
        foreach (var cuboid in cuboids)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Cuboids ({i + 1}/{cuboids.Length})", (float)i / cuboids.Length))
                return;

            // create cuboid .bin
            using (var fs = File.Create(Path.Combine(cuboidsFolder, $"{i:D4}.bin")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    cuboid.Write(writer);
                }
            }

            ++i;
        }
    }

    static void RebuildSplines(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var splinesFolder = Path.Combine(binFolder, FolderNames.BinaryGameplaySplineFolder);

        // build list of splines in scene
        var splines = mapConfig.GetSplines();

        // clear cuboids instance dir
        if (Directory.Exists(splinesFolder)) Directory.Delete(splinesFolder, true);
        Directory.CreateDirectory(splinesFolder);

        // build splines
        int i = 0;
        foreach (var spline in splines)
        {
            if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Splines ({i + 1}/{splines.Length})", (float)i / splines.Length))
                return;

            // create cuboid .bin
            using (var fs = File.Create(Path.Combine(splinesFolder, $"{i:D4}.bin")))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    spline.Write(writer);
                }
            }

            ++i;
        }
    }

    static void RebuildAreas(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var areaFile = Path.Combine(binFolder, FolderNames.BinaryGameplayAreaFile);

        // build list of splines and cuboids in scene
        var cuboids = mapConfig.GetCuboids();
        var splines = mapConfig.GetSplines();
        var areas = mapConfig.GetAreas();


        // create area .bin
        using (var fs = File.Create(areaFile))
        {
            using (var writer = new BinaryWriter(fs))
            {
                var splineIndices = new List<int>();
                var cuboidIndices = new List<int>();

                // build header
                writer.Write(0); // total size of area file

                writer.Write(areas.Length);
                writer.Write(0); // spline array offset
                writer.Write(0); // cuboid array offset
                writer.Write(0); // unk array offset
                writer.Write(0); // cylinders array offset
                writer.Write(0);
                writer.Write(0);
                writer.Write(0);


                // build areas
                int i = 0;
                foreach (var area in areas)
                {
                    if (RebuildLevelProgress(ref ctx.Cancel, $"Rebuilding Areas ({i + 1}/{areas.Length})", (float)i / areas.Length))
                        return;

                    // add splines + cuboids to indices arrays
                    var splineOffset = splineIndices.Count * 4;
                    var cuboidOffset = cuboidIndices.Count * 4;

                    if (area.Splines.Any())
                        foreach (var spline in area.Splines)
                            splineIndices.Add(Array.IndexOf(splines, spline));

                    if (area.Cuboids.Any())
                        foreach (var cuboid in area.Cuboids)
                            cuboidIndices.Add(Array.IndexOf(cuboids, cuboid));

                    // write
                    writer.Write(area.transform.position.x);
                    writer.Write(area.transform.position.z);
                    writer.Write(area.transform.position.y);
                    writer.Write(area.BSphereRadius);
                    writer.Write((short)area.Splines.Count);
                    writer.Write((short)area.Cuboids.Count);
                    writer.Write((short)0);
                    writer.Write((short)0);
                    writer.Write(0);
                    writer.Write(area.Splines.Any() ? splineOffset : 0);
                    writer.Write(area.Cuboids.Any() ? cuboidOffset : 0);
                    writer.Write(0);
                    writer.Write(0);
                    writer.Write(0);

                    ++i;
                }

                // write spline data
                var splineDataOffset = (int)writer.BaseStream.Position;
                foreach (var splineIdx in splineIndices)
                    writer.Write(splineIdx);

                // write cuboid data
                var cuboidDataOffset = (int)writer.BaseStream.Position;
                foreach (var cuboidIdx in cuboidIndices)
                    writer.Write(cuboidIdx);

                // write header
                var size = (int)writer.BaseStream.Position;
                writer.BaseStream.Position = 0;
                writer.Write(size - 4);
                writer.BaseStream.Position = 8;
                if (splineIndices.Any()) writer.Write(splineDataOffset - 4);
                writer.BaseStream.Position = 12;
                if (cuboidIndices.Any()) writer.Write(cuboidDataOffset - 4);
            }
        }

    }

    static void RebuildCode(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var mapRender = GameObject.FindObjectOfType<MapRender>();
        var binCodeFolder = Path.Combine(binFolder, FolderNames.BinaryCodeFolder);

        if (!mapRender) return;
        if (!mapConfig) return;

        // copy code
        var files = Directory.EnumerateFiles(Path.Combine(resourcesFolder, FolderNames.GetMapCodeFolder(Constants.GameVersion, GameRegion.NTSC)), "code.*.*");
        foreach (var file in files)
        {
            var ext = Path.GetExtension(file);
            if (ext == ".bin" || ext == ".def")
            {
                File.Copy(file, Path.Combine(binCodeFolder, Path.GetFileName(file)), true);
            }
        }

        // update radar map pos/scale
        var codeSegmentBinPath = Path.Combine(binFolder, FolderNames.BinaryCodeFolder, "code.0002.bin");
        if (!File.Exists(codeSegmentBinPath)) return;

        using (var fs = File.OpenWrite(codeSegmentBinPath))
        {
            using (var writer = new BinaryWriter(fs))
            {
                var mapIdx = (int)mapConfig.DLBaseMap - 41;
                fs.Position = 0x175C8 + (0x10 * mapIdx);

                // write map render pos/scale
                writer.Write(mapRender.transform.position.x);
                writer.Write(mapRender.transform.position.z);
                writer.Write(mapRender.transform.localScale.x);
                writer.Write(mapRender.transform.localScale.z);
            }
        }
    }

    static void RebuildPostProcess(RebuildContext ctx, string resourcesFolder, string binFolder)
    {
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return;

        var worldConfigBinFile = Path.Combine(binFolder, FolderNames.BinaryGameplayFolder, "0.bin");
        if (File.Exists(worldConfigBinFile))
        {
            using (var fs = File.OpenWrite(worldConfigBinFile))
            {
                using (var writer = new BinaryWriter(fs))
                {
                    fs.Position = 0x00;
                    writer.Write((int)(mapConfig.BackgroundColor.r * 255));
                    writer.Write((int)(mapConfig.BackgroundColor.g * 255));
                    writer.Write((int)(mapConfig.BackgroundColor.b * 255));
                    writer.Write((int)(mapConfig.FogColor.r * 255));
                    writer.Write((int)(mapConfig.FogColor.g * 255));
                    writer.Write((int)(mapConfig.FogColor.b * 255));
                    writer.Write(mapConfig.FogNearDistance * 1024.0f);
                    writer.Write(mapConfig.FogFarDistance * 1024.0f);
                    writer.Write((1 - mapConfig.FogNearIntensity) * 255.0f);
                    writer.Write((1 - mapConfig.FogFarIntensity) * 255.0f);

                    // write death barrier height
                    fs.Position = 0x28;
                    writer.Write(mapConfig.DeathPlane);
                }
            }
        }

        var worldLights = mapConfig.GetWorldLights();
        if (worldLights != null && worldLights.Any())
        {
            var worldLightBinFile = Path.Combine(binFolder, FolderNames.BinaryWorldInstancesFolder, "0.bin");
            if (File.Exists(worldLightBinFile))
            {
                using (var fs = File.OpenWrite(worldLightBinFile))
                {
                    using (var writer = new BinaryWriter(fs))
                    {
                        fs.Position = 0x00;
                        writer.Write(worldLights.Length);
                        writer.Write(new byte[12]);

                        foreach (var light in worldLights)
                        {
                            var ray0 = light.GetRay(0) * light.GetIntensity(0);
                            var ray1 = light.GetRay(1) * light.GetIntensity(1);
                            var color0 = light.GetColor(0);
                            var color1 = light.GetColor(1);

                            // ray 1
                            writer.Write(color0.r);
                            writer.Write(color0.g);
                            writer.Write(color0.b);
                            writer.Write(0f);
                            writer.Write(ray0.x);
                            writer.Write(ray0.z);
                            writer.Write(ray0.y);
                            writer.Write(0f);

                            // ray 2
                            writer.Write(color1.r);
                            writer.Write(color1.g);
                            writer.Write(color1.b);
                            writer.Write(0f);
                            writer.Write(ray1.x);
                            writer.Write(ray1.z);
                            writer.Write(ray1.y);
                            writer.Write(0f);
                        }
                    }
                }
            }
        }
    }

    static async void BuildDZOFiles(UnityEngine.SceneManagement.Scene scene)
    {
        var binFolder = FolderNames.GetMapBinFolder(scene.name, Constants.GameVersion);
        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!Directory.Exists(binFolder))
        {
            EditorUtility.DisplayDialog("Cannot build", $"Scene does not have matching level folder \"{scene.name}\"", "Ok");
            return;
        }

        if (!scene.isLoaded || !mapConfig)
            return;

        var buildFolder = FolderNames.GetMapBuildFolder(scene.name, Constants.GameVersion);
        var outGlbFile = Path.Combine(buildFolder, $"{mapConfig.MapFilename}.dzo.glb");
        var outMetadataFile = Path.Combine(buildFolder, $"{mapConfig.MapFilename}.dzo.json");
        if (!Directory.Exists(buildFolder)) Directory.CreateDirectory(buildFolder);

        // export glb
        // export metadata on success
        await MapExporter.ExportSceneForDZO(outGlbFile, outMetadataFile);

        CopyToBuildFolders(EditorSceneManager.GetActiveScene());

        Debug.Log("DZO build complete");
    }



    class RebuildContext
    {
        public string MapSceneName;
        public bool Cancel;
    }
}
