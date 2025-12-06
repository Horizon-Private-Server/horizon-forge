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
    public static readonly int SURVIVAL_VERSION = 4;

    public override DLCustomModeIds CustomMode => DLCustomModeIds.Survival;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;
    [Tooltip("How much of the render budget to allocate for the map.\n\nThe larger the number, the more mob billboards (shellshock) will appear.")] public int MapBaseComplexity = 5000;
    public PathGraph MobPathGraph;

    [Header("Config")]
    [Min(0), Tooltip("Increasing will accelerate the difficulty scaling curve.")] public float Difficulty = 1;
    [Min(0)] public float BoltMultiplier = 1;
    [Min(0)] public float XpMultiplier = 1;
    [Min(0), Tooltip("Decreasing the value will have mobs spawn closer to the players.")] public float SpawnDistanceFactor = 1;
    [Min(0), Tooltip("Influences how quickly a player advances rank.")] public float BoltRankMultiplier = 1;
    public List<SurvivalBakedSpawnPointItem> BakedSpawnPoints = new List<SurvivalBakedSpawnPointItem>();

    [Header("Mobs"), Tooltip("Your map's customized mob list. Max of 10.")]
    public List<SurvivalMobSpawnParam> Mobs = new List<SurvivalMobSpawnParam>()
    {
        new SurvivalMobSpawnParam() { Name = "Zombie" }
    };
    public Cuboid MobAllowedArea;

    [Header("Misc Mobs")]
    public string ReactorMinionMobName;
    public List<string> RussianDollMobNames = new List<string>();

    [Header("Special Rounds")]
    public List<SurvivalMobSpecialRoundParam> SpecialRounds = new List<SurvivalMobSpecialRoundParam>()
    {
        new SurvivalMobSpecialRoundParam() { Name = "Boss Round", Disabled = true, MinRound = 25, RepeatEveryNRounds = 25, UnlimitedPostRoundTime = true, MobNamesToSpawn = new List<string>() { "Zombie" } }
    };

    [Header("Gambits")]
    public UnityEngine.Object GambitsSourceCode;
    public List<SurvivalGambit> Gambits = new List<SurvivalGambit>();

    [Header("Stackables")]
    public bool EnableStackables;
    public List<SurvivalStackableItemId> Stackables = new List<SurvivalStackableItemId>();

    [Header("Mystery Box")]
    public List<SurvivalMysteryboxItem> MysteryboxItems = new List<SurvivalMysteryboxItem>()
    {
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.VoxTeddyBear, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.Quad, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.Shield, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.InvisibilityCloak, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.RandomizeWeaponPickups, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.HealthTornado, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.ReviveTotem, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.InfiniteAmmo, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.UpgradeWeapon, Probability = 0.09f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.DreadToken, Probability = 0.3f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.WeaponMod, Probability = 1, ProbabilityLucky = 0f },
    };

    [Header("Debug")]
    public bool DebugPath;
    public bool DebugMove;

    private void OnValidate()
    {
        while (Mobs != null && Mobs.Count > 10) Mobs.RemoveAt(10);
        while (SpecialRounds != null && SpecialRounds.Count > 16) SpecialRounds.RemoveAt(16);

        foreach (var gambit in Gambits)
        {
            if (gambit.Name != null && gambit.Name.Length > 32) gambit.Name = gambit.Name.Substring(0, 32);
            if (gambit.Description != null && gambit.Description.Length > 128) gambit.Description = gambit.Description.Substring(0, 128);
        }
    }

    public override void Write(BinaryWriter writer)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);

        // write version
        writer.Write(SURVIVAL_VERSION);

        // write gambits
        writer.Write(Gambits.Count);
        foreach (var gambit in Gambits)
        {
            writer.WriteCString(gambit.Name);
            writer.WriteCString(gambit.Description);
        }
    }

    #region CodeGen

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);
        var enabledMobs = Mobs.Where(x => !x.Disabled);

        state.SeparateCodeFile = true;

        // copy survival base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "survival"), buildFolder);

        // build config.c and path.c
        File.WriteAllText(Path.Combine(srcFolder, "config.c"), GetConfigContents());
        File.WriteAllText(Path.Combine(srcFolder, "path.c"), GetPathContents());

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/survival.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/path.o");

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/upgrade.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/drop.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mysterybox.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pathfind.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/utils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mobs/mob.o");

        if (EnableStackables)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/stackables.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/stackbox.o");
            state.LDFlags.Add("-DSTACKABLES");
        }

        if (GambitsSourceCode)
        {
            File.WriteAllText(Path.Combine(srcFolder, "gambits.c"), File.ReadAllText(AssetDatabase.GetAssetPath(GambitsSourceCode)));
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gambits.o");
            state.LDFlags.Add("-DGAMBITS");
        }

        state.LDFlags.Add("-DGATE");
        state.LDFlags.Add("-DSURVIVAL");
        state.LDFlags.Add($"-DMAP_BASE_COMPLEXITY={MapBaseComplexity}");
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
        state.Includes.Add("#include \"mob.h\"");
        state.Includes.Add("#include \"shared.h\"");
        state.Includes.Add("#include \"pathfind.h\"");

        // 
        state.Declarations.Add("void survivalInit(void);");
        state.Declarations.Add("void survivalTick(void);");

        // 
        state.InitBody.Add($"survivalInit();");
        state.MainBody.Add($"survivalTick();");

        // get gubers
        state.GetGuberCase.Add("case MYSTERY_BOX_OCLASS: return mboxGetGuber(moby);");

        // handle events
        state.HandleGuberEventCase.Add("case MYSTERY_BOX_OCLASS: mboxHandleEvent(moby, event); break;");

        // 
        state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
    }

    public void Configure(BuildState state)
    {
        state.MobyOClasses.Add(RaidsModeData.LASERBEAM_OCLASS);
        state.MobyOClasses.Add(0x2635); // mysterybox
        state.MobyOClasses.Add(0x2124); // bigal
        state.MobyOClasses.Add(0x263A); // vendor
        if (EnableStackables) state.MobyOClasses.Add(0x2083); // stackbox

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

        // todo
        // patch code segs with survival customizations
        // add custom sprites
        // 
    }

    string GetConfigContents()
    {
        var sb = new StringBuilder();
        var mapConfig = FindObjectOfType<MapConfig>();

        sb.AppendLine("#include <libdl/utils.h>");
        sb.AppendLine("#include \"game.h\"");
        sb.AppendLine("#include \"mob.h\"");
        sb.AppendLine();

        sb.AppendLine("extern struct SurvivalMapConfig MapConfig;");
        sb.AppendLine();

        // mob config
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("struct MobSpawnParams defaultSpawnParams[] = {");
        foreach (var mob in Mobs.Where(x => !x.Disabled))
            sb.AppendLine(mob.GetDef());
        sb.AppendLine("};");
        sb.AppendLine();

        // special rounds config
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("struct SurvivalSpecialRoundParam specialRoundParams[] = {");
        foreach (var param in SpecialRounds.Where(x => !x.Disabled))
            sb.AppendLine(param.GetDef(this));
        sb.AppendLine("};");
        sb.AppendLine();

        // baked config
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("SurvivalBakedConfig_t bakedConfig = {");
        sb.AppendLine($"\t.Difficulty = {Difficulty},");
        sb.AppendLine($"\t.BoltMultiplier = {BoltMultiplier},");
        sb.AppendLine($"\t.XpMultiplier = {XpMultiplier},");
        sb.AppendLine($"\t.SpawnDistanceFactor = {SpawnDistanceFactor},");
        sb.AppendLine($"\t.BoltRankMultiplier = {BoltRankMultiplier},");
        sb.AppendLine("\t.BakedSpawnPoints = {");
        foreach (var item in BakedSpawnPoints)
            sb.AppendLine(item.GetDef());
        sb.AppendLine("\t}");
        sb.AppendLine("};");
        sb.AppendLine();

        // stackbox items
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("int StackboxItems[] = {");
        foreach (var item in Stackables)
            sb.AppendLine($"\t{(int)item},");
        sb.AppendLine("};");
        sb.AppendLine("const int StackboxItemsCount = COUNT_OF(StackboxItems);");
        sb.AppendLine();

        // mysterybox items
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("struct MysteryBoxItemWeight MysteryBoxItemProbabilities[] = {");
        sb.AppendLine(GetMysteryBoxDefs(MysteryboxItems.Select(x => (x.Item, x.Probability))));
        sb.AppendLine("};");
        sb.AppendLine("const int MysteryBoxItemProbabilitiesCount = COUNT_OF(MysteryBoxItemProbabilities);");
        sb.AppendLine();

        // mysterybox lucky tems
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("struct MysteryBoxItemWeight MysteryBoxItemProbabilitiesLucky[] = {");
        sb.AppendLine(GetMysteryBoxDefs(MysteryboxItems.Select(x => (x.Item, x.ProbabilityLucky))));
        sb.AppendLine("};");
        sb.AppendLine("const int MysteryBoxItemProbabilitiesLuckyCount = COUNT_OF(MysteryBoxItemProbabilitiesLucky);");
        sb.AppendLine();

        // russian dolls
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("int russianDollSpawnParamIdxs[] = {");
        foreach (var item in RussianDollMobNames)
            if (Mobs.FirstOrDefault(x => x.Name == item) is SurvivalMobSpawnParam mob)
                sb.AppendLine($"\t{Mobs.IndexOf(mob)},");
        sb.AppendLine("};");
        sb.AppendLine("const int russianDollSpawnParamIdxsCount = COUNT_OF(russianDollSpawnParamIdxs);");
        sb.AppendLine($"int reactorMinionSpawnParamIdx = {Mobs.FindIndex(x => x.Name == ReactorMinionMobName)};");
        sb.AppendLine($"int mobAllowedCuboidIdx = {mapConfig.GetIndexOfCuboid(MobAllowedArea)};");

        sb.AppendLine();
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("void configInit(void)");
        sb.AppendLine("{");
        sb.AppendLine("\tMapConfig.DefaultSpawnParams = defaultSpawnParams;");
        sb.AppendLine("\tMapConfig.DefaultSpawnParamsCount = COUNT_OF(defaultSpawnParams);");
        sb.AppendLine("\tMapConfig.SpecialRoundParams = specialRoundParams;");
        sb.AppendLine("\tMapConfig.SpecialRoundParamsCount = COUNT_OF(specialRoundParams);");
        sb.AppendLine("}");
        sb.AppendLine();

        return sb.ToString();
    }

    string GetPathContents()
    {
        var sb = new StringBuilder();
        sb.AppendLine("#include <libdl/math3d.h>");
        sb.AppendLine();

        if (MobPathGraph)
        {
            sb.AppendLine(MobPathGraph.ExportAsSurvivalC());
        }
        
        return sb.ToString();
    }

    string GetMysteryBoxDefs(IEnumerable<(SurvivalMysteryboxItemId Item, float Probability)> items)
    {
        var sb = new StringBuilder();
        var sortedItems = items.Where(x => x.Probability > 0).OrderBy(x => x.Probability).ToList();

        // convert target probability into actual probability for our algorithm
        // sort each item by probability ascending
        // real probability is target probability divided by the probability the previous items weren't successful
        var totalProbability = 1f;
        for (int i = 0; i < sortedItems.Count; ++i)
        {
            var item = sortedItems[i];
            var probability = item.Probability / totalProbability;
            if (i == (sortedItems.Count - 1)) probability = 1; // last item is guaranteed to match roll if all others fail

            sb.AppendLine($"\t{{ {(int)item.Item}, {probability} }},");

            totalProbability *= (1 - item.Probability);
        }

        return sb.ToString().TrimEnd();
    }

    #endregion

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Survival/Create Survival Data", priority = 10)]
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
    Ranged_attack,
    Boss
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

