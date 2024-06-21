using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

[InitializeOnLoad]
static class ForgeSettingsChecker
{
    static ForgeSettingsChecker()
    {
        EditorApplication.update -= update;
        EditorApplication.update += update;
    }

    private static void update()
    {
        EditorApplication.update -= update;

        // check for the existent of ForgeSettings scriptableobject
        var forgeSettings = ForgeSettings.Load();
        if (!forgeSettings)
        {
            // prompt user to create
            ForgeStartupWindow.CreateNewWindow();
        }
    }
}
