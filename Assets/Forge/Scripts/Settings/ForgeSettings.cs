using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class ForgeSettings : ScriptableObject
{
    public static readonly string FORGE_SETTINGS_PATH = "Assets/ForgeSettings.asset";

    public string PathToCleanDeadlockedIso;
    public string PathToCleanUyaNtscIso;
    public string PathToCleanUyaPalIso;
    public string PathToCleanGcIso;

    public string PathToOutputDeadlockedIso;
    public string PathToOutputUyaNtscIso;
    public string PathToOutputUyaPalIso;

    public string[] DLBuildFolders;
    public string[] UYABuildFolders;

    public Color SelectionColor = new Color(0, 0, 1, 0.2f);

    public static ForgeSettings Load()
    {
        return AssetDatabase.LoadAssetAtPath<ForgeSettings>(ForgeSettings.FORGE_SETTINGS_PATH);
    }

    public string GetPathToCleanUyaIso() => string.IsNullOrEmpty(PathToCleanUyaNtscIso) ? PathToCleanUyaPalIso : PathToCleanUyaNtscIso;
}
