using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;

public class SurvivalModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int SURVIVAL_VERSION = 4;
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
    public bool IsLegacySurvivalMap = false;
    public List<SurvivalBakedSpawnPointItem> BakedSpawnPoints = new List<SurvivalBakedSpawnPointItem>();

    [Header("Mobs"), Tooltip("Your map's customized mob list. Max of 10.")]
    public List<SurvivalMobSpawnParam> Mobs = new List<SurvivalMobSpawnParam>()
    {
        new SurvivalMobSpawnParam() { Name = "Zombie" }
    };
    public Cuboid MobAllowedArea;
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

    [Header("Blessings")]
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

    [Header("Debug")]
    public bool DebugPath;
    public bool DebugMove;

    public List<SurvivalMobSpawnParam> GetEnabledMobs() => Mobs.Where(x => !x.Disabled).OrderBy(x => x.Probability).ThenBy(x => Mobs.IndexOf(x)).ToList();

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
        var enabledMobs = GetEnabledMobs();

        state.SeparateCodeFile = true;

        // write code seg patches
        WriteCodeSegPatches(mapConfig, new DirectoryInfo(buildFolder).Parent.FullName);

        // copy survival base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "survival"), buildFolder);

        // build config.c and path.c
        File.WriteAllText(Path.Combine(srcFolder, "config.c"), GetConfigContents());
        File.WriteAllText(Path.Combine(srcFolder, "path.c"), GetPathContents());

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/survival.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/path.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/gambits.o");

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/upgrade.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/drop.o");
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
        var mobTypes = enabledMobs.Select(x => x.Mob).Distinct();
        foreach (var mobType in mobTypes)
            state.LDFlags.Add($"-DMOB_{mobType.ToString().ToUpper()}");
        foreach (var mob in enabledMobs)
            state.LDFlags.Add($"-DMOB_SPAWN_PARAM_{mob.Name.ToUpper().Trim().Replace(" ", "_")}={enabledMobs.IndexOf(mob)}");

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
        state.GetGuberCase.Add("case UPGRADE_MOBY_OCLASS: return upgradeGetGuber(moby);");
        state.GetGuberCase.Add("case DROP_MOBY_OCLASS: return dropGetGuber(moby);");

        // handle events
        state.HandleGuberEventCase.Add("case MYSTERY_BOX_OCLASS: mboxHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case UPGRADE_MOBY_OCLASS: upgradeHandleEvent(moby, event); break;");
        state.HandleGuberEventCase.Add("case DROP_MOBY_OCLASS: dropHandleEvent(moby, event); break;");

        // 
        state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
    }

    public void Configure(BuildState state)
    {
        state.MobyOClasses.Add(RaidsModeData.LASERBEAM_OCLASS);
        state.MobyOClasses.Add(0x2635); // mysterybox
        state.MobyOClasses.Add(0x2124); // bigal
        state.MobyOClasses.Add(0x263A); // vendor
        state.MobyOClasses.Add(0x01F4); // drop
        state.MobyOClasses.Add(0x01F9); // upgrade
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
        // add custom sprites
    }

    string GetConfigContents()
    {
        var sb = new StringBuilder();
        var mapConfig = FindObjectOfType<MapConfig>();
        var enabledMobs = GetEnabledMobs();

        sb.AppendLine("#include <libdl/utils.h>");
        sb.AppendLine("#include \"game.h\"");
        sb.AppendLine("#include \"mob.h\"");
        sb.AppendLine();

        sb.AppendLine("extern struct SurvivalMapConfig MapConfig;");
        sb.AppendLine();

        // mob config
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("struct MobSpawnParams defaultSpawnParams[] = {");
        sb.AppendLine(GetMobDefs(enabledMobs));
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
        sb.AppendLine($"\t.StackboxBaseCost = {StackableBaseCost},");
        sb.AppendLine($"\t.StackboxCostPerPerk = {StackableIncrementCost},");
        sb.AppendLine("\t.BakedSpawnPoints = {");
        foreach (var item in BakedSpawnPoints)
            sb.AppendLine(item.GetDef());
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

        sb.AppendLine();
        sb.AppendLine("//--------------------------------------------------------------------------");
        sb.AppendLine("void configInit(void)");
        sb.AppendLine("{");
        sb.AppendLine("\tMapConfig.DefaultSpawnParams = defaultSpawnParams;");
        sb.AppendLine("\tMapConfig.DefaultSpawnParamsCount = COUNT_OF(defaultSpawnParams);");
        sb.AppendLine("\tMapConfig.SpecialRoundParams = specialRoundParams;");
        sb.AppendLine("\tMapConfig.SpecialRoundParamsCount = COUNT_OF(specialRoundParams);");
        sb.AppendLine($"\tMapConfig.WeaponPickupCooldownFactor = {WeaponPickupCooldownFactor};");
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

    string GetMobDefs(List<SurvivalMobSpawnParam> mobs)
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

            sb.Append(mob.GetDef(probability));

            totalProbability *= (1 - mob.Probability);
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

    #region Code Seg Patches

    void WriteCodeSegPatches(MapConfig mapConfig, string buildFolder)
    {
        var codeFolder = Path.Combine(buildFolder, FolderNames.BinaryCodeFolder);
        var survivalData = SurvivalMobsScriptableObject.Load();
        if (!survivalData) throw new Exception("missing survival mobs data");

        // write patches if not legacy
        if (!IsLegacySurvivalMap)
            WritePatches(survivalData, codeFolder);

        // write weapon stats
        WriteWeaponStats(survivalData, codeFolder);
    }

    void WritePatches(SurvivalMobsScriptableObject survivalData, string codeFolder)
    {
        foreach (var patch in survivalData.Patches)
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

    [MenuItem("Forge/Read Survival Legacy PathGraph", priority = 10)]
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

    [MenuItem("Forge/Read Survival Legacy Config", priority = 10)]
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

            if (survivalModeData)
            {
                survivalModeData.BakedSpawnPoints.Add(new SurvivalBakedSpawnPointItem()
                {
                    Type = type,
                    Transform = go.transform
                });
            }
        }
    }

    [MenuItem("Forge/Read Survival Legacy Gates", priority = 10)]
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
    Zombie,
    Swarmer,
    Swamper,
    StalkerTurret,
    Leviathan,
    DZStriker,
    Executioner,
    Reaper,
    Tremor,
    Reactor
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
        sb.AppendLine($"\t\t.DifficultyMultiplier = {DifficultyMultiplier},");
        sb.AppendLine($"\t\t.XpMultiplier = {XpMultiplier},");
        sb.AppendLine($"\t\t.BoltMultiplier = {BoltMultiplier},");
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
    [Min(0), Tooltip("Increase or decrease turn speed.")] public float TurnSpeedMultiplier = 1;
    [Min(0), Tooltip("For ranged attacks, how far away from the target the mob can be to fire.")] public float RangedAttackDistance = 50;
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

    public string GetDef(float? probabilityOverride = null)
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
        sb.AppendLine($"\t\t.Probability = {probabilityOverride ?? Probability},");
        sb.AppendLine($"\t\t.SpawnType = {(int)SpawnType},");
        sb.AppendLine($"\t\t.SpecialRoundOnly = {(SpecialRoundOnly ? 1 : 0)},");
        sb.AppendLine($"\t\t.StatId = {(int)defaults.StatId},");
        sb.AppendLine($"\t\t.Name = \"{name}\",");
        sb.AppendLine($"\t\t.Config = {{");
        sb.AppendLine($"\t\t\t.Xp = {(ushort)Math.Clamp(Xp.HasOverride ? Xp.OverrideValue : defaults.Xp, 0, ushort.MaxValue)},");
        sb.AppendLine($"\t\t\t.Bolts = {(int)(Bolts.HasOverride ? Bolts.OverrideValue : defaults.Bolts)},");
        sb.AppendLine($"\t\t\t.Bangles = 0x{(int)variant.Bangles:X4},");
        sb.AppendLine($"\t\t\t.Damage = {(Damage.HasOverride ? Damage.OverrideValue : defaults.Damage)},");
        sb.AppendLine($"\t\t\t.MaxDamage = {(DamageMax.HasOverride ? DamageMax.OverrideValue : defaults.DamageMax)},");
        sb.AppendLine($"\t\t\t.DamageScale = {(DamageScale.HasOverride ? DamageScale.OverrideValue : defaults.DamageScale)},");
        sb.AppendLine($"\t\t\t.Speed = {(Speed.HasOverride ? Speed.OverrideValue : defaults.Speed)},");
        sb.AppendLine($"\t\t\t.MaxSpeed = {(SpeedMax.HasOverride ? SpeedMax.OverrideValue : defaults.SpeedMax)},");
        sb.AppendLine($"\t\t\t.SpeedScale = {(SpeedScale.HasOverride ? SpeedScale.OverrideValue : defaults.SpeedScale)},");
        sb.AppendLine($"\t\t\t.Health = {(Health.HasOverride ? Health.OverrideValue : defaults.Health)},");
        sb.AppendLine($"\t\t\t.MaxHealth = {(HealthMax.HasOverride ? HealthMax.OverrideValue : defaults.HealthMax)},");
        sb.AppendLine($"\t\t\t.HealthScale = {(HealthScale.HasOverride ? HealthScale.OverrideValue : defaults.HealthScale)},");
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
public struct SurvivalFloatOverride
{
    public bool HasOverride;
    public float OverrideValue;
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
        sb.AppendLine($"\t\t.SpawnCountFactor = {SpawnCountFactor},");
        sb.AppendLine($"\t\t.SpawnRateFactor = {SpawnRateFactor},");
        sb.AppendLine($"\t\t.SpawnParamCount = {Math.Min(4, spawnParamIdxs.Count)},");
        sb.AppendLine("\t\t.SpawnParamIds = {");
        for (int i = 0; i < 4; ++i)
            sb.AppendLine($"\t\t\t{(i < spawnParamIdxs.Count ? spawnParamIdxs[i] : -1)},");
        sb.AppendLine("\t\t}");
        sb.AppendLine("\t},");

        return sb.ToString();
    }

}
