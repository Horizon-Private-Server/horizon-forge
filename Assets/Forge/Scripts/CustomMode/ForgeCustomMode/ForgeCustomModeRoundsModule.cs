using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Serialization;

[Serializable]
public class ForgeCustomModeRoundsModule : MonoBehaviour, IForgeCustomModeModule
{
	[Flags]
	public enum RoundResetFlags
	{
		[Description("CGM_ROUNDS_RESET_NONE")]
		None = 0,
		[Description("CGM_ROUNDS_RESET_RESPAWN_PLAYERS")]
		RespawnPlayers = 1 << 4,
		[Description("CGM_ROUNDS_RESET_REFILL_HEALTH")]
		RefillHealth = 1 << 5,
		[Description("CGM_ROUNDS_RESET_RETURN_FLAGS")]
		ReturnFlags = 1 << 6,
		[Description("CGM_ROUNDS_RESET_NODES")]
		ResetNodes = 1 << 7,
		[Description("CGM_ROUNDS_RESET_DESTROY_PLAYER_OBJECTS")]
		DestroyPlayerObjects = 1 << 8,
	}

	[Flags]
	public enum RoundCompleteFlags
	{
		[Description("CGM_ROUNDS_COMPLETE_NONE")]
		None = 0,
		[Description("CGM_ROUNDS_COMPLETE_TIME_REACHED")]
		TimeReached = 1 << 0,
		[Description("CGM_ROUNDS_COMPLETE_TARGET_REACHED")]
		TargetReached = 1 << 1,
		[Description("CGM_ROUNDS_COMPLETE_ALL_PLAYERS_DEAD")]
		AllPlayersDead = 1 << 2,
		[Description("CGM_ROUNDS_COMPLETE_ONE_TEAM_LEFT_ALIVE")]
		OneTeamLeftAlive = 1 << 3,
		[Description("CGM_ROUNDS_COMPLETE_CUSTOM")]
		Custom = 1 << 4,
	}

	public enum TargetSort
	{
		Most,
		Least
	}

	[Header("Rounds")]
	[HelpBox("Adds a Forge CGM round state machine. Custom code can also call cgmRoundsCompleteRoundWithCurrentWinner() or cgmRoundsCompleteRound(winner). Configure the Score Module with Rounds Completed or Round Points to decide when the game ends.", MessageType.Info)]
	[Tooltip("Enables the rounds runtime and generated cgm_rounds_config.c file for this custom mode.")]
	public bool Enabled;
	[Tooltip("Built-in reset actions to run when a new round starts, before the custom reset callback is invoked. Stats with a round aggregate type reset automatically.")]
	public RoundResetFlags ResetRound = RoundResetFlags.RespawnPlayers | RoundResetFlags.RefillHealth;
	[Tooltip("Built-in conditions that complete the current round. Multiple conditions can be enabled together.")]
	public RoundCompleteFlags RoundCompleteWhen = RoundCompleteFlags.TimeReached;
	[Tooltip("Seconds after a round starts before automatic round-complete conditions can fire. Helps avoid false completions while clients sync after reset.")]
	[Min(0)] public int RoundCompleteCheckDelaySeconds = 2;
	[Tooltip("Seconds before the current round completes when Time Reached is enabled. Set to 0 to disable the time check.")]
	[Min(0)] public int RoundTimeLimitSeconds = 60;
	[Tooltip("Ends the game through the Score Module after this many rounds have completed. Set to 0 for no round-count limit.")]
	[Min(0)] public int MaxRounds = 3;
	[Tooltip("Ends the game through the Score Module when any team/player reaches this many round points. Set to 0 for no round-point limit.")]
	[FormerlySerializedAs("MaxRoundWins")]
	[Min(0)] public int MaxRoundPoints = 0;
	[Tooltip("Points awarded by placement at the end of each round. Element 0 is 1st place, element 1 is 2nd place, and so on. Missing placements receive 0.")]
	public List<int> RoundPlacementPoints = new List<int>() { 3, 2, 1 };

	[Header("Round Objective")]
	[HelpBox("This is the round objective. The score HUD tracks this stat during each round, and round winners are sorted by this stat.", MessageType.Info)]
	[Tooltip("Whether the round winner is the team/player with the most or least of the selected round objective stat.")]
	public TargetSort Sort = TargetSort.Most;
	[Tooltip("Stat used by the score HUD during rounds and by the default round winner selection.")]
	public string RoundObjectiveStatName = "Kills";
	[Tooltip("Round objective value required to complete a round when Target Reached is enabled. Floats are stored internally with score precision.")]
	public string RoundObjectiveTarget = "0";
	[Tooltip("Displays the round objective score and target in the scoreboard HUD during rounds.")]
	public bool DisplayRoundTargetInScoreboardHud = true;
	[Tooltip("When the round objective uses a round aggregate, displays the live aggregate value in the scoreboard HUD.")]
	public bool ShowLiveAggregateScore = false;

