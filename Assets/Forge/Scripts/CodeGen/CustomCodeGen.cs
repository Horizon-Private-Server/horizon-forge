using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public class CustomCodeGen : MonoBehaviour, ICodeGen
{
    public List<string> Defines = new List<string>();
    public List<UnityEngine.Object> Files = new List<UnityEngine.Object>();
    public string InitFunctionName = "customModuleInit";
    public string CleanupFunctionName = "customModuleCleanup";
    public string TickFunctionName = "customModuleTick";
    public bool WaitForClientsReady = false;
    public List<CodeGenMeta> Metas = new List<CodeGenMeta>();

    public bool IsEnabled => this.isActiveAndEnabled;
    public int CodeGenOrder => 10;

    public void Configure(string buildFolder, CodeGenState state)
    {
        foreach (var file in Files)
        {
            if (!file) continue;

            var assetPath = AssetDatabase.GetAssetPath(file);
            if (string.IsNullOrEmpty(assetPath)) continue;

            var text = File.ReadAllText(assetPath);
            var name = Path.GetFileName(assetPath);
            var nameNoExt = Path.GetFileNameWithoutExtension(name);
            switch (Path.GetExtension(name))
            {
                case ".h":
                    {
                        File.WriteAllText(Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder, name), text);
                        break;
                    }
                case ".c":
                    {
                        File.WriteAllText(Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder, name), text);
                        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/{nameNoExt}.o");
                        break;
                    }
                default:
                    {
                        File.WriteAllText(Path.Combine(buildFolder, name), text);
                        break;
                    }
            }
        }

        if (!string.IsNullOrEmpty(InitFunctionName))
        {
            state.Declarations.Add($"void {InitFunctionName}(void);");
            state.InitBody.Add($"{InitFunctionName}();");
        }

        if (!string.IsNullOrEmpty(CleanupFunctionName))
        {
            state.Declarations.Add($"void {CleanupFunctionName}(void);");
            state.CleanupBody.Add($"{CleanupFunctionName}();");
        }

        if (!string.IsNullOrEmpty(TickFunctionName))
        {
            state.Declarations.Add($"void {TickFunctionName}(void);");
            if (WaitForClientsReady)
                state.MainBodyReady.Add($"{TickFunctionName}();");
            else
                state.MainBody.Add($"{TickFunctionName}();");
        }

        foreach (var define in Defines)
            state.LDFlags.Add($"-D{define}");

        if (Metas != null)
        {
            foreach (var meta in Metas)
            {
                if (!state.Meta.TryGetValue(meta.Key, out var list))
                    state.Meta.Add(meta.Key, list = new List<string>());

                list.AddRange(meta.Values);
            }
        }
    }

    [Serializable]
    public struct CodeGenMeta
    {
        public string Key;
        public string[] Values;
    }
}
