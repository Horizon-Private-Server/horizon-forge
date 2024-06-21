using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEngine;

public static class PackerHelper
{
    public enum PACKER_STATUS_CODES
    {
        SUCCESS = 0,

        // IO
        MISSING_DIR = -1,
        MISSING_FILE = -2,

        // Unpacking
        UNPACK_ASSET_FAILURE = -10,
        UNPACK_ASSET_FAILURE_MOBY = -11,
        UNPACK_ASSET_FAILURE_TIE = -12,
        UNPACK_ASSET_FAILURE_SHRUB = -13,
        UNPACK_ASSET_FAILURE_TERRAIN = -14,
        UNPACK_ASSET_FAILURE_SKYBOX = -15,
        UNPACK_ASSET_FAILURE_PARTICLE_TEX = -16,
        UNPACK_ASSET_FAILURE_FX_TEX = -17,
        UNPACK_ASSET_FAILURE_COLLISION = -18,
        UNPACK_ASSET_FAILURE_SOUND_REMAP = -19,
        UNPACK_ASSET_FAILURE_OCCLUSION = -20,
        UNPACK_ASSET_FAILURE_CHROME = -21,
        UNPACK_ASSET_FAILURE_GLASS = -22,
        UNPACK_ASSET_FAILURE_PARTICLE_DEF = -23,
        UNPACK_ASSET_FAILURE_MOBY_GS_STASH_LIST = -24,
        UNPACK_ASSET_FAILURE_MOBY_SOUND_REMAP = -25,
        UNPACK_ASSET_FAILURE_LIGHT_CUBOIDS = -26, // same offset in header
        UNPACK_ASSET_FAILURE_RATCHET_ANIMS = -26, // same offset in header
        UNPACK_ASSET_FAILURE_DECOMPRESS = -27,

        // Packing
        READ_ASSET_FAILURE = -50,
        READ_ASSET_FAILURE_MOBY = -51,
        READ_ASSET_FAILURE_TIE = -52,
        READ_ASSET_FAILURE_SHRUB = -53,
        READ_ASSET_FAILURE_TERRAIN = -54,
        READ_ASSET_FAILURE_SKYBOX = -55,
        READ_ASSET_FAILURE_PARTICLE_TEX = -56,
        READ_ASSET_FAILURE_FX_TEX = -57,
        READ_ASSET_FAILURE_COLLISION = -58,
        READ_ASSET_FAILURE_SOUND_REMAP = -59,
        READ_ASSET_FAILURE_OCCLUSION = -60,
        READ_ASSET_FAILURE_CHROME = -61,
        READ_ASSET_FAILURE_GLASS = -62,
        READ_ASSET_FAILURE_PARTICLE_DEF = -63,
        READ_ASSET_FAILURE_MOBY_GS_STASH_LIST = -64,
        READ_ASSET_FAILURE_TEXTURES = -65,

        PACK_ASSET_FAILURE = -80,
        PACK_ASSET_FAILURE_MOBY = -81,
        PACK_ASSET_FAILURE_TIE = -82,
        PACK_ASSET_FAILURE_SHRUB = -83,
        PACK_ASSET_FAILURE_TERRAIN = -84,
        PACK_ASSET_FAILURE_SKYBOX = -85,
        PACK_ASSET_FAILURE_PARTICLE_TEX = -86,
        PACK_ASSET_FAILURE_FX_TEX = -87,
        PACK_ASSET_FAILURE_COLLISION = -88,
        PACK_ASSET_FAILURE_SOUND_REMAP = -89,
        PACK_ASSET_FAILURE_OCCLUSION = -90,
        PACK_ASSET_FAILURE_CHROME = -91,
        PACK_ASSET_FAILURE_GLASS = -92,
        PACK_ASSET_FAILURE_PARTICLE_DEF = -93,
        PACK_ASSET_FAILURE_MOBY_GS_STASH_LIST = -94,
        PACK_ASSET_FAILURE_TEXTURES = -95,
        PACK_ASSET_FAILURE_FX_DEF = -96,
        PACK_ASSET_FAILURE_MOBY_SOUND_REMAP = -97,
        PACK_ASSET_FAILURE_LIGHT_CUBOIDS = -98, // same offset in header
        PACK_ASSET_FAILURE_RATCHET_ANIMS = -98, // same offset in header
        PACK_ASSET_FAILURE_MIPMAP_DEF = -99,


        PACK_ASSET_FAILURE_TODO = -99,

