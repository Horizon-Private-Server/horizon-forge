using System;
using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

[CustomEditor(typeof(ForgeSettings))]
public class ForgeSettingsEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var forgeSettings = target as ForgeSettings;

        GUILayout.Label("Clean ISO Paths");
        EditorGUI.BeginDisabledGroup(true);
        CreateBrowseFileGUI("Clean Deadlocked (NTSC) Iso", forgeSettings.PathToCleanDeadlockedIso);
        CreateBrowseFileGUI("Clean UYA (NTSC) Iso", forgeSettings.PathToCleanUyaNtscIso);
        CreateBrowseFileGUI("Clean R&C3 (PAL) Iso", forgeSettings.PathToCleanUyaPalIso);
        CreateBrowseFileGUI("Clean GC (NTSC) Iso", forgeSettings.PathToCleanGcIso);
        EditorGUI.EndDisabledGroup();

        if (GUILayout.Button("Configure in Startup Window"))
        {
            ForgeStartupWindow.CreateNewWindow();
        }

        GUILayout.Space(20);
        GUILayout.Label("Output ISO Paths");
        forgeSettings.PathToOutputDeadlockedIso = CreateBrowseFileGUI("Output Deadlocked Iso", forgeSettings.PathToOutputDeadlockedIso);
        //forgeSettings.PathToOutputUYAISO = CreateBrowseFileGUI("Output UYA Iso", forgeSettings.PathToOutputUYAISO);

        GUILayout.Space(20);
        GUILayout.Label("Miscellaneous");
        forgeSettings.SelectionColor = EditorGUILayout.ColorField("Selection Color", forgeSettings.SelectionColor);
        Shader.SetGlobalColor("_FORGE_SELECTION_COLOR", forgeSettings.SelectionColor);

        // init build folders
        if (forgeSettings.DLBuildFolders == null)
            forgeSettings.DLBuildFolders = new string[0];

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

        //base.OnInspectorGUI();
    }

    string CreateBrowseFileGUI(string title, string path)
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
            newPath = EditorUtility.OpenFilePanelWithFilters(title, newPath, new string[] { "ISO", "iso" });
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
