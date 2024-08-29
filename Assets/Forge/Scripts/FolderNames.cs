using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class FolderNames
{
    public static readonly string TieFolder = "Tie";
    public static readonly string ShrubFolder = "Shrub";
    public static readonly string MobyFolder = "Moby";
    public static readonly string TfragFolder = "Tfrag";
    public static readonly string SkyFolder = "Sky";
    public static readonly string CodeFolder = "Code";
    public static readonly string SoundsFolder = "Sounds";
    public static readonly string HUDFolder = "HUD";

    public static readonly string TempFolder = ".forgeartifacts";

    public static readonly string BinaryFolder = "levels";
    public static readonly string BinaryAssetsFolder = "assets";
    public static readonly string BinarySoundsFolder = "sounds";
    public static readonly string BinaryCodeFolder = "code";
    public static readonly string BinaryGameplayFolder = "gameplay";
    public static readonly string BinaryMissionsFolder = "missions";
    public static readonly string BuildFolder = "build";
    public static readonly string CodeBuildFolder = "src";
    public static readonly string[] BuildFolders = new[] { "", "", "", "uya", "dl" };
    public static readonly string BinaryGameplayMobyFolder = $"{BinaryGameplayFolder}/moby";
    public static readonly string BinaryGameplaySplineFolder = $"{BinaryGameplayFolder}/spline";
    public static readonly string BinaryGameplayCuboidFolder = $"{BinaryGameplayFolder}/cuboid";
    public static readonly string BinaryGameplayCameraFolder = $"{BinaryGameplayFolder}/camera";
    public static readonly string BinaryGameplayAmbientSoundFolder = $"{BinaryGameplayFolder}/ambient_sound";
    public static readonly string BinaryGameplayAreaFile = $"{BinaryGameplayFolder}/116.bin";
    public static readonly string BinaryOcclusionFile = $"{BinaryAssetsFolder}/occlusion.bin";
    public static readonly string BinaryCollisionBinFile = $"{BinaryAssetsFolder}/collision.bin";
    public static readonly string BinaryCollisionColladaFile = $"{BinaryAssetsFolder}/collision.dae";
    public static readonly string BinaryCollisionAssetFile = $"{BinaryAssetsFolder}/collision.asset";
    public static readonly string BinarySkyFolder = $"{BinaryAssetsFolder}/sky";
    public static readonly string BinarySkyBinFile = $"{BinaryAssetsFolder}/sky.bin";
    public static readonly string BinarySkyMeshFile = $"{BinarySkyFolder}/mesh.glb";
    public static readonly string BinaryTerrainFolder = $"{BinaryAssetsFolder}/terrain";
    public static readonly string BinaryTerrainBinFile = $"{BinaryTerrainFolder}/terrain.bin";
    public static readonly string BinaryChunkTerrainBinFile = $"terrain.bin";
    public static readonly string BinaryTieFolder = $"{BinaryAssetsFolder}/tie";
    public static readonly string BinaryShrubFolder = $"{BinaryAssetsFolder}/shrub";
    public static readonly string BinaryMobyFolder = $"{BinaryAssetsFolder}/moby";
    public static readonly string PvarOverlayFile = "pvar_overlay.json";

    public static readonly string AssetsFolder = "Assets";
    public static readonly string MapsFolder = $"{AssetsFolder}/Maps";
    public static readonly string ResourcesFolder = $"{AssetsFolder}/Resources";
    public static readonly string ForgeFolder = $"{AssetsFolder}/Forge";
    public static readonly string BlenderScriptFolder = $"{ForgeFolder}/Blender";

    public static string GetLevelWadFilename(int levelId, int racVersion) => (racVersion == RCVER.DL) ? $"level{levelId}.1.wad" : $"level{levelId}.0.wad";
    public static string GetWorldWadFilename(int levelId, int racVersion) => (racVersion == RCVER.DL) ? null : $"level{levelId}.2.wad";
    public static string GetSoundWadFilename(int levelId, int racVersion) => (racVersion == RCVER.DL) ? $"level{levelId}.2.wad" : $"level{levelId}.1.wad";

    public static string GetGlobalPrefabFolder(string prefabType, int racVersion = 0)
    {
        if (prefabType == MobyFolder) return $"{ForgeFolder}/Prefabs/{GetMapMobyFolder(racVersion)}";
        return $"{ForgeFolder}/Prefabs/{prefabType}";
    }

    public static string GetGlobalAssetFolder(string assetType, int racVersion = 0)
    {
        if (assetType == MobyFolder) return $"{ResourcesFolder}/{GetMapMobyFolder(racVersion)}";
        return $"{ResourcesFolder}/{assetType}";
    }

    public static string GetLocalAssetFolder(string assetType, int racVersion = 0)
    {
        var sceneName = SceneManager.GetActiveScene().name;

        if (assetType == MobyFolder) return $"{GetMapFolder(sceneName)}/{GetMapMobyFolder(racVersion)}";
        return $"{GetMapFolder(sceneName)}/{assetType}";
    }

    public static string GetMapFolder(string map)
    {
        return $"{MapsFolder}/{map}";
    }

    public static string GetMapCodeFolder(int racVersion, GameRegion region)
    {
        return $"{CodeFolder}/rc{racVersion}-{region}";
    }

    public static string GetMapMobyFolder(int racVersion)
    {
        if (racVersion != RCVER.UYA && racVersion != RCVER.DL) throw new NotImplementedException();
        return $"{MobyFolder}/rc{racVersion}";
    }

    public static string GetMapBinFolder(string map, int racVersion)
    {
        return $"{BinaryFolder}/{map}/rc{racVersion}";
    }

    public static string GetMapCodeBuildFolder(string map)
    {
        return $"{BinaryFolder}/{map}/{CodeBuildFolder}";
    }

    public static string GetMapBuildFolder(string map)
    {
        return $"{BinaryFolder}/{map}/{BuildFolder}";
    }

    public static string GetMapBuildFolder(string map, int racVersion)
    {
        return $"{BinaryFolder}/{map}/{BuildFolder}/{BuildFolders[racVersion]}";
    }

    public static string GetScenePath(string map)
    {
        return $"{MapsFolder}/{map}/{map}.unity";
    }

    public static string GetWorldInstanceFolder(int racVersion)
    {
        if (racVersion == RCVER.DL) return "world-instances";
        if (racVersion == RCVER.UYA || racVersion == RCVER.GC) return BinaryGameplayFolder;

        throw new NotImplementedException();
    }

    public static string GetWorldInstanceTiesFolder(int racVersion)
    {
        return Path.Combine(GetWorldInstanceFolder(racVersion), "ties");
    }

    public static string GetWorldInstanceShrubsFolder(int racVersion)
    {
        return Path.Combine(GetWorldInstanceFolder(racVersion), "shrubs");
    }

    public static string GetWorldInstanceOcclusionFolder(int racVersion)
    {
        return Path.Combine(GetWorldInstanceFolder(racVersion), "occlusion");
    }

    public static string GetTempFolder()
    {
        //var path = Path.Combine(Path.GetTempPath(), TempFolder);
        var path = Path.GetFullPath(Path.Combine(TempFolder));
        if (!Directory.Exists(path))
        {
            DirectoryInfo di = Directory.CreateDirectory(path);
            di.Attributes = FileAttributes.Directory | FileAttributes.Hidden;
        }

        return path;
    }

    public static string GetAssetFolderNameFromOClass(int oclass)
    {
        return $"{oclass}_{oclass:X4}";
    }

    public static int GetOClassFromAssetFolderName(string name)
    {
        return int.Parse(name.Split('_')[0]);
    }

    public static int GetOClassFromIndexedFolderName(string name)
    {
        return int.Parse(name.Split('_')[1]);
    }
}