        WRITE_ASSET_FAILURE = -110,
        WRITE_ASSET_FAILURE_MOBY = -111,
        WRITE_ASSET_FAILURE_TIE = -112,
        WRITE_ASSET_FAILURE_SHRUB = -113,
        WRITE_ASSET_FAILURE_TERRAIN = -114,
        WRITE_ASSET_FAILURE_SKYBOX = -115,
        WRITE_ASSET_FAILURE_PARTICLE_TEX = -116,
        WRITE_ASSET_FAILURE_FX_TEX = -117,
        WRITE_ASSET_FAILURE_COLLISION = -118,
        WRITE_ASSET_FAILURE_SOUND_REMAP = -119,
        WRITE_ASSET_FAILURE_OCCLUSION = -120,
        WRITE_ASSET_FAILURE_CHROME = -121,
        WRITE_ASSET_FAILURE_GLASS = -122,
        WRITE_ASSET_FAILURE_PARTICLE_DEF = -123,
        WRITE_ASSET_FAILURE_MOBY_GS_STASH_LIST = -124,
        WRITE_ASSET_FAILURE_TEXTURES = -125,
        WRITE_ASSET_FAILURE_FX_DEF = -126,
        WRITE_ASSET_FAILURE_LIGHT_CUBOIDS = -127, // same offset in header
        WRITE_ASSET_FAILURE_RATCHET_ANIMS = -127, // same offset in header
        WRITE_ASSET_FAILURE_MIPMAP_DEF = -99,


        // Unsupported operations
        UNSUPPORTED_RAC1 = -200,
        UNSUPPORTED_RAC_UNKNOWN = -201,
        UNSUPPORTED_OP = -202,
        UNSUPPORTED_NOT_IMPLEMENTED = -203,

        // Command line parser errors
        COMMAND_LINE_PARSER_FAILED = -300,
    }

    [Flags]
    public enum PACKER_PACK_OPS
    {
        PACK_WORLD_INSTANCES = 1,
        PACK_OCCLUSION = 2,
        PACK_CODE = 4,
        PACK_GAMEPLAY = 8,
        PACK_ASSETS = 16,
        PACK_LEVEL_WAD = 32,
        PACK_SOUND_WAD = 64,
    }

    public static bool IsInstalled()
    {
        var packerPath = Path.Combine("tools", "packer", "DL.Level.exe");
        return File.Exists(packerPath);
    }

    public static bool CanRun()
    {
        return RunPacker(null, true, out _) == PACKER_STATUS_CODES.COMMAND_LINE_PARSER_FAILED;
    }

    public static PACKER_STATUS_CODES RunPacker(string[] args, bool silent, out string output)
    {
        output = null;
        string consoleData = "";

        var packerPath = Path.Combine("tools", "packer", "DL.Level.exe");
        if (!File.Exists(packerPath))
        {
            throw new System.Exception("Packer not found in tools directory!");
        }

        var processArgs = args == null ? "" : string.Join(" ", args.Select(x => "\"" + x.Replace("\\", "/") + "\""));
        var startInfo = new System.Diagnostics.ProcessStartInfo(Path.GetFullPath(packerPath), processArgs)
        {
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            RedirectStandardInput = true,
            UseShellExecute = false,
        };

        var p = new System.Diagnostics.Process() { StartInfo = startInfo };
        p.OutputDataReceived += (s, e) => { consoleData += e.Data + "\n"; };
        p.ErrorDataReceived += (s, e) => { consoleData += e.Data + "\n"; };
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();

        output = consoleData;
        if (p.ExitCode < 0 && !silent)
            Debug.Log(output);
        return (PACKER_STATUS_CODES)p.ExitCode;
    }

    public static PACKER_STATUS_CODES RunPacker(out string output, params string[] args)
    {
        return RunPacker(args, false, out output);
    }

    public static int RunWAD2(out string output, params string[] args)
    {
        output = null;
        string consoleData = "";

        var exePath = Path.Combine("tools", "packer", "static", "wad2.exe");
        if (!File.Exists(exePath))
        {
            throw new System.Exception("wad2 packer not found in tools directory!");
        }

        var processArgs = string.Join(" ", args.Select(x => "\"" + x.Replace("\\", "/") + "\""));
        var startInfo = new System.Diagnostics.ProcessStartInfo(Path.GetFullPath(exePath), processArgs)
        {
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            RedirectStandardInput = true,
            UseShellExecute = false,
        };

        var p = new System.Diagnostics.Process() { StartInfo = startInfo };
        p.OutputDataReceived += (s, e) => { consoleData += e.Data + "\n"; };
        p.ErrorDataReceived += (s, e) => { consoleData += e.Data + "\n"; };
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();

        output = consoleData;
        if (p.ExitCode < 0)
            Debug.Log(output);
        return p.ExitCode;
    }

    public static PACKER_STATUS_CODES ExtractLevelWads(string isoPath, string destFolder, int levelId, int racVersion)
    {
        return RunPacker(out _, "extract", "-i", isoPath, "-l", levelId.ToString(), "-o", destFolder, "-v", racVersion.ToString());
    }

    public static PACKER_STATUS_CODES ExtractMinimap(string isoPath, string outPath, int levelId, int racVersion)
    {
        var dir = Path.GetDirectoryName(outPath);
        if (!Directory.Exists(dir))
            Directory.CreateDirectory(dir);

        return RunPacker(out _, "extract-minimap", "-i", isoPath, "-l", (levelId % 20).ToString(), "-t", "MP", "-v", racVersion.ToString(), "-o", outPath, "--double-alpha");
    }

