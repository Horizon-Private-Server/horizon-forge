using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

[Serializable]
public class ForgeCustomModeRoundsModule : MonoBehaviour, IForgeCustomModeModule
{
	[Flags]
	public enum RoundResetFlags
	{
		[Description("CGM_ROUNDS_RESET_NONE")]
		None = 0,
		[Description("CGM_ROUNDS_RESET_PLAYER_STATS")]
		PlayerStats = 1 << 0,
		[Description("CGM_ROUNDS_RESET_TEAM_STATS")]
		TeamStats = 1 << 1,
		[Description("CGM_ROUNDS_RESET_CUSTOM_PLAYER_STATS")]
		CustomPlayerStats = 1 << 2,
		[Description("CGM_ROUNDS_RESET_CUSTOM_TEAM_STATS")]
		CustomTeamStats = 1 << 3,
		[Description("CGM_ROUNDS_RESET_RESPAWN_PLAYERS")]
		RespawnPlayers = 1 << 4,
		[Description("CGM_ROUNDS_RESET_REFILL_HEALTH")]
		RefillHealth = 1 << 5,
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
		[Description("CGM_ROUNDS_COMPLETE_CUSTOM")]
		Custom = 1 << 3,
	}

	public enum TargetSort
	{
		Most,
		Least
	}

	[Header("Rounds")]
	[HelpBox("Adds a Forge CGM round state machine. Custom code can also call cgmRoundsCompleteRoundWithCurrentWinner() or cgmRoundsCompleteRound(winner). Configure the Score Module with Rounds Completed, Rounds Won, or Rounds Lost to decide when the game ends.", MessageType.Info)]
	[Tooltip("Enables the rounds runtime and generated cgm_rounds_config.c file for this custom mode.")]
	public bool Enabled;
	[Tooltip("Built-in reset actions to run when a new round starts, before the custom reset callback is invoked.")]
	public RoundResetFlags ResetRound = RoundResetFlags.PlayerStats | RoundResetFlags.TeamStats | RoundResetFlags.RespawnPlayers | RoundResetFlags.RefillHealth;
	[Tooltip("Built-in conditions that complete the current round. Multiple conditions can be enabled together.")]
	public RoundCompleteFlags RoundCompleteWhen = RoundCompleteFlags.TimeReached;
	[Tooltip("Seconds before the current round completes when Time Reached is enabled. Set to 0 to disable the time check.")]
	[Min(0)] public int RoundTimeLimitSeconds = 60;
	[Tooltip("Ends the game through the Score Module after this many rounds have completed. Set to 0 for no round-count limit.")]
	[Min(0)] public int MaxRounds = 3;
	[Tooltip("Ends the game through the Score Module when any team/player reaches this many round wins. Set to 0 for no round-win limit.")]
	[Min(0)] public int MaxRoundWins = 0;

	[Header("Round Objective")]
	[HelpBox("This is the round objective. The score HUD tracks this stat during each round, and round winners are sorted by this stat.", MessageType.Info)]
	[Tooltip("Whether the round winner is the team/player with the most or least of the selected round objective stat.")]
	public TargetSort Sort = TargetSort.Most;
	[Tooltip("Stat used by the score HUD during rounds and by the default round winner selection.")]
	public ForgeCustomModeScoreModule.StatSource RoundObjectiveSource = ForgeCustomModeScoreModule.StatSource.Kills_Minus_Suicides;
	[Tooltip("How to format the round objective value in the score HUD.")]
	public ForgeCustomModeScoreModule.StatValueType RoundObjectiveValueType = ForgeCustomModeScoreModule.StatValueType.Integer;
	[Tooltip("Round objective value required to complete a round when Target Reached is enabled. Floats are stored internally with score precision.")]
	public string RoundObjectiveTarget = "0";

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
		state.InitBody.Add("cgmRoundsInit();");
		state.MainBody.Remove("cgmScoreCheckTargetScoreReached();");
		state.MainBody.Add("cgmRoundsTick();");
		state.MainBody.Add("cgmScoreCheckTargetScoreReached();");
		state.DrawBody.Add("cgmRoundsDraw();");

		File.WriteAllText(Path.Combine(srcFolder, "cgm_rounds_config.c"), BuildRoundsConfig());
		state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm_rounds_config.o");
	}

	public void WriteExData(BinaryWriter writer)
	{
	}

	string BuildRoundsConfig()
	{
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
		sb.AppendLine($"\t.RoundTimeLimitSeconds = {Math.Max(0, RoundTimeLimitSeconds)},");
		sb.AppendLine($"\t.MaxRounds = {Math.Max(0, MaxRounds)},");
		sb.AppendLine($"\t.MaxRoundWins = {Math.Max(0, MaxRoundWins)},");
		sb.AppendLine($"\t.RoundObjectiveTarget = {GetRoundObjectiveTarget()},");
		sb.AppendLine($"\t.RoundObjectiveSource = {RoundObjectiveSource.GetDescription()},");
		sb.AppendLine($"\t.RoundObjectiveValueType = {RoundObjectiveValueType.GetDescription()},");
		sb.AppendLine($"\t.RoundObjectiveLowerScoreWins = {(Sort == TargetSort.Least ? 1 : 0)},");
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

	int GetRoundObjectiveTarget()
	{
		switch (RoundObjectiveValueType)
		{
			case ForgeCustomModeScoreModule.StatValueType.Float:
				if (float.TryParse(RoundObjectiveTarget, out var fValue))
					return Math.Max(0, (int)(fValue * 1024));
				break;
			case ForgeCustomModeScoreModule.StatValueType.TimeSeconds:
			case ForgeCustomModeScoreModule.StatValueType.Integer:
				if (int.TryParse(RoundObjectiveTarget, out var iValue))
					return Math.Max(0, iValue);
				break;
		}

		return 0;
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

		return string.Join(" | ", flags);
	}
}
