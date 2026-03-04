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

    public async Task<bool> Build(string map, int racVersion)
    {
        // DL only atm
        if (racVersion != RCVER.DL) return false;

#if DOCKER
        var folder = FolderNames.GetMapCodeBuildFolder(map, racVersion);
        var res = await DockerManager.RunCommandWithContainer("/bin/sh", "-c", $"cd \"/{folder}\" && make clean && make");
        if (res.ExitCode != 0)
        {
            Dispatcher.RunOnMainThread(() =>
            {
                if (!string.IsNullOrEmpty(res.Stderr))
                {
                    var compilerErrors = res.Stderr
                        .Split(new[] { "\r\n", "\n" }, StringSplitOptions.None)
                        .Select(x => x?.Trim())
                        .Where(x => !string.IsNullOrWhiteSpace(x));

                    foreach (var compilerError in compilerErrors)
                        Debug.LogError(compilerError);
                }
            });
        }

        return res.ExitCode == 0;
#else
        Debug.LogError("Custom code rebuild detected but docker is not installed & activated. Please configure the Code Manager inside your Map GameObject.", this.gameObject);
        return false;
#endif
    }

    public bool Generate(ForgeBuilder.RebuildContext ctx, CodeGenState state)
    {
        var scene = SceneManager.GetActiveScene();
        if (scene == null) return false;

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig) return false;

        // only DL is supported atm
        if (ctx.RacVersion != RCVER.DL) return false;

        state.Includes.Add($"#include \"common.h\"");
        state.ObjectFiles.Add("src/main.o");
        state.ObjectFiles.Add("src/common.o");

        // 
        var outDir = FolderNames.GetMapCodeBuildFolder(scene.name, RCVER.DL);
        var outIncludeDir = Path.Combine(outDir, FolderNames.CodeBuildIncludeFolder);
        var outSrcDir = Path.Combine(outDir, FolderNames.CodeBuildSrcFolder);
        var cMainPath = Path.Combine(outSrcDir, "main.c");

        // build src dir
        if (!Directory.Exists(outIncludeDir)) Directory.CreateDirectory(outIncludeDir);
        if (!Directory.Exists(outSrcDir)) Directory.CreateDirectory(outSrcDir);

        // pass to generators
        var generators = GameObject.FindObjectsOfType<GameObject>().SelectMany(x => x.GetComponents<ICodeGen>()).Where(x => x != null).OrderBy(x => x.CodeGenOrder).ToArray();
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
            .Replace("##CLEANUPBODY##", string.Join("\n", state.CleanupBody.Select(x => Indent(x, 1))))
            .Replace("##MAINBODYREADY##", string.Join("\n", state.MainBodyReady.Select(x => Indent(x, 2))))
            .Replace("##MAINBODY##", string.Join("\n", state.MainBody.Select(x => Indent(x, 1))))
            .Replace("##GETGUBERCASES##", string.Join("\n", state.GetGuberCase.Select(x => Indent(x, 2))))
            .Replace("##HANDLEEVENTCASES##", string.Join("\n", state.HandleGuberEventCase.Select(x => Indent(x, 2))))
            ;
        File.WriteAllText(cMainPath, cMainContent);

        // update linkfile
        var linkfilePath = Path.Combine(outDir, "linkfile");
        var linkfileContent = File.ReadAllText(linkfilePath)
            .Replace("##ADDRESS##", state.SeparateCodeFile ? "0x01B80000" : "0x01EF0000")
            ;
        File.WriteAllText(linkfilePath, linkfileContent);


        // update makefile
        var makefileInPath = Path.Combine(outDir, state.SeparateCodeFile ? "Makefile.code" : "Makefile");
        var makefileOutPath = Path.Combine(outDir, "Makefile");
        var makefileContent = File.ReadAllText(makefileInPath)
            .Replace("##EEOBJS##", string.Join(" ", state.ObjectFiles))
            .Replace("##EELDFLAGS##", string.Join("", state.LDFlags.Select(x => $"{x} \\\n\t")).Trim().TrimEnd('\\') + "\n")
            .Replace("##EEBUILD##", state.Debug ? "DEBUG" : "RELEASE")
            .Replace("##MAPNAME##", mapConfig.MapFilename)
            ;
        File.WriteAllText(makefileOutPath, makefileContent);

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
    public bool Debug { get; set; } = false;
    public bool SeparateCodeFile { get; set; } = false;

    // main.c
    public List<string> Includes { get; set; } = new List<string>();
    public List<string> Declarations { get; set; } = new List<string>();
    public List<string> Functions { get; set; } = new List<string>();
    public List<string> InitBody { get; set; } = new List<string>();
    public List<string> CleanupBody { get; set; } = new List<string>();
    public List<string> MainBodyReady { get; set; } = new List<string>();
    public List<string> MainBody { get; set; } = new List<string>();
    public List<string> HandleGuberEventCase { get; set; } = new List<string>();
    public List<string> GetGuberCase { get; set; } = new List<string>();

    // makefile
    public List<string> ObjectFiles { get; set; } = new List<string>();
    public List<string> LDFlags { get; set; } = new List<string>();

    // extra
    public Dictionary<string, List<string>> Meta { get; set; } = new Dictionary<string, List<string>>();
}
