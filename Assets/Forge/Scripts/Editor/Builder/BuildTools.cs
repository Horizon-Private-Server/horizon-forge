using GLTFast.Export;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Unity.IO.LowLevel.Unsafe;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEditor.SearchService;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using UnityEngine.UIElements;

public static class BuildTools
{
    private static string ResolveScenePath(string sceneName)
    {
        if (string.IsNullOrWhiteSpace(sceneName))
            return null;

        if (sceneName.EndsWith(".unity", StringComparison.OrdinalIgnoreCase))
        {
            if (File.Exists(sceneName))
                return sceneName.Replace('\\', '/');

            return null;
        }

        var targetSceneName = Path.GetFileNameWithoutExtension(sceneName);
        var guids = AssetDatabase.FindAssets($"t:Scene {targetSceneName}");
        return guids
            .Select(AssetDatabase.GUIDToAssetPath)
            .FirstOrDefault(x => string.Equals(Path.GetFileNameWithoutExtension(x), targetSceneName, StringComparison.OrdinalIgnoreCase));
    }

    private static async Task<bool> OpenAndRebuildScene(string sceneName, string dlOut = null, string uyaOut = null)
    {
        var scenePath = ResolveScenePath(sceneName);
        if (string.IsNullOrWhiteSpace(scenePath))
        {
            Debug.LogError($"BuildLevel failed: could not resolve scene '{sceneName}'.");
            return false;
        }

        var scene = EditorSceneManager.OpenScene(scenePath, OpenSceneMode.Single);
        if (!scene.IsValid() || !scene.isLoaded)
        {
            Debug.LogError($"BuildLevel failed: unable to open scene '{scenePath}'.");
            return false;
        }

        Debug.Log($"BuildLevel starting for scene '{scene.name}' ({scenePath})...");

        if (!await ForgeBuilder.RebuildLevel(scene))
        {
            Debug.LogError($"BuildLevel failed during rebuild for '{scene.name}'.");
            return false;
        }

        ForgeBuilder.CopyToBuildFolders(scene);

        if (!string.IsNullOrWhiteSpace(dlOut) || !string.IsNullOrWhiteSpace(uyaOut))
            ForgeBuilder.CopyToBuildFolder(scene, dlOut, uyaOut);

        Debug.Log($"BuildLevel complete for scene '{scene.name}'.");
        return true;
    }

    private static bool OpenAndIncrementVersionForScene(string sceneName)
    {
        var scenePath = ResolveScenePath(sceneName);
        if (string.IsNullOrWhiteSpace(scenePath))
        {
            Debug.LogError($"OpenAndIncrementVersion failed: could not resolve scene '{sceneName}'.");
            return false;
        }

        var scene = EditorSceneManager.OpenScene(scenePath, OpenSceneMode.Single);
        if (!scene.IsValid() || !scene.isLoaded)
        {
            Debug.LogError($"OpenAndIncrementVersion failed: unable to open scene '{scenePath}'.");
            return false;
        }

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (!mapConfig)
        {
            Debug.LogError($"OpenAndIncrementVersion failed: scene '{scene.name}' has no MapConfig.");
            return false;
        }

        mapConfig.MapVersion++;
        EditorUtility.SetDirty(mapConfig);
        EditorSceneManager.MarkSceneDirty(scene);

        AssetDatabase.SaveAssets();
        if (!EditorSceneManager.SaveScene(scene))
        {
            Debug.LogError($"OpenAndIncrementVersion failed: unable to save scene '{scenePath}'.");
            return false;
        }

        Debug.Log($"OpenAndIncrementVersion complete. {scene.name} MapVersion => {mapConfig.MapVersion}");
        return true;
    }

