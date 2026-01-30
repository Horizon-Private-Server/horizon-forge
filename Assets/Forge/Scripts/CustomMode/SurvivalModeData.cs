using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

public class SurvivalModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int SURVIVAL_VERSION = 7;
    public const int DEMONBELL_OCLASS = 0x2479;
    public const int BANK_OCLASS = 0x1F7;
    public const int STACKBOX_OCLASS = 0x2083;
    public const int SURVIVAL_MAX_SPAWNED_MOBS = 50;

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
    [Min(0), Tooltip("Lower values will increase weapon pickup respawn frequency.")] public float WeaponPickupCooldownFactor = 1;
    public bool HidePrestigeMachineEvery25Rounds = true;
    public bool RandomizeWeaponPickupsAtStart = true;
    [Range(0f, 1f)] public float AmmoDropProbability = 0;

    [Header("Mobs"), Tooltip("Your map's customized mob list. Max of 10.")]
    public List<SurvivalMobSpawnParam> Mobs = new List<SurvivalMobSpawnParam>()
    {
        new SurvivalMobSpawnParam() { Name = "Zombie" }
    };
    public Cuboid MobAllowedArea;
    public Area MobSpawnPoints;
    public string ReactorMinionMobName;

    [Header("Special Rounds")]
    public List<SurvivalMobSpecialRoundParam> SpecialRounds = new List<SurvivalMobSpecialRoundParam>()
    {
        new SurvivalMobSpecialRoundParam() { Name = "Boss Round", Disabled = true, MinRound = 25, RepeatEveryNRounds = 25, UnlimitedPostRoundTime = true, MobNamesToSpawn = new List<string>() { "Zombie" } }
    };

    [Header("Gambits")]
    public List<SurvivalGambit> Gambits = new List<SurvivalGambit>();

    [Header("Stackables")]
    public bool EnableStackables;
    public int StackableBaseCost = 250000;
    public int StackableIncrementCost = 250000;
    public List<SurvivalStackableItemId> Stackables = new List<SurvivalStackableItemId>();

    [Header("Blessings"), HideInInspector]
    public bool EnableBlessings;

    [Header("Mystery Box")]
    public List<SurvivalMysteryboxItem> MysteryboxItems = new List<SurvivalMysteryboxItem>()
    {
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.Quad, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.Shield, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.InvisibilityCloak, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.RandomizeWeaponPickups, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.HealthTornado, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.ReviveTotem, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.InfiniteAmmo, Probability = 0.05f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.UpgradeWeapon, Probability = 0.09f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.VoxTeddyBear, Probability = 0.1f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.DreadToken, Probability = 0.3f, ProbabilityLucky = 0f },
        new SurvivalMysteryboxItem() { Item = SurvivalMysteryboxItemId.WeaponMod, Probability = 1, ProbabilityLucky = 0f },
    };
	
	[Header("Prestige")]
	[Range(1, 5)] public int WeaponPrestigeMax = 5;
	public List<int> PrestigeCostPerLevel = new List<int>() {100000, 300000, 500000, 700000, 1000000};

    [HideInInspector] public bool DebugEnabled;
    [HideInInspector] public bool DebugPath;
    [HideInInspector] public bool DebugMove;
    [HideInInspector] public bool DebugManualSpawning;
    [HideInInspector] public bool DebugInfiniteHealth;
    [HideInInspector] public bool DebugInfiniteAmmo;
    [HideInInspector] public bool DebugPayday;
    [HideInInspector] public bool DebugMoonjump;
    [HideInInspector] public int DebugStartRound;

    public List<SurvivalMobSpawnParam> GetEnabledMobs() => Mobs.Where(x => !x.Disabled).OrderBy(x => x.Probability).ThenBy(x => Mobs.IndexOf(x)).ToList();

    private void OnValidate()
    {
        while (Mobs != null && Mobs.Count > 16) Mobs.RemoveAt(16);
        while (SpecialRounds != null && SpecialRounds.Count > 16) SpecialRounds.RemoveAt(16);

        foreach (var gambit in Gambits)
        {
            if (gambit.Name != null && gambit.Name.Length > 32) gambit.Name = gambit.Name.Substring(0, 32);
            if (gambit.Description != null && gambit.Description.Length > 128) gambit.Description = gambit.Description.Substring(0, 128);
        }
		
		WeaponPrestigeMax = Mathf.Clamp(WeaponPrestigeMax, 1, 5);
        while (PrestigeCostPerLevel.Count < WeaponPrestigeMax) {
			int boltValue;
			switch (PrestigeCostPerLevel.Count)
			{
				case 0: boltValue = 100000; break;
				case 1: boltValue = 300000; break;
				case 2: boltValue = 500000; break;
				case 3: boltValue = 700000; break;
				default: boltValue = 1000000; break;
			}
			PrestigeCostPerLevel.Add(boltValue);
		}
        while (PrestigeCostPerLevel.Count > WeaponPrestigeMax) PrestigeCostPerLevel.RemoveAt(PrestigeCostPerLevel.Count - 1);
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
        var enabledMobs = GetEnabledMobs();

        state.SeparateCodeFile = true;
        state.Debug = DebugEnabled; // override build settings

        // write code seg patches
        WriteCodeSegPatches(mapConfig, new DirectoryInfo(buildFolder).Parent.FullName);

        // ensure all custom mobys without pvar overlay have the correct pvar sizes
        EnsureSurvivalMobysHaveCorrectPVarSize();

        // copy survival base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "survival"), buildFolder);

        // build config.c and path.c
        File.WriteAllText(Path.Combine(srcFolder, "config.c"), GetConfigContents());
        File.WriteAllText(Path.Combine(srcFolder, "path.c"), GetPathContents());

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/survival.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/path.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gambits.o");

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/ammodrop.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/upgrade.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/drop.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/ammosupply.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pool.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/demonbell.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/bankbox.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mysterybox.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pathfind.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/utils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mobs/mob.o");

        if (EnableStackables)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/stackables.o");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/stackbox.o");
            state.LDFlags.Add("-DSTACKABLES");
            state.GetGuberCase.Add("case STACK_BOX_OCLASS: return sboxGetGuber(moby);");
            state.HandleGuberEventCase.Add("case STACK_BOX_OCLASS: sboxHandleEvent(moby, event); break;");
        }

        if (EnableBlessings)
        {
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/blessings.o");
            state.LDFlags.Add("-DBLESSINGS");
        }

        if (HidePrestigeMachineEvery25Rounds) state.LDFlags.Add("-DSHOW_PRESTIGE_EVERY_25");
        if (RandomizeWeaponPickupsAtStart) state.LDFlags.Add("-DRANDOMIZE_WEAPONS_AT_START");
        state.LDFlags.Add("-DGATE");
        state.LDFlags.Add("-DSURVIVAL");
        state.LDFlags.Add("-DGAMBITS");
        state.LDFlags.Add($"-DMAP_BASE_COMPLEXITY={MapBaseComplexity}");
        state.LDFlags.Add($"-DAMMO_DROP_PROBABILITY={AmmoDropProbability}");
        var mobTypes = enabledMobs.Select(x => x.Mob).Distinct();
        foreach (var mobType in mobTypes)
            state.LDFlags.Add($"-DMOB_{mobType.ToString().ToUpper()}");
        foreach (var mob in enabledMobs)
            state.LDFlags.Add($"-DMOB_SPAWN_PARAM_{mob.Name.ToUpper().Trim().Replace(" ", "_")}={enabledMobs.IndexOf(mob)}");

        if (state.Debug)
        {
            if (DebugPath) state.LDFlags.Add("-DDEBUG_PATH");
            if (DebugMove) state.LDFlags.Add("-DDEBUG_MOVE");
            if (DebugManualSpawning) state.LDFlags.Add("-DDEBUG_MANUAL_SPAWNING");
            if (DebugInfiniteHealth) state.LDFlags.Add("-DDEBUG_INFINITE_HEALTH");
            if (DebugInfiniteAmmo) state.LDFlags.Add("-DDEBUG_INFINITE_AMMO");
            if (DebugPayday) state.LDFlags.Add("-DDEBUG_PAYDAY");
            if (DebugMoonjump) state.LDFlags.Add("-DDEBUG_MOONJUMP");
            if (DebugStartRound > 0) state.LDFlags.Add($"-DDEBUG_START_ROUND={DebugStartRound}");
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
        state.GetGuberCase.Add("case UPGRADE_MOBY_OCLASS: return upgradeGetGuber(moby);");
        state.GetGuberCase.Add("case DROP_MOBY_OCLASS: return dropGetGuber(moby);");
        state.GetGuberCase.Add("case DEMONBELL_MOBY_OCLASS: return demonbellGetGuber(moby);");

        // handle events
        state.HandleGuberEventCase.Add("case MYSTERY_BOX_OCLASS: mboxHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case UPGRADE_MOBY_OCLASS: upgradeHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case DROP_MOBY_OCLASS: dropHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case DEMONBELL_MOBY_OCLASS: demonbellHandleEvent(moby, event); break;");

        // 
        state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
    }

    public void Configure(BuildState state, BuildStateStage stage)
    {
        if (state.RacVersion != RCVER.DL) return;

        switch (stage)
        {
            case BuildStateStage.BeforeBuild: OnBeforeBuild(state); break;
            case BuildStateStage.Cleanup: OnCleanupBuild(state); break;
        }
    }

    void OnBeforeBuild(BuildState state)
    {
        state.MobyOClasses.Add(RaidsModeData.LASERBEAM_OCLASS);
        state.MobyOClasses.Add(0x2075); // node base (sounds)
        state.MobyOClasses.Add(0x2635); // mysterybox
        state.MobyOClasses.Add(0x2124); // bigal
        state.MobyOClasses.Add(0x263A); // vendor
        state.MobyOClasses.Add(0x01F4); // drop
        state.MobyOClasses.Add(0x01F9); // upgrade
        state.MobyOClasses.Add(0x01F8); // trailshot
        if (EnableStackables) state.MobyOClasses.Add(8348); // stackbox base

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

        // add survival sprites if base sprites exist
        var mapConfig = FindObjectOfType<MapConfig>();
        if (mapConfig == null || mapConfig.DLSprites == null || mapConfig.DLSprites.Count == 0) return;

        var spriteContainer = new GameObject("Survival Sprites").AddComponent<SpriteContainer>();
        spriteContainer.transform.SetParent(this.transform, false);
        spriteContainer.gameObject.hideFlags = HideFlags.HideAndDontSave;
        spriteContainer.RacVersion = state.RacVersion;
        spriteContainer.Sprites = mobConfig.SurvivalMysteryBoxSprites.ToList();
        if (EnableStackables) spriteContainer.Sprites.AddRange(mobConfig.SurvivalStackableSprites);
        if (EnableBlessings) spriteContainer.Sprites.AddRange(mobConfig.SurvivalBlessingSprites);

        // add mob sprites
        foreach (var mob in this.Mobs.Where(x => !x.Disabled))
        {
            var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == mob.Mob);
            var variant = mobDefaults.Variants.ElementAtOrDefault(mob.Variant);
            if (variant == null) continue;

            // check sprite texture
            if (variant.SpriteTexture && !spriteContainer.Sprites.Any(x => x.m_Texture == variant.SpriteTexture))
            {
                spriteContainer.Sprites.Add(new SpriteDef()
                {
                    m_Bank = SpriteDef.SpriteDefBank.Bank1,
                    m_Texture = variant.SpriteTexture,
                    m_TextureSizeOverride = TextureSize._64,
                    m_Tint = variant.SpriteTextureTint,
                    m_Uid = (ushort)(30200 + spriteContainer.Sprites.Count),
                    m_Unknown = 1
                });
            }

            // check boss texture
            if (mob.Attributes == SurvivalMobAttributes.Boss && variant.BossTexture && !spriteContainer.Sprites.Any(x => x.m_Texture == variant.BossTexture))
            {
                spriteContainer.Sprites.Add(new SpriteDef()
                {
                    m_Bank = SpriteDef.SpriteDefBank.Bank1,
                    m_Texture = variant.BossTexture,
                    m_TextureSizeOverride = TextureSize._64,
                    m_Tint = variant.BossTextureTint,
                    m_Uid = (ushort)(30200 + spriteContainer.Sprites.Count),
                    m_Unknown = 1
                });
            }
        }
    }

    void OnCleanupBuild(BuildState state)
    {
        var spriteContainer = this.transform.Find("Survival Sprites");
        if (spriteContainer) GameObject.DestroyImmediate(spriteContainer.gameObject);
    }

    string GetConfigContents()
    {
        var sb = new StringBuilder();
        var mapConfig = FindObjectOfType<MapConfig>();
        var enabledMobs = GetEnabledMobs();
        var spriteDefs = mapConfig.GetSpriteDefs(RCVER.DL);

        // collect
        var upgradeSpawns = HierarchicalSorting.Sort(FindObjectsOfType<SurvivalUpgradeSpawn>(false));

        sb.AppendLine("#include <libdl/utils.h>");
        sb.AppendLine("#include \"game.h\"");
        sb.AppendLine("#include \"mob.h\"");
        sb.AppendLine();

        sb.AppendLine("extern struct SurvivalMapConfig MapConfig;");
        sb.AppendLine();

        // mob config
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("struct MobSpawnParams defaultSpawnParams[] = {");
        sb.AppendLine(GetMobDefs(enabledMobs, spriteDefs));
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
        sb.AppendLine($"\t.Difficulty = {Difficulty.ToInvariantCulture()},");
        sb.AppendLine($"\t.BoltMultiplier = {BoltMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t.XpMultiplier = {XpMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t.SpawnDistanceFactor = {SpawnDistanceFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t.BoltRankMultiplier = {BoltRankMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t.StackboxBaseCost = {StackableBaseCost},");
        sb.AppendLine($"\t.StackboxCostPerPerk = {StackableIncrementCost},");
		
		sb.AppendLine($"\t.WeaponPrestigeMax = {WeaponPrestigeMax},");
		sb.AppendLine("\t.PrestigeCostPerLevel = {");
		foreach(var cost in PrestigeCostPerLevel)
			sb.AppendLine($"\t\t{cost},");
		sb.AppendLine("\t},");
		
        sb.AppendLine("\t.BakedSpawnPoints = {");
        foreach (var item in upgradeSpawns)
            sb.AppendLine(SurvivalBakedSpawnPointItem.GetDef(item.transform, SurvivalBakedSpawnpointType.Upgrade));
        sb.AppendLine("\t}");
        sb.AppendLine("};");
        sb.AppendLine();

        // gambits
        sb.AppendLine("//--------------------------------------------------------------------------");
        foreach (var item in Gambits)
            sb.Append(item.GetForwardDef());
        sb.AppendLine("GambitDef_t gambitDefs[] = {");
        foreach (var item in Gambits)
            sb.AppendLine(item.GetDef());
        sb.AppendLine("};");
        sb.AppendLine("const int gambitDefsCount = COUNT_OF(gambitDefs);");
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

        // misc
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine($"int reactorMinionSpawnParamIdx = {enabledMobs.FindIndex(x => x.Name == ReactorMinionMobName)};");
        sb.AppendLine($"int mobAllowedCuboidIdx = {mapConfig.GetIndexOfCuboid(MobAllowedArea)};");
        sb.AppendLine($"int mobSpawnPointsAreaIdx = {mapConfig.GetIndexOfArea(MobSpawnPoints)};");
        
        sb.AppendLine();
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("void configInit(void)");
        sb.AppendLine("{");
        sb.AppendLine("\tMapConfig.DefaultSpawnParams = defaultSpawnParams;");
        sb.AppendLine("\tMapConfig.DefaultSpawnParamsCount = COUNT_OF(defaultSpawnParams);");
        sb.AppendLine("\tMapConfig.SpecialRoundParams = specialRoundParams;");
        sb.AppendLine("\tMapConfig.SpecialRoundParamsCount = COUNT_OF(specialRoundParams);");
        sb.AppendLine($"\tMapConfig.WeaponPickupCooldownFactor = {WeaponPickupCooldownFactor.ToInvariantCulture()};");
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

            sb.AppendLine($"\t{{ {(int)item.Item}, {probability.ToInvariantCulture()} }},");

            totalProbability *= (1 - item.Probability);
        }

        return sb.ToString().TrimEnd();
    }

    string GetMobDefs(List<SurvivalMobSpawnParam> mobs, SpriteDef[] spriteDefs)
    {
        var sb = new StringBuilder();

        // convert target probability into actual probability for our algorithm
        // sort each item by probability ascending
        // real probability is target probability divided by the probability the previous items weren't successful
        var totalProbability = 1f;
        for (int i = 0; i < mobs.Count; ++i)
        {
            var mob = mobs[i];
            var probability = mob.Probability / totalProbability;
            if (i == (mobs.Count - 1)) probability = 1; // last item is guaranteed to match roll if all others fail

            sb.Append(mob.GetDef(spriteDefs, probability));

            totalProbability *= (1 - mob.Probability);
        }

        return sb.ToString().TrimEnd();
    }

    void EnsureSurvivalMobysHaveCorrectPVarSize()
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);

        foreach (var moby in mobys)
        {
            int? expectedPvarSize = null;
            switch (moby.OClass)
            {
                case DEMONBELL_OCLASS: expectedPvarSize = 20; break;
                case BANK_OCLASS: expectedPvarSize = 16; break;
                case STACKBOX_OCLASS: expectedPvarSize = 16; break;
            }

            if (expectedPvarSize.HasValue)
            {
                var pvarData = moby.GetPVarData();
                if (pvarData == null) pvarData = new byte[0];

                if (pvarData.Length < expectedPvarSize)
                {
                    Array.Resize(ref pvarData, expectedPvarSize.Value);
                    moby.SetPVarData(pvarData);
                }
            }
        }
    }

    #endregion

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Survival/Create Survival Data", priority = 10)]
    public static void CreateSurvivalData()
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var cuboids = mapConfig.GetCuboids();
        var areas = mapConfig.GetAreas();
        var mobys = mapConfig.GetMobys(RCVER.DL);
        var mapName = SceneManager.GetActiveScene().name;
        var racVersion = RCVER.DL;

        // import missing mobys
        PackerHelper.ExtractAndInstallMobys(mapName, racVersion, DLMapIds.SP_Battledome, 36, false, new Dictionary<int, string>()
        {
            { 9786, "Vendor" },
            { 8484, "Big Al" },
            { 9795, "Prestige Machine" },
        });
        PackerHelper.ExtractAndInstallMoby(mapName, racVersion, DLMapIds.SP_Torval, 6, 9781, "Mystery Box", overwrite: false);
        PackerHelper.ExtractAndInstallMoby(mapName, racVersion, DLMapIds.SP_Maraxus, 3, 8323, "Stackables Vendor", overwrite: false);
        PackerHelper.ExtractAndInstallMoby(mapName, racVersion, DLMapIds.SP_Catacrom, -1, 8348, "Stackables Vendor Base", overwrite: false);
        PackerHelper.ExtractAndInstallMoby(mapName, racVersion, DLMapIds.SP_Catacrom, 2, 503, "Bank Box", overwrite: false);

        // check for existing survival data
        if (FindObjectOfType<SurvivalModeData>())
        {
            EditorUtility.DisplayDialog("Survival Importer", "Survival data already exists for this map! If you want to do a full reimport, delete your survival data first.", "Okay");
            return;
        }

        // create data
        var go = new GameObject("Survival");
        var survivalData = go.AddComponent<SurvivalModeData>();
        UnityHelper.OnAfterCreateGameObject(go);

        // set defaults
        survivalData.HidePrestigeMachineEvery25Rounds = true;
        survivalData.RandomizeWeaponPickupsAtStart = true;
        mapConfig.DLForceCustomMode = DLCustomModeIds.Survival;

        // enable code gen
        FindObjectOfType<CodeManager>().Enabled = true;
        if (!FindObjectOfType<CommonCodeGen>())
            go.AddComponent<CommonCodeGen>();

        // enable stackables
        survivalData.EnableStackables = true;
        survivalData.Stackables = ((SurvivalStackableItemId[])Enum.GetValues(typeof(SurvivalStackableItemId))).ToList();

        // create default mobs
        survivalData.Mobs = new List<SurvivalMobSpawnParam>()
        {
            new SurvivalMobSpawnParam()
            {
                Name = "Reactor",
                Mob = SurvivalMob.Reactor,
                Probability = 1f,
                Variant = 0,
                SpecialRoundOnly = true,
                MaxSpawnedPerRound = 1,
                Attributes = SurvivalMobAttributes.Boss,
            },
            new SurvivalMobSpawnParam()
            {
                Name = "Reaper",
                Mob = SurvivalMob.Reaper,
                Probability = 0.1f,
                Variant = 0,
                CooldownTicks = 60,
                CooldownOffsetPerRoundFactor = -0.6f,
                MinRound = 5
            },
            new SurvivalMobSpawnParam()
            {
                Name = "Zombie",
                Mob = SurvivalMob.Zombie,
                Probability = 0.5f,
                Variant = 1,
                CooldownTicks = 0,
            },
            new SurvivalMobSpawnParam()
            {
                Name = "Swarmer",
                Mob = SurvivalMob.Swarmer,
                Probability = 1f,
                Variant = 0,
                CooldownTicks = 0,
            },
        };

        // create boss round
        survivalData.SpecialRounds = new List<SurvivalMobSpecialRoundParam>()
        {
            new SurvivalMobSpecialRoundParam()
            {
                Name = "Boss Round",
                MinRound = 25,
                RepeatCount = 0,
                RepeatEveryNRounds = 25,
                MobNamesToSpawn = new List<string>() { "Zombie", "Swarmer" },
                SpawnRateFactor = 0.1f,
                SpawnCountFactor = 0.25f,
                UnlimitedPostRoundTime = true,
                DisableDrops = true,
            }
        };

        // create gambits
        survivalData.Gambits = new List<SurvivalGambit>()
        {
            new SurvivalGambit()
            {
                Name = "Easy Mode",
                Description = "Complete 100 rounds with reduced difficulty scaling and increased bolt/xp rates.",
                CompleteAfterRound = 100,
                BoltMultiplier = 2,
                XpMultiplier = 2,
                DifficultyMultiplier = 0.5f,
            },
            new SurvivalGambit()
            {
                Name = "Impossible Mode",
                Description = "Complete 50 rounds with increased difficulty scaling and reduced bolt/xp rates. Revives are disabled.",
                CompleteAfterRound = 50,
                BoltMultiplier = 0.5f,
                XpMultiplier = 0.5f,
                DifficultyMultiplier = 2f,
                MobHealthScaleMultiplier = 0.5f,
                DisableRevives = true,
                DisableBank = true,
            },
            new SurvivalGambit()
            {
                Name = "Flail Only",
                Description = "Complete 50 rounds using only the Scorpion Flail.",
                CompleteAfterRound = 50,
                ForceWeapon = DLGadgetIds.Flail
            },
        };

        // create player spawn
        var playerSpawnCuboid = new GameObject("Player Spawn").AddComponent<Cuboid>();
        if (playerSpawnCuboid)
        {
            playerSpawnCuboid.CuboidType = CuboidMaskType.Player;
            playerSpawnCuboid.transform.SetParent(go.transform, false);
            if (cuboids.FirstOrDefault(x => x.CuboidType == CuboidMaskType.Player) is Cuboid cuboid && cuboid)
                playerSpawnCuboid.transform.position = cuboid.transform.position;
        }

        // create mob allowed cuboid
        var mobAllowedCuboid = new GameObject("Mob Allowed Region").AddComponent<Cuboid>();
        if (mobAllowedCuboid)
        {
            survivalData.MobAllowedArea = mobAllowedCuboid;
            mobAllowedCuboid.transform.SetParent(go.transform, false);

            var center = cuboids.Select(x => x.transform.position).Average();
            var bounds = new Bounds(center, Vector3.one);
            foreach (var cuboid in cuboids)
                bounds.Encapsulate(cuboid.transform.position);
            bounds.Expand(50);

            mobAllowedCuboid.transform.position = bounds.center;
            mobAllowedCuboid.transform.localScale = bounds.size;
        }

        // create mob spawn area
        var mobSpawnArea = new GameObject("Mob Spawns").AddComponent<Area>();
        if (mobSpawnArea)
        {
            survivalData.MobSpawnPoints = mobSpawnArea;
            mobSpawnArea.transform.SetParent(go.transform, false);

            var mobSpawnCuboid = new GameObject("mob spawn").AddComponent<Cuboid>();
            mobSpawnCuboid.transform.SetParent(mobSpawnArea.transform, false);
            if (playerSpawnCuboid) mobSpawnCuboid.transform.position = playerSpawnCuboid.transform.position;
            mobSpawnCuboid.transform.localScale = new Vector3(10, 3, 10);
            mobSpawnArea.Cuboids.Add(mobSpawnCuboid);
        }

        // create default path graph
        var pathGraphPrefab = UnityHelper.GetRaidsPrefab("PathGraph");
        if (pathGraphPrefab)
        {
            var pathGraphGo = Instantiate(pathGraphPrefab);
            pathGraphGo.transform.SetParent(go.transform, false);
            pathGraphGo.transform.position = playerSpawnCuboid.transform.position;
            survivalData.MobPathGraph = pathGraphGo.GetComponent<PathGraph>();
        }

        // create mobys
        {
            var right = playerSpawnCuboid.transform.right;
            var forward = playerSpawnCuboid.transform.forward;
            var position = playerSpawnCuboid.transform.position + right * 5f;

            // misc mobys
            SurvivalSetupAddMoby(go.transform, 9786, "Vendor", position + forward * 0f);
            SurvivalSetupAddMoby(go.transform, 8484, "Big Al", position + forward * 5f);
            SurvivalSetupAddMoby(go.transform, 9795, "Prestige Machine", position + forward * 10f);

            // mystery boxes
            position += right * 5f;
            var mysteryBoxGo = new GameObject("Mystery Boxes");
            mysteryBoxGo.transform.SetParent(go.transform, false);
            mysteryBoxGo.transform.position = position;
            SurvivalSetupAddMoby(mysteryBoxGo.transform, 9781, "Mysterybox", position + forward * 1f);
            SurvivalSetupAddMoby(mysteryBoxGo.transform, 9781, "Mysterybox", position + forward * 3f);
            SurvivalSetupAddMoby(mysteryBoxGo.transform, 9781, "Mysterybox", position + forward * 5f);

            // vendors
            position += right * 5f;
            var stackablesGo = new GameObject("Stackable Vendors");
            stackablesGo.transform.SetParent(go.transform, false);
            stackablesGo.transform.position = position;
            SurvivalSetupAddMoby(stackablesGo.transform, 8323, "Stackbox", position + forward * 0f);
            SurvivalSetupAddMoby(stackablesGo.transform, 8323, "Stackbox", position + forward * 5f);
            SurvivalSetupAddMoby(stackablesGo.transform, 8323, "Stackbox", position + forward * 10f);

            // upgrades
            position += right * 5f;
            var upgradesGo = new GameObject("Upgrades");
            upgradesGo.transform.SetParent(go.transform, false);
            upgradesGo.transform.position = position;
            SurvivalSetupAddUpgrade(upgradesGo.transform, "Upgrade", position + forward * 0f);
            SurvivalSetupAddUpgrade(upgradesGo.transform, "Upgrade", position + forward * 2f);
            SurvivalSetupAddUpgrade(upgradesGo.transform, "Upgrade", position + forward * 4f);
            SurvivalSetupAddUpgrade(upgradesGo.transform, "Upgrade", position + forward * 6f);
            SurvivalSetupAddUpgrade(upgradesGo.transform, "Upgrade", position + forward * 8f);

            // demon bells
            position += right * 5f;
            position += Vector3.up * 5f;
            var demonbellsGo = new GameObject("Demonbells");
            demonbellsGo.transform.SetParent(go.transform, false);
            demonbellsGo.transform.position = position;
            SurvivalSetupAddMoby(demonbellsGo.transform, 9337, "Demonbell", position + forward * 0f).transform.localScale = Vector3.one * 0.5f;
            SurvivalSetupAddMoby(demonbellsGo.transform, 9337, "Demonbell", position + forward * 3f).transform.localScale = Vector3.one * 0.5f;
            SurvivalSetupAddMoby(demonbellsGo.transform, 9337, "Demonbell", position + forward * 6f).transform.localScale = Vector3.one * 0.5f;
        }

        // lastly install mobs
        survivalData.InstallMobDependencies(false);

        // prompt for cleanup
        if (EditorUtility.DisplayDialog("Survival Importer", "Would you like to also cleanup the extranneous mobys/cuboids from the base map? It's recommended to free up space for survival.", "Cleanup", "Skip"))
        {
            // remove all cuboids except the first
            // since the first is reserved
            foreach (var cuboid in cuboids.Skip(1))
                GameObject.DestroyImmediate(cuboid.gameObject);

            foreach (var area in areas)
                GameObject.DestroyImmediate(area.gameObject);

            foreach (var moby in mobys)
            {
                switch (moby.OClass)
                {
                    case 8309: // node base
                    case 9838: // flag base
                    case 4291: // teleport pad
                    case 5000: // ammo pad
                    case 4290: // player turret
                    case 5614: // health pad
                    case 6703: // node upgrade config
                    case 9758: // node container
                    case 9759: // pickup pad
                    case 9732: // hill
                    case 7215: // red flag
                    case 7217: // blue flag
                    case 9916: // green flag
                    case 9917: // orange flag
                    case 6529: // vehicle pad
                    case 8276: // hoverbike
                    case 8292: // puma
                    case 8366: // hovership
                    case 8248: // landstalker base
                    case 8249: // landstalker cabin
                        {
                            GameObject.DestroyImmediate(moby.gameObject);
                            break;
                        }
                }
            }
        }
    }

    private static Moby SurvivalSetupAddMoby(Transform parent, int oclass, string name, Vector3 position)
    {
        var moby = new GameObject(name).AddComponent<Moby>();
        moby.transform.SetParent(parent, false);
        moby.transform.position = position;
        moby.RCVersion = RCVER.DL;
        moby.OClass = oclass;
        moby.InitializePVarReferences(true);
        moby.UpdateAsset();

        return moby;
    }

    private static SurvivalUpgradeSpawn SurvivalSetupAddUpgrade(Transform parent, string name, Vector3 position)
    {
        var upgrade = new GameObject(name).AddComponent<SurvivalUpgradeSpawn>();
        upgrade.transform.SetParent(parent, false);
        upgrade.transform.position = position;
        return upgrade;
    }

    public void InstallMobDependencies(bool overwrite = false)
    {
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var mapName = SceneManager.GetActiveScene().name;
        var racVersion = RCVER.DL;

        foreach (var mob in Mobs)
        {
            var mobDefaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == mob.Mob);
            var variant = mobDefaults.Variants.ElementAtOrDefault(mob.Variant);
            if (variant == null) continue;

            foreach (var dependency in variant.Dependencies)
            {
                PackerHelper.ExtractAndInstallMoby(mapName, racVersion, dependency.SourceMapId, dependency.SourceMissionId, dependency.OClass, name: $"{mob.Name}:{variant.Name}", overwrite: overwrite);
            }
        }
    }

    #endregion

    #region Code Seg Patches

    void WriteCodeSegPatches(MapConfig mapConfig, string buildFolder)
    {
        var codeFolder = Path.Combine(buildFolder, FolderNames.BinaryCodeFolder);
        var survivalData = SurvivalMobsScriptableObject.Load();
        if (!survivalData) throw new Exception("missing survival mobs data");

        // write patches if not legacy
        WritePatches(survivalData, codeFolder);

        // write weapon stats
        WriteWeaponStats(survivalData, codeFolder);
    }

    void WritePatches(SurvivalMobsScriptableObject survivalData, string codeFolder)
    {
        foreach (var patch in survivalData.Patches.Where(x => !x.Disabled))
        {
            var path = Path.Combine(codeFolder, $"code.{patch.CodeSegIndex:D4}.bin");
            var offset = Convert.ToInt64(patch.CodeSegOffsetHex, 16);
            var bytes = BinaryHelper.GetBytesFromHexString(patch.Hex);

            using var fs = File.OpenWrite(path);
            using var bw = new BinaryWriter(fs);
            bw.BaseStream.Position = offset;
            bw.Write(bytes);
        }
    }

    void WriteWeaponStats(SurvivalMobsScriptableObject survivalData, string codeFolder)
    {
        var codeSeg2Path = Path.Combine(codeFolder, "code.0002.bin");
        using var fs = File.OpenWrite(codeSeg2Path);
        using var writer = new BinaryWriter(fs);

        // write weapon damage table
        {
            writer.BaseStream.Position = 0x1CF18;
            var gadgets = new DLGadgetIds[]
            {
                DLGadgetIds.Wrench,
                DLGadgetIds.DualVipers,
                DLGadgetIds.MagmaCannon,
                DLGadgetIds.Arbiter,
                DLGadgetIds.FusionRifle,
                DLGadgetIds.MineLauncher,
                DLGadgetIds.B6,
                DLGadgetIds.Holoshield,
                DLGadgetIds.Miniturret,
                DLGadgetIds.Harbinger,
                DLGadgetIds.Flail
            };

            for (int i = 0; i < gadgets.Length; ++i)
            {
                var gadget = gadgets[i];
                var count = gadget == DLGadgetIds.Wrench ? 8 : 10;
                var record = survivalData.WeaponStats.FirstOrDefault(x => x.Gadget == gadget);

                // skip empty space
                if (gadget == DLGadgetIds.Flail) writer.BaseStream.Position += 0x80;

                if (record == null)
                {
                    writer.BaseStream.Position += 0x20 * count;
                    continue;
                }

                for (int c = 0; c < count; ++c)
                {
                    if (c < record.Damages.Count)
                    {
                        writer.BaseStream.Position += 0x10;
                        writer.Write(record.Damages[c].x);
                        writer.Write(record.Damages[c].y);
                        writer.Write(record.Damages[c].z);
                        writer.Write(record.Damages[c].w);
                    }
                    else
                    {
                        writer.BaseStream.Position += 0x20;
                    }
                }
            }
        }

        // write weapon base ammo
        {
            var gadgets = new DLGadgetIds[]
            {
                DLGadgetIds.DualVipers,
                DLGadgetIds.MagmaCannon,
                DLGadgetIds.Arbiter,
                DLGadgetIds.FusionRifle,
                DLGadgetIds.MineLauncher,
                DLGadgetIds.B6,
                DLGadgetIds.Holoshield,
                DLGadgetIds.Flail
            };
            for (int i = 0; i < gadgets.Length; ++i)
            {
                var gadget = gadgets[i];
                var record = survivalData.WeaponStats.FirstOrDefault(x => x.Gadget == gadget);
                if (record == null) continue;

                writer.BaseStream.Position = 0x1DF44 + i * 0xB0;
                if (gadget == DLGadgetIds.Flail) writer.BaseStream.Position += 0x90 + 0xB0;

                writer.Write((ushort)record.BaseAmmo);
            }
        }

        // write weapon ammo mod amount
        {
            foreach (var record in survivalData.WeaponStats)
            {
                writer.BaseStream.Position = 0x1C728 + 4 * (int)record.Gadget;
                writer.Write((int)record.AmmoModAmount);
            }
        }
    }

    //[MenuItem("Forge/Read Survival Weapon Stats", priority = 10)]
    public static void ReadWeaponStats()
    {
        var survivalData = SurvivalMobsScriptableObject.Load();
        if (!survivalData) throw new Exception("missing survival mobs data");

        var codeSegPath = "M:\\Unity\\horizon-forge\\levels\\survival_torval\\rc4\\code\\code.0002.bin";
        var codeSegBytes = File.ReadAllBytes(codeSegPath);
        using var ms = new MemoryStream(codeSegBytes);
        using var reader = new BinaryReader(ms);

        // read weapon damage table
        reader.BaseStream.Position = 0x1CF18;
        var gadgets = new DLGadgetIds[]
        {
            DLGadgetIds.Wrench,
            DLGadgetIds.DualVipers,
            DLGadgetIds.MagmaCannon,
            DLGadgetIds.Arbiter,
            DLGadgetIds.FusionRifle,
            DLGadgetIds.MineLauncher,
            DLGadgetIds.B6,
            DLGadgetIds.Holoshield,
            DLGadgetIds.Miniturret,
            DLGadgetIds.Harbinger,
            DLGadgetIds.Flail
        };

        for (int i = 0; i < gadgets.Length; ++i)
        {
            var gadget = gadgets[i];
            var count = gadget == DLGadgetIds.Wrench ? 8 : 10;
            var record = survivalData.WeaponStats.FirstOrDefault(x => x.Gadget == gadget);

            // empty space
            if (gadget == DLGadgetIds.Flail) reader.BaseStream.Position += 0x80;

            for (int c = 0; c < count; ++c)
            {
                reader.ReadBytes(16);
                var dmg0 = reader.ReadSingle();
                var dmg1 = reader.ReadSingle();
                var dmg2 = reader.ReadSingle();
                var dmg3 = reader.ReadSingle();

                if (record != null)
                {
                    if (c >= record.Damages.Count)
                        record.Damages.Add(Vector4.zero);

                    record.Damages[c] = new Vector4(dmg0, dmg1, dmg2, dmg3);
                }
            }
        }

        // read weapon base ammo
        gadgets = new DLGadgetIds[]
        {
            DLGadgetIds.DualVipers,
            DLGadgetIds.MagmaCannon,
            DLGadgetIds.Arbiter,
            DLGadgetIds.FusionRifle,
            DLGadgetIds.MineLauncher,
            DLGadgetIds.B6,
            DLGadgetIds.Holoshield,
            DLGadgetIds.Flail
        };
        for (int i = 0; i < gadgets.Length; ++i)
        {
            var gadget = gadgets[i];
            var record = survivalData.WeaponStats.FirstOrDefault(x => x.Gadget == gadget);
            if (record == null) continue;

            reader.BaseStream.Position = 0x1DF44 + i * 0xB0;
            if (gadget == DLGadgetIds.Flail) reader.BaseStream.Position += 0x90 + 0xB0;

            var ammo = reader.ReadUInt16();
            record.BaseAmmo = ammo;
        }
    }

    #endregion

    #region Port Legacy

    static string _lastOpenFileDir;

    //[MenuItem("Forge/Read Survival Legacy PathGraph", priority = 10)]
    public static void PortLegacyPath()
    {
        var path = EditorUtility.OpenFilePanelWithFilters("Open path.c", _lastOpenFileDir, new string[] { "C Files", "c" });
        if (string.IsNullOrEmpty(path)) return;

        _lastOpenFileDir = new FileInfo(path).DirectoryName;
        var text = File.ReadAllText(path);

        var fileNodes = new List<Vector4>();
        var fileEdges = new List<Vector2Int>();
        var fileCornering = new List<float>();
        var fileRequired = new List<float?>();
        var filePathFit = new List<float>();
        var fileJumpPadSpeed = new List<float>();
        var fileJumpPadAt = new List<float>();

        Regex regexCtx = new Regex(@"(\w+) (\w+)\[\]");
        Regex regexInt = new Regex(@"^\s+(\d+),?$");
        Regex regexInt2 = new Regex(@"\{ (\d+), (\d+) \},?");
        Regex regexVector = new Regex(@"\{ ([+-]?([0-9]*[.])?[0-9]+), ([+-]?([0-9]*[.])?[0-9]+), ([+-]?([0-9]*[.])?[0-9]+), ([+-]?([0-9]*[.])?[0-9]+) \}");

        var lines = text.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
        var ctx = "";
        foreach (var line in lines)
        {
            // look for context
            if (string.IsNullOrEmpty(ctx))
            {
                var match = regexCtx.Match(line);
                if (match.Success)
                {
                    ctx = match.Groups[2].Value;
                }

                continue;
            }

            // look for context end
            if (line.Trim() == "};")
            {
                ctx = string.Empty;
                continue;
            }

            switch (ctx)
            {
                case "MOB_PATHFINDING_NODES":
                    {
                        var match = regexVector.Match(line);
                        if (match.Success)
                        {
                            fileNodes.Add(new Vector4(
                                float.Parse(match.Groups[1].Value), 
                                float.Parse(match.Groups[3].Value), 
                                float.Parse(match.Groups[5].Value), 
                                float.Parse(match.Groups[7].Value)));
                        }
                        break;
                    }
                case "MOB_PATHFINDING_EDGES":
                    {
                        var match = regexInt2.Match(line);
                        if (match.Success)
                        {
                            var e0 = int.Parse(match.Groups[1].Value);
                            var e1 = int.Parse(match.Groups[2].Value);
                            fileEdges.Add(new Vector2Int(e0, e1));
                        }
                        break;
                    }
                case "MOB_PATHFINDING_NODES_CORNERING":
                    {
                        var match = regexInt.Match(line);
                        if (match.Success)
                        {
                            var value = float.Parse(match.Groups[1].Value) / 255f;
                            fileCornering.Add(value);
                        }
                        break;
                    }
                case "MOB_PATHFINDING_EDGES_REQUIRED":
                    {
                        var match = regexInt.Match(line);
                        if (match.Success)
                        {
                            var value = int.Parse(match.Groups[1].Value);
                            if (value == 0) fileRequired.Add(null);
                            else fileRequired.Add((value - 1) / 255f);
                        }
                        break;
                    }
                case "MOB_PATHFINDING_EDGES_PATHFIT":
                    {
                        var match = regexInt.Match(line);
                        if (match.Success)
                        {
                            var value = float.Parse(match.Groups[1].Value) / 255f;
                            filePathFit.Add(value);
                        }
                        break;
                    }
                case "MOB_PATHFINDING_EDGES_JUMPPADSPEED":
                    {
                        var match = regexInt.Match(line);
                        if (match.Success)
                        {
                            var value = float.Parse(match.Groups[1].Value);
                            fileJumpPadSpeed.Add(value);
                        }
                        break;
                    }
                case "MOB_PATHFINDING_EDGES_JUMPPADAT":
                    {
                        var match = regexInt.Match(line);
                        if (match.Success)
                        {
                            var value = float.Parse(match.Groups[1].Value) / 255f;
                            fileJumpPadAt.Add(value);
                        }
                        break;
                    }
            }
        }


        var go = new GameObject("Path Graph");
        var pathGraph = go.AddComponent<PathGraph>();
        go.transform.position = ((Vector3)fileNodes.Aggregate((a, b) => a + b)).SwizzleXZY() / fileNodes.Count;
        

        for (int i = 0; i < fileNodes.Count; ++i)
        {
            var nodeGo = new GameObject($"node {i}");
            nodeGo.transform.SetParent(go.transform, false);

            var node = nodeGo.AddComponent<PathGraphNode>();
            node.transform.position = ((Vector3)fileNodes[i]).SwizzleXZY();
            node.Radius = fileNodes[i].w;
            node.Cornering = fileCornering[i];
        }

        pathGraph.RefreshCache();
        var nodes = pathGraph.GetNodes();

        for (int i = 0; i < fileEdges.Count; ++i)
        {
            var fileEdge = fileEdges[i];
            var node0 = nodes.ElementAtOrDefault(fileEdge.x);
            var node1 = nodes.ElementAtOrDefault(fileEdge.y);

            var edge = new PathGraphEdge();
            pathGraph.Edges.Add(edge);
            edge.From = node0;
            edge.To = node1;
            edge.PathFitStartEnd = filePathFit[i];
            edge.Required = fileRequired[i].HasValue;
            edge.RequiredUntil = fileRequired[i] ?? 0;
            edge.JumpPad = fileJumpPadSpeed[i] > 0;
            edge.JumpPadSpeed = fileJumpPadSpeed[i];
            edge.JumpPadAt = fileJumpPadAt[i];
        }
    }

    //[MenuItem("Forge/Read Survival Legacy Config", priority = 10)]
    public static void PortLegacyConfig()
    {
        var path = EditorUtility.OpenFilePanelWithFilters("Open config.c", _lastOpenFileDir, new string[] { "C Files", "c" });
        if (string.IsNullOrEmpty(path)) return;

        _lastOpenFileDir = new FileInfo(path).DirectoryName;
        var text = File.ReadAllText(path);
        var fileTypes = new List<SurvivalBakedSpawnpointType>();
        var filePositions = new List<Vector3>();
        var fileRotations = new List<Vector3>();

        Regex regexCtx = new Regex(@"\.BakedSpawnPoints = \{");
        Regex regexType = new Regex(@"\{ \.Type = (\w+),");
        Regex regexPosition = new Regex(@"\.Position = \{ ([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?), ([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?), ([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?) \}");
        Regex regexRotation = new Regex(@"\.Rotation = \{ ([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?), ([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?), ([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?) \}");

        var lines = text.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
        var foundBakedSpawnpoints = false;
        foreach (var line in lines)
        {
            // look for context
            if (!foundBakedSpawnpoints)
            {
                var match = regexCtx.Match(line);
                if (match.Success)
                {
                    foundBakedSpawnpoints = true;
                }

                continue;
            }

            // look for context end
            if (line.Trim() == "}")
                break;

            // item starts with {
            if (!line.Trim().StartsWith("{")) continue;

            if (regexType.IsMatch(line))
            {
                var match = regexType.Match(line);
                var type = match.Groups[1].Value;
                switch (type)
                {
                    case "BAKED_SPAWNPOINT_PLAYER_START": fileTypes.Add(SurvivalBakedSpawnpointType.PlayerStart); break;
                    case "BAKED_SPAWNPOINT_DEMON_BELL": fileTypes.Add(SurvivalBakedSpawnpointType.DemonBell); break;
                    case "BAKED_SPAWNPOINT_STACK_BOX": fileTypes.Add(SurvivalBakedSpawnpointType.StackBox); break;
                    case "BAKED_SPAWNPOINT_MYSTERY_BOX": fileTypes.Add(SurvivalBakedSpawnpointType.MysteryBox); break;
                    case "BAKED_SPAWNPOINT_UPGRADE": fileTypes.Add(SurvivalBakedSpawnpointType.Upgrade); break;
                    default: throw new NotImplementedException();
                }
            }
            if (regexPosition.IsMatch(line))
            {
                var match = regexPosition.Match(line);
                filePositions.Add(new Vector3(
                    float.Parse(match.Groups[1].Value),
                    float.Parse(match.Groups[2].Value),
                    float.Parse(match.Groups[3].Value)));
            }
            if (regexRotation.IsMatch(line))
            {
                var match = regexRotation.Match(line);
                fileRotations.Add(new Vector3(
                    float.Parse(match.Groups[1].Value),
                    float.Parse(match.Groups[2].Value),
                    float.Parse(match.Groups[3].Value)));
            }
        }

        var survivalModeData = FindObjectOfType<SurvivalModeData>();
        var rootGo = new GameObject("Baked Spawnpoints");
        for (int i = 0; i < fileTypes.Count; i++)
        {
            var type = fileTypes[i];
            var pos = filePositions[i].SwizzleXZY();
            var rot = fileRotations[i].SwizzleXZY() * -Mathf.Rad2Deg;

            switch (type)
            {
                case SurvivalBakedSpawnpointType.PlayerStart:
                case SurvivalBakedSpawnpointType.MysteryBox:
                    {
                        rot.y += 90f;
                        break;
                    }
            }

            var go = new GameObject($"{type} {i}");
            go.transform.position = pos;
            go.transform.rotation = Quaternion.Euler(rot);

            go.transform.SetParent(rootGo.transform, true);

            //if (survivalModeData)
            //{
            //    survivalModeData.BakedSpawnPoints.Add(new SurvivalBakedSpawnPointItem()
            //    {
            //        Type = type,
            //        Transform = go.transform
            //    });
            //}
        }
    }

    //[MenuItem("Forge/Read Survival Legacy Gates", priority = 10)]
    public static void PortLegacyGates()
    {
        var path = EditorUtility.OpenFilePanelWithFilters("Open main.c", _lastOpenFileDir, new string[] { "C Files", "c" });
        if (string.IsNullOrEmpty(path)) return;

        _lastOpenFileDir = new FileInfo(path).DirectoryName;
        var text = File.ReadAllText(path);
        var filePositions = new List<(Vector3 v0, Vector3 v1)>();
        var fileHeights = new List<float>();
        var fileCosts = new List<int>();

        Regex regexCtx = new Regex(@"VECTOR GateLocations\[\] = \{");
        Regex regexVector = new Regex(@"\{ ([+-]?([0-9]*[.])?[0-9]+), ([+-]?([0-9]*[.])?[0-9]+), ([+-]?([0-9]*[.])?[0-9]+), ([+-]?([0-9]*[.])?[0-9]+) \}");

        var lines = text.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
        var foundGates = false;
        foreach (var line in lines)
        {
            // look for context
            if (!foundGates)
            {
                var match = regexCtx.Match(line);
                if (match.Success)
                {
                    foundGates = true;
                }

                continue;
            }

            // look for context end
            if (line.Trim() == "};")
                break;

            // item starts with {
            if (!line.Trim().StartsWith("{")) continue;

            if (regexVector.IsMatch(line))
            {
                var matches = regexVector.Matches(line);
                var edge0 = new Vector3(
                    float.Parse(matches[0].Groups[1].Value),
                    float.Parse(matches[0].Groups[3].Value),
                    float.Parse(matches[0].Groups[5].Value));
                var edge1 = new Vector3(
                    float.Parse(matches[1].Groups[1].Value),
                    float.Parse(matches[1].Groups[3].Value),
                    float.Parse(matches[1].Groups[5].Value));

                filePositions.Add((edge0, edge1));
                fileHeights.Add(float.Parse(matches[0].Groups[7].Value));
                fileCosts.Add(int.Parse(matches[1].Groups[7].Value));
            }
        }

        var rootGo = new GameObject("Gates");
        for (int i = 0; i < filePositions.Count; i++)
        {
            var v0 = filePositions[i].v0.SwizzleXZY();
            var v1 = filePositions[i].v1.SwizzleXZY();
            var height = fileHeights[i];
            var cost = fileCosts[i];

            var go = CommonCodeGen.CreateGateMoby();
            go.name = $"Gate {i}";
            go.transform.position = (v0 + v1) * 0.5f;
            go.transform.rotation = Quaternion.LookRotation(v1 - v0, Vector3.up);
            go.transform.SetParent(rootGo.transform, true);

            var moby = go.GetComponent<Moby>();
            moby.PVarValues[".Default State"] = "1";
            moby.PVarValues[".Height"] = height.ToString();
            moby.PVarValues[".Length"] = Vector3.Distance(v0, v1).ToString();
            moby.PVarValues[".Cost"] = cost.ToString();
        }
    }

    #endregion

}