public enum SurvivalMysteryboxItemId
{
    WeaponMod = 0,
    ActivatePower,
    UpgradeWeapon,
    DreadToken,
    InvisibilityCloak,
    ReviveTotem,
    InfiniteAmmo,
    ResetGate,
    VoxTeddyBear,
    Quad,
    Shield,
    HealthTornado,
    RandomizeWeaponPickups,
    Count
};

public enum SurvivalBakedSpawnpointType
{
    None = 0,
    Upgrade = 1,
    PlayerStart = 2,
    MysteryBox = 3,
    DemonBell = 4,
    StackBox = 5
};

public enum SurvivalStackableItemId
{
    LowHealthDamageBuff = 0, // stack dmg buf
    ExtraJump = 1, // stack +1 jump
    ExtraShot = 2, // stack +1 shot (dmg mult)
    Hoverboots = 3, // stack movement speed
    AlphaModSpeed = 4, // stack +2 speed mod
    AlphaModImpact = 5, // stack +2 impact mod
    AlphaModArea = 6, // stack +2 area mod
    AlphaModAmmo = 7, // stack +2 ammo mod
    Vampire = 8, // stack +X health gain
    ExplodingEnemies = 9, // stack +X damage per explosion
    Count
};

public enum SurvivalMobStatIds
{
    None = 0,
    Zombie = 1,
    ZombieFreeze = 2,
    ZombieAcid = 3,
    ZombieGhost = 4,
    ZombieExplode = 5,
    Tremor = 6,
    Executioner = 7,
    Swarmer = 8,
    Reactor = 9,
    Reaper = 10,
    Leviathan = 11,
    Count
};

