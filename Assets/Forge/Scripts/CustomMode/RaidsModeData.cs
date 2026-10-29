using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Serialization;

public class RaidsModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int RAIDS_VERSION = 4;
    public const int MAX_ACTIONS_PER_MOB = 8;
    public const int MAX_PARAMS_PER_MOB = 16;
    public static readonly int[] DEFAULT_CLASS_XP_REWARDS = new int[] { 250, 750, 1500, 2500, 4000 };
    [NonSerialized] private SpriteContainer _buildSprites;

    public static readonly int MOB_SPAWNER_OCLASS = 0x4001;
    public static readonly int NPC_OCLASS = 0x4006;
    public static readonly int GOLDBOLT_OCLASS = 0x400E;
    public static readonly int SKILLFIELD_OCLASS = 0x4013;

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
    [HideInInspector, FormerlySerializedAs("MissionXpOverrides")] public RaidsDifficultyValues<Int32Override> ClassXpOverrides = new RaidsDifficultyValues<Int32Override>();

    [HideInInspector] public bool DebugEnabled;
    [HideInInspector] public bool DebugMenu;
    [HideInInspector] public bool DebugPath;
    [HideInInspector] public bool DebugMove;

    [Header("Music")]
    public bool OverrideTrackList = false;
    public List<DLMusicTracks> TrackWhitelist = new List<DLMusicTracks>();

    public int GetClassXpReward(int difficulty)
    {
        if (difficulty < 0 || difficulty >= 5) throw new ArgumentOutOfRangeException(nameof(difficulty));
        if (MissionType != RAIDS_MISSION_TYPES.Raid) return 0;

        var reward = ClassXpOverrides.Get(difficulty).GetValue(DEFAULT_CLASS_XP_REWARDS[difficulty]);
        if (reward < 0)
            throw new InvalidOperationException($"Class XP reward must be nonnegative ({difficulty + 1} stars).");
        return reward;
    }

    public T[] GetDefinitions<T>() where T : Component
    {
        return HierarchicalSorting.Sort(FindObjectsOfType<T>(true));
    }

    // Disabled/inactive mobs retain their index slots for spawner PVars.
    public List<RaidsMobDef> GetMobs()
    {
        return GetDefinitions<RaidsMobDef>().ToList();
    }

    public List<RaidsZoneDef> GetZones()
    {
        return GetDefinitions<RaidsZoneDef>().Where(x => x.isActiveAndEnabled).ToList();
    }

    public List<RaidsChallengeDef> GetChallenges()
    {
        return GetDefinitions<RaidsChallengeDef>().Where(x => x.isActiveAndEnabled).ToList();
    }

    public List<RaidsContractDef> GetContractRules()
    {
        return GetDefinitions<RaidsContractDef>().Where(x => x.isActiveAndEnabled).ToList();
    }

    private void ValidateDefinitionLimits()
    {
        if (GetMobs().Count > 16)
            throw new InvalidOperationException("Raids supports at most 16 mob definitions, including disabled mobs. No definitions have been removed.");
        var mobs = GetMobs().Where(x => x.isActiveAndEnabled).ToList();
        foreach (var mob in mobs) mob.ValidateConfig();

        var contracts = GetContractRules();
        if (contracts.Count > 16)
            throw new InvalidOperationException("Raids supports at most 16 contract groups.");
        var names = new HashSet<string>();
        foreach (var contract in contracts)
        {
            contract.GetMobMask(mobs);
            if (!names.Add(contract.ExportName))
                throw new InvalidOperationException($"Duplicate exported contract group name: {contract.ExportName}.");
        }
    }

    public Dictionary<string, long> GetMobOptions(bool asMask = false, int maxCount = int.MaxValue, bool includeNone = false)
    {
        var mobs = GetMobs();
        var labels = mobs.Select(x => x.isActiveAndEnabled ? x.Name : $"{x.Name} (DISABLED)").Take(maxCount).ToList();
        return GetDefinitionOptions(labels, asMask, includeNone);
    }

    public Dictionary<string, long> GetChallengeOptions()
    {
        return GetDefinitionOptions(GetChallenges().Select(x => x.Name).ToList());
    }

    private static Dictionary<string, long> GetDefinitionOptions(List<string> names, bool asMask = false, bool includeNone = false)
    {
        var options = new Dictionary<string, long>();
        if (includeNone) options.Add("None", -1);

        // Reserve original names so a generated label cannot hide a later entry.
        var reservedNames = new HashSet<string>(names.Where(x => !string.IsNullOrEmpty(x)));
        var counts = names.Where(x => !string.IsNullOrEmpty(x)).GroupBy(x => x).ToDictionary(x => x.Key, x => x.Count());
        for (var i = 0; i < names.Count; ++i)
        {
            var name = names[i];
            var label = string.IsNullOrEmpty(name) ? $"Entry [{i}]" : name;
            if (string.IsNullOrEmpty(name) || counts[name] > 1 || options.ContainsKey(label))
            {
                label = string.IsNullOrEmpty(name) ? $"Entry [{i}]" : $"{name} [{i}]";
                while (options.ContainsKey(label) || reservedNames.Contains(label)) label += " (entry)";
            }
            options.Add(label, asMask ? 1L << i : i);
        }
        return options;
    }

    private void OnValidate()
    {
        // at some point migrate the reuseable moby logic out of raids and into common
        var commonCodeGen = FindObjectOfType<CommonCodeGen>();
        if (!commonCodeGen)
           this.gameObject.AddComponent<CommonCodeGen>();
    }

    public void Configure(string buildFolder, CodeGenState state)
    {
        state.Debug = DebugEnabled; // override build settings
        ValidateDefinitionLimits();
        var mapConfig = FindObjectOfType<MapConfig>();
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);
        var enabledMobs = GetMobs().Where(x => x.isActiveAndEnabled);

        state.SeparateCodeFile = true;

        // copy raids base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "raids"), buildFolder);

        // build config.c and path.c
        File.WriteAllText(Path.Combine(srcFolder, "config.c"), GetConfigContents());
        File.WriteAllText(Path.Combine(srcFolder, "path.c"), GetPathContents());

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/config.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/path.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/levelselect.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/contracts.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/inventory.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/menu.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/spawner.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/vendor.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/badges.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/bank.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/skills.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/wallwalk.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/visibomb.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/skillfield.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/interop.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/classes.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/collectible.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/mobs/mob.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/npc.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/map.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/pathfind.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/utils.o");
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/ammodrop.o");

        if (state.Debug && DebugMenu)
        {
            state.LDFlags.Add("-DDEBUG_MENU");
            state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/debugmenu.o");
            state.Includes.Add("#include \"debugmenu.h\"");
            state.InitBody.Add("debugmenuInit();");
            state.CleanupBody.Add("debugmenuClose();");
            state.DrawBody.Add("debugmenuFrameTick();");
        }

        state.LDFlags.Add("-DRAIDS");
        state.LDFlags.Add($"-DRAIDS_VERSION={RAIDS_VERSION}");
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
        state.Includes.Add("#include \"utils.h\"");
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
        state.Includes.Add("#include \"mobs/mob.h\"");
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
        state.Includes.Add("#include \"classes.h\"");
        state.Includes.Add("#include \"interop.h\"");
        state.Includes.Add("#include \"skills.h\"");
        state.Includes.Add("#include \"skillfield.h\"");

        // declarations
        {
            state.Declarations.Add("void configInit(void);");
            state.Declarations.Add("struct RaidsMapConfig MapConfig __attribute__((section(\".config\"))) = {\r\n  .Magic = MAP_CONFIG_MAGIC,\r\n  .State = NULL,  .TrackWhitelist = NULL,\r\n};");
        }

        // init
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
            state.InitBody.Add($"classesInit();");
            state.InitBody.Add("interopInit();");
            state.InitBody.Add("skillfieldInit();");
            state.InitBody.Add("skillsInit();");

            state.InitBody.Add("respawnAllPlayers();");
        }

        // cleanup
        {
            state.CleanupBody.Add("skillsCleanup();");
            state.CleanupBody.Add("interopCleanup();");
            state.CleanupBody.Add("bankCleanup();");
        }

        // get gubers
        {
            state.GetGuberCase.Add("case SPAWNER_OCLASS: return spawnerGetGuber(moby);");
            state.GetGuberCase.Add("case SKILLFIELD_OCLASS: return skillfieldGetGuber(moby);");
            // if (mobTypes.Contains(RaidsMob.DZStriker))
                // state.GetGuberCase.Add("case MOBY_ID_DZ_STRIKER_TORSO_RED: return (moby->PParent ? moby->PParent->Guber : moby->Guber);");
        }

        // handle events
        {
            state.HandleGuberEventCase.Add("case SPAWNER_OCLASS: spawnerHandleEvent(moby, event); break;");
            state.HandleGuberEventCase.Add("case SKILLFIELD_OCLASS: skillfieldHandleEvent(moby, event); break;");
            // if (mobTypes.Contains(RaidsMob.DZStriker))
                // state.HandleGuberEventCase.Add("case MOBY_ID_DZ_STRIKER_TORSO_RED: dzstrikerTorsoOnSpawn(moby, event); break;");
        }

        // draw functions
        {
            state.DrawBody.Insert(0, "mapDrawHud();");
            state.DrawBody.Add("levelselectFrameTick();");
            state.DrawBody.Add("contractsFrameTick();");
            state.DrawBody.Add("inventoryFrameTick();");
            state.DrawBody.Add("skillsFrameTick();");
        }

        // ready
        {
            state.MainBodyReady.Add("mapStart();");
            state.MainBodyReady.Add("bankStart();");
            state.MainBodyReady.Add("levelselectStart();");
            state.MainBodyReady.Add("spawnerStart();");
            state.MainBodyReady.Add("vendorStart();");
            state.MainBodyReady.Add("badgesStart();");
            state.MainBodyReady.Add("ammodropStart();");
            state.MainBodyReady.Add("npcStart();");
            if (MissionType == RAIDS_MISSION_TYPES.OpenWorld)
                state.MainBodyReady.Add("mapApplyZoning();");
        }

        // tick
        {
            state.MainBody.Add("mapTick();");
            state.MainBody.Add("bankTick();");
            state.MainBody.Add("skillsTick();");
            if (state.Debug && DebugMenu) state.MainBody.Add("debugmenuTick();");
            state.MainBody.Add("inventoryTick();");
            state.MainBody.Add("contractsTick();");
            state.MainBody.Add("mobTick();");

            state.MainBody.Add("int i;");
            state.MainBody.Add("for (i = 0; i < PathsCount; ++i) pathTick(&Paths[i]);");
            state.MainBody.Add($"if (MapConfig.State) {{\r\n    MapConfig.State->MapBaseComplexity = {MapBaseComplexity};\r\n  }}");
            state.MainBody.Add("mapTickEnd();");
        }
    }

    public void Configure(BuildState state, BuildStateStage stage)
    {
        if (stage == BuildStateStage.Cleanup)
        {
            if (_buildSprites) DestroyImmediate(_buildSprites.gameObject);
            _buildSprites = null;
            return;
        }
        if (stage != BuildStateStage.BeforeBuild) return;
        ValidateDefinitionLimits();
        PrepareMobSprites(state);

        state.MobyOClasses.Add(8309); // node base (for capture sound)
        state.MobyOClasses.Add(6898); // health box (for health sound; nanoleech)
        state.MobyOClasses.Add(9278); // weapon pickup (for loot drops)
        state.MobyOClasses.Add(13);   // bolt (for gold bolt)
        state.MobyOClasses.Add(SKILLFIELD_OCLASS);
        if (GetMobs().Any(x => x.isActiveAndEnabled && x.Mob == RaidsMob.Reactor))
            state.MobyOClasses.Add(0x01F8); // Reactor trailshot

        // add mob oclasses
        var mobConfig = RaidsMobsScriptableObject.Load();
        foreach (var mob in GetMobs().Where(x => x.isActiveAndEnabled))
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

    private void PrepareMobSprites(BuildState state)
    {
        if (_buildSprites) DestroyImmediate(_buildSprites.gameObject);
        _buildSprites = null;
        var mapConfig = FindObjectOfType<MapConfig>();
        var sprites = mapConfig.DLSprites.Count > 0 ? mapConfig.GetSpriteDefs(RCVER.DL).ToList() : new List<SpriteDef>();
        var added = new List<SpriteDef>();
        foreach (var mob in GetMobs().Where(x => x.isActiveAndEnabled))
        {
            var variant = mob.GetDefaults().Variants[mob.Variant];
            Add(variant.SpriteTexture, variant.SpriteTextureTint);
            if (mob.IsBoss) Add(variant.BossTexture, variant.BossTextureTint);
        }
        if (added.Count == 0) return;
        if (mapConfig.DLSprites.Count == 0)
            throw new InvalidOperationException("Raids mob sprites require the map's base DL sprite bank.");
        _buildSprites = new GameObject("Raids Build Sprites").AddComponent<SpriteContainer>();
        _buildSprites.transform.SetParent(transform, false);
        _buildSprites.gameObject.hideFlags = HideFlags.HideAndDontSave;
        _buildSprites.RacVersion = state.RacVersion;
        _buildSprites.Sprites = added;

        void Add(Texture2D texture, Color tint)
        {
            if (!texture || sprites.Concat(added).Any(x => x.m_Texture == texture && (x.m_Tint ?? Color.white) == tint)) return;
            var uid = sprites.Concat(added).Select(x => (int)x.m_Uid).DefaultIfEmpty(0).Max() + 1;
            if (uid > ushort.MaxValue) throw new InvalidOperationException("No sprite UIDs remain for raids mob textures.");
            added.Add(new SpriteDef
            {
                m_Bank = SpriteDef.SpriteDefBank.Bank1,
                m_Texture = texture,
                m_TextureSizeOverride = SpriteDef.SpriteTextureSize._64,
                m_Tint = tint,
                m_Uid = (ushort)uid,
                m_Unknown = 1
            });
        }
    }

    string GetConfigContents()
    {
        var mobs = GetMobs().Where(x => x.isActiveAndEnabled).ToList();
        var mapConfig = FindObjectOfType<MapConfig>();
        var spriteDefs = mapConfig.DLSprites.Count > 0 ? mapConfig.GetSpriteDefs(RCVER.DL) : Array.Empty<SpriteDef>();
        var zones = GetZones();
        var contractRules = GetContractRules();
        var sb = new StringBuilder();

        sb.AppendLine("#include <libdl/utils.h>");
        sb.AppendLine("#include \"game.h\"");
        sb.AppendLine("#include \"mobs/mob.h\"");
        sb.AppendLine("");

        sb.AppendLine("extern struct RaidsMapConfig MapConfig;");
        sb.AppendLine("");

        sb.AppendLine($"struct MobActionConfig mobActionConfigs[][RAIDS_DIFFICULTY_COUNT][MOB_MAX_ACTIONS_PER_MOB] = {{");
        foreach (var mob in mobs)
        {
            sb.AppendLine("{");
            for (var difficulty = 0; difficulty < 5; ++difficulty)
                sb.AppendLine("{\n" + mob.GetActionDefs(difficulty) + "},");
            sb.AppendLine("},");
        }
        sb.AppendLine("};");
        sb.AppendLine($"union MobParameter mobParameters[][RAIDS_DIFFICULTY_COUNT][MOB_MAX_PARAMS_PER_MOB] = {{");
        foreach (var mob in mobs)
        {
            sb.AppendLine("{");
            for (var difficulty = 0; difficulty < 5; ++difficulty)
                sb.AppendLine("{\n" + mob.GetParameterDefs(difficulty) + "},");
            sb.AppendLine("},");
        }
        sb.AppendLine("};");

        sb.AppendLine("struct MobSpawnParams mobSpawnParams[] = {");
        foreach (var mob in mobs)
            sb.AppendLine(mob.GetDef(spriteDefs));
        sb.AppendLine("};");
        sb.AppendLine("");

        sb.AppendLine("struct RaidsDifficultyZone mapDifficultyZones[] = {");
        foreach (var zone in zones)
            sb.AppendLine(zone.GetDef());
        sb.AppendLine("};");
        sb.AppendLine($"int mapDifficultyZonesCount = {zones.Count};");
        sb.AppendLine("");

        sb.AppendLine("struct RaidsMobContractRule mobContractRules[] = {");
        foreach (var contractRule in contractRules)
        {
            sb.AppendLine(contractRule.GetDef(mobs));
        }
        sb.AppendLine("};");
        sb.AppendLine($"int mobContractRulesCount = {contractRules.Count};");
        sb.AppendLine("");

        sb.AppendLine($"int musicTrackWhitelistEnabled = {(OverrideTrackList ? 1 : 0)};");
        sb.AppendLine($"int musicTrackWhitelistCount = {TrackWhitelist.Count};");
        sb.AppendLine("int musicTrackWhitelist[] = {");
        foreach (var track in TrackWhitelist)
            sb.AppendLine($"\t{(int)track},");
        sb.AppendLine("};");
        sb.AppendLine("");

        sb.AppendLine("//--------------------------------------------------------------------------\r\nvoid configInit(void)\r\n{\r\n  MapConfig.MobSpawnParams = mobSpawnParams;\r\n  MapConfig.MobSpawnParamsCount = COUNT_OF(mobSpawnParams);\r\n  MapConfig.MobContractRules = mobContractRules;\r\n  MapConfig.MobContractRulesCount = mobContractRulesCount;\r\n  MapConfig.TrackWhitelist = musicTrackWhitelist;\r\n  MapConfig.TrackWhitelistCount = musicTrackWhitelistCount;\r\n  MapConfig.TrackWhitelistEnabled = musicTrackWhitelistEnabled;\r\n");
        for (var difficulty = 0; difficulty < 5; ++difficulty)
            sb.AppendLine($"  MapConfig.ClassXpRewards[{difficulty}] = {GetClassXpReward(difficulty)};");
        sb.AppendLine("}\r\n");

        return sb.ToString();
    }

    string GetPathContents()
    {
        return PathGraph.ExportGraphsAsC();
    }

    public override void Write(BinaryWriter writer)
    {
        ValidateDefinitionLimits();
        var challenges = GetChallenges();
        var mapConfig = FindObjectOfType<MapConfig>();
        var mobys = mapConfig.GetMobys(RCVER.DL);
        var goldBoltCount = mobys.Count(x => x.OClass == GOLDBOLT_OCLASS);
        var challengesCount = challenges.Count;
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
        writer.WriteString(BinaryHelper.StrToRatchetStr(mapConfig.MapAuthor), 31);
        writer.Write((byte)0);
        writer.WriteString(BinaryHelper.StrToRatchetStr(mapConfig.MapDescription), 255);
        writer.Write((byte)0);

        // prewrite header
        var offsets = new List<int>();
        var headerOffset = writer.BaseStream.Position;
        writer.Write(new byte[8 * challengesCount]);

        if (challenges.Count > 0)
        {
            foreach (var challenge in challenges)
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
    Tremor,
    Reactor = 9
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