    public static PACKER_STATUS_CODES ExtractTransitionBackground(string isoPath, string outPath, int levelId, int racVersion)
    {
        var dir = Path.GetDirectoryName(outPath);
        if (!Directory.Exists(dir))
            Directory.CreateDirectory(dir);

        switch (racVersion)
        {
            case 4:
                {
                    if (Constants.DLMapIndex.TryGetValue((DLMapIds)levelId, out var levelIndex))
                        return RunPacker(out _, "extract-transition-bg", "-i", isoPath, "-l", (levelIndex + 2).ToString(), "-o", outPath, "-v", racVersion.ToString());

                    break;
                }
        }

        return PACKER_STATUS_CODES.SUCCESS; // not supported
    }

    public static PACKER_STATUS_CODES DecompressAndUnpackLevelWad(string wadPath, string outFolder)
    {
        return RunPacker(out _, "unpack", "-i", wadPath, "-o", outFolder);
    }

    public static PACKER_STATUS_CODES UnpackSounds(string inFile, string outFolder, int racVersion)
    {
        return RunPacker(out _, "unpack-sounds", "-i", inFile, "-o", outFolder, "-v", racVersion.ToString());
    }

    public static PACKER_STATUS_CODES UnpackAssets(string inFolder, string outFolder, int racVersion)
    {
        return RunPacker(out _, "unpack-assets", "-i", inFolder, "-o", outFolder, "-v", racVersion.ToString());
    }

    public static PACKER_STATUS_CODES UnpackMissions(string inFolder, int racVersion)
    {
        return RunPacker(out _, "unpack-missions", "-i", inFolder);
    }

    public static PACKER_STATUS_CODES UnpackChunk(string inChunkFile, string outFolder)
    {
        return RunPacker(out _, "unpack-chunk", "-i", inChunkFile, "-o", outFolder);
    }

    public static PACKER_STATUS_CODES UnpackWorldInstances(string inFolder, string outFolder, int racVersion)
    {
        return RunPacker(out _, "unpack-world-instances", "-i", inFolder, "-o", outFolder, "-v", racVersion.ToString());
    }

    public static PACKER_STATUS_CODES UnpackWorldInstanceTies(string inFolder, string outFolder, int racVersion)
    {
        return RunPacker(out _, "unpack-world-instance-ties", "-i", inFolder, "-o", outFolder, "-v", racVersion.ToString());
    }

    public static PACKER_STATUS_CODES UnpackWorldInstanceShrubs(string inFolder, string outFolder, int racVersion)
    {
        return RunPacker(out _, "unpack-world-instance-shrubs", "-i", inFolder, "-o", outFolder, "-v", racVersion.ToString());
    }

    public static PACKER_STATUS_CODES UnpackGameplay(string inFolder, string outFolder, int levelId, int racVersion)
    {
        return RunPacker(out _, "unpack-gameplay", "-i", inFolder, "-o", outFolder, "-v", racVersion.ToString(), "-l", levelId.ToString());
    }

    public static bool UnpackCollision(string collisionBinFile, string outColladaFile)
    {
        var r = RunWAD2(out _, "extract_collision", collisionBinFile, outColladaFile);
        return r == 0;
    }

    public static PACKER_STATUS_CODES UnpackOcclusion(string inFile, string worldInstancesFolder, string outFolder, int racVersion)
    {
        var mappingFileName = "144.bin";
        switch (racVersion)
        {
            case 4: mappingFileName = "28.bin"; break;
        }

        return RunPacker(out _, "unpack-occlusion", "-i", inFile, "-m", Path.Combine(worldInstancesFolder, mappingFileName), "-o", outFolder);
    }

    public static PACKER_STATUS_CODES UnpackCode(string inFolder, string outFolder, int racVersion)
    {
        var fileName = "00.bin";
        switch (racVersion)
        {
            case 4: fileName = "08.bin"; break;
        }

        return RunPacker(out _, "unpack-code", "-i", Path.Combine(inFolder, fileName), "-o", outFolder);
    }

    public static PACKER_STATUS_CODES Pack(string inFolder, int baseLevelId, int racVersion, PACKER_PACK_OPS ops, Action<float> onProgressCallback = null)
    {
        PACKER_STATUS_CODES result;

        var levelWadFilename = FolderNames.GetLevelWadFilename(baseLevelId);
        var soundWadFilename = FolderNames.GetSoundWadFilename(baseLevelId);

        // world instances
        if (ops.HasFlag(PACKER_PACK_OPS.PACK_WORLD_INSTANCES))
        {
            // pack shrubs
            result = RunPacker(out _, "pack-world-instance-shrubs", "-i", Path.Combine(inFolder, FolderNames.BinaryWorldInstanceShrubFolder), "-o", Path.Combine(inFolder, FolderNames.BinaryWorldInstancesFolder));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.1f);

            // pack ties
            result = RunPacker(out _, "pack-world-instance-ties", "-i", Path.Combine(inFolder, FolderNames.BinaryWorldInstanceTieFolder), "-o", Path.Combine(inFolder, FolderNames.BinaryWorldInstancesFolder));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.2f);