[Flags]
public enum SurvivalMobSpawnType
{
    Random = 0,
    SemiNearPlayer = 1,
    NearPlayer = 2,
    OnPlayer = 4,
    NearHealthbox = 8,
};

[Serializable]
public class SurvivalGambit
{
    public string Name;
    public string Description;
}

[Serializable]
public class SurvivalMysteryboxItem
{
    public SurvivalMysteryboxItemId Item;
    [Range(0f, 1f)] public float Probability;
    [Range(0f, 1f)] public float ProbabilityLucky;
}

[Serializable]
public class SurvivalBakedSpawnPointItem
{
    public SurvivalBakedSpawnpointType Type;
    public Transform Transform;

    public string GetDef()
    {
        if (!Transform) return string.Empty;

        StringBuilder sb = new StringBuilder();

        var pos = Transform.position;
        var rot = Transform.eulerAngles;

        switch (Type)
        {
            case SurvivalBakedSpawnpointType.PlayerStart:
            case SurvivalBakedSpawnpointType.MysteryBox:
                {
                    rot.y -= 90f;
                    break;
                }
        }

        var rotX = Mathf.DeltaAngle(0, rot.x) * -Mathf.Deg2Rad;
        var rotY = Mathf.DeltaAngle(0, rot.y) * -Mathf.Deg2Rad;
        var rotZ = Mathf.DeltaAngle(0, rot.z) * -Mathf.Deg2Rad;
        sb.Append($"\t\t{{ .Type = {(int)Type}, .Params = 0, .Position = {{ {pos.x}, {pos.z}, {pos.y} }}, .Rotation = {{ {rotX}, {rotZ}, {rotY} }} }},");

        return sb.ToString();
    }
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

