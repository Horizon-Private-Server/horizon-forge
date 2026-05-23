using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Internal;

// NOTE
// add [RequireComponent(typeof(ModuleImplementationType))] for modules that write to ExData
// otherwise buffer will not be consistent
public class ForgeCustomModeData : CustomModeData, ICodeGen, IBuildHook
{
    public static readonly int FORGE_CGM_VERSION = 0;

	public enum ForgeCgmTeamType
	{
		AllowAll = 0,
		AllBlue,
		AllRed,
		FreeForAll,
		RedBlueEvenRandom,
		TwoTeamEvenRandom,
		ThreeTeamEvenRandom
	}
	
    public override DLCustomModeIds CustomMode => DLCustomModeIds.ForgeCustomMode;
    public override bool IsEnabled => Enabled && this.isActiveAndEnabled;
    public int CodeGenOrder => 99999999;

    public bool Enabled = true;
	public string CustomModeName = "Custom Mode";

	[HelpBox("If set, all Forge Cgm maps with the same code will share a leaderboard for Ranked stats.")]
	public string SharedRankCode = "";

    [Header("Base Game Settings")]
    public DLGameRules BaseGameMode = DLGameRules.Deathmatch;
	public ForgeCgmTeamType TeamRule = ForgeCgmTeamType.AllowAll;

	[Tooltip("If true, will prevent players from hurting each other even when on different teams.")]
	public bool DisablePvp = false;

	[Header("Stats")]
	[HelpBox("Whether vanilla leaderboard stats like Kills will be changed by this mode.")]
	public bool TrackBaseStats = false;

	[HelpBox("Whether custom stats are tracked.")]
	public bool TrackCustomStats = false;

	[HelpBox("Number of teams required for game to count towards Rank/Wins/Losses/Ranked Custom Stats.\nSet to 11 to disable rank.")]
	[Range(1, 11)] public int MinTeamsForRank = 2;

	[HelpBox("Number of teams required for game to count towards Games Played/Time Played/Custom Stats.")]
	[Range(1, 11)] public int MinTeamsForStats = 2;

	[Header("Custom Parameters")]
	public List<ForgeCustomModeParameter> Parameters = new List<ForgeCustomModeParameter>();

	[Header("Game Settings")]
	public ForgeCustomModeGameSettings GameSettings = new ForgeCustomModeGameSettings();

    private void OnValidate()
    {
        // make sure we always have the common code
        var commonCodeGen = FindObjectOfType<CommonCodeGen>(true);
        if (!commonCodeGen)
        {
            this.gameObject.AddComponent<CommonCodeGen>();
        }

		CustomModeName = CustomModeName.MaxLength(31);
		SharedRankCode = SharedRankCode.MaxLength(31);

		// make sure HillTimeToWin and BoltsToWin are multiples of 30 and 10, respectively.
		if (GameSettings.HillTimeToWin.HasOverride && (GameSettings.HillTimeToWin.OverrideValue % 30) != 0)
			GameSettings.HillTimeToWin.OverrideValue = ((GameSettings.HillTimeToWin.OverrideValue + 29) / 30) * 30;
		if (GameSettings.BoltsToWin.HasOverride && (GameSettings.BoltsToWin.OverrideValue % 10) != 0)
			GameSettings.BoltsToWin.OverrideValue = ((GameSettings.BoltsToWin.OverrideValue + 9) / 10) * 10;

		while (Parameters.Count > 4)
			Parameters.RemoveAt(4);
		foreach (var parameter in Parameters)
			parameter.OnValidate();
    }

	private IEnumerable<IForgeCustomModeModule> GetModules()
	{
		return this.GetComponentsInChildren<IForgeCustomModeModule>().OrderBy(x => x.ExecutionOrder).ToArray();
	}

    #region CodeGen

