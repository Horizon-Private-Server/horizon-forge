using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(ForgeSettings))]
public class ForgeSettingsEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var isoExts = new string[] { "iso", "ISO" };
        var forgeSettings = target as ForgeSettings;

        EditorGUI.BeginChangeCheck();
        GUILayout.Label("Blender");
        var blenderTitle = "Blender executable path";
        string[] blenderExt = null;
#if UNITY_STANDALONE_WIN
        blenderExt = new string[] { "Exe", "exe" };
        blenderTitle = "Blender executable path (optional)";
#endif
        CreateBrowseFileGUI(blenderTitle, forgeSettings.PathToBlender, blenderExt);

        GUILayout.Label("Clean ISO Paths");
        EditorGUI.BeginDisabledGroup(true);
        CreateBrowseFileGUI("Clean Deadlocked (NTSC) Iso", forgeSettings.PathToCleanDeadlockedIso, isoExts);
        CreateBrowseFileGUI("Clean UYA (NTSC) Iso", forgeSettings.PathToCleanUyaNtscIso, isoExts);
        CreateBrowseFileGUI("Clean R&C3 (PAL) Iso", forgeSettings.PathToCleanUyaPalIso, isoExts);
        CreateBrowseFileGUI("Clean GC (NTSC) Iso", forgeSettings.PathToCleanGcIso, isoExts);
        EditorGUI.EndDisabledGroup();

        if (GUILayout.Button("Configure in Startup Window"))
        {
            ForgeStartupWindow.CreateNewWindow();
        }

        GUILayout.Space(20);
        GUILayout.Label("Output ISO Paths");
        forgeSettings.PathToOutputDeadlockedIso = CreateSaveFileGUI("Output Deadlocked Iso", forgeSettings.PathToOutputDeadlockedIso);
        forgeSettings.PathToOutputUyaNtscIso = CreateSaveFileGUI("Output UYA NTSC Iso", forgeSettings.PathToOutputUyaNtscIso);
        forgeSettings.PathToOutputUyaPalIso = CreateSaveFileGUI("Output UYA PAL Iso", forgeSettings.PathToOutputUyaPalIso);

        GUILayout.Space(20);
        GUILayout.Label("Miscellaneous");
        forgeSettings.SelectionColor = EditorGUILayout.ColorField("Selection Color", forgeSettings.SelectionColor);
        Shader.SetGlobalColor("_FORGE_SELECTION_COLOR", forgeSettings.SelectionColor);

        // init build folders
        if (forgeSettings.DLBuildFolders == null)
            forgeSettings.DLBuildFolders = new string[0];
        if (forgeSettings.UYABuildFolders == null)
            forgeSettings.UYABuildFolders = new string[0];

        GUILayout.Space(20);
        GUILayout.Label("Deadlocked Build Folders");
        for (int i = 0; i < forgeSettings.DLBuildFolders.Length; i++)
        {
            GUILayout.BeginHorizontal();

            forgeSettings.DLBuildFolders[i] = CreateBrowseFolderGUI("", forgeSettings.DLBuildFolders[i], 30);

            if (GUILayout.Button("-"))
            {
                Undo.RecordObject(forgeSettings, "Remove DL Build Folder");
                var newFolders = new string[forgeSettings.DLBuildFolders.Length - 1];
                Array.Copy(forgeSettings.DLBuildFolders, 0, newFolders, 0, i);
                Array.Copy(forgeSettings.DLBuildFolders, i+1, newFolders, i, forgeSettings.DLBuildFolders.Length - i - 1);
                forgeSettings.DLBuildFolders = newFolders;
            }

            GUILayout.EndHorizontal();
        }

        GUILayout.BeginHorizontal();
        if (GUILayout.Button("+"))
        {
            Undo.RecordObject(forgeSettings, "Add DL Build Folder");
            Array.Resize(ref forgeSettings.DLBuildFolders, forgeSettings.DLBuildFolders.Length + 1);
        }
        GUILayout.EndHorizontal();

        GUILayout.Space(20);
        GUILayout.Label("UYA Build Folders");
        for (int i = 0; i < forgeSettings.UYABuildFolders.Length; i++)
        {
            GUILayout.BeginHorizontal();

            forgeSettings.UYABuildFolders[i] = CreateBrowseFolderGUI("", forgeSettings.UYABuildFolders[i], 30);

            if (GUILayout.Button("-"))
            {
                Undo.RecordObject(forgeSettings, "Remove UYA Build Folder");
                var newFolders = new string[forgeSettings.UYABuildFolders.Length - 1];
                Array.Copy(forgeSettings.UYABuildFolders, 0, newFolders, 0, i);
                Array.Copy(forgeSettings.UYABuildFolders, i + 1, newFolders, i, forgeSettings.UYABuildFolders.Length - i - 1);
                forgeSettings.UYABuildFolders = newFolders;
            }

            GUILayout.EndHorizontal();
        }

        GUILayout.BeginHorizontal();
        if (GUILayout.Button("+"))
        {
            Undo.RecordObject(forgeSettings, "Add UYA Build Folder");
            Array.Resize(ref forgeSettings.UYABuildFolders, forgeSettings.UYABuildFolders.Length + 1);
        }
        GUILayout.EndHorizontal();
        
        if (EditorGUI.EndChangeCheck())
        {
            EditorUtility.SetDirty(forgeSettings);
            //AssetDatabase.SaveAssetIfDirty(forgeSettings);
        }

        //base.OnInspectorGUI();
    }

    string CreateSaveFileGUI(string title, string path)
    {
        var width = 130;

        GUILayout.BeginHorizontal();

        // label
        if (!String.IsNullOrEmpty(title))
        {
            GUILayout.Label(title, GUILayout.Width(200));
            width += 200;
        }

        // path
        var newPath = GUILayout.TextField(path, GUILayout.Width(Screen.width - width));

        // browse
        var openFileDialog = GUILayout.Button("Browse", GUILayout.Width(100));

        GUILayout.EndHorizontal();

        if (openFileDialog)
        {
            var dir = string.IsNullOrEmpty(newPath) ? "" : Path.GetDirectoryName(newPath);
            var filename = string.IsNullOrEmpty(newPath) ? "" : Path.GetFileName(newPath);

            var openPath = EditorUtility.SaveFilePanel(title, dir, filename, "iso");
            if (!string.IsNullOrEmpty(openPath))
                newPath = openPath;
        }

        if (newPath != path) EditorUtility.SetDirty(target);

        return newPath;
    }

    string CreateBrowseFileGUI(string title, string path, params string[] extensions)
    {
        var width = 130;

        GUILayout.BeginHorizontal();

        // label
        if (!String.IsNullOrEmpty(title))
        {
            GUILayout.Label(title, GUILayout.Width(200));
            width += 200;
        }

        // path
        var newPath = GUILayout.TextField(path, GUILayout.Width(Screen.width - width));

        // browse
        var openFileDialog = GUILayout.Button("Browse", GUILayout.Width(100));

        GUILayout.EndHorizontal();

        if (openFileDialog)
        {
            string openPath = null;
            if (extensions == null)
                openPath = EditorUtility.OpenFilePanel(title, newPath, "");
            else
                openPath = EditorUtility.OpenFilePanelWithFilters(title, newPath, extensions);
                
            if (!string.IsNullOrEmpty(openPath)) newPath = openPath;
        }

        if (newPath != path) EditorUtility.SetDirty(target);

        return newPath;
    }

    string CreateBrowseFolderGUI(string title, string path, int width = 0)
    {
        width += 130;

        GUILayout.BeginHorizontal();

        // label
        if (!String.IsNullOrEmpty(title))
        {
            GUILayout.Label(title, GUILayout.Width(200));
            width += 200;
        }

        // path
        var newPath = GUILayout.TextField(path, GUILayout.Width(Screen.width - width));

        // browse
        var openFileDialog = GUILayout.Button("Browse", GUILayout.Width(100));

        GUILayout.EndHorizontal();

        if (openFileDialog)
        {
            newPath = EditorUtility.OpenFolderPanel(title, newPath, "");
        }

        if (newPath != path) EditorUtility.SetDirty(target);

        return newPath;
    }
}