    [Header("Spawn Parameters")]
    public bool SpecialRoundOnly = false;
    public int MinRound = 0;
    public int MaxSpawnedAtOnce = 0;
    public int MaxSpawnedPerRound = 0;
    [Range(0, 1f)] public float Probability = 1;
    public SurvivalMobSpawnType SpawnType = SurvivalMobSpawnType.Random;
    public int CooldownTicks = 60;
    public float CooldownOffsetPerRoundFactor = 0;

    [Header("Mob Parameters")]
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

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobCreate = &{mobPrefix}Create,");
        sb.AppendLine($"\t\t.MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"\t\t.RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"\t\t.Scale = {SizeMultiplier},");
        sb.AppendLine($"\t\t.OClass = {variant.OClass},");
        sb.AppendLine($"\t\t.BlipType = {(int)defaults.BlipType},");
        sb.AppendLine($"\t\t.MaxSpawnedAtOnce = {MaxSpawnedAtOnce},");
        sb.AppendLine($"\t\t.MaxSpawnedPerRound = {MaxSpawnedPerRound},");
        sb.AppendLine($"\t\t.MinRound = {Math.Clamp(MinRound, 0, int.MaxValue)},");
        sb.AppendLine($"\t\t.CooldownTicks = {(int)CooldownTicks},");
        sb.AppendLine($"\t\t.CooldownOffsetPerRoundFactor = {CooldownOffsetPerRoundFactor},");
        sb.AppendLine($"\t\t.Probability = {Probability},");
        sb.AppendLine($"\t\t.SpawnType = {(int)SpawnType},");
        sb.AppendLine($"\t\t.SpecialRoundOnly = {(SpecialRoundOnly ? 1 : 0)},");
        sb.AppendLine($"\t\t.StatId = {(int)defaults.StatId},");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {(ushort)Math.Clamp(defaults.Xp * XpMultiplier, 0, ushort.MaxValue)},");
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
        sb.AppendLine($"\t\t\t.AttackRadius = {defaults.AttackRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.HitRadius = {defaults.HitRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.CollRadius = {defaults.CollRadius * SizeMultiplier},");
        sb.AppendLine($"\t\t\t.ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"\t\t\t.AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t\t.MobAttribute = {(int)Attributes},");
        sb.AppendLine($"\t\t\t.SharedXp = {1},");
        sb.AppendLine($"\t\t}}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}

[Serializable]
public class SurvivalMobSpecialRoundParam
{
    public string Name;
    public bool Disabled;
    public int MinRound = 25;
    public int RepeatEveryNRounds = 25;
    public int RepeatCount = 0;
    public float SpawnCountFactor = 1;
    public float SpawnRateFactor = 1;
    [Range(0, 50)] public int MaxSpawnedAtOnce = 50;
    public bool UnlimitedPostRoundTime = false;
    public bool DisableDrops = false;

    public List<string> MobNamesToSpawn = new List<string>();

    public string GetDef(SurvivalModeData survivalData)
    {
        var sb = new StringBuilder();

        var name = Name?.Replace("\"", "") ?? string.Empty;
        if (name.Length >= 32)
            name = name.Substring(0, 31);

        var spawnParamIdxs = new List<int>();
        foreach (var item in MobNamesToSpawn)
        {
            var spawnParam = survivalData.Mobs.FirstOrDefault(x => x.Name == item);
            if (spawnParam == null)
            {
                Debug.LogWarning($"SpecialRound unable to find mob with name {item}");
                continue;
            }

            if (spawnParam.Disabled) continue;

            spawnParamIdxs.Add(survivalData.Mobs.IndexOf(spawnParam));
        }

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.MinRound = {MinRound},");
        sb.AppendLine($"\t\t.RepeatEveryNRounds = {RepeatEveryNRounds},");
        sb.AppendLine($"\t\t.RepeatCount = {RepeatCount},");
        sb.AppendLine($"\t\t.UnlimitedPostRoundTime = {(UnlimitedPostRoundTime ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisableDrops = {(DisableDrops ? 1 : 0)},");
        sb.AppendLine($"\t\t.MaxSpawnedAtOnce = {MaxSpawnedAtOnce},");
        sb.AppendLine($"\t\t.SpawnCountFactor = {SpawnCountFactor},");
        sb.AppendLine($"\t\t.SpawnRateFactor = {SpawnRateFactor},");
        sb.AppendLine($"\t\t.SpawnParamCount = {Math.Min(4, spawnParamIdxs.Count)},");
        sb.AppendLine("\t\t.SpawnParamIds = {");
        for (int i = 0; i < 4; ++i)
            sb.AppendLine($"\t\t\t{(i < spawnParamIdxs.Count ? spawnParamIdxs[i] : -1)}");
        sb.AppendLine("\t\t}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}