    public void Configure(string buildFolder, CodeGenState state)
    {
        var mapConfig = FindObjectOfType<MapConfig>();
        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
        var includeFolder = Path.Combine(buildFolder, FolderNames.CodeBuildIncludeFolder);

        state.SeparateCodeFile = true;

        // copy base code
        CodeManager.CopySourceFilesIntoWorkingDirectory(FolderNames.GetCodeGenFolder(RCVER.DL, "forge-cgm"), buildFolder);

		// inject function handlers
		var cgmSourcePath = Path.Combine(srcFolder, "cgm.c");
		var cgmSource = File.ReadAllText(cgmSourcePath);
		File.WriteAllText(cgmSourcePath, cgmSource);

        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm.o");
        state.LDFlags.Add("-DFORGE_CGM");
        state.Declarations.Add("void cgmInit(void);");
        state.InitBody.Add($"cgmInit();");

		if (DisablePvp)
			state.LDFlags.Add("-DFORGE_CGM_NO_PVP");

		// configure modules
		var modules = GetModules();
		foreach (var module in modules)
			module.Configure(buildFolder, state);
    }

    public void Configure(BuildState state, BuildStateStage stage)
    {

    }

    public override void Write(BinaryWriter writer)
    {
		int startPos = (int)writer.BaseStream.Position;
		var modules = GetModules().ToArray();

        writer.Write(FORGE_CGM_VERSION);
		writer.WriteString(CustomModeName, 32);
		writer.WriteString(SharedRankCode, 32);
		writer.Write((int)0); // placeholder for parameters position
		writer.WriteEnumOverride32(GameSettings.Gadgets);
        writer.Write((byte)BaseGameMode);
		writer.WriteEnumOverride8(GameSettings.RadarBlips);
		writer.WriteBoolOverride8(GameSettings.Vehicles);
		writer.WriteEnumOverride8(GameSettings.SpecialPickups);
		writer.WriteBoolOverride8(GameSettings.SpawnWithChargeboots);
		writer.WriteBoolOverride8(GameSettings.AutospawnWeapons);
		writer.WriteBoolOverride8(GameSettings.UnlimitedAmmo);
		writer.WriteIntOverride8(GameSettings.Timelimit);
		writer.WriteIntOverride8(GameSettings.RespawnTime);

		writer.WriteIntOverride8(GameSettings.KillsToWin);
		writer.WriteBoolOverride8(GameSettings.Survivor);
		writer.WriteEnumOverride8(GameSettings.JuggernautVis);
		writer.WriteEnumOverride8(GameSettings.JuggernautHealing);

		writer.WriteIntOverride8(GameSettings.CapsToWin);
		writer.WriteBoolOverride8(GameSettings.CrazyMode);
		writer.WriteBoolOverride8(GameSettings.FlagReturn);
		writer.WriteBoolOverride8(GameSettings.VehicleCarry);
		writer.WriteBoolOverride8(GameSettings.CtfHalftime);
		writer.WriteBoolOverride8(GameSettings.CtfOvertime);

		writer.WriteSteppedIntOverride8(GameSettings.HillTimeToWin, 30);
		writer.WriteIntOverride8(GameSettings.MovingHillTime);
		writer.WriteBoolOverride8(GameSettings.HillSharing);
		writer.WriteIntOverride8(GameSettings.HillArmor);

		writer.WriteSteppedIntOverride8(GameSettings.BoltsToWin, 10);
		writer.WriteEnumOverride8(GameSettings.SpecialRules);
		writer.WriteEnumOverride8(GameSettings.NodeType);
		writer.WriteBoolOverride8(GameSettings.Turrets);
		writer.WriteBoolOverride8(GameSettings.TeleporterUpgrade);
		writer.WriteIntOverride8(GameSettings.UpgradeTimer);
		writer.WriteIntOverride8(GameSettings.VoteTime);
		writer.WriteBoolOverride8(GameSettings.CqSaveCaptureProgress);
		writer.WriteBoolOverride8(GameSettings.CqDisableUpgrades);

		// patch
		writer.WriteBoolOverride8(GameSettings.DamageCooldown);
		writer.WriteBoolOverride8(GameSettings.Healthbars);
		writer.WriteEnumOverride8(GameSettings.Healthboxes);
		writer.WriteBoolOverride8(GameSettings.InstantDeath);
		writer.WriteBoolOverride8(GameSettings.Nametags);
		writer.WriteEnumOverride8(GameSettings.RadarShortDistance);
		writer.WriteBoolOverride8(GameSettings.RadarShortShared);
		writer.WriteBoolOverride8(GameSettings.SpawnImmunity);
		writer.WriteEnumOverride8(GameSettings.V2s);
		writer.WriteEnumOverride8(GameSettings.Vampire);
		writer.WriteBoolOverride8(GameSettings.WeaponPacks);
		writer.WriteBoolOverride8(GameSettings.WeaponPickups);

		// party
		writer.WriteBoolOverride8(GameSettings.ChargebootForever);
		writer.WriteEnumOverride8(GameSettings.Headbutt);
		writer.WriteBoolOverride8(GameSettings.HeadbuttFriendlyFire);
		writer.WriteBoolOverride8(GameSettings.RotatingWeapons);

		writer.Write((byte)TeamRule);
		writer.Write(TrackBaseStats);
		writer.Write(TrackCustomStats);
		writer.Write((byte)MinTeamsForRank);
		writer.Write((byte)MinTeamsForStats);

		// align to 4 bytes
		writer.Write(new byte[3]);

		// write modules
		foreach (var module in modules)
			module.WriteExData(writer);
			
		// write parameters
		int parametersStart = (int)writer.BaseStream.Position;
		writer.Write(Parameters.Count);
		foreach (var parameter in Parameters)
			parameter.Serialize(writer, startPos);
		
		writer.BaseStream.Position = startPos + 64 + 4;
		writer.Write(parametersStart - startPos);
		writer.Seek(0, SeekOrigin.End);
    }

