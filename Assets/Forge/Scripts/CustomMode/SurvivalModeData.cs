using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

public class SurvivalModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int SURVIVAL_VERSION = 3;

    public override DLCustomModeIds CustomMode => DLCustomModeIds.Survival;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;
    [Tooltip("How much of the render budget to allocate for the map.\n\nThe larger the number, the more mob billboards (shellshock) will appear.")] public int MapBaseComplexity = 5000;

    [Header("Mobs"), Tooltip("Your map's customized mob list. Max of 16.")]
    public List<SurvivalMobSpawnParam> Mobs = new List<SurvivalMobSpawnParam>()
    {
        new SurvivalMobSpawnParam() { Name = "Zombie" }
    };

    [Header("Debug")]
    public bool DebugPath;
    public bool DebugMove;

    private void OnValidate()
    {
        while (Mobs != null && Mobs.Count > 16) Mobs.RemoveAt(16);
    }

    #region CodeGen

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var isSurvivalMap = mapConfig.DLForceCustomMode == DLCustomModeIds.Survival;
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);
        var enabledMobs = Mobs.Where(x => !x.Disabled);

        state.SeparateCodeFile = true;

        // copy raids base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "survival"), buildFolder);

        // build config.c and path.c
        File.WriteAllText(Path.Combine(srcFolder, "config.c"), GetConfigContents());
        File.WriteAllText(Path.Combine(srcFolder, "path.c"), GetPathContents());

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/path.o");

        if (isSurvivalMap)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/spawner.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mobs/mob.o");
        }

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/map.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gate.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/messager.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mover.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/controller.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laserbeam.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/laser.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pvarpoke.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pathfind.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/maputils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/hackerorb.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/blip.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/dummy.o");

        state.LDFlags.Add("-DGATE");
        if (isSurvivalMap) state.LDFlags.Add("-DSURVIVAL");
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

        state.Declarations.Add("void configInit(void);");
        state.Declarations.Add("struct RaidsMapConfig MapConfig __attribute__((section(\".config\"))) = {\r\n  .Magic = MAP_CONFIG_MAGIC,\r\n  .State = NULL,  .TrackWhitelist = NULL,\r\n};");

        if (isSurvivalMap)
        {
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid mobForceIntoMapBounds(Moby* moby)\r\n{{\r\n\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nint mapPathCanBeSkippedForTarget(struct PathGraph* path, Moby* moby)\r\n{{\r\n  return 1;\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nint createMob(struct MobCreateArgs* args)\r\n{{\r\n  if (args->SpawnParamsIdx < 0 || args->SpawnParamsIdx >= MapConfig.MobSpawnParamsCount) {{\r\n    DPRINTF(\"unhandled create spawnParamsIdx %d\\n\", args->SpawnParamsIdx);\r\n    return 0;\r\n  }}\r\n\r\n  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];\r\n  if (spawnParams->MobCreate)\r\n    return spawnParams->MobCreate(args);\r\n\r\n  DPRINTF(\"unhandled create spawnParamsIdx %d\\n\", args->SpawnParamsIdx);\r\n  return 0;\r\n}}\r\n");
            state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid mapOnFrameTick(void)\r\n{{\r\n  dlPreUpdate();\r\n\r\n  messagerFrameUpdate();\r\n  levelselectFrameTick();\r\n  inventoryFrameTick();\r\n  {String.Join("  \r\n", state.Meta.GetValueOrDefault("RAIDS_FRAMEUPDATE") ?? new List<string>())}\r\n\r\n  dlPostUpdate();\r\n}}\r\n");
        }

        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes(void)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)())0x005ce1d8)();\r\n}}\r\n");
        state.Functions.Add($"//--------------------------------------------------------------------------\r\nvoid onBeforeUpdateHeroes2(u32 a0)\r\n{{\r\n  gateSetCollision(1);\r\n  ((void (*)(u32))0x0059b320)(a0);\r\n}}\r\n");

        if (isSurvivalMap)
        {
            state.InitBody.Add($"configInit();");
            state.InitBody.Add($"bankInit();");
            state.InitBody.Add($"inventoryInit();");
            state.InitBody.Add($"mapInit();");
            state.InitBody.Add($"mobInit();");
            state.InitBody.Add($"levelselectInit();");
            state.InitBody.Add($"spawnerInit();");
            state.InitBody.Add($"vendorInit();");
            state.InitBody.Add($"ammodropInit();");
            state.InitBody.Add($"badgesInit();");
            state.InitBody.Add($"collectibleInit();");
            state.InitBody.Add($"npcInit();");
        }
        state.InitBody.Add($"moverInit();");
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
        if (isSurvivalMap)
        {
            state.GetGuberCase.Add("case SPAWNER_OCLASS: return spawnerGetGuber(moby);");
            state.GetGuberCase.Add("case MOBY_ID_DZ_STRIKER_TORSO_RED: return (moby->PParent ? moby->PParent->Guber : moby->Guber);");
        }
        state.GetGuberCase.Add("case GATE_OCLASS: return gateGetGuber(moby);");
        state.GetGuberCase.Add("case MOVER_OCLASS: return moverGetGuber(moby);");
        state.GetGuberCase.Add("case CONTROLLER_OCLASS: return controllerGetGuber(moby);");
        state.GetGuberCase.Add("case CHECKPOINT_MANAGER_OCLASS: return checkpointGetGuber(moby);");
        state.GetGuberCase.Add("case CHECKPOINT_OCLASS: return checkpointGetGuber(moby);");
        state.GetGuberCase.Add("case DUMMY_OCLASS: return dummyGetGuber(moby);");
        state.GetGuberCase.Add("case MOBY_ID_HACKER_ORB: return hackerorbGetGuber(moby);");

        // handle events
        if (isSurvivalMap)
        {
            state.HandleGuberEventCase.Add("case SPAWNER_OCLASS: spawnerHandleEvent(moby, event); break;");
            state.HandleGuberEventCase.Add("case MOBY_ID_DZ_STRIKER_TORSO_RED: dzstrikerTorsoOnSpawn(moby, event); break;");
        }
        state.HandleGuberEventCase.Add("case GATE_OCLASS: gateHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case MOVER_OCLASS: moverHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CONTROLLER_OCLASS: controllerHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CHECKPOINT_MANAGER_OCLASS: checkpointHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case CHECKPOINT_OCLASS: checkpointHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case DUMMY_OCLASS: dummyHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case MOBY_ID_HACKER_ORB: hackerorbHandleEvent(moby, event); break;");

        if (isSurvivalMap)
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

        if (isSurvivalMap)
        {
            state.InitBody.Add("respawnAllPlayers();");
        }

        if (isSurvivalMap)
        {
            state.MainBodyReady.Add("mapStart();");
            state.MainBodyReady.Add("levelselectStart();");
            state.MainBodyReady.Add("spawnerStart();");
            state.MainBodyReady.Add("checkpointStart();");
            state.MainBodyReady.Add("vendorStart();");
            state.MainBodyReady.Add("badgesStart();");
            state.MainBodyReady.Add("ammodropStart();");
            state.MainBodyReady.Add("npcStart();");
        }
        state.MainBodyReady.Add("moverStart();");
        state.MainBodyReady.Add("controllerStart();");
        state.MainBodyReady.Add("gateStart();");
        state.MainBodyReady.Add("laserStart();");
        state.MainBodyReady.Add("pvarpokeStart();");
        state.MainBodyReady.Add("dummyStart();");

        state.MainBody.Add("mapTick();");
        if (isSurvivalMap)
        {
            state.MainBody.Add("bankTick();");
            state.MainBody.Add("inventoryTick();");
            state.MainBody.Add("mobTick();");
        }
        state.MainBody.Add("for (i = 0; i < PathsCount; ++i) pathTick(&Paths[i]);");
        state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
        state.MainBody.Add("mapTickEnd();");
    }

    public void Configure(BuildState state)
    {
        state.MobyOClasses.Add(RaidsModeData.LASERBEAM_OCLASS);

        // add mob oclasses
        var mobConfig = SurvivalMobsScriptableObject.Load();
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
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);

        writer.Write(SURVIVAL_VERSION);
    }

    #endregion

    #region Menu Items

    [MenuItem("GameObject/Forge/Survival/Create Survival Data", priority = 10)]
    public static void CreateSurvivalData()
    {
        var go = new GameObject("Survival");
        var survivalData = go.AddComponent<SurvivalModeData>();

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

}

public enum SurvivalMob
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

public enum SurvivalMobAttributes
{
    None = 0,
    Freeze,
    Acid,
    Ghost,
    Explode,
    Russian_doll,
}

[Flags]
public enum SurvivalMobBangle
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
public class SurvivalMobSpawnParam
{
    public string Name;
    public bool Disabled;
    public SurvivalMob Mob;
    public int Variant;
    public int Behavior;
    public SurvivalMobAttributes Attributes;
    [ColorUsage(showAlpha: false)] public Color PrimaryColor = new Color(0.3f, 0.3f, 0.3f, 1);
    [ColorUsage(showAlpha: false)] public Color GlowColor = new Color(0.1f, 0.1f, 0.1f, 1);
    [ColorUsage(showAlpha: false)] public Color LODColor = new Color(0.5f, 0.5f, 0.5f, 1);

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

    public string GetDef()
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new SurvivalMobsScriptableObject.SurvivalMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);

        var name = Name?.Replace("\"", "") ?? string.Empty;
        if (name.Length >= 32)
            name = name.Substring(0, 31);

        sb.AppendLine("  {");
        sb.AppendLine($"    .MobCreate = &{mobPrefix}Create,");
        sb.AppendLine($"    .MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"    .Cost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"    .Scale = {SizeMultiplier},");
        sb.AppendLine($"    .OClass = {variant.OClass},");
        sb.AppendLine($"    .BlipType = {(int)defaults.BlipType},");
        sb.AppendLine($"    .BlipTeam = {(int)defaults.BlipTeam},");
        sb.AppendLine($"    .PrimaryColor = {UnityHelper.GetColor(PrimaryColor, forceAlpha: 0)},");
        sb.AppendLine($"    .GlowColor = {UnityHelper.GetColor(GlowColor, forceAlpha: 0x80)},");
        sb.AppendLine($"    .LODColor = {UnityHelper.GetColor(LODColor, forceAlpha: 0)},");
        //sb.AppendLine($"    .TeamPalette = {(int)TexturePalette},");
        sb.AppendLine($"    .Name = \"{name}\",");
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
        sb.AppendLine($"      .TurnSpeed = {defaults.TurnSpeed * TurnSpeedMultiplier},");
        sb.AppendLine($"      .AttackRadius = {defaults.AttackRadius * SizeMultiplier},");
        sb.AppendLine($"      .HitRadius = {defaults.HitRadius * SizeMultiplier},");
        sb.AppendLine($"      .CollRadius = {defaults.CollRadius * SizeMultiplier},");
        sb.AppendLine($"      .RangedMaxDistanceToTarget = {RangedAttackDistance},");
        sb.AppendLine($"      .ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"      .AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"    }}");
        sb.AppendLine("  },");

        return sb.ToString();
    }

}
