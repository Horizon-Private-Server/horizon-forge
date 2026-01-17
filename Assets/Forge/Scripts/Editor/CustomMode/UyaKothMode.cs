using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public static class UyaKothMode
{
    public static readonly int HILL_OCLASS = 0x3000;

    [MenuItem("GameObject/Forge/UYA/King of the Hill/Create Hill Moby", priority = 10)]
    public static void CreateHill()
    {
        var go = new GameObject("Hill Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = HILL_OCLASS;
        moby.RCVersion = RCVER.UYA;
        moby.DrawDistance = 128;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 1f);
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    private static void OnAfterCreateGameObject(GameObject go)
    {
        // place under selected object
        // or try and spawn on top of scene camera
        if (Selection.activeGameObject)
            go.transform.SetParent(Selection.activeGameObject.transform, false);
        else if (SceneView.lastActiveSceneView.camera)
            go.transform.position = SceneView.lastActiveSceneView.camera.transform.position + (SceneView.lastActiveSceneView.camera.transform.forward * 5);

        Selection.activeGameObject = go;
    }

}
