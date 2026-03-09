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
    /// <summary>
    /// Opens a target scene in the Unity Editor, runs a full Forge rebuild for that scene,
    /// and copies the resulting outputs to the configured build folders.
    ///
    /// Command-line invocation (Unity batch mode):
    /// -executeMethod BuildTools.OpenAndRebuild -scene &lt;sceneNameOrPath&gt; [--dl-out &lt;path&gt;] [--uya-out &lt;path&gt;]
    /// Example:
    /// Unity.exe -batchmode -logFile - -projectPath "M:/Unity/horizon-forge" -executeMethod BuildTools.OpenAndRebuild -scene "CrashSite" --dl-out "M:/build/dl" --uya-out "M:/build/uya"
    ///
    /// Supported argument forms:
    /// - -scene &lt;value&gt; or -scene=&lt;value&gt;
    /// - --dl-out &lt;value&gt; or --dl-out=&lt;value&gt;
    /// - --uya-out &lt;value&gt; or --uya-out=&lt;value&gt;
    /// </summary>
    public static async void OpenAndRebuild()
    {
        var isBatchMode = Application.isBatchMode;

        try
        {
            // Support:
            //   -scene <nameOrPath> or -scene=<nameOrPath>
            //   --dl-out <path> or --dl-out=<path>
            //   --uya-out <path> or --uya-out=<path>
			var sceneName = "";
			string dlOut = null;
			string uyaOut = null;
			var args = Environment.GetCommandLineArgs();
			for (int i = 0; i < args.Length; ++i)
			{
				if (string.Equals(args[i], "-scene", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
				{
					sceneName = args[i + 1];
					continue;
				}

				if (args[i].StartsWith("-scene=", StringComparison.OrdinalIgnoreCase))
				{
					sceneName = args[i].Substring("-scene=".Length);
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

            if (string.IsNullOrWhiteSpace(sceneName))
            {
                Debug.LogError("BuildLevel failed: missing scene. Pass -scene <sceneNameOrPath>.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            // Resolve scene path from either a direct .unity path or bare scene name.
            string scenePath = null;
            if (sceneName.EndsWith(".unity", StringComparison.OrdinalIgnoreCase))
            {
                if (File.Exists(sceneName))
                    scenePath = sceneName.Replace('\\', '/');
            }
            else
            {
                var targetSceneName = Path.GetFileNameWithoutExtension(sceneName);
                var guids = AssetDatabase.FindAssets($"t:Scene {targetSceneName}");
                scenePath = guids
                    .Select(AssetDatabase.GUIDToAssetPath)
                    .FirstOrDefault(x => string.Equals(Path.GetFileNameWithoutExtension(x), targetSceneName, StringComparison.OrdinalIgnoreCase));
            }

            if (string.IsNullOrWhiteSpace(scenePath))
            {
                Debug.LogError($"BuildLevel failed: could not resolve scene '{sceneName}'.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            var scene = EditorSceneManager.OpenScene(scenePath, OpenSceneMode.Single);
            if (!scene.IsValid() || !scene.isLoaded)
            {
                Debug.LogError($"BuildLevel failed: unable to open scene '{scenePath}'.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            Debug.Log($"BuildLevel starting for scene '{scene.name}' ({scenePath})...");

            if (!await ForgeBuilder.RebuildLevel(scene))
            {
                Debug.LogError("BuildLevel failed during rebuild.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            ForgeBuilder.CopyToBuildFolders(scene);

			if (!string.IsNullOrWhiteSpace(dlOut) || !string.IsNullOrWhiteSpace(uyaOut))
				ForgeBuilder.CopyToBuildFolder(scene, dlOut, uyaOut);

            Debug.Log("BuildLevel complete.");
            if (isBatchMode) EditorApplication.Exit(0);
        }
        catch (Exception ex)
        {
            Debug.LogError($"BuildLevel exception: {ex}");
            if (isBatchMode) EditorApplication.Exit(1);
        }
    }

    /// <summary>
    /// Opens a target scene in the Unity Editor, increments MapConfig.MapVersion,
    /// marks and saves the scene/assets, then exits with an appropriate code in batch mode.
    ///
    /// Command-line invocation (Unity batch mode):
    /// -executeMethod BuildTools.OpenAndIncrementVersion -scene &lt;sceneNameOrPath&gt;
    /// Example:
    /// Unity.exe -batchmode -logFile - -projectPath "M:/Unity/horizon-forge" -executeMethod BuildTools.OpenAndIncrementVersion -scene "CrashSite"
    ///
    /// Supported argument forms:
    /// - -scene &lt;value&gt; or -scene=&lt;value&gt;
    /// </summary>
    public static void OpenAndIncrementVersion()
    {
        var isBatchMode = Application.isBatchMode;

        try
        {
            // Support:
            //   -scene <nameOrPath> or -scene=<nameOrPath>
            var sceneName = "";
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length; ++i)
            {
                if (string.Equals(args[i], "-scene", StringComparison.OrdinalIgnoreCase) && (i + 1) < args.Length)
                {
                    sceneName = args[i + 1];
                    continue;
                }

                if (args[i].StartsWith("-scene=", StringComparison.OrdinalIgnoreCase))
                {
                    sceneName = args[i].Substring("-scene=".Length);
                    continue;
                }
            }

            if (string.IsNullOrWhiteSpace(sceneName))
            {
                Debug.LogError("OpenAndIncrementVersion failed: missing scene. Pass -scene <sceneNameOrPath>.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            // Resolve scene path from either a direct .unity path or bare scene name.
            string scenePath = null;
            if (sceneName.EndsWith(".unity", StringComparison.OrdinalIgnoreCase))
            {
                if (File.Exists(sceneName))
                    scenePath = sceneName.Replace('\\', '/');
            }
            else
            {
                var targetSceneName = Path.GetFileNameWithoutExtension(sceneName);
                var guids = AssetDatabase.FindAssets($"t:Scene {targetSceneName}");
                scenePath = guids
                    .Select(AssetDatabase.GUIDToAssetPath)
                    .FirstOrDefault(x => string.Equals(Path.GetFileNameWithoutExtension(x), targetSceneName, StringComparison.OrdinalIgnoreCase));
            }

            if (string.IsNullOrWhiteSpace(scenePath))
            {
                Debug.LogError($"OpenAndIncrementVersion failed: could not resolve scene '{sceneName}'.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            var scene = EditorSceneManager.OpenScene(scenePath, OpenSceneMode.Single);
            if (!scene.IsValid() || !scene.isLoaded)
            {
                Debug.LogError($"OpenAndIncrementVersion failed: unable to open scene '{scenePath}'.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            var mapConfig = GameObject.FindObjectOfType<MapConfig>();
            if (!mapConfig)
            {
                Debug.LogError($"OpenAndIncrementVersion failed: scene '{scene.name}' has no MapConfig.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            mapConfig.MapVersion++;
            EditorUtility.SetDirty(mapConfig);
            EditorSceneManager.MarkSceneDirty(scene);

            AssetDatabase.SaveAssets();
            if (!EditorSceneManager.SaveScene(scene))
            {
                Debug.LogError($"OpenAndIncrementVersion failed: unable to save scene '{scenePath}'.");
                if (isBatchMode) EditorApplication.Exit(1);
                return;
            }

            Debug.Log($"OpenAndIncrementVersion complete. {scene.name} MapVersion => {mapConfig.MapVersion}");
            if (isBatchMode) EditorApplication.Exit(0);
        }
        catch (Exception ex)
        {
            Debug.LogError($"OpenAndIncrementVersion exception: {ex}");
            if (isBatchMode) EditorApplication.Exit(1);
        }
    }
}
