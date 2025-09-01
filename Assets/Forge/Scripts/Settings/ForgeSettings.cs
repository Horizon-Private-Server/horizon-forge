using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

public class ForgeSettings : ScriptableObject
{
    public static readonly string FORGE_SETTINGS_PATH = "Assets/ForgeSettings.asset";
    public static ForgeSettings Singleton { get; private set; }

    public int Version = 0;

    public string PathToCleanDeadlockedIso;
    public string PathToCleanUyaNtscIso;
    public string PathToCleanUyaPalIso;
    public string PathToCleanGcIso;

    public string PathToOutputDeadlockedIso;
    public string PathToOutputUyaNtscIso;
    public string PathToOutputUyaPalIso;

    public string PathToBlender;

    public string[] DLBuildFolders;
    public string[] UYABuildFolders;

    public Color SelectionColor = new Color(0, 0, 1, 0.2f);

    public static ForgeSettings Load()
    {
        return Singleton = AssetDatabase.LoadAssetAtPath<ForgeSettings>(ForgeSettings.FORGE_SETTINGS_PATH);
    }

    public string GetPathToCleanUyaIso() => string.IsNullOrEmpty(PathToCleanUyaNtscIso) ? PathToCleanUyaPalIso : PathToCleanUyaNtscIso;
}
