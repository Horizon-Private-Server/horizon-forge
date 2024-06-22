using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;


public class PackerImporterWindow : EditorWindow
{
    static readonly List<string> Destinations = new List<string>() { "Global", "Current Map" };
    static readonly List<string> AssetTypes = new List<string>() { FolderNames.TieFolder, FolderNames.ShrubFolder, FolderNames.MobyFolder };
    static readonly List<string> PackerAssetFolders = new List<string>() { "tie", "shrub", "moby" };
    static object lockObject = new object();

    DropdownField destDropdown;
    DropdownField typeDropdown;
    Toggle overwriteToggle;
    Button importButton;
    string importFolder;
    string importClass;

    public class PackerAssetImport
    {
        public int RacVersion = 4;
        public string AssetFolder;
        public string DestinationFolder;
        public string Name;
        public string AssetType;
        public bool PrependModelNameToTextures;
        public int? MaxTexSize;
        public int? GenerateCollisionId;
        public Func<string, string> GetTexName;
        public Action OnImport;
        public string[] AdditionalTags;
    }

    [MenuItem("Forge/Tools/Importers/Asset Importer")]
    public static void CreateNewWindow()
    {
        var wnd = GetWindow<PackerImporterWindow>();
        wnd.titleContent = new GUIContent("Asset Importer");
    }

    public void CreateGUI()
    {
        // Each editor window contains a root VisualElement object
        VisualElement root = rootVisualElement;
        root.BuildPadding();

        // VisualElements objects can contain other VisualElement following a tree hierarchy
        var titleLabel = new Label("Import Asset");
        titleLabel.style.unityTextAlign = TextAnchor.MiddleCenter;
        titleLabel.style.fontSize = 18;
        root.Add(titleLabel);

        root.BuildPadding();

        // Create destination dropdown
        root.BuildRow("Destination", (container) =>
        {
            destDropdown = new DropdownField();
            destDropdown.choices = Destinations;
            destDropdown.index = 0;
            container.Add(destDropdown);
        });

        // Create type dropdown
        root.BuildRow("Type", (container) =>
        {
            typeDropdown = new DropdownField();
            typeDropdown.choices = AssetTypes;
            typeDropdown.index = 0;
            container.Add(typeDropdown);
        });

        // Create type dropdown
        root.BuildRow("OClass", (container) =>
        {
            var field = new TextField();
            field.SetValueWithoutNotify(importClass);
            field.RegisterValueChangedCallback((e) => importClass = e.newValue);
            container.Add(field);
        });

        // Create overwrite toggle
        root.BuildRow("Overwrite existing", (container) =>
        {
            overwriteToggle = new Toggle();
            overwriteToggle.value = true;
            container.Add(overwriteToggle);
        });

        root.BuildPadding();
        importButton = new Button(OnImport);
        importButton.text = "Import";
        root.Add(importButton);
    }

    private string GetImportDestinationFolder()
    {
        var assetType = AssetTypes[typeDropdown.index];

        // output to current scene's resources path
        if (destDropdown.index == 1)
            return FolderNames.GetLocalAssetFolder(assetType);

        return FolderNames.GetGlobalAssetFolder(assetType);
    }

    private void OnImport()
    {
        var type = AssetTypes[typeDropdown.index];
        var overwrite = overwriteToggle.value;
        var folder = EditorUtility.OpenFolderPanel($"Select unpacked level {AssetTypes[typeDropdown.index]} folder", importFolder, "");
        if (String.IsNullOrEmpty(folder)) return;

        // save folder for reuse later
        importFolder = folder;

        // get imports
        try
        {
            var imports = GetAssetImportsSingle(folder);

            if (EditorUtility.DisplayDialog("Confirmation", $"This will import {imports.Count} assets into\n{String.Join("\n", imports.Select(x => x.DestinationFolder))}", "Yes", "No"))
                Import(imports, overwrite);
        }
        catch (Exception ex)
        {
            EditorUtility.DisplayDialog("Error", ex.Message, "Ok");
            Debug.LogException(ex);
        }
    }