            // pack occlusion before world instances
            if (ops.HasFlag(PACKER_PACK_OPS.PACK_OCCLUSION))
            {
                result = RunPacker(out _, "pack-occlusion", "-i", Path.Combine(inFolder, FolderNames.BinaryWorldInstanceOcclusionFolder), "--out-mapping", Path.Combine(inFolder, FolderNames.BinaryWorldInstancesFolder, "28.bin"), "--out-level", Path.Combine(inFolder, FolderNames.BinaryOcclusionFile));
                if (result != PACKER_STATUS_CODES.SUCCESS) return result;
                onProgressCallback?.Invoke(0.3f);
            }

            // pack world instances
            result = RunPacker(out _, "pack-world-instances", "-i", Path.Combine(inFolder, FolderNames.BinaryWorldInstancesFolder), "-o", Path.Combine(inFolder, "58.wad"));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.4f);
        }

        // occlusion
        else if (ops.HasFlag(PACKER_PACK_OPS.PACK_OCCLUSION))
        {
            result = RunPacker(out _, "pack-occlusion", "-i", Path.Combine(inFolder, FolderNames.BinaryWorldInstanceOcclusionFolder), "--out-mapping", Path.Combine(inFolder, FolderNames.BinaryWorldInstancesFolder, "28.bin"), "--out-level", Path.Combine(inFolder, FolderNames.BinaryOcclusionFile));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.3f);
        }

        // code
        if (ops.HasFlag(PACKER_PACK_OPS.PACK_CODE))
        {
            result = RunPacker(out _, "pack-code", "-i", Path.Combine(inFolder, FolderNames.BinaryCodeFolder), "-o", Path.Combine(inFolder, "08.bin"));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.5f);
        }

        // gameplay
        if (ops.HasFlag(PACKER_PACK_OPS.PACK_GAMEPLAY))
        {
            result = RunPacker(out _, "pack-gameplay", "-i", Path.Combine(inFolder, FolderNames.BinaryGameplayFolder), "-o", Path.Combine(inFolder, "60.wad"));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.6f);
        }

        // assets
        if (ops.HasFlag(PACKER_PACK_OPS.PACK_ASSETS))
        {
            result = RunPacker(out _, "pack-assets", "-i", Path.Combine(inFolder, FolderNames.AssetsFolder), "-o", inFolder);
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.7f);
        }

        // level wad
        if (ops.HasFlag(PACKER_PACK_OPS.PACK_LEVEL_WAD))
        {
            result = RunPacker(out _, "pack", "-i", inFolder, "-o", Path.Combine(inFolder, levelWadFilename));
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.8f);
        }

        // sound wad
        if (ops.HasFlag(PACKER_PACK_OPS.PACK_SOUND_WAD))
        {
            result = RunPacker(out _, "pack-sounds", "-i", Path.Combine(inFolder, FolderNames.BinarySoundsFolder), "-o", Path.Combine(inFolder, soundWadFilename), "-v", Constants.GameVersion.ToString());
            if (result != PACKER_STATUS_CODES.SUCCESS) return result;
            onProgressCallback?.Invoke(0.9f);
        }

        return PACKER_STATUS_CODES.SUCCESS;
    }

    public static PACKER_STATUS_CODES Patch(string inFolder, string targetIsoPath, string cleanIsoPath, int baseLevelId)
    {
        return RunPacker(out _, "patch-old", "--input", inFolder, "--id", baseLevelId.ToString(), "-i", targetIsoPath, "--cleaniso", cleanIsoPath);
    }

    public static PACKER_STATUS_CODES PatchMinimap(string isoPath, string cleanIsoPath, string mapFilePath, int levelId, int racVersion)
    {
        return RunPacker(out _, "patch-minimap", "-i", isoPath, "--cleaniso", cleanIsoPath, "-l", (levelId % 20).ToString(), "-t", "MP", "-v", racVersion.ToString(), "--input", mapFilePath);
    }

    public static PACKER_STATUS_CODES PatchTransitionBackground(string isoPath, string cleanIsoPath, string bgFilePath, int levelId, int racVersion)
    {
        switch (racVersion)
        {
            case 4:
                {
                    if (Constants.DLMapIndex.TryGetValue((DLMapIds)levelId, out var levelIndex))
                        return RunPacker(out _, "patch-transition-bg", "-i", isoPath, "--cleaniso", cleanIsoPath, "-l", (levelIndex + 2).ToString(), "-v", racVersion.ToString(), "--input", bgFilePath);

                    break;
                }
        }

        return PACKER_STATUS_CODES.SUCCESS; // not supported
    }

    public static PACKER_STATUS_CODES ConvertAssetTextures(string rootFolder, bool mipmaps = true, bool outSwizzle = true)
    {
        return RunPacker(out _, "texture", "-i", rootFolder, "-m", "PNG_FOLDER_TO_ASSET_TEXTURE", "-o", rootFolder, "-r", mipmaps ? "--mipmaps" : "", outSwizzle ? "--out-swizzle" : "");
    }

    public static PACKER_STATUS_CODES ConvertPngToPif4bpp(string inFile, string outFolder, bool half_alpha = true, bool outSwizzle = true)
    {
        return RunPacker(out _, "texture", "-i", inFile, "-m", "PNG_TO_PIF_4BPP", "-o", outFolder, half_alpha ? "--half-alpha" : "", outSwizzle ? "--out-swizzle" : "");
    }

    public static PACKER_STATUS_CODES ConvertPngToPif8bpp(string inFile, string outFolder, bool outSwizzle = true)
    {
        return RunPacker(out _, "texture", "-i", inFile, "-m", "PNG_TO_PIF_8BPP", "-o", outFolder, outSwizzle ? "--out-swizzle" : "");
    }

    public static PACKER_STATUS_CODES ConvertPngToLoadingScreen(string inFile, string outFolder)
    {
        return RunPacker(out _, "convert-bg", "-i", inFile, "-o", outFolder);
    }

    public static PACKER_STATUS_CODES ConvertCollision(string inBinFile, string outBinFile, int fromGameVersion, int toGameVersion)
    {
        string op = null;
        if (fromGameVersion == 3 && toGameVersion == 4) op = "UYA_TO_DL";
        else if (fromGameVersion == 2 && toGameVersion == 4) op = "UYA_TO_DL";
        else if (fromGameVersion == 4 && toGameVersion == 3) op = "DL_TO_UYA";
        if (string.IsNullOrEmpty(op)) return PACKER_STATUS_CODES.SUCCESS;

        return RunPacker(out _, "collision", "-i", inBinFile, "-o", outBinFile, "-m", op);
    }

    public static PACKER_STATUS_CODES ConvertTies(string inTiesFolder, string outTiesFolder, int fromGameVersion, int toGameVersion)
    {
        if (fromGameVersion == toGameVersion) return PACKER_STATUS_CODES.SUCCESS;

        // gc tie need to be fixed
        if (fromGameVersion == 2)
        {
            var tieFixGcResult = RunPacker(out _, "tie-fix-gc", "-i", inTiesFolder);
            if (tieFixGcResult != PACKER_STATUS_CODES.SUCCESS)
                return tieFixGcResult;
        }

        string op = null;
        if (fromGameVersion == 3 && toGameVersion == 4) op = "UYA_TO_DL";
        else if (fromGameVersion == 2 && toGameVersion == 4) op = "UYA_TO_DL";
        else if (fromGameVersion == 4 && toGameVersion == 3) op = "DL_TO_UYA";
        if (string.IsNullOrEmpty(op)) return PACKER_STATUS_CODES.SUCCESS;

        return RunPacker(out _, "tie", "-i", inTiesFolder, "-r", "-m", op);
    }

    public static PACKER_STATUS_CODES ConvertSky(string inBinFile, string outBinFile, int fromGameVersion, int toGameVersion)
    {
        string op = null;
        if (fromGameVersion == 3 && toGameVersion == 4) op = "UYA_TO_DL";
        else if (fromGameVersion == 2 && toGameVersion == 4) op = "UYA_TO_DL";
        else if (fromGameVersion == 4 && toGameVersion == 3) op = "DL_TO_UYA";
        if (string.IsNullOrEmpty(op)) return PACKER_STATUS_CODES.SUCCESS;

        return RunPacker(out _, "skybox", "-i", inBinFile, "-o", outBinFile, "-m", op);
    }

    public static List<Vector3> ReadOcclusionBlock(BinaryReader reader, out int instanceId, out int id)
    {
        var octants = new List<Vector3>();
        instanceId = reader.ReadInt32();
        id = reader.ReadInt32();
        var count = reader.ReadInt32();

        var i = 0;
        while (i < count)
        {
            octants.Add(new Vector3()
            {
                x = reader.ReadInt16() * 4,
                z = reader.ReadInt16() * 4,
                y = reader.ReadInt16() * 4
            });

            ++i;
        }

        // align 0x10
        var off = reader.BaseStream.Position % 0x10;
        if (off != 0)
            reader.BaseStream.Position += 0x10 - off;

        return octants;
    }

    public static void WriteOcclusionBlock(BinaryWriter writer, int instanceIdx, int occlusionId, IEnumerable<Vector3> octants)
    {
        writer.Write(instanceIdx);
        writer.Write(occlusionId);

        if (octants == null)
        {
            // count
            writer.Write((int)0);
        }
        else
        {
            // count
            writer.Write((int)octants.Count());
            foreach (var xyz in octants)
            {
                writer.Write((short)(xyz.x / 4));
                writer.Write((short)(xyz.z / 4));
                writer.Write((short)(xyz.y / 4));
            }
        }

        // align 0x10
        var off = writer.BaseStream.Position % 0x10;
        if (off != 0)
            writer.BaseStream.Position += 0x10 - off;
    }

    public static void DuplicateAsset(string srcAssetFolder, string dstAssetFolder, string assetType)
    {
        var i = 0;
        var shader = Shader.Find("Horizon Forge/Universal");
        var files = Directory.GetFiles(srcAssetFolder, "*.*", SearchOption.AllDirectories);
        var filesToCopy = new List<string>();
        var texturesToConfigureImporterSettings = new List<string>();
        var modelsToConfigureImporterSettings = new List<string>();
        string[] tags = null;
        foreach (var file in files)
        {
            if (Path.GetExtension(file) == ".fbx")
                filesToCopy.Add(file);
            else if (Path.GetFileName(file).EndsWith("_col.fbx") || Path.GetFileName(file).EndsWith("_col.blend"))
                filesToCopy.Add(file);
            else if (Path.GetFileName(file) == "core.bin")
                filesToCopy.Add(file);
            else if (Path.GetFileName(file) == "tex.bin")
                filesToCopy.Add(file);
            else if (Path.GetExtension(file) == ".png")
                filesToCopy.Add(file);
            else if (Path.GetExtension(file) == ".sound")
                filesToCopy.Add(file);
        }

        // copy
        foreach (var file in filesToCopy)
        {
            if (!File.Exists(file)) continue;
            var dir = Path.GetDirectoryName(file);
            var relDir = Path.GetRelativePath(srcAssetFolder, dir);
            if (relDir == ".") relDir = "";
            var dstDir = Path.Combine(dstAssetFolder, relDir);
            var dstFile = Path.Combine(dstDir, Path.GetFileName(file));

            if (!Directory.Exists(dstDir)) Directory.CreateDirectory(dstDir);
            File.Copy(file, dstFile, true);

            switch (Path.GetExtension(file))
            {
                case ".fbx":
                case ".blend":
                    {
                        var assetLabels = AssetDatabase.GetLabels(AssetDatabase.LoadAssetAtPath<GameObject>(file));
                        if (assetLabels != null)
                        {
                            foreach (var assetLabel in assetLabels)
                            {
                                if (assetLabel == "GC" || assetLabel == "UYA" || assetLabel == "DL")
                                    tags = new string[] { assetLabel };
                            }
                        }
                        break;
                    }
            }

            switch (Path.GetExtension(dstFile))
            {
                case ".png": texturesToConfigureImporterSettings.Add(dstFile); break;
                case ".fbx": modelsToConfigureImporterSettings.Add(dstFile); break;
                case ".blend": modelsToConfigureImporterSettings.Add(dstFile); break;
            }
        }

        AssetDatabase.Refresh();

        // configure texture importers
        try
        {
            AssetDatabase.StartAssetEditing();
            i = 0;
            foreach (var textureAssetPath in texturesToConfigureImporterSettings)
            {
                WrenchHelper.SetDefaultWrenchModelTextureImportSettings(textureAssetPath);

                // create matching material
                var matAssetPath = Path.Combine(dstAssetFolder, "Materials", Path.GetFileNameWithoutExtension(textureAssetPath) + ".mat");
                var mat = new Material(shader);

                if (!Directory.Exists(Path.GetDirectoryName(matAssetPath))) { Directory.CreateDirectory(Path.GetDirectoryName(matAssetPath)); }
                mat.SetTexture("_MainTex", AssetDatabase.LoadAssetAtPath<Texture2D>(textureAssetPath));
                AssetDatabase.CreateAsset(mat, matAssetPath);

                EditorUtility.DisplayProgressBar($"Importing Textures", $"{textureAssetPath} ({i}/{texturesToConfigureImporterSettings.Count})", i / (float)texturesToConfigureImporterSettings.Count);
                ++i;
            }
        }
        finally
        {
            AssetDatabase.StopAssetEditing();
        }

        AssetDatabase.Refresh();

        // configure model importers
        foreach (var modelAssetPath in modelsToConfigureImporterSettings)
        {
            if (Path.GetFileNameWithoutExtension(modelAssetPath).EndsWith("_col"))
                WrenchHelper.SetDefaultWrenchModelImportSettings(modelAssetPath, "Collider", true);
            else
                WrenchHelper.SetDefaultWrenchModelImportSettings(modelAssetPath, assetType, true, tags: tags);
        }
    }

    private struct PackSoundDef
    {
        public uint GrainOffset;
        public int GrainSize;
        public uint DefOffset;
        public int DefSize;
        public uint BinOffset;
        public int BinSize;
        public uint WavOffset;
        public int WavSize;
    }

    public static void PackSound(string inFolder, string outFile)
    {
        var defs = new List<PackSoundDef>();
        var idx = 0;

        // count defs
        var count = 0;
        while (true)
        {
            var grainFile = Path.Combine(inFolder, $"grain_{count:0000}.def");
            if (!File.Exists(grainFile)) break;

            ++count;
        }

        using (var fs = File.Create(outFile))
        {
            using (var writer = new BinaryWriter(fs))
            {
                var headerSize = 8 + (0x20 * count);

                // allocate space for header later
                writer.Write(new byte[headerSize]);

                // write parent.def
                writer.Write(File.ReadAllBytes(Path.Combine(inFolder, "parent.def")));

                // read sound data
                while (true)
                {
                    var grainFile = Path.Combine(inFolder, $"grain_{idx:0000}.def");
                    var defFile = Path.Combine(inFolder, $"sound_{idx}.def");
                    var binFile = Path.Combine(inFolder, $"sound_{idx}.bin");
                    var wavFile = Path.Combine(inFolder, $"sound_{idx}.wav");

                    // each sound needs a grain
                    // we expect that there are no gaps in indicies
                    if (!File.Exists(grainFile)) break;

                    // write data
                    var def = new PackSoundDef();
                    var bytes = File.ReadAllBytes(grainFile);
                    def.GrainOffset = (uint)fs.Position;
                    def.GrainSize = bytes.Length;
                    writer.Write(bytes);

                    // def
                    if (File.Exists(defFile))
                    {
                        bytes = File.ReadAllBytes(defFile);
                        def.DefOffset = (uint)fs.Position;
                        def.DefSize = bytes.Length;
                        writer.Write(bytes);
                    }

                    // bin
                    if (File.Exists(binFile))
                    {
                        bytes = File.ReadAllBytes(binFile);
                        def.BinOffset = (uint)fs.Position;
                        def.BinSize = bytes.Length;
                        writer.Write(bytes);
                    }

                    // wav
                    if (File.Exists(wavFile))
                    {
                        bytes = File.ReadAllBytes(wavFile);
                        def.WavOffset = (uint)fs.Position;
                        def.WavSize = bytes.Length;
                        writer.Write(bytes);
                    }

                    defs.Add(def);
                    ++idx;
                }

                // write header
                fs.Position = 0;
                writer.Write(defs.Count);
                writer.Write(headerSize); // parent.def

                // write defs
                foreach (var def in defs)
                {
                    writer.Write(def.GrainOffset);
                    writer.Write(def.GrainSize);
                    writer.Write(def.DefOffset);
                    writer.Write(def.DefSize);
                    writer.Write(def.BinOffset);
                    writer.Write(def.BinSize);
                    writer.Write(def.WavOffset);
                    writer.Write(def.WavSize);
                }
            }
        }
    }

    public static int UnpackSound(string inFile, string outFolder)
    {
        int idx = 0;

        using (var fs = File.OpenRead(inFile))
        {
            using (var reader = new BinaryReader(fs))
            {
                var count = reader.ReadInt32();
                var parentDefOffset = reader.ReadUInt32();
                var defs = new List<PackSoundDef>();

                // read defs
                for (int i = 0; i < count; ++i)
                {
                    defs.Add(new PackSoundDef()
                    {
                        GrainOffset = reader.ReadUInt32(),
                        GrainSize = reader.ReadInt32(),
                        DefOffset = reader.ReadUInt32(),
                        DefSize = reader.ReadInt32(),
                        BinOffset = reader.ReadUInt32(),
                        BinSize = reader.ReadInt32(),
                        WavOffset = reader.ReadUInt32(),
                        WavSize = reader.ReadInt32(),
                    });
                }

                // unpack parent.def
                int parentDefSize = count > 0 ? (int)(defs[0].GrainOffset - parentDefOffset) : (int)(fs.Length - parentDefOffset);
                fs.Position = parentDefOffset;
                File.WriteAllBytes(Path.Combine(outFolder, "parent.def"), reader.ReadBytes(parentDefSize));

                while (idx < count)
                {
                    var def = defs[idx];
                    var grainFile = Path.Combine(outFolder, $"grain_{idx:0000}.def");
                    var defFile = Path.Combine(outFolder, $"sound_{idx}.def");
                    var binFile = Path.Combine(outFolder, $"sound_{idx}.bin");
                    var wavFile = Path.Combine(outFolder, $"sound_{idx}.wav");

                    // write grain
                    fs.Position = def.GrainOffset;
                    File.WriteAllBytes(grainFile, reader.ReadBytes(def.GrainSize));

                    // write def
                    if (def.DefOffset > 0 && def.DefSize > 0)
                    {
                        fs.Position = def.DefOffset;
                        File.WriteAllBytes(defFile, reader.ReadBytes(def.DefSize));
                    }

                    // write bin
                    if (def.BinOffset > 0 && def.BinSize > 0)
                    {
                        fs.Position = def.BinOffset;
                        File.WriteAllBytes(binFile, reader.ReadBytes(def.BinSize));
                    }

                    // write wav
                    if (def.WavOffset > 0 && def.WavSize > 0)
                    {
                        fs.Position = def.WavOffset;
                        File.WriteAllBytes(wavFile, reader.ReadBytes(def.WavSize));
                    }

                    ++idx;
                }
            }
        }

        return idx;
    }

    public static Dictionary<string, byte[]> UnpackSoundWAVs(byte[] bytes)
    {
        var wavs = new Dictionary<string, byte[]>();
        int idx = 0;

        using (var ms = new MemoryStream(bytes))
        {
            using (var reader = new BinaryReader(ms))
            {
                var count = reader.ReadInt32();
                var parentDefOffset = reader.ReadUInt32();
                var defs = new List<PackSoundDef>();

                // read defs
                for (int i = 0; i < count; ++i)
                {
                    defs.Add(new PackSoundDef()
                    {
                        GrainOffset = reader.ReadUInt32(),
                        GrainSize = reader.ReadInt32(),
                        DefOffset = reader.ReadUInt32(),
                        DefSize = reader.ReadInt32(),
                        BinOffset = reader.ReadUInt32(),
                        BinSize = reader.ReadInt32(),
                        WavOffset = reader.ReadUInt32(),
                        WavSize = reader.ReadInt32(),
                    });
                }

                while (idx < count)
                {
                    var def = defs[idx];
                    ms.Position = def.WavOffset;
                    if (def.WavOffset > 0 && def.WavSize > 0)
                        wavs.Add($"{idx}", reader.ReadBytes(def.WavSize));

                    ++idx;
                }
            }
        }

        return wavs;
    }

    private struct PackAssetTextureDef
    {
        public int Offset;
        public short Width;
        public short Height;
        public bool Mipmaps;
        public bool IsGsTex;
    }

    public static void PackAssetTextures(string inFolder, string outFile)
    {
        var textureDefs = new List<PackAssetTextureDef>();

        using (var fs = File.Create(outFile))
        {
            using (var writer = new BinaryWriter(fs))
            {
                // allocate space for header later
                writer.Write(new byte[16 * 0x10]);

                // iterate textures in dir
                for (int idx = 0; idx < 16; ++idx)
                {
                    var baseTexPath = Path.Combine(inFolder, $"tex.{idx:0000}");
                    var texDefFile = baseTexPath + ".def";
                    var texPaletteFile = baseTexPath + ".palette";
                    var def = new PackAssetTextureDef();
                    if (!File.Exists(texDefFile) || !File.Exists(texPaletteFile)) break;

                    var texDefBytes = File.ReadAllBytes(texDefFile);
                    var texPaletteBytes = File.ReadAllBytes(texPaletteFile);

                    def.Offset = (int)fs.Position;
                    def.Width = BitConverter.ToInt16(texDefBytes, 4);
                    def.Height = BitConverter.ToInt16(texDefBytes, 6);
                    def.IsGsTex = File.Exists(baseTexPath + ".3.bin");
                    def.Mipmaps = File.Exists(baseTexPath + ".1.bin") && File.Exists(baseTexPath + ".2.bin");

                    writer.Write(texDefBytes);
                    writer.Write(texPaletteBytes);
                    writer.Write(File.ReadAllBytes(baseTexPath + $".{(def.IsGsTex ? "3" : "0")}.bin"));
                    if (def.Mipmaps)
                    {
                        writer.Write(File.ReadAllBytes(baseTexPath + ".1.bin"));
                        writer.Write(File.ReadAllBytes(baseTexPath + ".2.bin"));
                    }

                    textureDefs.Add(def);
                }

                // write defs
                fs.Position = 0;
                foreach (var textureDef in textureDefs)
                {
                    writer.Write(textureDef.Offset);
                    writer.Write(textureDef.Width);
                    writer.Write(textureDef.Height);
                    writer.Write(textureDef.Mipmaps);
                    writer.Write(textureDef.IsGsTex);
                    writer.Write(new byte[6]);
                }
            }
        }
    }

    public static int UnpackAssetTextures(string inFile, string outFolder)
    {
        int idx = 0;

        using (var fs = File.OpenRead(inFile))
        {
            using (var reader = new BinaryReader(fs))
            {
                while (idx < 16)
                {
                    var texBasePath = Path.Combine(outFolder, $"tex.{idx:0000}");

                    fs.Position = 0x10 * idx;
                    var offset = reader.ReadInt32();
                    var width = reader.ReadInt16();
                    var height = reader.ReadInt16();
                    var mipmaps = reader.ReadBoolean();
                    var gsTex = reader.ReadBoolean();

                    if (width == 0 || height == 0) break;

                    // write def
                    fs.Position = offset;
                    File.WriteAllBytes(texBasePath + ".def", reader.ReadBytes(0x10));

                    // write palette
                    File.WriteAllBytes(texBasePath + ".palette", reader.ReadBytes(0x400));

                    // write textures
                    File.WriteAllBytes(texBasePath + $".{(gsTex ? "3" : "0")}.bin", reader.ReadBytes(width * height));

                    if (mipmaps)
                    {
                        for (int i = 1; i < 3; ++i)
                        {
                            width /= 2;
                            height /= 2;

                            File.WriteAllBytes(texBasePath + $".{i}.bin", reader.ReadBytes(width * height));
                        }
                    }

                    ++idx;
                }
            }
        }

        return idx;
    }
}