    /// <summary>
    /// Opens one or more target scenes in the Unity Editor, runs a full Forge rebuild for each scene,
    /// and copies the resulting outputs to the configured build folders.
    ///
    /// Command-line invocation (Unity batch mode):
    /// -executeMethod BuildTools.OpenAndRebuildScenes -scene &lt;sceneNameOrPath&gt; [-scene &lt;sceneNameOrPath&gt; ...] [--dl-out &lt;path&gt;] [--uya-out &lt;path&gt;]
    /// -executeMethod BuildTools.OpenAndRebuildScenes -scenes &lt;sceneA,sceneB,sceneC&gt; [--dl-out &lt;path&gt;] [--uya-out &lt;path&gt;]
    ///
    /// Supported argument forms:
    /// - -scene &lt;value&gt; or -scene=&lt;value&gt; (repeatable)
    /// - -scenes &lt;csv&gt; or -scenes=&lt;csv&gt;
    /// - --dl-out &lt;value&gt; or --dl-out=&lt;value&gt;
    /// - --uya-out &lt;value&gt; or --uya-out=&lt;value&gt;
    /// </summary>
    public static async void OpenAndRebuildScenes()
    {
        var isBatchMode = Application.isBatchMode;

        try
        {
            var scenes = new List<string>();
            string dlOut = null;
            string uyaOut = null;
            var args = Environment.GetCommandLineArgs();

            void AddScenes(string value)
            {
                if (string.IsNullOrWhiteSpace(value))
                    return;

                foreach (var part in value.Split(new[] { ',', ';' }, StringSplitOptions.RemoveEmptyEntries))
                {
                    var trimmed = part.Trim();
                    if (!string.IsNullOrWhiteSpace(trimmed))
                        scenes.Add(trimmed);
                }
            }

            for (int i = 0; i < args.Length; ++i)
            {
                if (string.Equals(args[i], "-scene", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    AddScenes(args[i + 1]);
                    continue;
                }

                if (args[i].StartsWith("-scene=", StringComparison.OrdinalIgnoreCase))
                {
                    AddScenes(args[i].Substring("-scene=".Length));
                    continue;
                }

                if (string.Equals(args[i], "-scenes", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    AddScenes(args[i + 1]);
                    continue;
                }

                if (args[i].StartsWith("-scenes=", StringComparison.OrdinalIgnoreCase))
                {
                    AddScenes(args[i].Substring("-scenes=".Length));
                    continue;
                }

                if (string.Equals(args[i], "--dl-out", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    dlOut = args[i + 1];
                    continue;
                }

                if (args[i].StartsWith("--dl-out=", StringComparison.OrdinalIgnoreCase))
                {
                    dlOut = args[i].Substring("--dl-out=".Length);
                    continue;
                }

                if (string.Equals(args[i], "--uya-out", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    uyaOut = args[i + 1];
                    continue;
                }

                if (args[i].StartsWith("--uya-out=", StringComparison.OrdinalIgnoreCase))
                {
                    uyaOut = args[i].Substring("--uya-out=".Length);
                    continue;
                }
            }

            if (scenes.Count == 0)
            {
                Debug.LogError("BuildLevel failed: missing scenes. Pass one or more -scene <sceneNameOrPath> or -scenes <sceneA,sceneB,...>.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            var allSucceeded = true;
            foreach (var sceneName in scenes)
            {
                if (!await OpenAndRebuildScene(sceneName, dlOut, uyaOut))
                    allSucceeded = false;
            }

            if (isBatchMode) EditorApplication.Exit(allSucceeded ? 0 : 1);
        }
        catch (Exception ex)
        {
            Debug.LogError($"BuildLevel exception: {ex}");
            if (isBatchMode) EditorApplication.Exit(1);
        }
    }

    /// <summary>
    /// Opens one or more target scenes in the Unity Editor, increments MapConfig.MapVersion for each,
    /// marks and saves the scene/assets, then exits with an appropriate code in batch mode.
    ///
    /// Command-line invocation (Unity batch mode):
    /// -executeMethod BuildTools.OpenAndIncrementScenes -scene &lt;sceneNameOrPath&gt; [-scene &lt;sceneNameOrPath&gt; ...]
    /// -executeMethod BuildTools.OpenAndIncrementScenes -scenes &lt;sceneA,sceneB,sceneC&gt;
    /// Example:
    /// Unity.exe -batchmode -logFile - -projectPath "M:/Unity/horizon-forge" -executeMethod BuildTools.OpenAndIncrementScenes -scene "CrashSite"
    ///
    /// Supported argument forms:
    /// - -scene &lt;value&gt; or -scene=&lt;value&gt; (repeatable)
    /// - -scenes &lt;csv&gt; or -scenes=&lt;csv&gt;
    /// </summary>
    public static void OpenAndIncrementScenes()
    {
        var isBatchMode = Application.isBatchMode;

        try
        {
            var scenes = new List<string>();
            var args = Environment.GetCommandLineArgs();

            void AddScenes(string value)
            {
                if (string.IsNullOrWhiteSpace(value))
                    return;

                foreach (var part in value.Split(new[] { ',', ';' }, StringSplitOptions.RemoveEmptyEntries))
                {
                    var trimmed = part.Trim();
                    if (!string.IsNullOrWhiteSpace(trimmed))
                        scenes.Add(trimmed);
                }
            }

            for (int i = 0; i < args.Length; ++i)
            {
                if (string.Equals(args[i], "-scene", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    AddScenes(args[i + 1]);
                    continue;
                }

                if (args[i].StartsWith("-scene=", StringComparison.OrdinalIgnoreCase))
                {
                    AddScenes(args[i].Substring("-scene=".Length));
                    continue;
                }

                if (string.Equals(args[i], "-scenes", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    AddScenes(args[i + 1]);
                    continue;
                }

                if (args[i].StartsWith("-scenes=", StringComparison.OrdinalIgnoreCase))
                {
                    AddScenes(args[i].Substring("-scenes=".Length));
                    continue;
                }
            }

            if (scenes.Count == 0)
            {
                Debug.LogError("OpenAndIncrementVersion failed: missing scenes. Pass one or more -scene <sceneNameOrPath> or -scenes <sceneA,sceneB,...>.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            var allSucceeded = true;
            foreach (var sceneName in scenes)
            {
                if (!OpenAndIncrementVersionForScene(sceneName))
                    allSucceeded = false;
            }

            if (isBatchMode) EditorApplication.Exit(allSucceeded ? 0 : 1);
        }
        catch (Exception ex)
        {
            Debug.LogError($"OpenAndIncrementVersion exception: {ex}");
            if (isBatchMode) EditorApplication.Exit(1);
        }
    }
}