    public static void Import(List<PackerAssetImport> imports, bool overwrite)
    {
        int i = 0;
        var shader = Shader.Find("Horizon Forge/Universal");
        var skyShader = Shader.Find("Horizon Forge/Skymesh");

        try
        {
            List<(PackerAssetImport, string, string)> modelsToConfigureImporterSettings = new List<(PackerAssetImport, string, string)>();
            List<bool> modelPrependedToTextureNames = new List<bool>();
            List<(PackerAssetImport Import, string Path, int Idx)> texturesToConfigureImporterSettings = new List<(PackerAssetImport, string, int)>();
            List<(PackerAssetImport Import, string ColladaPath)> importedColladaFiles = new List<(PackerAssetImport Import, string ColladaPath)>();
            List<string> materialsToConfigureImporterSettings = new List<string>();
            var cancel = false;

            AssetDatabase.StartAssetEditing();

            // create an action for each asset to import
            var importActions = imports.Select((import) => (Action)(() =>
            {
                if (cancel)
                {
                    lock (lockObject) { ++i; }
                    return;
                }

                try
                {
                    // get files in wrench asset folder
                    var assetFiles = Directory.EnumerateFiles(import.AssetFolder).ToList();

                    // prepare asset folder for import
                    var className = import.Name;
                    var assetDestFolder = import.DestinationFolder;
                    var textureDestFolder = Path.Combine(assetDestFolder, "Textures");
                    var materialDestFolder = Path.Combine(assetDestFolder, "Materials");
                    var texCount = 0;
                    if (!Directory.Exists(assetDestFolder)) { Directory.CreateDirectory(assetDestFolder); }
                    if (!Directory.Exists(textureDestFolder)) { Directory.CreateDirectory(textureDestFolder); }
                    //if (!Directory.Exists(materialDestFolder)) { Directory.CreateDirectory(materialDestFolder); }

                    // count textures
                    foreach (var assetFile in assetFiles)
                    {
                        if (!assetFile.EndsWith(".png")) continue;

                        var filename = Path.GetFileNameWithoutExtension(assetFile);

                        // count textures as #.png
                        if (!filename.Contains(".") && int.TryParse(filename, out var texId))
                        {
                            if (texId >= texCount) texCount = texId + 1;
                            continue;
                        }

                        // count high lod or gs stash textures tex.#.0.png or tex.#.3.png
                        if (filename.StartsWith("tex.") && (filename.EndsWith(".0") || filename.EndsWith(".3")) && int.TryParse(filename.Split('.')[1], out texId))
                        {
                            if (texId >= texCount) texCount = texId + 1;
                            continue;
                        }
                    }

                    // import textures
                    foreach (var assetFile in assetFiles)
                    {
                        var ext = Path.GetExtension(assetFile);
                        var name = Path.GetFileNameWithoutExtension(assetFile);

                        switch (ext)
                        {
                            case ".png":
                                {
                                    // parse tex idx from name
                                    var parts = name.Split('.');
                                    var idx = "0000";
                                    var mipmap = "0";
                                    var postfix = "";
                                    if (parts.Length == 1)
                                    {
                                        idx = parts[0];
                                    }
                                    else if (parts.Length == 3)
                                    {
                                        idx = parts[1];
                                        mipmap = parts[2];
                                    }

                                    // only copy highest quality mipmap
                                    if (mipmap == "1" || mipmap == "2") continue;

                                    // gs stash
                                    if (mipmap == "3")
                                    {
                                        postfix = ".gs";
                                    }

                                    var texIdx = int.TryParse(idx, out var tidx) ? tidx : -1;
                                    if (import.GetTexName != null)
                                    {
                                        idx = import.GetTexName(Path.GetFileName(assetFile)) ?? idx;
                                    }
                                    else
                                    {
                                        idx = int.Parse(idx).ToString();
                                    }

                                    // copy texture
                                    var texName = $"{idx}{postfix}{ext}";
                                    var matName = $"{idx}.mat";
                                    if (import.PrependModelNameToTextures)
                                    {
                                        texName = $"{className}-{idx}{postfix}{ext}";
                                        matName = $"{className}-{idx}.mat";
                                    }

                                    var texOutPath = Path.Combine(textureDestFolder, texName);
                                    var matOutPath = Path.Combine(materialDestFolder, matName);
                                    var texAssetPath = UnityHelper.GetProjectRelativePath(texOutPath);
                                    var matAssetPath = UnityHelper.GetProjectRelativePath(matOutPath);

                                    // copy if not exists or overwrite existing
                                    if (!overwrite && File.Exists(texOutPath)) continue;
                                    File.Copy(assetFile, texOutPath, true);

                                    lock (lockObject)
                                    {
                                        texturesToConfigureImporterSettings.Add((import, texAssetPath, texIdx));
                                        materialsToConfigureImporterSettings.Add(matAssetPath);
                                    }
                                    break;
                                }
                            case ".bin":
                                {
                                    // copy asset bin
                                    if (name != "shrub" && name != "tie" && name != "moby") break;
                                    var coreOutPath = Path.Combine(assetDestFolder, $"core.bin");

                                    // copy if not exists or overwrite existing
                                    if (!overwrite && File.Exists(coreOutPath)) continue;
                                    File.Copy(assetFile, coreOutPath, true);

                                    // convert to collada
                                    var meshFileName = assetFile;
                                    switch (name)
                                    {
                                        case "shrub":
                                            meshFileName = assetFile + ".glb";
                                            WrenchHelper.ExportShrub(assetFile, meshFileName, texCount, import.RacVersion);
                                            break;
                                        case "tie":
                                            meshFileName = assetFile + ".dae";
                                            WrenchHelper.ExportTie(assetFile, meshFileName, import.RacVersion);
                                            importedColladaFiles.Add((import, meshFileName));
                                            break;
                                        case "moby":

                                            // read team tex count
                                            // if moby has team textures then save copy of raw textures
                                            // since repacking won't produce the correct palettes
                                            var teamTexCnt = File.ReadAllBytes(assetFile)[0xb];
                                            if (teamTexCnt > 0)
                                            {
                                                var dir = Path.GetDirectoryName(assetFile);
                                                var outTeamTexFile = Path.Combine(assetDestFolder, "tex.bin");
                                                PackerHelper.PackAssetTextures(dir, outTeamTexFile);
                                            }

                                            meshFileName = assetFile + ".dae";
                                            WrenchHelper.ExportMoby(assetFile, meshFileName, import.RacVersion);
                                            break;
                                    }

                                    // import mesh
                                    if (!BlenderHelper.ImportMesh(meshFileName, assetDestFolder, className, overwrite, out var outPath, fixNormals: true)) continue;

                                    // import collision
                                    if (import.GenerateCollisionId.HasValue)
                                    {
                                        var colFile = Path.Combine(assetDestFolder, $"{className}_col.fbx");
                                        BlenderHelper.PrepareMeshFileForCollider(meshFileName, colFile, $"col_{import.GenerateCollisionId.Value:x}");

                                        if (File.Exists(colFile))
                                        {
                                            lock (lockObject)
                                            {
                                                modelsToConfigureImporterSettings.Add((import, "Collider", colFile));
                                                modelPrependedToTextureNames.Add(import.PrependModelNameToTextures);
                                            }
                                        }
                                    }

                                    lock (lockObject)
                                    {
                                        modelsToConfigureImporterSettings.Add((import, import.AssetType, outPath));
                                        modelPrependedToTextureNames.Add(import.PrependModelNameToTextures);
                                    }
                                    break;
                                }
                            case ".glb":
                                {
                                    // copy asset bin
                                    if (name != "mesh" || import.Name != "sky") break;
                                    var outPath = Path.Combine(assetDestFolder, $"sky.glb");

                                    // copy if not exists or overwrite existing
                                    if (!overwrite && File.Exists(outPath)) continue;
                                    //File.Copy(assetFile, outPath, true);

                                    // import mesh
                                    if (!BlenderHelper.ImportMeshAsBlend(assetFile, assetDestFolder, className, overwrite, out outPath)) continue;

                                    lock (lockObject)
                                    {
                                        modelsToConfigureImporterSettings.Add((import, import.AssetType, outPath));
                                        modelPrependedToTextureNames.Add(import.PrependModelNameToTextures);
                                    }
                                    break;
                                }
                        }
                    }

                    // import billboard
                    var billboardPath = Path.Combine(import.AssetFolder, "billboard");
                    if (Directory.Exists(billboardPath))
                    {
                        var files = Directory.EnumerateFiles(billboardPath, "*.png");
                        foreach (var file in files)
                        {
                            var name = Path.GetFileNameWithoutExtension(file);

                            // parse tex idx from name
                            var parts = name.Split('.');
                            var idx = parts[1];
                            var mipmap = parts[2];

                            // only copy highest quality mipmap
                            if (mipmap != "0") continue;

                            var texIdx = int.Parse(idx);
                            var texPath = Path.Combine(textureDestFolder, $"billboard-{className}-{texIdx}.png");
                            File.Copy(file, texPath, true);

                            lock (lockObject)
                            {
                                texturesToConfigureImporterSettings.Add((import, UnityHelper.GetProjectRelativePath(texPath), texIdx));
                                materialsToConfigureImporterSettings.Add(null);
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    // log
                    Dispatcher.RunOnMainThread(() => Debug.LogException(ex));
                }

                lock (lockObject) { ++i; }
            })).ToArray();

            // import each action in parallel
            // wrap in Task.Run so that it's non blocking
            Task.Run(() => Parallel.Invoke(importActions.ToArray()));

            // wait for imports to finish
            while (i < imports.Count)
            {
                if (!cancel && EditorUtility.DisplayCancelableProgressBar($"Importing", $"({i}/{imports.Count})", i / (float)imports.Count))
                    cancel = true;

                Thread.Sleep(100);
            }

            AssetDatabase.StopAssetEditing();
            AssetDatabase.Refresh();

            // configure texture importers
            try
            {
                AssetDatabase.StartAssetEditing();
                i = 0;
                foreach (var texConfig in texturesToConfigureImporterSettings)
                {
                    Material mat;
                    // import
                    //AssetDatabase.ImportAsset(texAssetPath, ImportAssetOptions.Default);
                    var matchingColladaImport = importedColladaFiles.FirstOrDefault(x => x.Import == texConfig.Import).ColladaPath;
                    var wraps = new Dictionary<int, (TextureWrapMode?, TextureWrapMode?)>();
                    if (File.Exists(matchingColladaImport))
                        wraps = WrenchHelper.GetColladaTextureWraps(matchingColladaImport);

                    WrenchHelper.SetDefaultWrenchModelTextureImportSettings(texConfig.Path, texConfig.Import.MaxTexSize, wraps.GetValueOrDefault(texConfig.Idx).Item1, wraps.GetValueOrDefault(texConfig.Idx).Item2);

                    // create matching material
                    var matAssetPath = materialsToConfigureImporterSettings[i];
                    if (matAssetPath == null) { ++i; continue; }

                    // ensure materials dir exists
                    var matAssetDir = Path.GetDirectoryName(matAssetPath).Replace("\\", "/");
                    if (!Directory.Exists(matAssetDir))
                        AssetDatabase.CreateFolder(UnityHelper.GetProjectRelativePath(Directory.GetParent(matAssetDir).FullName), "Materials");

                    switch (texConfig.Import.AssetType)
                    {
                        case "Sky":
                            {
                                mat = new Material(skyShader);
                                break;
                            }
                        default:
                            {
                                mat = new Material(shader);
                                break;
                            }
                    }

                    var tex = AssetDatabase.LoadAssetAtPath<Texture2D>(texConfig.Path);
                    mat.SetTexture("_MainTex", tex);
                    AssetDatabase.CreateAsset(mat, matAssetPath);

                    EditorUtility.DisplayProgressBar($"Importing Textures", $"{texConfig} ({i}/{texturesToConfigureImporterSettings.Count})", i / (float)texturesToConfigureImporterSettings.Count);
                    ++i;
                }
            }
            catch (Exception ex)
            {
                Debug.LogException(ex);
            }
            finally
            {
                AssetDatabase.StopAssetEditing();
            }

            AssetDatabase.Refresh();

            // configure model importers
            try
            {
                AssetDatabase.StartAssetEditing();
                i = 0;
                foreach (var modelAssetPath in modelsToConfigureImporterSettings)
                {
                    WrenchHelper.SetDefaultWrenchModelImportSettings(modelAssetPath.Item3, modelAssetPath.Item2, modelPrependedToTextureNames[i], tags: modelAssetPath.Item1?.AdditionalTags);

                    EditorUtility.DisplayProgressBar($"Configuring Assets", $"{modelAssetPath} ({i}/{modelsToConfigureImporterSettings.Count})", i / (float)modelsToConfigureImporterSettings.Count);
                    ++i;
                }
            }
            finally
            {
                AssetDatabase.StopAssetEditing();
            }

            // finalize
            imports.ForEach(x => x.OnImport?.Invoke());
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
        }
        finally
        {
            EditorUtility.ClearProgressBar();
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh(ImportAssetOptions.ForceUpdate);
        }
    }

    List<PackerAssetImport> GetAssetImportsSingle(string folder)
    {
        var imports = new List<PackerAssetImport>();
        var type = AssetTypes[typeDropdown.index];
        var oclass = 0;

        // parse asset name
        if (int.TryParse(importClass, out var classInt))
            oclass = classInt;
        else if (importClass.StartsWith("0x") && int.TryParse(importClass.Substring(2), System.Globalization.NumberStyles.HexNumber, null, out var classHex))
            oclass = classHex;
        else
            throw new Exception("Unable to parse OClass. Please enter a valid number. Ex: 1234 (decimal) or 0x4D2 (hex)");

        // validate oclass
        if (oclass <= 0 || oclass >= short.MaxValue)
            throw new Exception($"OClass must be greater than 0 and less than {short.MaxValue}");

        // get import destination folder
        var destFolder = GetImportDestinationFolder();
        var fullDestFolder = Path.Combine(Environment.CurrentDirectory, destFolder);
        if (!Directory.Exists(fullDestFolder)) { Directory.CreateDirectory(fullDestFolder); }

        var name = oclass.ToString();
        imports.Add(new PackerAssetImport()
        {
            Name = name,
            DestinationFolder = Path.Combine(fullDestFolder, name),
            AssetFolder = folder,
            AssetType = type,
            PrependModelNameToTextures = true
        });

        return imports;
    }

    List<PackerAssetImport> GetAssetImportsWrench(string folder)
    {
        var imports = new List<PackerAssetImport>();
        var type = AssetTypes[typeDropdown.index];
        var packerAssetFolder = PackerAssetFolders[typeDropdown.index];
        var i = 0;

        // get import destination folder
        var destFolder = GetImportDestinationFolder();
        var fullDestFolder = Path.Combine(Environment.CurrentDirectory, destFolder);
        if (!Directory.Exists(fullDestFolder)) { Directory.CreateDirectory(fullDestFolder); }

        // we're expecting a folder containing a bunch of levels in the format of /levelId/assets/...
        var levelDirs = Directory.EnumerateDirectories(folder).ToArray();
        foreach (var levelDir in levelDirs)
        {
            var levelIdName = Path.GetFileName(levelDir);
            if (int.TryParse(levelIdName, out var levelId))
            {
                var assetFolder = Path.Combine(levelDir, "assets", packerAssetFolder);
                if (!Directory.Exists(assetFolder)) continue;
                var assetSubDirs = Directory.EnumerateDirectories(assetFolder);
                foreach (var assetSubDir in assetSubDirs)
                {
                    var assetSurDirName = Path.GetFileName(assetSubDir);
                    var parts = assetSurDirName.Split('_');
                    var idx = parts[0];
                    var oclass = parts[1];
                    var name = Convert.ToInt32(oclass, 16).ToString();

                    // skip repeats
                    if (imports.Any(x => x.Name == name)) continue;

                    imports.Add(new PackerAssetImport()
                    {
                        Name = name,
                        DestinationFolder = Path.Combine(fullDestFolder, name),
                        AssetFolder = assetSubDir,
                        AssetType = type,
                        PrependModelNameToTextures = true
                    });
                }
            }

            EditorUtility.DisplayProgressBar($"Reading Assets from Folder(s)", $"{levelDir} ({i}/{levelDirs.Length})", i / (float)levelDirs.Length);
            ++i;
        }

        return imports;
    }

    int GetChunkFromTfragPath(string path)
    {
        const string chunkIdentifier = "/chunks/";

        // Tfrags follow the following path structure
        // /{levelId}_{levelName}/chunks/{chunkId}/tfrags/

        // force path to use /
        path = path.Replace("\\", "/");

        // find where /chunks/ exists in path
        var chunkIdentifierIdx = path.IndexOf(chunkIdentifier);
        if (chunkIdentifierIdx < 0) return -1;
        else chunkIdentifierIdx += chunkIdentifier.Length;

        // find where next / exists in path
        var chunkIdEndIdx = path.IndexOf("/", chunkIdentifierIdx);
        if (chunkIdEndIdx < 0) return -1;

        // use two markers to extract chunkId
        var chunkStr = path.Substring(chunkIdentifierIdx, chunkIdEndIdx - chunkIdentifierIdx);
        if (int.TryParse(chunkStr, out var chunkId))
            return chunkId;

        return -1;
    }

    int GetLevelFromTfragPath(string path)
    {
        const string chunkIdentifier = "chunks";

        // Tfrags follow the following path structure
        // /{levelId}_{levelName}/chunks/{chunkId}/tfrags/

        // break up path
        var pathParts = path.Split(new char[] { '\\', '/' });

        // find where /chunks/ exists in path
        var chunkIdentifierIdx = Array.IndexOf(pathParts, chunkIdentifier);
        if (chunkIdentifierIdx < 0) return -1;

        // extract {levelId}_{levelName}
        var levelStr = pathParts[chunkIdentifierIdx - 1];

        // parse levelId
        var levelParts = levelStr.Split('_');
        if (int.TryParse(levelParts[0], out var levelId))
            return levelId;

        return -1;
    }
}
