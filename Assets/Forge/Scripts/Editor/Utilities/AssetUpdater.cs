using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;

public class AssetUpdater : Editor
{
    static List<IAsset> assets = new List<IAsset>();
    static List<IAsset> lastSelectedAssets = null;

    [InitializeOnLoadMethod]
    private static void OnInitialized()
    {
        SceneView.beforeSceneGui -= SceneView_duringSceneGui;
        SceneView.beforeSceneGui += SceneView_duringSceneGui;
        Selection.selectionChanged -= SelectionChanged;
        Selection.selectionChanged += SelectionChanged;
        SceneVisibilityManager.pickingChanged -= SceneVisibilityManager_pickingChanged;
        SceneVisibilityManager.pickingChanged += SceneVisibilityManager_pickingChanged;
        SceneVisibilityManager.visibilityChanged -= SceneVisibilityManager_visibilityChanged;
        SceneVisibilityManager.visibilityChanged += SceneVisibilityManager_visibilityChanged;

        SelectionChanged();

        assets = FindObjectsOfType<GameObject>().Select(x => x.GetComponent<IAsset>()).Where(x => x != null).ToList();
        assets.ForEach(x => x.UpdateAsset());
    }

    private static void SceneVisibilityManager_visibilityChanged()
    {
        // update assets that have had their visibility changed
        foreach (var asset in assets)
        {
            var isHidden = SceneVisibilityManager.instance.IsHidden(asset.GameObject);
            if (asset.IsHidden != isHidden)
                asset.UpdateAsset();
        }
    }

    private static void SceneVisibilityManager_pickingChanged()
    {

    }

    private static void SceneView_duringSceneGui(SceneView sceneView)
    {
        if (!sceneView.camera.GetComponent<SceneColorSetup>())
            sceneView.camera.gameObject.AddComponent<SceneColorSetup>();
    }

    private static void UpdateAllAssets()
    {
        foreach (var asset in assets)
            if (asset != null && asset.GameObject)
                asset.UpdateAsset();
    }

    private static void SelectionChanged()
    {
        var newlySelectedAssets = Selection.gameObjects?.Select(x => x.GetComponent<IAsset>())?.Where(x => x != null)?.ToList();

        if (lastSelectedAssets != null)
        {
            foreach (var asset in lastSelectedAssets)
                if (asset != null && asset.GameObject)
                    asset.UpdateAsset();
        }

        lastSelectedAssets = newlySelectedAssets;
        if (lastSelectedAssets != null)
        {
            foreach (var asset in lastSelectedAssets)
                if (asset != null && asset.GameObject)
                    asset.UpdateAsset();
        }
    }

    public static void RegisterAsset(IAsset asset)
    {
        if (!assets.Contains(asset))
        {
            assets.Add(asset);

            // add to selection if selected
            if (Selection.gameObjects.Any(go => go.GetComponent<IAsset>() == asset))
                lastSelectedAssets.Add(asset);
        }
    }

    public static void UnregisterAsset(IAsset asset)
    {
        assets.Remove(asset);

        // remove from selection (if selected)
        if (lastSelectedAssets != null) lastSelectedAssets.Remove(asset);
    }
}