public enum SurvivalMob
{
    Zombie = 0,
    Swarmer = 1,
    Swamper = 2,
    //StalkerTurret = 3,
    Leviathan = 4,
    //DZStriker = 5,
    Executioner = 6,
    Reaper = 7,
    Tremor = 8,
    Reactor = 9
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

    [Header("Options")]
    [Min(0)] public int CompleteAfterRound = 50;
    public DLGadgetIds ForceWeapon = DLGadgetIds.Undefined;
    public bool DisableRevives;
    public bool DisableVendor;
    public bool DisableBank;
    public bool DisablePrestigeMachine;
    public bool DisableStackables;
    public bool InstantRespawnMysteryBox;
    public float DifficultyMultiplier = 1;
    public float XpMultiplier = 1;
    public float BoltMultiplier = 1;
    public float MobDamageScaleMultiplier = 1;
    public float MobSpeedScaleMultiplier = 1;
    public float MobHealthScaleMultiplier = 1;
    public int InitialBolts = 0;
    public int InitialTokens = 0;

    [Header("Custom")]
    public string CustomInitFunctionName;
    public string CustomTickFunctionName;
    public string CustomOnRoundCompleteFunctionName;

    public string GetForwardDef()
    {
        StringBuilder sb = new StringBuilder();

        if (!string.IsNullOrEmpty(CustomInitFunctionName))
            sb.AppendLine($"void {CustomInitFunctionName}(void);");
        if (!string.IsNullOrEmpty(CustomTickFunctionName))
            sb.AppendLine($"void {CustomTickFunctionName}(void);");
        if (!string.IsNullOrEmpty(CustomOnRoundCompleteFunctionName))
            sb.AppendLine($"void {CustomOnRoundCompleteFunctionName}(int);");

        return sb.ToString();
    }

