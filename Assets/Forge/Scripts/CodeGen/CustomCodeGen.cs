using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public class CustomCodeGen : MonoBehaviour, ICodeGen
{
    public List<string> Defines;
    public List<TextAsset> Files;
    public string InitFunctionName = "customModuleInit";
    public string TickFunctionName = "customModuleTick";
    public bool WaitForClientsReady = false;

    public bool IsEnabled => this.isActiveAndEnabled;

    public void Configure(string buildFolder, CodeGenState state)
    {
        foreach (var file in Files)
        {
            if (!file) continue;

            var name = file.name;
            var nameNoExt = Path.GetFileNameWithoutExtension(name);
            switch (Path.GetExtension(name))
            {
                case ".h":
                    {
                        File.WriteAllText(Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder, name), file.text);
                        break;
                    }
                case ".c":
                    {
                        File.WriteAllText(Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder, name), file.text);
                        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/{nameNoExt}.o");
                        break;
                    }
                default:
                    {
                        File.WriteAllText(Path.Combine(buildFolder, name), file.text);
                        break;
                    }
            }
        }

        if (!string.IsNullOrEmpty(InitFunctionName))
        {
            state.Declarations.Add($"void {InitFunctionName}(void);");
            state.InitBody.Add($"{InitFunctionName}();");
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
    }
}
