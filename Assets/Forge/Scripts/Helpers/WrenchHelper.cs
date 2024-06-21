using System;
using System.Collections;
using System.Collections.Generic;
using System.Dynamic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public static class WrenchHelper
{
    private static readonly Dictionary<int, string> GAME_NAMES = new()
    {
        { 4, "dl" },
        { 3, "uya" },
        { 2, "gc" },
    };

    private static string GetGame(int version)
    {
        return GAME_NAMES.GetValueOrDefault(version) ?? "dl";
    }

    public static bool IsInstalled()
    {
        var exePath = Path.Combine("tools", "wrench", "wrenchbuild.exe");
        return File.Exists(exePath);
    }

    public static int RunWrench(out string output, params string[] args)
    {
        var exePath = Path.Combine("tools", "wrench", "wrenchbuild.exe");
        return RunWrench(exePath, out output, args);
    }

    private static int RunWrench(string exePath, out string output, string[] args)
    {
        output = null;
        string consoleData = "";

        if (!File.Exists(exePath))
        {
            throw new System.Exception("wrench not found in tools directory!");
        }

        var processArgs = string.Join(" ", args.Select(x => "\"" + x.Replace("\\", "/") + "\""));
        var startInfo = new System.Diagnostics.ProcessStartInfo(Path.GetFullPath(exePath), processArgs)
        {
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            RedirectStandardInput = true,
            UseShellExecute = false,
        };

        var p = new System.Diagnostics.Process() { StartInfo = startInfo };
        p.OutputDataReceived += (s, e) => { consoleData += e.Data + "\n"; };
        p.ErrorDataReceived += (s, e) => { consoleData += e.Data + "\n"; };
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();

        output = consoleData;
        if (p.ExitCode != 0)
            Debug.LogError(output);
        return p.ExitCode;
    }

    public static bool ExportTfrags(string binFile, string outColladaFile, int game)
    {
        var r = RunWrench(out _, "extract_tfrags", binFile, "-o", outColladaFile, "-g", GetGame(game));
        return r == 0;
    }

    public static bool ExportShrub(string binFile, string outGlbFile, int texCount, int game)
    {
        var r = RunWrench(out _, "extract_shrub", binFile.Replace("\\", "/"), "-o", outGlbFile.Replace("\\", "/"), "-h", texCount.ToString());
        return r == 0;
    }

    public static bool ExportTie(string binFile, string outColladaFile, int game)
    {
        var r = RunWrench(out _, "extract_tie", binFile, "-o", outColladaFile, "-g", GetGame(game));
        return r == 0;
    }

    public static bool ExportMoby(string binFile, string outColladaFile, int game)
    {
        var r = RunWrench(out _, "extract_moby", binFile, "-o", outColladaFile, "-g", GetGame(game));
        return r == 0;
    }

    public static bool ExportCollision(string collisionBinFile, string outFolder)
    {
        var r = RunWrench(out _, "unpack_collision", collisionBinFile, "-o", outFolder);
        return r == 0;
    }

    public static bool ExportSky(string skyBinFile, string outFolder, int game)
    {
        var r = RunWrench(out _, "unpack_sky", skyBinFile, "-o", outFolder, "-g", GetGame(game));
        return r == 0;
    }

    public static bool BuildCollision(string collisionAssetPath, string outCollisionBinFile)
    {
        var r = RunWrench(out _, "build_collision", Path.GetDirectoryName(collisionAssetPath).Replace("\\", "/"), "-o", outCollisionBinFile.Replace("\\", "/"), "-h", Path.GetFileNameWithoutExtension(collisionAssetPath));
        return r == 0;
    }

    public static bool ConvertToShrub(string workingDir, string outFilePath, string assetFileNameNoExt)
    {
        var r = RunWrench(out _, "build_shrub", workingDir.Replace("\\", "/"), "-o", outFilePath.Replace("\\", "/"), "-h", assetFileNameNoExt);
        return r == 0;
    }

    public static bool ConvertToSky(string workingDir, string outFilePath, int game)
    {
        var r = RunWrench(out _, "build_sky", workingDir.Replace("\\", "/"), "-o", outFilePath.Replace("\\", "/"), "-g", GetGame(game));
        return r == 0;
    }

    public static void SetDefaultWrenchModelImportSettings(string path, string type, bool remapMaterialsByModelNameAndMaterialName, string[] tags = null)
    {
        var assetPath = UnityHelper.GetProjectRelativePath(path);

        var extension = Path.GetExtension(assetPath);

        var obj = AssetDatabase.LoadAssetAtPath<GameObject>(assetPath);
        var asset = AssetDatabase.LoadMainAssetAtPath(assetPath);
        if (!obj) return;

        // add label
        var newTags = new string[2 + (tags?.Length ?? 0)];
        newTags[0] = "wrench";
        newTags[1] = type;
        if (tags != null) Array.Copy(tags, 0, newTags, 2, tags.Length);
        AssetDatabase.SetLabels(asset, newTags);

        switch (extension)
        {
            case ".fbx":
            case ".blend":
                {
                    ModelImporter importer = (ModelImporter)ModelImporter.GetAtPath(assetPath);
                    if (!importer) return;

                    importer.materialName = remapMaterialsByModelNameAndMaterialName ? ModelImporterMaterialName.BasedOnModelNameAndMaterialName : ModelImporterMaterialName.BasedOnTextureName;
                    importer.materialSearch = ModelImporterMaterialSearch.Local;
                    importer.preserveHierarchy = true;
                    importer.bakeAxisConversion = true;
                    importer.isReadable = true;
                    importer.importNormals = ModelImporterNormals.Calculate;
                    importer.keepQuads = false;
                    importer.addCollider = string.Equals(type, "collider", StringComparison.OrdinalIgnoreCase);

                    var joint0 = UnityHelper.FindInHierarchy(obj.transform, "joint_0");
                    if (joint0)
                    {
                        importer.avatarSetup = ModelImporterAvatarSetup.CreateFromThisModel;
                        importer.motionNodeName = UnityHelper.GetPath(obj.transform, joint0);
                    }

                    importer.SearchAndRemapMaterials(importer.materialName, importer.materialSearch);
                    importer.SaveAndReimport();
                    break;
                }
            case ".glb":
                {
                    break;
                }
        }
    }

    public static void SetDefaultWrenchModelTextureImportSettings(string path, int? maxTexSize = null)
    {
        var assetPath = UnityHelper.GetProjectRelativePath(path);

        TextureImporter importer = (TextureImporter)TextureImporter.GetAtPath(assetPath);
        if (importer == null) return;

        importer.alphaIsTransparency = true;

        if (maxTexSize.HasValue)
            importer.maxTextureSize = maxTexSize.Value;

        //importer.isReadable = true;
        //importer.SaveAndReimport();
    }

    public static dynamic ParseWrenchAssetFile(string assetPath)
    {
        dynamic asset = new ExpandoObject();
        Stack<dynamic> ctxStack = new Stack<dynamic>();
        ctxStack.Push(asset);

        using (var fs = File.OpenRead(assetPath))
        {
            using (var reader = new StreamReader(fs))
            {
                while (!reader.EndOfStream)
                {
                    var ctx = ctxStack.Peek() as IDictionary<string, System.Object>;
                    var line = reader.ReadLine();
                    var words = line?.Split(' ', System.StringSplitOptions.RemoveEmptyEntries);

                    if (string.IsNullOrEmpty(line)) continue;

                    if (line.EndsWith("{"))
                    {
                        // field decl
                        if (words.Length == 2)
                        {
                            var newctx = new ExpandoObject();
                            ctx.Add(words[0].Trim(), newctx);
                            ctxStack.Push(newctx);
                        }
                        // field inst
                        else if (words.Length == 3)
                        {
                            var newctx = new ExpandoObject();
                            ctx.Add(words[1].Trim(), newctx);
                            ctxStack.Push(newctx);
                        }
                    }
                    else if (line.EndsWith("}"))
                    {
                        ctxStack.Pop();
                    }
                    else if (line.Contains(":"))
                    {
                        ctx.Add(words[0].TrimEnd(':').Trim(), WrenchAssetParseDataValue(string.Join(" ", words.Skip(1))));
                    }
                }
            }
        }

        return asset;
    }

    private static object WrenchAssetParseDataValue(string value)
    {
        if (string.IsNullOrEmpty(value)) return null;
        if (value == "false") return false;
        if (value == "true") return true;
        if (value.StartsWith("\"") && value.EndsWith("\"")) return value.Substring(1, value.Length - 2);
        if (int.TryParse(value, out var intValue)) return intValue;
        if (float.TryParse(value, out var floatValue)) return floatValue;

        // array
        if (value.StartsWith("[") && value.EndsWith("]"))
            return value.Substring(1, value.Length - 2).Split(' ').Select(x => WrenchAssetParseDataValue(x)).ToArray();

        return value;
    }
}
