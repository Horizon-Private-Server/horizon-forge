using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[CustomEditor(typeof(MapConfig))]
public class MapConfigEditor : Editor
{
    private void OnEnable()
    {
    }

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        GUILayout.Space(20);
        if (GUILayout.Button("Open Build Folder"))
        {
            var path = FolderNames.GetMapBuildFolder(SceneManager.GetActiveScene().name) + "/";
            EditorUtility.RevealInFinder(path);
        }
    }
}
