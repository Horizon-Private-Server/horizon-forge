using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public static class BlenderHelper
{
    public static string GetBlenderPath(bool forceFindBlenderPath = false)
    {
        if (!forceFindBlenderPath)
        {
            var forgeSettings = ForgeSettings.Singleton;
            if (forgeSettings != null && !string.IsNullOrEmpty(forgeSettings.PathToBlender))
                return forgeSettings.PathToBlender;
        }

#if UNITY_STANDALONE_WIN
        return Win32Helper.AssocQueryString(Win32Helper.AssocStr.Executable, ".blend");
#else
        // path to blender not configured
        return null;
#endif
    }

    public static bool RunBlender(string pythonScript, string args, string blendFile = null)
    {
        var sbError = new StringBuilder();
        var sbOut = new StringBuilder();

        // we need blender
        var blenderPath = GetBlenderPath();
        var pyScriptPath = Path.GetFullPath(Path.Combine(FolderNames.BlenderScriptFolder, pythonScript)).Replace("\\", "/");
        if (!File.Exists(blenderPath))
        {
            throw new System.Exception("Blender not found! Please install Blender.");
        }

        var processArgs = $"--background --python \"{pyScriptPath}\" -- {args}";
        if (!String.IsNullOrEmpty(blendFile)) processArgs = $"\"{blendFile}\" " + processArgs;
        var startInfo = new System.Diagnostics.ProcessStartInfo(Path.GetFullPath(blenderPath), processArgs)
        {
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            RedirectStandardInput = true,
            UseShellExecute = false,
        };

        var success = false;
        var p = new System.Diagnostics.Process() { StartInfo = startInfo };
        p.OutputDataReceived += (s, e) => { sbOut.Append(e.Data); if (e.Data?.Contains("FORGE SCRIPT COMPLETE") == true) success = true; };
        p.ErrorDataReceived += (s, e) => { sbError.Append(e.Data); };
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();

        if (!success)
        {
            Debug.LogError($"{p.ExitCode}: out:{sbOut} error:{sbError}");
            return false;
        }

        return true;
    }

    public static bool PackCollision(string inBlendFile, string outDaeFile, params string[] additionalMeshes)
    {
        inBlendFile = Path.GetFullPath(inBlendFile).Replace("\\", "/");
        outDaeFile = Path.GetFullPath(outDaeFile).Replace("\\", "/");

        var additionalMeshesArgs = String.Join(" ", additionalMeshes.Where(x => !string.IsNullOrEmpty(x)).Select(x => "\"" + Path.GetFullPath(x).Replace("\\", "/") + "\""));

        return RunBlender("export-collision.py", "\"" + outDaeFile + "\" " + additionalMeshesArgs, blendFile: inBlendFile);
    }

    public static bool PrepareFileForShrubConvert(string inFile, string outGlbFile, string objectsToSelect)
    {
        inFile = Path.GetFullPath(inFile).Replace("\\", "/");
        outGlbFile = Path.GetFullPath(outGlbFile).Replace("\\", "/");

        return RunBlender("prepare-model-for-shrub-convert.py", "\"" + inFile + "\"" + " \"" + outGlbFile + "\"" + " \"" + objectsToSelect + "\"");
    }

    public static bool PrepareMeshFileForCollider(string inFile, string outFbxFile, string defaultMatId)
    {
        inFile = Path.GetFullPath(inFile).Replace("\\", "/");
        outFbxFile = Path.GetFullPath(outFbxFile).Replace("\\", "/");

        return RunBlender("prepare-model-for-collider.py", "\"" + inFile + "\"" + " \"" + outFbxFile + "\"" + " \"" + defaultMatId + "\"");
    }

    public static bool ImportCollision(string meshFile, string outDir, string name, bool overwrite, out string outMeshFile)
    {
        // import mesh
        meshFile = Path.GetFullPath(meshFile).Replace("\\", "/");
        outMeshFile = Path.GetFullPath(Path.Combine(outDir, $"{name}.blend")).Replace("\\", "/");

        // check the file we want to import exists
        // and that the out file doesn't exist, or overwrite existing
        if (File.Exists(meshFile) && (overwrite || !File.Exists(outMeshFile)))
        {
            RunBlender("import-collision.py", $"\"{meshFile}\" \"{outMeshFile}\"");

            if (File.Exists(outMeshFile))
                return true;
            else
                Debug.Log($"Failed to import mesh {meshFile}");
        }

        return false;
    }

    public static bool ImportMesh(string meshFile, string outDir, string name, bool overwrite, out string outMeshFile, bool fixNormals = false)
    {
        var extension = Path.GetExtension(meshFile);

        // import mesh
        meshFile = Path.GetFullPath(meshFile).Replace("\\", "/");
        outMeshFile = Path.GetFullPath(Path.Combine(outDir, $"{name}.fbx")).Replace("\\", "/");

        // check the file we want to import exists
        // and that the out file doesn't exist, or overwrite existing
        if (File.Exists(meshFile) && (overwrite || !File.Exists(outMeshFile)))
        {
            switch (extension)
            {
                case ".dae":
                    File.WriteAllText(meshFile, File.ReadAllText(meshFile).Replace("mat_", ""));
                    RunBlender("convert-mesh.py", $"\"{meshFile}\" \"{outMeshFile}\" {(fixNormals ? "1" : "0")}");
                    break;
                default:
                    RunBlender("convert-mesh.py", $"\"{meshFile}\" \"{outMeshFile}\" {(fixNormals ? "1" : "0")}");
                    break;
            }

            if (File.Exists(outMeshFile))
            {
                //AssetDatabase.ImportAsset(outMeshFile, ImportAssetOptions.Default);
                return true;
            }
            else
            {
                Debug.Log($"Failed to import mesh {meshFile}");
            }
        }

        return false;
    }

    public static bool ImportMeshAsBlend(string meshFile, string outDir, string name, bool overwrite, out string outMeshFile, bool fixNormals = false)
    {
        var extension = Path.GetExtension(meshFile);

        // import mesh
        meshFile = Path.GetFullPath(meshFile).Replace("\\", "/");
        outMeshFile = Path.GetFullPath(Path.Combine(outDir, $"{name}.blend")).Replace("\\", "/");

        // check the file we want to import exists
        // and that the out file doesn't exist, or overwrite existing
        if (File.Exists(meshFile) && (overwrite || !File.Exists(outMeshFile)))
        {
            switch (extension)
            {
                case ".dae":
                    File.WriteAllText(meshFile, File.ReadAllText(meshFile).Replace("mat_", ""));
                    RunBlender("convert-mesh.py", $"\"{meshFile}\" \"{outMeshFile}\" {(fixNormals ? "1" : "0")}");
                    break;
                default:
                    RunBlender("convert-mesh.py", $"\"{meshFile}\" \"{outMeshFile}\" {(fixNormals ? "1" : "0")}");
                    break;
            }

            if (File.Exists(outMeshFile))
            {
                return true;
            }
            else
            {
                Debug.Log($"Failed to import mesh {meshFile}");
            }
        }

        return false;
    }

    public static bool ImportMeshAsGlb(string meshFile, string outDir, string name, bool overwrite, out string outMeshFile, bool fixNormals = false)
    {
        var extension = Path.GetExtension(meshFile);

        // import mesh
        meshFile = Path.GetFullPath(meshFile).Replace("\\", "/");
        outMeshFile = Path.GetFullPath(Path.Combine(outDir, $"{name}.glb")).Replace("\\", "/");

        // check the file we want to import exists
        // and that the out file doesn't exist, or overwrite existing
        if (File.Exists(meshFile) && (overwrite || !File.Exists(outMeshFile)))
        {
            switch (extension)
            {
                case ".dae":
                    File.WriteAllText(meshFile, File.ReadAllText(meshFile).Replace("mat_", ""));
                    RunBlender("convert-mesh.py", $"\"{meshFile}\" \"{outMeshFile}\" {(fixNormals ? "1" : "0")}");
                    break;
                default:
                    RunBlender("convert-mesh.py", $"\"{meshFile}\" \"{outMeshFile}\" {(fixNormals ? "1" : "0")}");
                    break;
            }

            if (File.Exists(outMeshFile))
            {
                return true;
            }
            else
            {
                Debug.Log($"Failed to import mesh {meshFile}");
            }
        }

        return false;
    }

}
