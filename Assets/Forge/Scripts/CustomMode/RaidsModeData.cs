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
    public static readonly int GOLDBOLT_OCLASS = 0x400E;
    public static readonly int COUNTER_OCLASS = 0x400F;
    public static readonly int SOULCOLLECTOR_OCLASS = 0x4010;

    public static readonly float[] DIFFICULTY_FACTORS = new float[]
    {
        0,
        25,
        150,
        500,
        1250
    };

    public enum RAIDS_MISSION_TYPES
    {
        Hub,
        OpenWorld,
        Raid
    }

    public override DLCustomModeIds CustomMode => DLCustomModeIds.Raids;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;
    public RAIDS_MISSION_TYPES MissionType = RAIDS_MISSION_TYPES.OpenWorld;
    [Tooltip("How much of the render budget to allocate for the map.\n\nThe larger the number, the more mob billboards (shellshock) will appear.")] public int MapBaseComplexity = 5000;
    [Tooltip("Minimum player level required to visit planet.")] public uint MinLevelRequired = 0;
    public int Cost1Star = 0;
    public int Cost2Star = 0;
    public int Cost3Star = 0;
    public int Cost4Star = 0;
    public int Cost5Star = 0;
    public string Author;
    [Multiline] public string Description;

    [Header("Zones (Open World Only)")]
    public List<RaidsZone> Zones = new List<RaidsZone>();

    [Header("Challenges")]
    public List<RaidsChallenge> Challenges = new List<RaidsChallenge>();

    [Header("Contracts")]
    public List<RaidsContractRule> ContractRules = new List<RaidsContractRule>();

    [Header("Mobs"), Tooltip("Your map's customized mob list. Max of 16.")]
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
        while (Mobs != null && Mobs.Count > 16) Mobs.RemoveAt(16);

        // at some point migrate the reuseable moby logic out of raids and into common
        //var commonCodeGen = FindObjectOfType<CommonCodeGen>();
        //if (!commonCodeGen)
        //{
        //    this.gameObject.AddComponent<CommonCodeGen>();
        //}
    }

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var isRaidsMap = mapConfig.DLForceCustomMode == DLCustomModeIds.Raids;
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

        if (isRaidsMap)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/levelselect.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/contracts.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/inventory.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/spawner.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/vendor.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/badges.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/bank.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/collectible.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mobs/mob.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/npc.o");
        }

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/map.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gate.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/messager.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/checkpoint.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mover.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/platform.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/controller.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laserbeam.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laser.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pvarpoke.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pathfind.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/maputils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/ammodrop.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/hackerorb.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/blip.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/dummy.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/window.o");

        state.LDFlags.Add("-DGATE");
        if (isRaidsMap) state.LDFlags.Add("-DRAIDS");
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
        state.Includes.Add("#include \"bank.h\"");
        state.Includes.Add("#include \"inventory.h\"");
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
        state.Includes.Add("#include \"platform.h\"");
        state.Includes.Add("#include \"mob.h\"");
        state.Includes.Add("#include \"shared.h\"");
        state.Includes.Add("#include \"pathfind.h\"");
        state.Includes.Add("#include \"vendor.h\"");
        state.Includes.Add("#include \"badges.h\"");
        state.Includes.Add("#include \"ammodrop.h\"");
        state.Includes.Add("#include \"hackerorb.h\"");
        state.Includes.Add("#include \"blip.h\"");
        state.Includes.Add("#include \"dummy.h\"");
        state.Includes.Add("#include \"collectible.h\"");
        state.Includes.Add("#include \"levelselect.h\"");
        state.Includes.Add("#include \"contracts.h\"");

        state.Declarations.Add("void configInit(void);");
        state.Declarations.Add("struct RaidsMapConfig MapConfig __attribute__((section(\".config\"))) = {\r\n  .Magic = MAP_CONFIG_MAGIC,\r\n  .State = NULL,  .TrackWhitelist = NULL,\r\n};");

        if (isRaidsMap)
        {
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid mobForceIntoMapBounds(Moby* moby)\r\n{{\r\n\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nint mapPathCanBeSkippedForTarget(struct PathGraph* path, Moby* moby)\r\n{{\r\n  return 1;\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nint createMob(struct MobCreateArgs* args)\r\n{{\r\n  if (args->SpawnParamsIdx < 0 || args->SpawnParamsIdx >= MapConfig.MobSpawnParamsCount) {{\r\n    DPRINTF(\"unhandled create spawnParamsIdx %d\\n\", args->SpawnParamsIdx);\r\n    return 0;\r\n  }}\r\n\r\n  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];\r\n  if (spawnParams->MobCreate)\r\n    return spawnParams->MobCreate(args);\r\n\r\n  DPRINTF(\"unhandled create spawnParamsIdx %d\\n\", args->SpawnParamsIdx);\r\n  return 0;\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid mapOnFrameTick(void)\r\n{{\r\n  dlPreUpdate();\r\n\r\n  messagerFrameUpdate();\r\n  levelselectFrameTick();\r\n  contractsFrameTick();\r\n  inventoryFrameTick();\r\n  {String.Join("\t\r\n", state.Meta.GetValueOrDefault("RAIDS_FRAMEUPDATE") ?? new List<string>())}\r\n\r\n  dlPostUpdate();\r\n}}\r\n");
        }

        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes(void)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)())0x005ce1d8)();\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes2(u32 a0)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)(u32))0x0059b320)(a0);\r\n}}\r\n");

        if (isRaidsMap)
        {
            state.InitBody.Add($"configInit();");
            state.InitBody.Add($"bankInit();");
            state.InitBody.Add($"inventoryInit();");
            state.InitBody.Add($"mapInit();");
            state.InitBody.Add($"mobInit();");
            state.InitBody.Add($"levelselectInit();");
            state.InitBody.Add($"contractsInit();");
            state.InitBody.Add($"spawnerInit();");
            state.InitBody.Add($"vendorInit();");
            state.InitBody.Add($"ammodropInit();");
            state.InitBody.Add($"badgesInit();");
            state.InitBody.Add($"collectibleInit();");
            state.InitBody.Add($"npcInit();");
        }
        state.InitBody.Add($"moverInit();");
        state.InitBody.Add($"platformInit();");
        state.InitBody.Add($"controllerInit();");
        state.InitBody.Add($"gateInit();");
        state.InitBody.Add($"messagerInit();");
        state.InitBody.Add($"checkpointInit();");
        state.InitBody.Add($"laserInit();");
        state.InitBody.Add($"pvarpokeInit();");
        state.InitBody.Add($"hackerorbInit();");
        state.InitBody.Add($"blipInit();");
        state.InitBody.Add($"dummyInit();");

        // get gubers
        if (isRaidsMap)
        {
            state.GetGuberCase.Add("case SPAWNER_OCLASS: return spawnerGetGuber(moby);");
            if (mobTypes.Contains(RaidsMob.DZStriker))
                state.GetGuberCase.Add("case MOBY_ID_DZ_STRIKER_TORSO_RED: return (moby->PParent ? moby->PParent->Guber : moby->Guber);");
        }
        state.GetGuberCase.Add("case GATE_OCLASS: return gateGetGuber(moby);");
        state.GetGuberCase.Add("case MOVER_OCLASS: return moverGetGuber(moby);");
        state.GetGuberCase.Add("case PLATFORM_FLIPPER_MOBY_OCLASS: return platformGetGuber(moby);");
        state.GetGuberCase.Add("case PLATFORM_PIVOT_MOBY_OCLASS: return platformGetGuber(moby);");
        state.GetGuberCase.Add("case CONTROLLER_OCLASS: return controllerGetGuber(moby);");
        state.GetGuberCase.Add("case CHECKPOINT_MANAGER_OCLASS: return checkpointGetGuber(moby);");
        state.GetGuberCase.Add("case CHECKPOINT_OCLASS: return checkpointGetGuber(moby);");
        state.GetGuberCase.Add("case DUMMY_OCLASS: return dummyGetGuber(moby);");
        state.GetGuberCase.Add("case MOBY_ID_HACKER_ORB: return hackerorbGetGuber(moby);");

        // handle events
        if (isRaidsMap)
        {
            state.HandleGuberEventCase.Add("case SPAWNER_OCLASS: spawnerHandleEvent(moby, event); break;");
            if (mobTypes.Contains(RaidsMob.DZStriker))
                state.HandleGuberEventCase.Add("case MOBY_ID_DZ_STRIKER_TORSO_RED: dzstrikerTorsoOnSpawn(moby, event); break;");
        }
        state.HandleGuberEventCase.Add("case GATE_OCLASS: gateHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case MOVER_OCLASS: moverHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case PLATFORM_FLIPPER_MOBY_OCLASS: platformHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case PLATFORM_PIVOT_MOBY_OCLASS: platformHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CONTROLLER_OCLASS: controllerHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CHECKPOINT_MANAGER_OCLASS: checkpointHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CHECKPOINT_OCLASS: checkpointHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case DUMMY_OCLASS: dummyHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case MOBY_ID_HACKER_ORB: hackerorbHandleEvent(moby, event); break;");

        if (isRaidsMap)
        {
            state.InitBody.Add($"MapConfig.OnMobCreateFunc = &createMob;");
            state.InitBody.Add($"MapConfig.OnMobUpdateFunc = &mapOnMobUpdate;");
            state.InitBody.Add($"MapConfig.OnMobDamagedFunc = &mapOnMobDamaged;");
            state.InitBody.Add($"MapConfig.OnMobKilledFunc = &mapOnMobKilled;");
            state.InitBody.Add($"MapConfig.OnMobDestroyedFunc = &mapOnMobDestroyed;");
            state.InitBody.Add($"MapConfig.OnMobSpawnedFunc = &mapOnMobSpawned;");
            state.InitBody.Add($"MapConfig.CreateAmmoDropAtFunc = &ammodropCreateAt;");
            state.InitBody.Add($"MapConfig.OnFrameTickFunc = &mapOnFrameTick;");
        }

        state.InitBody.Add($"HOOK_JAL(0x003bd854, &onBeforeUpdateHeroes);");
        state.InitBody.Add($"HOOK_JAL(0x0051f648, &onBeforeUpdateHeroes2);");

        if (isRaidsMap)
        {
            state.InitBody.Add("respawnAllPlayers();");
        }

        if (isRaidsMap)
        {
            state.MainBodyReady.Add("mapStart();");
            state.MainBodyReady.Add("levelselectStart();");
            state.MainBodyReady.Add("spawnerStart();");
            state.MainBodyReady.Add("checkpointStart();");
            state.MainBodyReady.Add("vendorStart();");
            state.MainBodyReady.Add("badgesStart();");
            state.MainBodyReady.Add("ammodropStart();");
            state.MainBodyReady.Add("npcStart();");
            if (MissionType == RAIDS_MISSION_TYPES.OpenWorld)
                state.MainBodyReady.Add("mapApplyZoning();");
        }
        state.MainBodyReady.Add("moverStart();");
        state.MainBodyReady.Add("controllerStart();");
        state.MainBodyReady.Add("gateStart();");
        state.MainBodyReady.Add("laserStart();");
        state.MainBodyReady.Add("pvarpokeStart();");
        state.MainBodyReady.Add("dummyStart();");

        state.MainBody.Add("mapTick();");
        if (isRaidsMap)
        {
            state.MainBody.Add("bankTick();");
            state.MainBody.Add("inventoryTick();");
            state.MainBody.Add("contractsTick();");
            state.MainBody.Add("mobTick();");
        }
        state.MainBody.Add("for (i = 0; i < PathsCount; ++i) pathTick(&Paths[i]);");
        state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
        state.MainBody.Add("mapTickEnd();");
    }

    public void Configure(BuildState state, BuildStateStage stage)
    {
        if (stage != BuildStateStage.BeforeBuild) return;

        state.MobyOClasses.Add(8309); // node base (for capture sound)
        state.MobyOClasses.Add(6898); // health box (for health sound; nanoleech)
        state.MobyOClasses.Add(9278); // weapon pickup (for loot drops)
        state.MobyOClasses.Add(13); // bolt (for gold bolt)
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

        // update gold bolt indices
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);
        var goldBoltCount = 0;
        foreach (var goldBoltMoby in mobys.Where(x => x.OClass == GOLDBOLT_OCLASS))
        {
            // set index
            goldBoltMoby.GetPVarValues()[".Index"] = goldBoltCount.ToString();
            ++goldBoltCount;
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

        sb.AppendLine("struct RaidsDifficultyZone mapDifficultyZones[] = {");
        foreach (var zone in Zones)
            sb.AppendLine(zone.GetDef());
        sb.AppendLine("};");
        sb.AppendLine($"int mapDifficultyZonesCount = {Zones.Count};");
        sb.AppendLine("");

        sb.AppendLine("struct RaidsMobContractRule mobContractRules[] = {");
        var contractMobs = new HashSet<RaidsMob>();
        foreach (var contractRule in ContractRules)
        {
            if (contractRule.Disabled) continue;
            if (contractMobs.Count >= 16)
            {
                Debug.LogWarning($"Contract rules has more than the max of 16 entries.");
                break;
            }
            if (!Mobs.Any(m => !m.Disabled && m.Mob == contractRule.Mob && m.Variant == contractRule.Variant))
            {
                Debug.LogWarning($"Contract rules has entry for unused mob {contractRule.Mob} variant {contractRule.Variant}");
                continue;
            }
            if (contractMobs.Contains(contractRule.Mob))
            {
                Debug.LogWarning($"Contract rules has duplicate entry for mob {contractRule.Mob}");
                continue;
            }

            contractMobs.Add(contractRule.Mob);
            sb.AppendLine(contractRule.GetDef());
        }
        sb.AppendLine("};");
        sb.AppendLine($"int mobContractRulesCount = {contractMobs.Count};");
        sb.AppendLine("");

        sb.AppendLine($"int musicTrackWhitelistEnabled = {(OverrideTrackList ? 1 : 0)};");
        sb.AppendLine($"int musicTrackWhitelistCount = {TrackWhitelist.Count};");
        sb.AppendLine("int musicTrackWhitelist[] = {");
        foreach (var track in TrackWhitelist)
            sb.AppendLine($"\t{(int)track},");
        sb.AppendLine("};");
        sb.AppendLine("");

        sb.AppendLine("//--------------------------------------------------------------------------\r\nvoid configInit(void)\r\n{\r\n  MapConfig.MobSpawnParams = mobSpawnParams;\r\n  MapConfig.MobSpawnParamsCount = COUNT_OF(mobSpawnParams);\r\n  MapConfig.MobContractRules = mobContractRules;\r\n  MapConfig.MobContractRulesCount = mobContractRulesCount;\r\n  MapConfig.TrackWhitelist = musicTrackWhitelist;\r\n  MapConfig.TrackWhitelistCount = musicTrackWhitelistCount;\r\n  MapConfig.TrackWhitelistEnabled = musicTrackWhitelistEnabled;\r\n}\r\n");

        return sb.ToString();
    }

    string GetPathContents()
    {
        return PathGraph.ExportGraphsAsC();
    }

    public override void Write(BinaryWriter writer)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);
        var goldBoltCount = mobys.Count(x => x.OClass == GOLDBOLT_OCLASS);
        var challengesCount = Challenges != null ? Challenges.Count : 0;
        var baseOffset = writer.BaseStream.Position;

        writer.Write(RAIDS_VERSION);
        writer.Write((int)MissionType);
        writer.Write(MinLevelRequired);
        writer.Write(goldBoltCount);
        writer.Write(challengesCount);
        writer.Write(Cost1Star);
        writer.Write(Cost2Star);
        writer.Write(Cost3Star);
        writer.Write(Cost4Star);
        writer.Write(Cost5Star);
        writer.WriteString(BinaryHelper.StrToRatchetStr(Author), 31);
        writer.Write((byte)0);
        writer.WriteString(BinaryHelper.StrToRatchetStr(Description), 255);
        writer.Write((byte)0);

        // prewrite header
        var offsets = new List<int>();
        var headerOffset = writer.BaseStream.Position;
        writer.Write(new byte[8 * challengesCount]);

        if (Challenges != null)
        {
            foreach (var challenge in Challenges)
            {
                offsets.Add((int)(writer.BaseStream.Position - baseOffset));
                writer.WriteCString(BinaryHelper.StrToRatchetStr(challenge.Name));
                offsets.Add((int)(writer.BaseStream.Position - baseOffset));
                writer.WriteCString(BinaryHelper.StrToRatchetStr(challenge.Description));
            }
        }

        var endOffset = writer.BaseStream.Position;

        // rewrite header
        writer.BaseStream.Position = headerOffset;
        foreach (var offset in offsets)
            writer.Write(offset);

        writer.BaseStream.Position = endOffset;
    }

    public uint GetSubSort()
    {
        return MinLevelRequired + (uint)((int)MissionType * 10000);
    }

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Create Raids Data", priority = 10)]
    public static void CreateRaidsData()
    {
        var go = new GameObject("Raids");
        var raidsData = go.AddComponent<RaidsModeData>();

        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Mob Spawner Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Mover Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Controller Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Counter Moby", priority = 10)]
    public static void CreateCounterMoby()
    {
        var go = new GameObject("Counter");
        var moby = go.AddComponent<Moby>();
        moby.OClass = COUNTER_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Counter");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Deadlocked/Raids/PVar Poke Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Gate Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Messager Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/NPC Controller Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Checkpoint Manager Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Checkpoint Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Laser (Tripwire) Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Radar Blip Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Health Proxy Moby", priority = 10)]
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

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Gold Bolt Moby", priority = 10)]
    public static void CreateGoldBoltMoby()
    {
        var go = new GameObject("Gold Bolt Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = GOLDBOLT_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.DrawDistance = 128;
        moby.UpdateDistance = 128;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetAssetPrefab(FolderNames.MobyFolder, "13", RCVER.DL, includeGlobal: true);
        moby.PrefabOverrideTransformation = Matrix4x4.TRS(Vector3.zero, Quaternion.FromToRotation(Vector3.right, Vector3.up), Vector3.one * 3);
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Deadlocked/Raids/Soul Collector Moby", priority = 10)]
    public static void CreateSoulCollectorMoby()
    {
        var go = new GameObject("Soul Collector Moby");
        var moby = go.AddComponent<Moby>();
        moby.OClass = SOULCOLLECTOR_OCLASS;
        moby.RCVersion = RCVER.DL;
        moby.UpdateDistance = 255;
        moby.Color = new Color(1, 1, 1, 0.5f);
        moby.PrefabOverride = UnityHelper.GetRaidsPrefab("Soul Collector");
        moby.InitializePVarReferences();
        OnAfterCreateGameObject(go);
    }

    [MenuItem("GameObject/Forge/Deadlocked/Raids/PathGraph", priority = 10)]
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
    Executioner,
    Reaper,
    Tremor
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

    [Min(0), Tooltip("Increase or decrease turn speed.")] public float TurnSpeedMultiplier = 1;
    [Min(0), Tooltip("For ranged attacks, how far away from the target the mob can be to fire.")] public float RangedAttackDistance = 50;
    [Min(0), Tooltip("How far out a mob can lock onto a target from.")] public float VisionRange = 50;
    [Min(0), Tooltip("How narrow or wide the mob's vision is."), Range(0, 360)] public float PeripheralVisionDegrees = 135;
    [Min(0), Tooltip("Range that a mob will always aggro a target, regardless of their peripheral vision.")] public float ForceAggroRange = 10;
    [Min(0), Tooltip("In seconds, how long after the mob loses sight of its target before it will exit the Aggro state.")] public float OutOfSightDeAggroTime = 60;

    public string GetDef()
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = RaidsMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new RaidsMobsScriptableObject.RaidsMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobCreate = &{mobPrefix}Create,");
        sb.AppendLine($"\t\t.MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"\t\t.RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"\t\t.Scale = {SizeMultiplier},");
        sb.AppendLine($"\t\t.OClass = {variant.OClass},");
        sb.AppendLine($"\t\t.BlipType = {(int)defaults.BlipType},");
        sb.AppendLine($"\t\t.BlipTeam = {(int)defaults.BlipTeam},");
        sb.AppendLine($"\t\t.TeamPalette = {(int)TexturePalette},");
        //sb.AppendLine($"\t\t.Name = \"{Name?.Replace("\"", "")}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {(int)(defaults.Xp * XpMultiplier)},");
        sb.AppendLine($"\t\t\t.Bolts = {(int)(defaults.Bolts * BoltsMultiplier)},");
        sb.AppendLine($"\t\t\t.Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"\t\t\t.Damage = {defaults.Damage * DamageMultiplier},");
        sb.AppendLine($"\t\t\t.MaxDamage = {defaults.DamageMax},");
        sb.AppendLine($"\t\t\t.DamageScale = {defaults.DamageScale * DamageDifficultyRateMultiplier},");
        sb.AppendLine($"\t\t\t.Speed = {defaults.Speed * SpeedMultiplier},");
        sb.AppendLine($"\t\t\t.MaxSpeed = {defaults.SpeedMax},");
        sb.AppendLine($"\t\t\t.SpeedScale = {defaults.SpeedScale * SpeedDifficultyRateMultiplier},");
        sb.AppendLine($"\t\t\t.Health = {defaults.Health * HealthMultiplier},");
        sb.AppendLine($"\t\t\t.MaxHealth = {defaults.HealthMax},");
        sb.AppendLine($"\t\t\t.HealthScale = {defaults.HealthScale * HealthDifficultyRateMultiplier},");
        sb.AppendLine($"\t\t\t.TurnSpeed = {defaults.TurnSpeed * TurnSpeedMultiplier},");
        sb.AppendLine($"\t\t\t.AttackRadius = {defaults.AttackRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.HitRadius = {defaults.HitRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.CollRadius = {defaults.CollRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.AutoAggroMaxRange = {ForceAggroRange},");
        sb.AppendLine($"\t\t\t.VisionRange = {VisionRange},");
        sb.AppendLine($"\t\t\t.RangedMaxDistanceToTarget = {RangedAttackDistance},");
        sb.AppendLine($"\t\t\t.PeripheryRangeTheta = {PeripheralVisionDegrees * 0.5f * Mathf.Deg2Rad},");
        sb.AppendLine($"\t\t\t.OutOfSightDeAggroTickCount = {(int)(OutOfSightDeAggroTime * 60)},");
        sb.AppendLine($"\t\t\t.ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"\t\t\t.AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t}}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}

[Serializable]
public class RaidsChallenge
{
    public string Name;
    public string Description;
}

[Serializable]
public class RaidsContractRule
{
    public RaidsMob Mob;

    [Tooltip("Use to specify moby oclass. If variants use same oclass, all will be accepted by the contract.")]
    public int Variant;

    [Min(1)]
    public ushort MinCount = 10;
    [Min(1)]
    public ushort MaxCount = 1000;
    public float BaseXpPerMob = 1;
    public float BaseBoltsPerMob = 1;
    public ushort ExpirationTimeMinutes = 30;
    public bool Disabled;

    public string GetDef()
    {
        var sb = new StringBuilder();

        // add mob oclasses
        var mobConfig = RaidsMobsScriptableObject.Load();
        var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == Mob);
        var variant = mobDefaults.Variants.ElementAtOrDefault(Variant);

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobOClass = {variant.OClass},");
        sb.AppendLine($"\t\t.MinCount = {MinCount},");
        sb.AppendLine($"\t\t.MaxCount = {MaxCount},");
        sb.AppendLine($"\t\t.XpMult = {BaseXpPerMob},");
        sb.AppendLine($"\t\t.BoltMult = {BaseBoltsPerMob},");
        sb.AppendLine($"\t\t.ExpirationMinutes = {ExpirationTimeMinutes}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}

[Serializable]
public class RaidsZone
{
    public string Name;
    public Cuboid Cuboid;
    public DLRaidsDifficulties Difficulty;

    public string GetDef()
    {
        var sb = new StringBuilder();

        var mapConfig = GameObject.FindObjectOfType<MapConfig>();
        var cuboidIdx = mapConfig.GetIndexOfCuboid(this.Cuboid);
        if (cuboidIdx < 0)
            Debug.LogWarning($"Missing cuboid for Zone {this.Name}");

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.CuboidIdx = {cuboidIdx},");
        sb.AppendLine($"\t\t.Difficulty = {(int)this.Difficulty}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }
}