	[Header("Custom Hooks")]
	[Tooltip("Optional: void FunctionName(int roundNumber); Runs after built-in reset logic when a new round starts.")]
	public string ResetRoundFunctionName;
	[Tooltip("Optional: void FunctionName(int roundNumber); Runs after a new round starts.")]
	public string RoundStartedFunctionName;
	[Tooltip("Optional: void FunctionName(int roundNumber); Runs after a round winner has been recorded.")]
	public string RoundCompletedFunctionName;
	[Tooltip("Optional: void FunctionName(int roundNumber); Runs once when the 3 second post-round grace period begins.")]
	public string PostRoundStartedFunctionName;
	[Tooltip("Optional: int FunctionName(void); Return non-zero to complete the current round when Custom is enabled.")]
	public string CustomRoundCompleteConditionFunctionName;
	[Tooltip("Optional: int FunctionName(void); Return the winning team/player id for the round, or -1 for a draw.")]
	public string SelectRoundWinnerFunctionName;

	public int ExecutionOrder => 3;

	public void OnValidate()
	{
		ResetRound &= RoundResetFlags.RespawnPlayers | RoundResetFlags.RefillHealth | RoundResetFlags.ReturnFlags | RoundResetFlags.ResetNodes | RoundResetFlags.DestroyPlayerObjects;
		if (RoundPlacementPoints == null)
			RoundPlacementPoints = new List<int>() { 3, 2, 1 };

		for (var i = 0; i < RoundPlacementPoints.Count; ++i)
			RoundPlacementPoints[i] = Math.Max(0, RoundPlacementPoints[i]);
		EnsureRoundObjectiveStatName();
	}

	public void Configure(string buildFolder, CodeGenState state)
	{
		if (!Enabled)
			return;

		if (!state.LDFlags.Contains("-DFORGE_CGM_SCORE"))
			throw new InvalidOperationException("Forge CGM Rounds Module requires the Score Module to be enabled.");

		var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);

		state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm_rounds.o");
		state.LDFlags.Add("-DFORGE_CGM_ROUNDS");
		state.Includes.Add("#include \"cgm_rounds.h\"");
		state.CleanupBody.Add("cgmRoundsCleanup();");
		state.InitBody.Add("cgmRoundsInit();");
		state.MainBodyReady.Remove("cgmScoreCheckTargetScoreReached();");
		state.MainBodyReady.Add("cgmRoundsTick();");
		state.MainBodyReady.Add("cgmScoreCheckTargetScoreReached();");
		state.DrawBody.Add("cgmRoundsDraw();");

