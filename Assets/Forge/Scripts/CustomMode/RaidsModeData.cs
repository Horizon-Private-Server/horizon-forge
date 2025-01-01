using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public class RaidsModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int RAIDS_VERSION = 0;

    public static readonly int MOB_SPAWNER_OCLASS = 0x4001;
    public static readonly int MOVER_OCLASS = 0x4002;
    public static readonly int CONTROLLER_OCLASS = 0x4003;
    public static readonly int GATE_OCLASS = 0x4004;
    public static readonly int MESSAGER_OCLASS = 0x4005;
    public static readonly int NPC_OCLASS = 0x4006;
    public static readonly int CHECKPOINT_MANAGER_OCLASS = 0x4007;
    public static readonly int CHECKPOINT_OCLASS = 0x4008;
    public static readonly int LASERBEAM_OCLASS = 0x4009;
    public static readonly int LASER_OCLASS = 0x400A;
    public static readonly int PVARPOKE_OCLASS = 0x400B;
    public static readonly int BLIP_OCLASS = 0x400C;
    public static readonly int DUMMY_OCLASS = 0x400D;

    public static readonly float[] DIFFICULTY_FACTORS = new float[]
    {
        0,
        25,
        150,
        500,
        1250
    };

    public override DLCustomModeIds CustomMode => DLCustomModeIds.Raids;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;
    [Tooltip("How much of the render budget to allocate for the map.\n\nThe larger the number, the more mob billboards (shellshock) will appear.")] public int MapBaseComplexity = 5000;
    [Range(1, 10), Tooltip("Estimated map difficulty, indicated to the user on a scale of 1 to 10.")] public float DifficultyApproximate = 5;
    public int Cost1Star = 0;
    public int Cost2Star = 0;
    public int Cost3Star = 0;
    public int Cost4Star = 0;
    public int Cost5Star = 0;
    public string Author;
    [Multiline] public string Description;

    public List<RaidsMobSpawnParam> Mobs = new List<RaidsMobSpawnParam>()
    {
        new RaidsMobSpawnParam() { Name = "Zombie" }
    };

    [Header("Debug")]
    public bool DebugPath;
    public bool DebugMove;

    [Header("Music")]
    public bool OverrideTrackList = false;
    public List<DLMusicTracks> TrackWhitelist = new List<DLMusicTracks>();

    private void OnValidate()
    {
        if (Author != null && Author.Length > 32) Author = Author.Substring(0, 32);
        if (Description != null && Description.Length > 256) Description = Description.Substring(0, 256);
    }

    public void Configure(string buildFolder, CodeGenState state)
    {
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);
        var enabledMobs = Mobs.Where(x => !x.Disabled);

        state.SeparateCodeFile = true;

        // copy raids base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "raids"), buildFolder);

        // build config.c and path.c
        File.WriteAllText(Path.Combine(srcFolder, "config.c"), GetConfigContents());
        File.WriteAllText(Path.Combine(srcFolder, "path.c"), GetPathContents());

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/path.o");

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/map.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/levelselect.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gate.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/spawner.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/messager.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/checkpoint.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mover.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/controller.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/npc.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laserbeam.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laser.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pvarpoke.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pathfind.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/maputils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/vendor.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/badges.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/bank.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/ammodrop.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/hackerorb.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/blip.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/dummy.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/window.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mobs/mob.o");

        state.LDFlags.Add("-DGATE");
        var mobTypes = enabledMobs.Select(x => x.Mob).Distinct();
        foreach (var mobType in mobTypes)
            state.LDFlags.Add($"-DMOB_{mobType.ToString().ToUpper()}");

        if (state.Debug)
        {
            if (DebugPath)
                state.LDFlags.Add("-DDEBUGPATH");
            if (DebugMove)
                state.LDFlags.Add("-DDEBUGMOVE");
        }

        state.Includes.Add("#include \"game.h\"");
        state.Includes.Add("#include \"maputils.h\"");
        state.Includes.Add("#include \"game.h\"");
        state.Includes.Add("#include \"gate.h\"");
        state.Includes.Add("#include \"npc.h\"");
        state.Includes.Add("#include \"messager.h\"");
        state.Includes.Add("#include \"spawner.h\"");
        state.Includes.Add("#include \"checkpoint.h\"");
        state.Includes.Add("#include \"laser.h\"");
        state.Includes.Add("#include \"controller.h\"");
        state.Includes.Add("#include \"pvarpoke.h\"");
        state.Includes.Add("#include \"mover.h\"");
        state.Includes.Add("#include \"mob.h\"");
        state.Includes.Add("#include \"shared.h\"");
        state.Includes.Add("#include \"pathfind.h\"");
        state.Includes.Add("#include \"vendor.h\"");
        state.Includes.Add("#include \"badges.h\"");
        state.Includes.Add("#include \"ammodrop.h\"");
        state.Includes.Add("#include \"hackerorb.h\"");
        state.Includes.Add("#include \"blip.h\"");
        state.Includes.Add("#include \"dummy.h\"");
        state.Includes.Add("#include \"levelselect.h\"");

        state.Declarations.Add("void configInit(void);");
        state.Declarations.Add("struct RaidsMapConfig MapConfig __attribute__((section(\".config\"))) = {\r\n  .Magic = MAP_CONFIG_MAGIC,\r\n  .State = NULL,  .TrackWhitelist = NULL,\r\n};");

        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid mobForceIntoMapBounds(Moby* moby)\r\n{{\r\n\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nint mapPathCanBeSkippedForTarget(struct PathGraph* path, Moby* moby)\r\n{{\r\n  return 1;\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nint createMob(struct MobCreateArgs* args)\r\n{{\r\n  if (args->SpawnParamsIdx < 0 || args->SpawnParamsIdx >= MapConfig.MobSpawnParamsCount) {{\r\n    DPRINTF(\"unhandled create spawnParamsIdx %d\\n\", args->SpawnParamsIdx);\r\n    return 0;\r\n  }}\r\n\r\n  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];\r\n  if (spawnParams->MobCreate)\r\n    return spawnParams->MobCreate(args);\r\n\r\n  DPRINTF(\"unhandled create spawnParamsIdx %d\\n\", args->SpawnParamsIdx);\r\n  return 0;\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid mapOnFrameTick(void)\r\n{{\r\n  dlPreUpdate();\r\n\r\n  levelselectFrameTick();\r\n  messagerFrameUpdate();\r\n  {String.Join("  \r\n", state.Meta.GetValueOrDefault("RAIDS_FRAMEUPDATE") ?? new List<string>())}\r\n\r\n  dlPostUpdate();\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes(void)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)())0x005ce1d8)();\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes2(u32 a0)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)(u32))0x0059b320)(a0);\r\n}}\r\n");

        state.InitBody.Add($"configInit();");
        state.InitBody.Add($"mapInit();");
        state.InitBody.Add($"mobInit();");
        state.InitBody.Add($"levelselectInit();");
        state.InitBody.Add($"spawnerInit();");
        state.InitBody.Add($"moverInit();");
        state.InitBody.Add($"controllerInit();");
        state.InitBody.Add($"gateInit();");
        state.InitBody.Add($"npcInit();");
        state.InitBody.Add($"messagerInit();");
        state.InitBody.Add($"checkpointInit();");
        state.InitBody.Add($"laserInit();");
        state.InitBody.Add($"pvarpokeInit();");
        state.InitBody.Add($"vendorInit();");
        state.InitBody.Add($"badgesInit();");
        state.InitBody.Add($"ammodropInit();");
        state.InitBody.Add($"hackerorbInit();");
        state.InitBody.Add($"blipInit();");
        state.InitBody.Add($"dummyInit();");

        state.InitBody.Add($"MapConfig.OnMobCreateFunc = &createMob;");
        state.InitBody.Add($"MapConfig.OnMobUpdateFunc = &mapOnMobUpdate;");
        state.InitBody.Add($"MapConfig.OnMobKilledFunc = &mapOnMobKilled;");
        state.InitBody.Add($"MapConfig.OnMobDestroyedFunc = &mapOnMobDestroyed;");
        state.InitBody.Add($"MapConfig.OnMobSpawnedFunc = &mapOnMobSpawned;");
        state.InitBody.Add($"MapConfig.CreateAmmoDropAtFunc = &ammodropCreateAt;");
        state.InitBody.Add($"MapConfig.OnFrameTickFunc = &mapOnFrameTick;");

        state.InitBody.Add($"HOOK_JAL(0x003bd854, &onBeforeUpdateHeroes);");
        state.InitBody.Add($"HOOK_JAL(0x0051f648, &onBeforeUpdateHeroes2);");

        state.InitBody.Add("respawnAllPlayers();");

        state.MainBodyReady.Add("mapStart();");
        state.MainBodyReady.Add("levelselectStart();");
        state.MainBodyReady.Add("spawnerStart();");
        state.MainBodyReady.Add("moverStart();");
        state.MainBodyReady.Add("controllerStart();");
        state.MainBodyReady.Add("gateStart();");
        state.MainBodyReady.Add("npcStart();");
        state.MainBodyReady.Add("checkpointStart();");
        state.MainBodyReady.Add("laserStart();");
        state.MainBodyReady.Add("pvarpokeStart();");
        state.MainBodyReady.Add("vendorStart();");
        state.MainBodyReady.Add("badgesStart();");
        state.MainBodyReady.Add("ammodropStart();");
        state.MainBodyReady.Add("dummyStart();");

        state.MainBody.Add("mobTick();");
        state.MainBody.Add("for (i = 0; i < PathsCount; ++i) pathTick(&Paths[i]);");
        state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
    }

    public void Configure(BuildState state)
    {
        state.MobyOClasses.Add(8309); // node base (for capture sound)
        state.MobyOClasses.Add(6898); // health box (for health sound; nanoleech)
        state.MobyOClasses.Add(9278); // weapon pickup (for loot drops)
        state.MobyOClasses.Add(LASERBEAM_OCLASS);

        // add mob oclasses
        var mobConfig = RaidsMobsScriptableObject.Load();
        foreach (var mob in this.Mobs.Where(x => !x.Disabled))
        {
            var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == mob.Mob);
            var variant = mobDefaults.Variants.ElementAtOrDefault(mob.Variant);
            if (variant == null) continue;

            state.MobyOClasses.Add(variant.OClass);
            if (variant.Dependencies != null)
                state.MobyOClasses.AddRange(variant.Dependencies.Select(x => x.OClass));
        }
    }

    string GetConfigContents()
    {
        var sb = new StringBuilder();

        sb.AppendLine("#include <libdl/utils.h>");
        sb.AppendLine("#include \"game.h\"");
        sb.AppendLine("#include \"mob.h\"");
        sb.AppendLine("");

        sb.AppendLine("extern struct RaidsMapConfig MapConfig;");
        sb.AppendLine("");

        sb.AppendLine("struct MobSpawnParams mobSpawnParams[] = {");
        foreach (var mob in Mobs.Where(x => !x.Disabled))
            sb.AppendLine(mob.GetDef());
        sb.AppendLine("};");
        sb.AppendLine("");

        sb.AppendLine($"int musicTrackWhitelistEnabled = {(OverrideTrackList ? 1 : 0)};");
        sb.AppendLine($"int musicTrackWhitelistCount = {TrackWhitelist.Count};");
        sb.AppendLine("int musicTrackWhitelist[] = {");
        foreach (var track in TrackWhitelist)
            sb.AppendLine($"  {(int)track},");
        sb.AppendLine("};");
        sb.AppendLine("");

        sb.AppendLine("//--------------------------------------------------------------------------\r\nvoid configInit(void)\r\n{\r\n  MapConfig.MobSpawnParams = mobSpawnParams;\r\n  MapConfig.MobSpawnParamsCount = COUNT_OF(mobSpawnParams);\r\n  MapConfig.TrackWhitelist = musicTrackWhitelist;\r\n  MapConfig.TrackWhitelistCount = musicTrackWhitelistCount;\r\n  MapConfig.TrackWhitelistEnabled = musicTrackWhitelistEnabled;\r\n}\r\n");

        return sb.ToString();
    }

    string GetPathContents()
    {
        return PathGraph.ExportGraphsAsC();
    }

    public override void Write(BinaryWriter writer)
    {
        writer.Write(RAIDS_VERSION);
        writer.Write(DifficultyApproximate);
        writer.Write(new byte[8]);
        writer.Write(Cost1Star);
        writer.Write(Cost2Star);
        writer.Write(Cost3Star);
        writer.Write(Cost4Star);
        writer.Write(Cost5Star);
        writer.WriteString(BinaryHelper.StrToRatchetStr(Author), 31);
        writer.Write((byte)0);
        writer.WriteString(BinaryHelper.StrToRatchetStr(Description), 255);
        writer.Write((byte)0);
    }


    #region Menu Items

    [MenuItem("GameObject/Forge/Raids/Create Raids Data", priority = 10)]
    public static void CreateRaidsData()
    {
        var go = new GameObject("Raids");
        var raidsData = go.AddComponent<RaidsModeData>();

        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Mob Spawner Moby", priority = 10)]
    public static void CreateSpawnerMoby()
    {
        var go = new GameObject("Raids Mob Spawner");
        var moby = go.AddComponent<Moby>();
        moby.OClass = MOB_SPAWNER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("MobSpawner");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Mover Moby", priority = 10)]
    public static void CreateMoverMoby()
    {
        var go = new GameObject("Mover");
        var moby = go.AddComponent<Moby>();
        moby.OClass = MOVER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Mover");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Controller Moby", priority = 10)]
    public static void CreateControllerMoby()
    {
        var go = new GameObject("Controller");
        var moby = go.AddComponent<Moby>();
        moby.OClass = CONTROLLER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Controller");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/PVar Poke Moby", priority = 10)]
    public static void CreatePVarPokeMoby()
    {
        var go = new GameObject("PVar Poke");
        var moby = go.AddComponent<Moby>();
        moby.OClass = PVARPOKE_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("PVarPoke");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Gate Moby", priority = 10)]
    public static void CreateGateMoby()
    {
        var mobyBin = Convert.FromBase64String("sAAAAAEAAAEAAAAAAQD/AMAAAAAAAQAAAAEAAAABAABgAgAAq6qqPAAAAAAABQAAnFNJvZxTybycU8m8PMQHR3h4eIAAAAAQYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgqh7vgAAAACcU8m8N8QHRwH/AAAAAAAAJAAAADAAAAACABQAAAAAAAAAAAAAAAAAAgIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAQAACAAEAJABAAANAwIIAAAAAAAAAAAQAAAAIAAAAAAAAKAAYAAAAAAAYABgAAAAAABgAKAAAAAAAKAAoAAAAAIBbwIAA28BAwBvAwECbwAAAAAAAAAAAAAAAAAAAADCgAh1AAAAEAAQABAAEAAAAAAAAAAAABAAEAAQABAAAAAAAAAtgQRu/gQBAACCBAOFhggHAQEBAAAAAAAxgQRskv8AAAQAAAAEAKBBAAAAAAAAAAAAAAAACAAAAAAAAAD/////AAAAAAYAAAAAAAAAAAAAAAAAAAA0AAAAAAAAAAEAAAAAAAgAAAAIACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA9AAAAAAAAAAAAGAAYAAAAPQAAAAAAAAAAACgAGAAAAD0AAAAAAAAAAAAoACgAAAA9AAAAAAAAAAAAGAAoAAAAPQAAAAAgAAAAACgAGAAAAD0AAAAAIAAAAAAYABgAAAA9AAAAACAAAAAAGAAoP8AAPQAAAAAgAAAAACgAKD/AAAAAAAAAAAAAAAAAAAA/wAAAAAAAAAAAAAAAAAAAP8AAAD/AP8A/wD/AAAAAAD///////////////9AAQCA");

        var go = new GameObject("Gate");
        var moby = go.AddComponent<Moby>();
        moby.OClass = GATE_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 0, 0, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Gate");
        moby.InitializePVarReferences();

        // create gate core.bin (has collision)
        var mobyFolder = FolderNames.GetLocalAssetFolder(FolderNames.MobyFolder, RCVER.DL);
        if (Directory.Exists(mobyFolder))
        {
            var gateMobyFolder = Path.Combine(mobyFolder, GATE_OCLASS.ToString());
            if (!Directory.Exists(gateMobyFolder))
            {
                Directory.CreateDirectory(gateMobyFolder);
                File.WriteAllBytes(Path.Combine(gateMobyFolder, "core.bin"), mobyBin);
            }
        }

        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Messager Moby", priority = 10)]
    public static void CreateMessagerMoby()
    {
        var go = new GameObject("Messager");
        var moby = go.AddComponent<Moby>();
        moby.OClass = MESSAGER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Messager");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/NPC Controller Moby", priority = 10)]
    public static void CreateNpcControllerMoby()
    {
        var go = new GameObject("NPC Controller");
        var moby = go.AddComponent<Moby>();
        moby.OClass = NPC_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("NPC Controller");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Checkpoint Manager Moby", priority = 10)]
    public static void CreateCheckpointManagerMoby()
    {
        var go = new GameObject("Checkpoint Manager");
        var moby = go.AddComponent<Moby>();
        moby.OClass = CHECKPOINT_MANAGER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        //moby.PrefabOverride = UnityHelper.GetRaidsPrefab("NPC Controller");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Checkpoint Moby", priority = 10)]
    public static void CreateCheckpointMoby()
    {
        var go = new GameObject("Checkpoint");
        var moby = go.AddComponent<Moby>();
        moby.OClass = CHECKPOINT_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Checkpoint");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Laser (Tripwire) Moby", priority = 10)]
    public static void CreateLaserMoby()
    {
        var go = new GameObject("Laser (Tripwire)");
        var moby = go.AddComponent<Moby>();
        moby.OClass = LASER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Laser");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Radar Blip Moby", priority = 10)]
    public static void CreateRadarBlipMoby()
    {
        var go = new GameObject("Radar Blip Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = BLIP_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Radar Blip");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/Health Proxy Moby", priority = 10)]
    public static void CreateHealthProxyMoby()
    {
        var go = new GameObject("Health Proxy Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = DUMMY_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Health Proxy");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Raids/PathGraph", priority = 10)]
    public static void CreatePathGraph()
    {
        var prefab = UnityHelper.GetRaidsPrefab("PathGraph");
        if (!prefab)
        {
            Debug.LogError("Unable to find Raids PathGraph prefab");
            return;
        }

        var go = Instantiate(prefab);
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

    #endregion


    #region Helpers

    public static float ScaleDamage(float baseDamage, float maxDamage, float scale, int difficulty)
    {
        var dfactor = DIFFICULTY_FACTORS[difficulty];
        var damage = baseDamage * (1 + (0.03f * scale * dfactor));
        if (maxDamage > 0 && damage > maxDamage)
            damage = maxDamage;

        return damage;
    }

    public static float ScaleSpeed(float baseSpeed, float maxSpeed, float scale, int difficulty)
    {
        var dfactor = DIFFICULTY_FACTORS[difficulty];
        float speed = baseSpeed * (1 + (0.03f * scale * dfactor));
        if (maxSpeed > 0 && speed > maxSpeed)
            speed = maxSpeed;

        return speed;
    }

    public static float ScaleHealth(float baseHealth, float maxHealth, float scale, int difficulty)
    {
        var dfactor = DIFFICULTY_FACTORS[difficulty];
        var health = baseHealth * Mathf.Pow(1 + (0.05f * scale * dfactor), 2);
        if (maxHealth > 0 && health > maxHealth)
            health = maxHealth;

        return health;
    }

    #endregion

}

public enum RaidsMob
{
    Zombie,
    Swarmer,
    Swamper,
    StalkerTurret,
    Leviathan,
    DZStriker,
    Executioner
}

[Flags]
public enum RaidsMobBangle
{
    BANGLE_0001 = 0x0001,
    BANGLE_0002 = 0x0002,
    BANGLE_0004 = 0x0004,
    BANGLE_0008 = 0x0008,
    BANGLE_0010 = 0x0010,
    BANGLE_0020 = 0x0020,
    BANGLE_0040 = 0x0040,
    BANGLE_0080 = 0x0080,
    BANGLE_0100 = 0x0100,
    BANGLE_0200 = 0x0200,
    BANGLE_0400 = 0x0400,
    BANGLE_0800 = 0x0800,
    BANGLE_1000 = 0x1000,
    BANGLE_2000 = 0x2000,
    BANGLE_4000 = 0x4000,
}

[Serializable]
public class RaidsMobSpawnParam
{
    public string Name;
    public bool Disabled;
    public RaidsMob Mob;
    public int Variant;
    [Tooltip("For Mobs with team textures only.")]
    public DLTeamIds TexturePalette = DLTeamIds.Blue;

    [Min(0)] public float SizeMultiplier = 1;

    [Min(0)] public float XpMultiplier = 1;
    [Min(0)] public float BoltsMultiplier = 1;

    [Min(0)] public float DamageMultiplier = 1;
    [Min(0), Tooltip("Adjusts the rate at which the mob's damage will scale with respect to the difficulty. A larger value will result in stronger mobs in higher difficulties.")] public float DamageDifficultyRateMultiplier = 1;

    [Min(0)] public float SpeedMultiplier = 1;
    [Min(0), Tooltip("Adjusts the rate at which the mob's speed will scale with respect to the difficulty. A larger value will result in faster mobs in higher difficulties.")] public float SpeedDifficultyRateMultiplier = 1;

    [Min(0)] public float HealthMultiplier = 1;
    [Min(0), Tooltip("Adjusts the rate at which the mob's health will scale with respect to the difficulty. A larger value will result in tougher mobs in higher difficulties.")] public float HealthDifficultyRateMultiplier = 1;

    [Min(0), Tooltip("For ranged attacks, how far away from the target the mob can be to fire.")] public float RangedAttackDistance = 50;
    [Min(0), Tooltip("How far out a mob can lock onto a target from.")] public float VisionRange = 50;
    [Min(0), Tooltip("How narrow or wide the mob's vision is."), Range(0, 360)] public float PeripheralVisionDegrees = 135;
    [Min(0), Tooltip("Range that a mob will always aggro a target, regardless of their peripheral vision.")] public float ForceAggroRange = 10;
    [Min(0), Tooltip("In seconds, how long after the mob loses sight of its target before it will exit the Aggro state.")] public float OutOfSightDeAggroTime = 15;

    public string GetDef()
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = RaidsMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new RaidsMobsScriptableObject.RaidsMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);

        sb.AppendLine("  {");
        sb.AppendLine($"    .MobCreate = &{mobPrefix}Create,");
        sb.AppendLine($"    .MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"    .RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"    .Scale = {SizeMultiplier},");
        sb.AppendLine($"    .OClass = {variant.OClass},");
        sb.AppendLine($"    .BlipType = {(int)defaults.BlipType},");
        sb.AppendLine($"    .BlipTeam = {(int)defaults.BlipTeam},");
        sb.AppendLine($"    .TeamPalette = {(int)TexturePalette},");
        //sb.AppendLine($"    .Name = \"{Name?.Replace("\"", "")}\",");
        sb.AppendLine($"    .Config = {{");
        sb.AppendLine($"      .Xp = {(int)(defaults.Xp * XpMultiplier)},");
        sb.AppendLine($"      .Bolts = {(int)(defaults.Bolts * BoltsMultiplier)},");
        sb.AppendLine($"      .Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"      .Damage = {defaults.Damage * DamageMultiplier},");
        sb.AppendLine($"      .MaxDamage = {defaults.DamageMax},");
        sb.AppendLine($"      .DamageScale = {defaults.DamageScale * DamageDifficultyRateMultiplier},");
        sb.AppendLine($"      .Speed = {defaults.Speed * SpeedMultiplier},");
        sb.AppendLine($"      .MaxSpeed = {defaults.SpeedMax},");
        sb.AppendLine($"      .SpeedScale = {defaults.SpeedScale * SpeedDifficultyRateMultiplier},");
        sb.AppendLine($"      .Health = {defaults.Health * HealthMultiplier},");
        sb.AppendLine($"      .MaxHealth = {defaults.HealthMax},");
        sb.AppendLine($"      .HealthScale = {defaults.HealthScale * HealthDifficultyRateMultiplier},");
        sb.AppendLine($"      .AttackRadius = {defaults.AttackRadius * SizeMultiplier},");
        sb.AppendLine($"      .HitRadius = {defaults.HitRadius * SizeMultiplier},");
        sb.AppendLine($"      .CollRadius = {defaults.CollRadius * SizeMultiplier},");
        sb.AppendLine($"      .AutoAggroMaxRange = {ForceAggroRange},");
        sb.AppendLine($"      .VisionRange = {VisionRange},");
        sb.AppendLine($"      .RangedMaxDistanceToTarget = {RangedAttackDistance},");
        sb.AppendLine($"      .PeripheryRangeTheta = {PeripheralVisionDegrees * 0.5f * Mathf.Deg2Rad},");
        sb.AppendLine($"      .OutOfSightDeAggroTickCount = {(int)(OutOfSightDeAggroTime * 60)},");
        sb.AppendLine($"      .ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"      .AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"    }}");
        sb.AppendLine("  },");

        return sb.ToString();
    }

}
