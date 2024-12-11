using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.SceneManagement;

public class CodeManager : MonoBehaviour
{
    public bool Enabled = false;
    public bool DebugBuild = true;

    public async Task<bool> Build(string map, int racVersion)
    {
        // DL only atm
        if (racVersion != RCVER.DL) return false;

#if DOCKER
        var manager = DockerManager.GetOrCreate();
        if (!manager) return false;
        if (!manager.ContainerStarting() && !manager.ContainerReady()) await manager.Run();

        // waiting on docker container
        if (!manager.ContainerReady()) return false;

        var folder = FolderNames.GetMapCodeBuildFolder(map, racVersion);
        var res = await manager.ExecuteAsync("/bin/sh", "-c", $"cd /{folder} && make clean && make");
        if (res.ExitCode != 0)
        {
            Dispatcher.RunOnMainThread(() =>
            {
                Debug.Log(res.ExitCode + ": " + res.Stdout);
                if (!string.IsNullOrEmpty(res.Stderr))
                    Debug.LogError(res.Stderr);
            });
        }

        return res.ExitCode == 0;
#else
        Debug.LogError("Custom code rebuild detected but docker is not installed & activated. Please configure the Code Manager inside your Map GameObject.", this.gameObject);
        return false;
#endif
    }

    public bool Generate(CodeGenState state)
    {
        var scene = SceneManager.GetActiveScene();
        if (scene == null) return false;

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return false;

        // only DL is supported atm
        if (mapConfig.FirstRacVersion != RCVER.DL && mapConfig.SecondRacVersion != RCVER.DL) return false;

        state.Includes.Add($"#include \"common.h\"");
        state.ObjectFiles.Add("src/main.o");
        state.ObjectFiles.Add("src/common.o");

        // 
        var outDir = FolderNames.GetMapCodeBuildFolder(scene.name, RCVER.DL);
        var outIncludeDir = Path.Combine(outDir, FolderNames.CodeBuildIncludeFolder);
        var outSrcDir = Path.Combine(outDir, FolderNames.CodeBuildSrcFolder);
        var cMainPath = Path.Combine(outSrcDir, "main.c");
        var makefilePath = Path.Combine(outDir, "Makefile");

        // build src dir
        if (!Directory.Exists(outIncludeDir)) Directory.CreateDirectory(outIncludeDir);
        if (!Directory.Exists(outSrcDir)) Directory.CreateDirectory(outSrcDir);

        // pass to generators
        var generators = GameObject.FindObjectsOfType<MonoBehaviour>().Select(x => x.GetComponent<ICodeGen>()).Where(x => x != null).ToArray();
        foreach (var generator in generators)
        {
            if (!generator.IsEnabled) continue;
            generator.Configure(outDir, state);
        }

        // copy base into working dir
        CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "base"), outDir);

        // update main.c
        var cMainContent = File.ReadAllText(cMainPath)
            .Replace("##INCLUDES##", string.Join("\n", state.Includes))
            .Replace("##DECLARATIONS##", string.Join("\n", state.Declarations))
            .Replace("##FUNCTIONS##", string.Join("\n", state.Functions))
            .Replace("##INITBODY##", string.Join("\n", state.InitBody.Select(x => Indent(x, 1))))
            .Replace("##MAINBODYREADY##", string.Join("\n", state.MainBodyReady.Select(x => Indent(x, 2))))
            .Replace("##MAINBODY##", string.Join("\n", state.MainBody.Select(x => Indent(x, 1))))
            ;
        File.WriteAllText(cMainPath, cMainContent);

        // update makefile
        var makefileContent = File.ReadAllText(makefilePath)
            .Replace("##EEOBJS##", string.Join(" ", state.ObjectFiles))
            .Replace("##EELDFLAGS##", string.Join(" ", state.LDFlags))
            .Replace("##EEBUILD##", this.DebugBuild ? "DEBUG" : "RELEASE")
            ;
        File.WriteAllText(makefilePath, makefileContent);

        // write hook
        File.WriteAllBytes(Path.Combine(outDir, "hook.bin"), BitConverter.GetBytes(0x08000000 | (0x01EF0000 >> 2)));

        return true;
    }

    public void PostBuild(CodeGenState state)
    {

    }

    string Indent(string str, int indent)
    {
        if (str == null) return null;

        return str.Trim().PadLeft(str.Trim().Length + (indent * 2), ' ');
    }

    public static string[] CopySourceFilesIntoWorkingDirectory(string srcFolder, string destFolder)
    {
        var copiedFiles = new List<string>();
        var outIncludeDir = Path.Combine(destFolder, FolderNames.CodeBuildIncludeFolder);
        var outSrcDir = Path.Combine(destFolder, FolderNames.CodeBuildSrcFolder);
        var baseFiles = Directory.EnumerateFiles(srcFolder, "*.*", SearchOption.AllDirectories);
        foreach (var baseFile in baseFiles)
        {
            var subPath = Path.GetRelativePath(srcFolder, baseFile);
            string outPath;
            switch (Path.GetExtension(baseFile))
            {
                case ".meta":
                    {
                        // skip meta files
                        continue;
                    }
                case ".h":
                    {
                        outPath = Path.Combine(outIncludeDir, subPath);
                        break;
                    }
                case ".c":
                    {
                        outPath = Path.Combine(outSrcDir, subPath);
                        break;
                    }
                default:
                    {
                        outPath = Path.Combine(destFolder, subPath);
                        break;
                    }
            }

            if (!string.IsNullOrEmpty(outPath))
            {
                if (!Directory.Exists(Path.GetDirectoryName(outPath))) Directory.CreateDirectory(Path.GetDirectoryName(outPath));
                File.Copy(baseFile, outPath, true);
                copiedFiles.Add(outPath);
            }
        }

        return copiedFiles.ToArray();
    }
}

public class CodeGenState
{
    // main.c
    public List<string> Includes { get; set; } = new List<string>();
    public List<string> Declarations { get; set; } = new List<string>();
    public List<string> Functions { get; set; } = new List<string>();
    public List<string> InitBody { get; set; } = new List<string>();
    public List<string> MainBodyReady { get; set; } = new List<string>();
    public List<string> MainBody { get; set; } = new List<string>();

    // makefile
    public List<string> ObjectFiles { get; set; } = new List<string>();
    public List<string> LDFlags { get; set; } = new List<string>();
}
