using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public static class ForgeUpgradeScripts
{
    public const int FORGE_UPGRADE_VERSION = 1;

    [InitializeOnLoadMethod]
    public static void CheckForUpgrade()
    {
        var forgeSettings = ForgeSettings.Load();
        if (!forgeSettings) return;

        if (forgeSettings.Version >= FORGE_UPGRADE_VERSION) return;

        int version = forgeSettings.Version;
        while (version < FORGE_UPGRADE_VERSION)
        {
            RunMigration(version + 1);
            ++version;
        }

        forgeSettings.Version = version;
        EditorUtility.SetDirty(forgeSettings);
        AssetDatabase.SaveAssetIfDirty(forgeSettings);
    }

    private static void RunMigration(int version)
    {
        switch (version)
        {
            case 1: // Move global Resources/Mobys/ into Resources/Mobys/rc4
                {
                    var srcMobyDir = $"{FolderNames.ResourcesFolder}/{FolderNames.MobyFolder}";
                    var dstMobyDir = $"{FolderNames.GetGlobalAssetFolder(FolderNames.MobyFolder, RCVER.DL)}";

                    // check if moby folder has mobys
                    var dirsToMove = new List<string>();
                    var subdirs = Directory.EnumerateDirectories(srcMobyDir);
                    foreach (var subdir in subdirs)
                    {
                        // moby folder is a whole number (base 10)
                        var subdirName = Path.GetFileName(subdir);
                        if (int.TryParse(subdirName, out _))
                        {
                            dirsToMove.Add(Path.GetFullPath(subdir));
                        }
                    }

                    // prompt user and migrate data
                    if (dirsToMove.Any())
                    {
                        if (!EditorUtility.DisplayDialog("Forge Upgrade", $"Forge needs to migrate {dirsToMove.Count} mobys in {srcMobyDir} to {dstMobyDir}.\n", "Okay"))
                            throw new InvalidOperationException();

                        // migrate
                        if (!Directory.Exists(dstMobyDir)) Directory.CreateDirectory(dstMobyDir);
                        foreach (var dir in dirsToMove)
                        {
                            IOHelper.CopyDirectory(dir, Path.Combine(dstMobyDir, Path.GetFileName(dir)));
                            Directory.Delete(dir, true);
                            File.Delete(dir + ".meta");
                        }

                        // refresh assets
                        AssetDatabase.Refresh();
                        var assets = GameObject.FindObjectsOfType<MonoBehaviour>().Where(x => x is IAsset).Select(x => x as IAsset);
                        foreach (var asset in assets)
                            asset.UpdateAsset();
                    }
                    break;
                }
        }
    }
}

