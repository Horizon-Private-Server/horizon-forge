using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEditor.Overlays;
using UnityEngine;
using UnityEngine.UIElements;

[Overlay(typeof(SceneView), "Forge", true)]
public class ForgeSceneOverlay : Overlay
{
    public override VisualElement CreatePanelContent()
    {
        var root = new VisualElement() { name = "My Toolbar Root" };

        {
            var renderInstancedCollidersToggle = new Toggle("Render All Instanced Colliders");
            renderInstancedCollidersToggle.RegisterValueChangedCallback<bool>(e => { UpdateForceRenderAllCollisionHandles(e.newValue); });
            renderInstancedCollidersToggle.SetValueWithoutNotify(CollisionRenderHandle.ForceRenderAllCollisionHandles);
            root.Add(renderInstancedCollidersToggle);
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
}
