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
    public static string GetBlenderPath()
    {
        return Win32Helper.AssocQueryString(Win32Helper.AssocStr.Executable, ".blend");
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

        var p = new System.Diagnostics.Process() { StartInfo = startInfo };
        p.OutputDataReceived += (s, e) => { sbOut.AppendLine(e.Data); };
        p.ErrorDataReceived += (s, e) => { sbError.AppendLine(e.Data); };
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();

        if (p.ExitCode != 1) Debug.LogError($"{p.ExitCode}: out:{sbOut} error:{sbError}");
        return p.ExitCode == 1;
    }

    public static bool PackCollision(string inBlendFile, string outDaeFile, params string[] additionalMeshes)
    {
        inBlendFile = Path.GetFullPath(inBlendFile).Replace("\\", "/");
        outDaeFile = Path.GetFullPath(outDaeFile).Replace("\\", "/");

        var additionalMeshesArgs = String.Join(" ", additionalMeshes.Select(x => "\"" + Path.GetFullPath(x).Replace("\\", "/") + "\""));

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