		File.WriteAllText(Path.Combine(srcFolder, "cgm_rounds_config.c"), BuildRoundsConfig());
		state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm_rounds_config.o");
	}

	public void WriteExData(BinaryWriter writer)
	{
	}

	string BuildRoundsConfig()
	{
		if (GetRoundObjectiveStatIndex() < 0)
			throw new InvalidOperationException($"Round objective stat '{RoundObjectiveStatName}' was not found in Score Module Stats.");

		var sb = new StringBuilder();

		sb.AppendLine("#include \"cgm_rounds.h\"");
		sb.AppendLine();
		AppendPrototype(sb, "void", ResetRoundFunctionName, "int");
		AppendPrototype(sb, "void", RoundStartedFunctionName, "int");
		AppendPrototype(sb, "void", RoundCompletedFunctionName, "int");
		AppendPrototype(sb, "void", PostRoundStartedFunctionName, "int");
		AppendPrototype(sb, "int", CustomRoundCompleteConditionFunctionName, "void");
		AppendPrototype(sb, "int", SelectRoundWinnerFunctionName, "void");
		sb.AppendLine();

		sb.AppendLine("struct CgmRoundsConfig cgmRoundsConfig = {");
		sb.AppendLine($"\t.ResetFlags = {BuildFlags(ResetRound)},");
		sb.AppendLine($"\t.RoundCompleteFlags = {BuildFlags(RoundCompleteWhen)},");
		sb.AppendLine($"\t.RoundCompleteCheckDelaySeconds = {Math.Max(0, RoundCompleteCheckDelaySeconds)},");
		sb.AppendLine($"\t.RoundTimeLimitSeconds = {Math.Max(0, RoundTimeLimitSeconds)},");
		sb.AppendLine($"\t.MaxRounds = {Math.Max(0, MaxRounds)},");
		sb.AppendLine($"\t.MaxRoundPoints = {Math.Max(0, MaxRoundPoints)},");
		sb.AppendLine($"\t.RoundPlacementPoints = {{ {BuildRoundPlacementPoints()} }},");
		sb.AppendLine($"\t.RoundObjectiveTarget = {GetRoundObjectiveTarget()},");
		sb.AppendLine($"\t.RoundObjectiveStatIndex = {GetRoundObjectiveStatIndex()},");
		sb.AppendLine($"\t.RoundObjectiveLowerScoreWins = {(Sort == TargetSort.Least ? 1 : 0)},");
		sb.AppendLine($"\t.DisplayRoundTargetInScoreboardHud = {(DisplayRoundTargetInScoreboardHud ? 1 : 0)},");
		sb.AppendLine($"\t.ShowLiveAggregateScore = {(ShowLiveAggregateScore ? 1 : 0)},");
		sb.AppendLine($"\t.ResetRound = {GetFuncOrNull(ResetRoundFunctionName)},");
		sb.AppendLine($"\t.RoundStarted = {GetFuncOrNull(RoundStartedFunctionName)},");
		sb.AppendLine($"\t.RoundCompleted = {GetFuncOrNull(RoundCompletedFunctionName)},");
		sb.AppendLine($"\t.PostRoundStarted = {GetFuncOrNull(PostRoundStartedFunctionName)},");
		sb.AppendLine($"\t.CustomRoundCompleteCondition = {GetFuncOrNull(CustomRoundCompleteConditionFunctionName)},");
		sb.AppendLine($"\t.SelectRoundWinner = {GetFuncOrNull(SelectRoundWinnerFunctionName)},");
		sb.AppendLine("};");

		return sb.ToString();
	}

	static void AppendPrototype(StringBuilder sb, string returnType, string functionName, string args)
	{
		if (string.IsNullOrWhiteSpace(functionName))
			return;

		sb.AppendLine($"{returnType} {functionName.Trim()}({args});");
	}

	static string GetFuncOrNull(string functionName)
	{
		return string.IsNullOrWhiteSpace(functionName) ? "NULL" : functionName.Trim();
	}

	string BuildRoundPlacementPoints()
	{
		if (RoundPlacementPoints == null)
			return "";

		var points = RoundPlacementPoints
			.Take(10)
			.Select(x => Math.Max(0, x));

		return string.Join(", ", points);
	}

	int GetRoundObjectiveTarget()
	{
		switch (GetRoundObjectiveValueType())
		{
			case ForgeCustomModeScoreModule.StatValueType.Float:
				if (float.TryParse(RoundObjectiveTarget, out var fValue))
					return Math.Max(0, (int)(fValue * 1024));
				break;
			case ForgeCustomModeScoreModule.StatValueType.TimeSeconds:
			case ForgeCustomModeScoreModule.StatValueType.TimeMilliseconds:
			case ForgeCustomModeScoreModule.StatValueType.Integer:
				if (int.TryParse(RoundObjectiveTarget, out var iValue))
					return Math.Max(0, iValue);
				break;
		}

		return 0;
	}

	public int GetRoundObjectiveStatIndex()
	{
		var scoreModule = GetScoreModule();
		return scoreModule ? scoreModule.GetStatIndexByName(RoundObjectiveStatName) : -1;
	}

	public ForgeCustomModeScoreModule.StatValueType GetRoundObjectiveValueType()
	{
		var scoreModule = GetScoreModule();
		return scoreModule ? scoreModule.GetStatValueTypeByName(RoundObjectiveStatName) : ForgeCustomModeScoreModule.StatValueType.Integer;
	}

	public void EnsureRoundObjectiveStatName()
	{
		var scoreModule = GetScoreModule();
		if (!scoreModule || scoreModule.Stats.Count <= 0)
			return;

		if (scoreModule.GetStatIndexByName(RoundObjectiveStatName) < 0)
			RoundObjectiveStatName = scoreModule.Stats[0].Name;
	}

	ForgeCustomModeScoreModule GetScoreModule()
	{
		var modeData = GetComponentInParent<ForgeCustomModeData>(true);
		if (modeData)
			return modeData.GetComponentInChildren<ForgeCustomModeScoreModule>(true);

		return FindObjectOfType<ForgeCustomModeScoreModule>(true);
	}

	static string BuildFlags(Enum value)
	{
		var intValue = Convert.ToInt32(value);
		if (intValue == 0)
			return "0";

		var flags = Enum.GetValues(value.GetType())
			.Cast<Enum>()
			.Where(x => Convert.ToInt32(x) != 0 && (intValue & Convert.ToInt32(x)) == Convert.ToInt32(x))
			.Select(x => x.GetDescription());

		var result = string.Join(" | ", flags);
		return string.IsNullOrWhiteSpace(result) ? "0" : result;
	}
}
