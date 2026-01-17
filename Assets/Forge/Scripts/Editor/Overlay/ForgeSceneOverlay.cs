using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Overlays;
using UnityEngine;
using UnityEngine.UIElements;

[Overlay(typeof(SceneView), "Forge", true)]
public class ForgeSceneOverlay : Overlay
{
    public StyleSheet OverlayStyles;

    public override VisualElement CreatePanelContent()
    {
        if (!OverlayStyles)
            OverlayStyles = AssetDatabase.LoadAssetAtPath<StyleSheet>(Path.Combine(FolderNames.ForgeFolder, "Scripts/Editor/Overlay/ForgeSceneOverlay.uss"));

        var root = new VisualElement() { name = "My Toolbar Root" };
        root.styleSheets.Add(OverlayStyles);

        {
            var renderInstancedCollidersToggle = new Toggle("Render All Instanced Colliders");
            renderInstancedCollidersToggle.AddToClassList("fill-space");
            renderInstancedCollidersToggle.RegisterValueChangedCallback<bool>(e => { UpdateForceRenderAllCollisionHandles(e.newValue); });
            renderInstancedCollidersToggle.SetValueWithoutNotify(CollisionRenderHandle.ForceRenderAllCollisionHandles);
            root.Add(renderInstancedCollidersToggle);
        }

        {
            var renderMapRenderLayersToggle = new Toggle("Render Map Render Layers");
            renderMapRenderLayersToggle.AddToClassList("fill-space");
            renderMapRenderLayersToggle.RegisterValueChangedCallback<bool>(e => { UpdateForceRenderAllMapRenderLayers(e.newValue); });
            renderMapRenderLayersToggle.SetValueWithoutNotify(MapRenderLayer.ForceRender);
            root.Add(renderMapRenderLayersToggle);
        }

        {
            var hideFogToggle = new Toggle("Hide Fog");
            hideFogToggle.AddToClassList("fill-space");
            hideFogToggle.RegisterValueChangedCallback<bool>(e => { UpdateHideFog(e.newValue); });
            hideFogToggle.SetValueWithoutNotify(MapConfig.HideFog);
            root.Add(hideFogToggle);
        }

        {
            var hideFogToggle = new Toggle("Show Moby Lines");
            hideFogToggle.AddToClassList("fill-space");
            hideFogToggle.RegisterValueChangedCallback<bool>(e => { UpdateShowMobyLines(e.newValue); });
            hideFogToggle.SetValueWithoutNotify(Moby.DrawPVarMobyLines);
            root.Add(hideFogToggle);
        }

        {
            var hideFogToggle = new Toggle("Show Moby/Tie Group Lines");
            hideFogToggle.AddToClassList("fill-space");
            hideFogToggle.RegisterValueChangedCallback<bool>(e => { UpdateShowMobyTieGroupIdLines(e.newValue); });
            hideFogToggle.SetValueWithoutNotify(Moby.DrawPVarMobyTieGroupIdLines);
            root.Add(hideFogToggle);
        }

        {
            var disableScenePickingToggle = new Toggle("Disable Scene Picking (speedup)");
            disableScenePickingToggle.AddToClassList("fill-space");
            disableScenePickingToggle.RegisterValueChangedCallback<bool>(e => { UpdateDisableScenePicking(e.newValue); });
            disableScenePickingToggle.SetValueWithoutNotify(AssetGizmosDrawer.Disabled);
            root.Add(disableScenePickingToggle);
        }

        return root;
    }

    private void UpdateForceRenderAllCollisionHandles(bool force)
    {
        CollisionRenderHandle.ForceRenderAllCollisionHandles = force;

        var handles = GameObject.FindObjectsOfType<GameObject>(includeInactive: false).SelectMany(x => x.GetComponentsInChildren<IInstancedCollider>()).Where(x => x != null && x.HasInstancedCollider()).Select(x => x.GetInstancedCollider());
        foreach (var handle in handles)
        {
            handle.UpdateMaterials();
        }
    }

    private void UpdateForceRenderAllMapRenderLayers(bool force)
    {
        MapRenderLayer.ForceRender = force;

        if (force)
            Shader.EnableKeyword("_MAPRENDER_SCENEVIEW");
        else
            Shader.DisableKeyword("_MAPRENDER_SCENEVIEW");

        var mapRender = GameObject.FindObjectOfType<MapRender>();
        if (mapRender) mapRender.UpdateCamera();

        var handles = HierarchicalSorting.Sort(GameObject.FindObjectsOfType<MapRenderLayer>(includeInactive: false));
        foreach (var handle in handles)
        {
            if (force)
                handle.OnPreMapRender();
            else
                handle.OnPostMapRender();
        }
    }

    private void UpdateHideFog(bool hide)
    {
        MapConfig.HideFog = hide;

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        if (mapConfig)
            mapConfig.UpdateShaderGlobals();
    }
    
    private void UpdateShowMobyTieGroupIdLines(bool show)
    {
        Moby.DrawPVarMobyTieGroupIdLines = show;
    }

    private void UpdateShowMobyLines(bool show)
    {
        Moby.DrawPVarMobyLines = show;
    }

    private void UpdateDisableScenePicking(bool disable)
    {
        AssetGizmosDrawer.Disabled = disable;
    }

}