    public string GetDef()
    {
        StringBuilder sb = new StringBuilder();

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.CustomInit = {(string.IsNullOrEmpty(CustomInitFunctionName) ? "NULL" : CustomInitFunctionName)},");
        sb.AppendLine($"\t\t.CustomTick = {(string.IsNullOrEmpty(CustomTickFunctionName) ? "NULL" : CustomTickFunctionName)},");
        sb.AppendLine($"\t\t.CustomOnRoundComplete = {(string.IsNullOrEmpty(CustomOnRoundCompleteFunctionName) ? "NULL" : CustomOnRoundCompleteFunctionName)},");
        sb.AppendLine($"\t\t.CompleteAfterRound = {CompleteAfterRound},");
        sb.AppendLine($"\t\t.DifficultyMultiplier = {DifficultyMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.XpMultiplier = {XpMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.BoltMultiplier = {BoltMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.MobDamageScaleMultiplier = {MobDamageScaleMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.MobSpeedScaleMultiplier = {MobSpeedScaleMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.MobHealthScaleMultiplier = {MobHealthScaleMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.InitialBolts = {InitialBolts},");
        sb.AppendLine($"\t\t.InitialTokens = {InitialTokens},");
        sb.AppendLine($"\t\t.ForceWeaponId = {(int)ForceWeapon},");
        sb.AppendLine($"\t\t.DisableRevives = {(DisableRevives ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisableVendor = {(DisableVendor ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisableBank = {(DisableBank ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisablePrestigeMachine = {(DisablePrestigeMachine ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisableStackables = {(DisableStackables ? 1 : 0)},");
        sb.AppendLine($"\t\t.InstantRespawnMysteryBox = {(InstantRespawnMysteryBox ? 1 : 0)},");
        sb.AppendLine("\t},");

        return sb.ToString().TrimEnd();
    }
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
        return GetDef(Transform, Type);
    }


    public static string GetDef(Transform transform, SurvivalBakedSpawnpointType type)
    {
        if (!transform) return string.Empty;

        StringBuilder sb = new StringBuilder();

        var pos = transform.position;
        var rot = transform.eulerAngles;

        switch (type)
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
        sb.Append($"\t\t{{ .Type = {(int)type}, .Params = 0, .Position = {{ {pos.x.ToInvariantCulture()}, {pos.z.ToInvariantCulture()}, {pos.y.ToInvariantCulture()} }}, .Rotation = {{ {rotX.ToInvariantCulture()}, {rotZ.ToInvariantCulture()}, {rotY.ToInvariantCulture()} }} }},");

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
    public SurvivalEnumOverride<DLBlipTypes> BlipType;

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
    [Min(0), Tooltip("Increase or decrease turn speed.")] public float TurnSpeedMultiplier = 1;
    [Min(0), Tooltip("For ranged attacks, how far away from the target the mob can be to fire.")] public SurvivalFloatOverride RangedAttackDistance;
    public SurvivalFloatOverride Xp;
    public SurvivalFloatOverride Bolts;

    [Header("Damage")]
    public SurvivalFloatOverride Damage;
    public SurvivalFloatOverride DamageMax;
    public SurvivalFloatOverride DamageScale;

    [Header("Speed")]
    public SurvivalFloatOverride Speed;
    public SurvivalFloatOverride SpeedMax;
    public SurvivalFloatOverride SpeedScale;

    [Header("Health")]
    public SurvivalFloatOverride Health;
    public SurvivalFloatOverride HealthMax;
    public SurvivalFloatOverride HealthScale;

    [Header("Colors")]
    public SurvivalColorOverride BaseColor;
    public SurvivalColorOverride GlowColor;
    public SurvivalColorOverride SpriteColor;

    public string GetDef(SpriteDef[] spriteDefs, float? probabilityOverride = null)
    {
        var sb = new StringBuilder();

        var mobPrefix = this.Mob.ToString().ToLower();
        var mobConfig = SurvivalMobsScriptableObject.Load();
        var defaults = mobConfig.Mobs.FirstOrDefault(x => x.Mob == this.Mob) ?? new SurvivalMobsScriptableObject.SurvivalMobsConfig();
        var variant = defaults?.Variants?.ElementAtOrDefault(Variant);
        var spriteIdx = Array.FindIndex(spriteDefs, x => x.m_Texture == variant.SpriteTexture);
        var bossSpriteDef = Array.Find(spriteDefs, x => (variant.ExistingBossSpriteUid > 0 && x.m_Uid == variant.ExistingBossSpriteUid && !variant.BossTexture) || x.m_Texture == variant.BossTexture);

        var name = Name?.Replace("\"", "") ?? string.Empty;
        if (name.Length >= 32)
            name = name.Substring(0, 31);

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.MobVTable = &{this.Mob}VTable,");
        sb.AppendLine($"\t\t.RenderCost = {mobPrefix.ToUpper()}_RENDER_COST,");
        sb.AppendLine($"\t\t.Scale = {SizeMultiplier.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.OClass = {variant.OClass},");
        sb.AppendLine($"\t\t.BlipType = {(BlipType.HasOverride ? (int)BlipType.OverrideValue : (int)defaults.BlipType)},");
        sb.AppendLine($"\t\t.MaxSpawnedAtOnce = {MaxSpawnedAtOnce},");
        sb.AppendLine($"\t\t.MaxSpawnedPerRound = {MaxSpawnedPerRound},");
        sb.AppendLine($"\t\t.MinRound = {Math.Clamp(MinRound, 0, int.MaxValue)},");
        sb.AppendLine($"\t\t.CooldownTicks = {(int)CooldownTicks},");
        sb.AppendLine($"\t\t.CooldownOffsetPerRoundFactor = {CooldownOffsetPerRoundFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.Probability = {(probabilityOverride ?? Probability).ToInvariantCulture()},");
        sb.AppendLine($"\t\t.RangedAttackDistance = {(RangedAttackDistance.HasOverride ? RangedAttackDistance.OverrideValue : defaults.RangedAttackDistance).ToInvariantCulture()},");
        sb.AppendLine($"\t\t.SpawnType = {(int)SpawnType},");
        sb.AppendLine($"\t\t.SpecialRoundOnly = {(SpecialRoundOnly ? 1 : 0)},");
        sb.AppendLine($"\t\t.StatId = {(int)defaults.StatId},");
        sb.AppendLine($"\t\t.BaseColor = 0x{RCHelper.GetAbgrHex(BaseColor.HasOverride ? BaseColor.OverrideValue : defaults.BaseColor, overrideAlpha: 0):X8},");
        sb.AppendLine($"\t\t.GlowColor = 0x{RCHelper.GetAbgrHex(GlowColor.HasOverride ? GlowColor.OverrideValue : defaults.GlowColor, overrideAlpha: 0.5f):X8},");
        sb.AppendLine($"\t\t.SpriteColor = 0x{RCHelper.GetAbgrHex(SpriteColor.HasOverride ? SpriteColor.OverrideValue : defaults.SpriteColor, overrideAlpha: 0):X8},");
        sb.AppendLine($"\t\t.SpriteTexId = {(spriteIdx < 0 ? 127 : spriteIdx)},");
        sb.AppendLine($"\t\t.BossTexUid = {(bossSpriteDef != null ? bossSpriteDef.m_Uid : 30130)},");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {(ushort)Math.Clamp(Xp.HasOverride ? Xp.OverrideValue : defaults.Xp, 0, ushort.MaxValue)},");
        sb.AppendLine($"\t\t\t.Bolts = {(int)(Bolts.HasOverride ? Bolts.OverrideValue : defaults.Bolts)},");
        sb.AppendLine($"\t\t\t.Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"\t\t\t.Damage = {(Damage.HasOverride ? Damage.OverrideValue : defaults.Damage).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.MaxDamage = {(DamageMax.HasOverride ? DamageMax.OverrideValue : defaults.DamageMax).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.DamageScale = {(DamageScale.HasOverride ? DamageScale.OverrideValue : defaults.DamageScale).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.Speed = {(Speed.HasOverride ? Speed.OverrideValue : defaults.Speed).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.MaxSpeed = {(SpeedMax.HasOverride ? SpeedMax.OverrideValue : defaults.SpeedMax).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.SpeedScale = {(SpeedScale.HasOverride ? SpeedScale.OverrideValue : defaults.SpeedScale).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.Health = {(Health.HasOverride ? Health.OverrideValue : defaults.Health).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.MaxHealth = {(HealthMax.HasOverride ? HealthMax.OverrideValue : defaults.HealthMax).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.HealthScale = {(HealthScale.HasOverride ? HealthScale.OverrideValue : defaults.HealthScale).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.AttackRadius = {(defaults.AttackRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.HitRadius = {(defaults.HitRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.CollRadius = {(defaults.CollRadius * SizeMultiplier).ToInvariantCulture()},");
        sb.AppendLine($"\t\t\t.ReactionTickCount = {(int)(defaults.ReactionDelaySeconds * 60)},");
        sb.AppendLine($"\t\t\t.AttackCooldownTickCount = {(int)(defaults.AttackCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t\t.DamageCooldownTickCount = {(int)(defaults.DamageCooldownSeconds * 60)},");
        sb.AppendLine($"\t\t\t.MobAttribute = {(int)Attributes},");
        sb.AppendLine($"\t\t\t.Behavior = {Behavior},");
        sb.AppendLine($"\t\t\t.SharedXp = {1},");
        sb.AppendLine($"\t\t}}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}

[Serializable]
public struct SurvivalFloatOverride
{
    public bool HasOverride;
    public float OverrideValue;
}

[Serializable]
public struct SurvivalColorOverride
{
    public bool HasOverride;
    [ColorUsage(false)] public Color OverrideValue;
}

[Serializable]
public struct SurvivalEnumOverride<T>
{
    public bool HasOverride;
    public T OverrideValue;
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
    [Range(0, SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS)] public int MaxSpawnedAtOnce = SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS;
    public bool UnlimitedPostRoundTime = false;
    public bool DisableDrops = false;

    public List<string> MobNamesToSpawn = new List<string>();

    public string GetDef(SurvivalModeData survivalData)
    {
        var sb = new StringBuilder();
        var enabledMobs = survivalData.GetEnabledMobs();

        var maxSpawnedAtOnce = MaxSpawnedAtOnce <= 0 ? SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS : Math.Clamp(MaxSpawnedAtOnce, 1, SurvivalModeData.SURVIVAL_MAX_SPAWNED_MOBS);
        var name = Name?.Replace("\"", "") ?? string.Empty;
        if (name.Length >= 32)
            name = name.Substring(0, 31);

        var spawnParamIdxs = new List<int>();
        foreach (var item in MobNamesToSpawn)
        {
            var spawnParam = enabledMobs.FirstOrDefault(x => x.Name == item);
            if (spawnParam == null)
            {
                Debug.LogWarning($"SpecialRound unable to find mob with name {item}");
                continue;
            }

            spawnParamIdxs.Add(enabledMobs.IndexOf(spawnParam));
        }

        sb.AppendLine("\t{");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.MinRound = {MinRound},");
        sb.AppendLine($"\t\t.RepeatEveryNRounds = {RepeatEveryNRounds},");
        sb.AppendLine($"\t\t.RepeatCount = {RepeatCount},");
        sb.AppendLine($"\t\t.UnlimitedPostRoundTime = {(UnlimitedPostRoundTime ? 1 : 0)},");
        sb.AppendLine($"\t\t.DisableDrops = {(DisableDrops ? 1 : 0)},");
        sb.AppendLine($"\t\t.MaxSpawnedAtOnce = {maxSpawnedAtOnce},");
        sb.AppendLine($"\t\t.SpawnCountFactor = {SpawnCountFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.SpawnRateFactor = {SpawnRateFactor.ToInvariantCulture()},");
        sb.AppendLine($"\t\t.SpawnParamCount = {Math.Min(4, spawnParamIdxs.Count)},");
        sb.AppendLine("\t\t.SpawnParamIds = {");
        for (int i = 0; i < 4; ++i)
            sb.AppendLine($"\t\t\t{(i < spawnParamIdxs.Count ? spawnParamIdxs[i] : -1)},");
        sb.AppendLine("\t\t}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}