    #endregion

    #region Menu Items

    [MenuItem("GameObject/Forge/Deadlocked/Forge CGM/Create Forge CGM Data", priority = 10)]
    public static void CreateDataData()
    {
        var go = new GameObject("Forge CGM");
        var modeData = go.AddComponent<ForgeCustomModeData>();

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

public interface IForgeCustomModeModule
{
	/// <summary>
	/// Indicate order in which module is processed.
	/// </summary>
	int ExecutionOrder { get; }

	void Configure(string buildFolder, CodeGenState state);
	void WriteExData(BinaryWriter writer);
}

[Serializable]
public class ForgeCustomModeGameSettings
{
	public enum CQSpecialRules
	{
		None,
		Lockdown,
		Homenodes
	}

	public enum CQNodeType
	{
		BoltCrank,
		HackerOrbs
	}

	public enum DLSpecialPickups
	{
		Off,
		On,
		Random
	}

	public enum DLRadarBlips
	{
		On,
		Short,
		Off
	}

	public enum DLJuggernautVis
	{
		Hit,
		Move,
		Always,
	}

	public enum DLJuggernautHealing
	{
		Off = 0,
		Low = 2,
		Normal = 4,
		High = 6,
	}

	public enum DLHeadbutt
	{
		Off,
		LowDamage,
		MediumDamage,
		HighDamage
	}

	public enum DLHealthboxes
	{
		On,
		NoBox,
		Off
	}

	public enum DLRadarShortMultiplier
	{
		X1,
		X2,
		X3,
		X4
	}

	public enum DLV2s
	{
		On,
		Always,
		Off
	}

	public enum DLVampire
	{
		Off,
		QuarterHeal,
		HalfHeal,
		FullHeal
	}

	// general
	[Header("General Overrides")]
    [OverrideNoDefault] public EnumOverride<DLGadgetMask> Gadgets;
    [OverrideNoDefault] public EnumOverride<DLRadarBlips> RadarBlips;
    [OverrideNoDefault] public BoolOverride Vehicles;
    [OverrideNoDefault] public EnumOverride<DLSpecialPickups> SpecialPickups;
    [OverrideNoDefault] public BoolOverride SpawnWithChargeboots;
    [OverrideNoDefault] public BoolOverride AutospawnWeapons;
    [OverrideNoDefault] public BoolOverride UnlimitedAmmo;
    [OverrideNoDefault, OverrideRange(0, 60)] public Int32Override Timelimit;
    [OverrideNoDefault, OverrideRange(0, 10)] public Int32Override RespawnTime;

	// DM
	[Header("Deathmatch Overrides")]
    [OverrideNoDefault, OverrideRange(0, 50)] public Int32Override KillsToWin;
    [OverrideNoDefault] public BoolOverride Survivor;

	// JUGG
	[Header("Juggernaut Overrides")]
    [OverrideNoDefault] public EnumOverride<DLJuggernautVis> JuggernautVis;
    [OverrideNoDefault] public EnumOverride<DLJuggernautHealing> JuggernautHealing;

	// CTF
	[Header("CTF Overrides")]
    [OverrideNoDefault, OverrideRange(0, 10)] public Int32Override CapsToWin;
    [OverrideNoDefault] public BoolOverride CrazyMode;
    [OverrideNoDefault] public BoolOverride FlagReturn;
    [OverrideNoDefault] public BoolOverride VehicleCarry;
    [OverrideNoDefault] public BoolOverride CtfOvertime;
    [OverrideNoDefault] public BoolOverride CtfHalftime;

	// KOTH
	[Header("KOTH Overrides")]
    [OverrideNoDefault, OverrideRange(0, 300)] public Int32Override HillTimeToWin;
    [OverrideNoDefault, OverrideRange(0, 180)] public Int32Override MovingHillTime;
    [OverrideNoDefault] public BoolOverride HillSharing;
    [OverrideNoDefault, OverrideRange(0, 100)] public Int32Override HillArmor;

	// CQ
	[Header("CQ Overrides")]
    [OverrideNoDefault, OverrideRange(0, 500)] public Int32Override BoltsToWin;
	[OverrideNoDefault] public EnumOverride<CQSpecialRules> SpecialRules;
	[OverrideNoDefault] public EnumOverride<CQNodeType> NodeType;
    [OverrideNoDefault] public BoolOverride Turrets;
    [OverrideNoDefault] public BoolOverride TeleporterUpgrade;
    [OverrideNoDefault, OverrideRange(0, 120)] public Int32Override UpgradeTimer;
    [OverrideNoDefault, OverrideRange(5, 15)] public Int32Override VoteTime;
    [OverrideNoDefault] public BoolOverride CqSaveCaptureProgress;
    [OverrideNoDefault] public BoolOverride CqDisableUpgrades;

	// Patch
	[Header("Patch Overrides")]
    [OverrideNoDefault] public BoolOverride DamageCooldown;
    [OverrideNoDefault] public BoolOverride Healthbars;
    [OverrideNoDefault] public EnumOverride<DLHealthboxes> Healthboxes;
    [OverrideNoDefault] public BoolOverride InstantDeath;
    [OverrideNoDefault] public BoolOverride Nametags;
    [OverrideNoDefault] public EnumOverride<DLRadarShortMultiplier> RadarShortDistance;
    [OverrideNoDefault] public BoolOverride RadarShortShared;
    [OverrideNoDefault] public BoolOverride SpawnImmunity;
    [OverrideNoDefault] public EnumOverride<DLV2s> V2s;
    [OverrideNoDefault] public EnumOverride<DLVampire> Vampire;
    [OverrideNoDefault] public BoolOverride WeaponPacks;
    [OverrideNoDefault] public BoolOverride WeaponPickups;

	// Party
	[Header("Party Overrides")]
    [OverrideNoDefault] public BoolOverride ChargebootForever;
    [OverrideNoDefault] public EnumOverride<DLHeadbutt> Headbutt;
    [OverrideNoDefault] public BoolOverride HeadbuttFriendlyFire;
    [OverrideNoDefault] public BoolOverride RotatingWeapons;

}

[Serializable]
public class ForgeCustomModeParameter
{
	public string Name;
	public string Description;
	public List<string> Options = new List<string>();

	public void OnValidate()
	{
		Name = Name.MaxLength(15);
		Description = Description.MaxLength(63);

		while (Options.Count > 16)
			Options.RemoveAt(16);
		for (int i = 0; i < Options.Count; ++i)
			Options[i] = Options[i].MaxLength(15);
	}

	public void Serialize(BinaryWriter writer, int exDataStartPos)
	{
		var startPos = (int)writer.BaseStream.Position;

		writer.Write(0);
		writer.Write(Options.Count);
		writer.WriteCString(Name);
		writer.WriteCString(Description);
		foreach (var option in Options)
			writer.WriteCString(option);

		writer.Align(4);

		var endPos = (int)writer.BaseStream.Position;
		writer.Seek(startPos, SeekOrigin.Begin);
		writer.Write(endPos - exDataStartPos);
		writer.Seek(0, SeekOrigin.End);


		// writer.WriteString(Name, 16);
		// writer.WriteString(Description, 64);
		// writer.Write(Options.Count);
		// foreach (var option in Options)
		// 	writer.WriteString(option, 16);
	}
}
