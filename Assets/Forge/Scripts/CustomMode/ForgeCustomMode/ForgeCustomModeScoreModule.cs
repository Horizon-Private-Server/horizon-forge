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
using UnityEngine.Serialization;

[Serializable]
public class ForgeCustomModeScoreModule : MonoBehaviour, IForgeCustomModeModule
{
	public enum StatSource
	{
		[Description("CGM_SCORE_STAT_KILLS")]
		Kills,
		[Description("CGM_SCORE_STAT_DEATHS")]
		Deaths,
		[Description("CGM_SCORE_STAT_SUICIDES")]
		Suicides,
		[Description("CGM_SCORE_STAT_KILLS_MINUS_SUICIDES")]
		Kills_Minus_Suicides,
		[Description("CGM_SCORE_STAT_HILL_TIME")]
		HillTime,
		[Description("CGM_SCORE_STAT_JUGGERNAUT_TIME")]
		JuggernautTime,
		[Description("CGM_SCORE_STAT_CAPS")]
		Caps,
		[Description("CGM_SCORE_STAT_SAVES")]
		Saves,
		[Description("CGM_SCORE_STAT_NODES")]
		NodesCaptured,
		[Description("CGM_SCORE_STAT_POINTS")]
		Points,
		[Description("CGM_SCORE_STAT_PLAYER_1")]
		PlayerStat1,
		[Description("CGM_SCORE_STAT_PLAYER_2")]
		PlayerStat2,
		[Description("CGM_SCORE_STAT_PLAYER_3")]
		PlayerStat3,
		[Description("CGM_SCORE_STAT_PLAYER_4")]
		PlayerStat4,
		[Description("CGM_SCORE_STAT_TEAM_1")]
		TeamStat1,
		[Description("CGM_SCORE_STAT_TEAM_2")]
		TeamStat2,
		[Description("CGM_SCORE_STAT_TEAM_3")]
		TeamStat3,
		[Description("CGM_SCORE_STAT_TEAM_4")]
		TeamStat4,
		[Description("CGM_SCORE_STAT_DISTANCE_TRAVELLED")]
		DistanceTravelled,
		[Description("CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS")]
		TimeAliveMilliseconds,
		[Description("CGM_SCORE_STAT_ROUNDS_COMPLETED")]
		RoundsCompleted,
		[Description("CGM_SCORE_STAT_ROUND_POINTS")]
		RoundPoints,
		[Obsolete("Use RoundPoints.")]
		[Description("CGM_SCORE_STAT_ROUND_POINTS")]
		RoundsWon = RoundPoints,
		[Obsolete("Use RoundPoints.")]
		[Description("CGM_SCORE_STAT_ROUND_POINTS")]
		RoundsLost
	}

	public enum ScoreTargetSource
	{
		[Description("CGM_SCORE_TARGET_KILLS_TO_WIN")]
		KillsToWin,
		[Description("CGM_SCORE_TARGET_BOLTS_TO_WIN")]
		BoltsToWin,
		[Description("CGM_SCORE_TARGET_HILL_TIME_TO_WIN")]
		HillTimeToWin,
		[Description("CGM_SCORE_TARGET_CAPS_TO_WIN")]
		CapsToWin,
		[Description("CGM_SCORE_TARGET_NODES_TO_WIN")]
		NodesToWin,
		[Description("CGM_SCORE_TARGET_CUSTOM")]
		Custom
	}

	public enum ScoreTargetSort
	{
		Most,
		Least
	}

	public enum StatValueType
	{
		[Description("CGM_SCORE_STAT_TYPE_INT")]
		Integer,
		[Description("CGM_SCORE_STAT_TYPE_TIME_SECONDS")]
		TimeSeconds,
		[Description("CGM_SCORE_STAT_TYPE_FLOAT")]
		Float,
		[Description("CGM_SCORE_STAT_TYPE_TIME_MILLISECONDS")]
		TimeMilliseconds,
	}
	
	public enum StatTrackerType
	{
		[Description("CGM_SCORE_STAT_TRACK_ADD")]
		Cumulative,
		[Description("CGM_SCORE_STAT_TRACK_MAX")]
		Largest,
		[Description("CGM_SCORE_STAT_TRACK_MIN")]
		Smallest,
		[Description("CGM_SCORE_STAT_TRACK_SET")]
		Newest
	}
	
	public enum StatTrackerSave
	{
		[Description("CGM_SCORE_STAT_TRACK_SAVE_WITH_STATS")]
		WhenMinTeamsForStatsMet,
		[Description("CGM_SCORE_STAT_TRACK_SAVE_WITH_RANK")]
		WhenMinTeamsForRankMet,
	}

	public enum ScoreRoundAggregateType
	{
		[Description("CGM_SCORE_ROUND_AGGREGATE_NONE")]
		None,
		[Description("CGM_SCORE_ROUND_AGGREGATE_SUM")]
		Sum,
		[Description("CGM_SCORE_ROUND_AGGREGATE_MAX")]
		Max,
		[Description("CGM_SCORE_ROUND_AGGREGATE_MIN")]
		Min,
		[Description("CGM_SCORE_ROUND_AGGREGATE_AVERAGE")]
		Average,
		[Description("CGM_SCORE_ROUND_AGGREGATE_LATEST")]
		Latest,
	}
	
	public enum StatTrackerSlot
	{
		None,
		Slot1,
		Slot2,
		Slot3,
		Slot4,
		Slot5,
		Slot6,
		Slot7,
		Slot8,
	}
	
	public enum ScoreboardType
	{
		[Description("CGM_SCORE_BOARD_TYPE_NORMAL")]
		Normal,
		[Description("CGM_SCORE_BOARD_TYPE_HIDDEN")]
		Hidden
	}

	[Serializable]
	public class ScoreObjective
	{
		[Tooltip("Whether the winning team is the one with the Most or Least of the following stat.")]
		public ScoreTargetSort Sort = ScoreTargetSort.Most;
		[Tooltip("Stat from Stats to use for final scoring. Stored by name so stats can be reordered safely.")]
		public string StatName = "Kills";
		[Tooltip("Whether to use the configured value when deciding when to end the game.")]
		public ScoreTargetSource Target = ScoreTargetSource.KillsToWin;
		public ScoreboardType Scoreboard = ScoreboardType.Normal;
		public string CustomTarget;
	}

	[Serializable]
	public class ScoreStat
	{
		public string Name;
		public bool DisplayOnEndGameScoreboard;
		public StatSource Source = StatSource.Kills;
		public StatValueType ValueType = StatValueType.Integer;
		[Tooltip("Optional in-match aggregate to keep behind the scenes when rounds reset this stat.")]
		public ScoreRoundAggregateType RoundAggregateType = ScoreRoundAggregateType.None;

		[Tooltip("If, and how, stat should be tracked by server between games.")]
		public StatTrackerSlot TrackerSlot = StatTrackerSlot.None;
		public StatTrackerType TrackerType = StatTrackerType.Cumulative;
		public StatTrackerSave TrackerSave = StatTrackerSave.WhenMinTeamsForStatsMet;
	}

	[HelpBox("Overrides game score and game end logic.", MessageType.Info)]
	public bool Enabled;

	public ScoreObjective Objective = new ScoreObjective();
	public List<ScoreStat> Stats = new List<ScoreStat>()
	{
		new ScoreStat() { Name = "Kills", Source = StatSource.Kills, ValueType = StatValueType.Integer, DisplayOnEndGameScoreboard = true },
		new ScoreStat() { Name = "Deaths", Source = StatSource.Deaths, ValueType = StatValueType.Integer, DisplayOnEndGameScoreboard = true },
	};

	public int ExecutionOrder => 1;

	public void OnValidate()
	{
		// limit stats to 16
		while (Stats.Count > 16)
			Stats.RemoveAt(16);

		foreach (var stat in Stats)
		{
			stat.Name = stat.Name.MaxLength(15);
			if (stat.Source == StatSource.TimeAliveMilliseconds && stat.ValueType == StatValueType.TimeSeconds)
				stat.ValueType = StatValueType.TimeMilliseconds;
		}

		if (string.IsNullOrWhiteSpace(Objective.StatName))
			Objective.StatName = Stats.FirstOrDefault()?.Name;

		EnsureObjectiveStatName();
	}

	public void Configure(string buildFolder, CodeGenState state)
	{
		if (!Enabled)
			return;

        var srcFolder = Path.Combine(buildFolder, FolderNames.CodeBuildSrcFolder);
		var scoreConfig = BuildScoreConfig();

		// add cgm_score
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm_score.o");
        state.LDFlags.Add("-DFORGE_CGM_SCORE");
        state.Includes.Add("#include \"cgm_score.h\"");
        state.CleanupBody.Add("cgmScoreCleanup();");
        state.InitBody.Add("cgmScoreInit();");
		state.MainBodyReady.Add("cgmScoreCheckTargetScoreReached();");
		state.MainBodyReady.Add("cgmScoreCheckForBroadcastCustomStats();");

		// add cgm_score_config
		File.WriteAllText(Path.Combine(srcFolder, "cgm_score_config.c"), scoreConfig);
        state.ObjectFiles.Add($"{FolderNames.CodeBuildSrcFolder}/cgm_score_config.o");
	}

	public void WriteExData(BinaryWriter writer)
	{
		// const int MAX_TRACKED_STATS = 8;
		// var emptyStat = new ScoreStat() { Name = null, TrackerSlot = StatTrackerSlot.None, ValueType = StatValueType.Integer, TrackerType = StatTrackerType.Cumulative };

		// var trackedStats = Stats.Where(x => x.TrackerSlot > StatTrackerSlot.None).ToList();
		// var duplicateSlots = trackedStats.GroupBy(x => x.TrackerSlot).Where(x => x.Count() > 1).Select(x => x.Key).ToList();
		// if (trackedStats.Count > MAX_TRACKED_STATS)
		// 	throw new InvalidOperationException($"Tracked Stats exceeds max of {MAX_TRACKED_STATS}");
		// if (duplicateSlots.Any())
		// 	throw new InvalidOperationException($"Tracked Stat {duplicateSlots.First()} is assigned too many stats.");

		// for (int i = 0; i < MAX_TRACKED_STATS; ++i)
		// {
		// 	var stat = Enabled ? (trackedStats.ElementAtOrDefault(i) ?? emptyStat) : emptyStat;

		// 	writer.WriteString(stat.Name, 16);
		// 	writer.Write((byte)stat.ValueType);
		// 	writer.Write((byte)stat.TrackerType);
		// 	writer.Write((sbyte)stat.TrackerSlot - 1);
		// }
	}

	string BuildScoreConfig()
	{
		ValidateStatNames();
		if (GetObjectiveStatIndex() < 0)
			throw new InvalidOperationException($"Score objective stat '{Objective.StatName}' was not found in Stats.");

		var sb = new StringBuilder();
		var objectiveValueType = GetObjectiveValueType();

		var customTarget = 0;
		switch (objectiveValueType)
		{
			case StatValueType.TimeSeconds:
			case StatValueType.TimeMilliseconds:
			case StatValueType.Integer:
				{
					if (int.TryParse(Objective.CustomTarget, out var iValue))
						customTarget = iValue;
					break;
				}
			case StatValueType.Float:
				{
					if (float.TryParse(Objective.CustomTarget, out var fValue))
						customTarget = (int)(fValue * 1024);
					break;
				}
		}

		sb.AppendLine("#include <libdl/utils.h>");
		sb.AppendLine("#include \"cgm_score.h\"");
		sb.AppendLine();

		sb.AppendLine("struct CgmScoreTarget cgmScoreTarget = {");
		sb.AppendLine($"\t.Target = {Objective.Target.GetDescription()},");
		sb.AppendLine($"\t.SortAscending = {(Objective.Sort == ScoreTargetSort.Least ? 1 : 0)},");
		sb.AppendLine($"\t.Scoreboard = {Objective.Scoreboard.GetDescription()},");
		sb.AppendLine($"\t.StatIndex = {GetObjectiveStatIndex()},");
		sb.AppendLine($"\t.CustomTarget = {customTarget},");
		sb.AppendLine("};");
		sb.AppendLine("");

		sb.AppendLine("struct CgmScoreStat cgmScoreStats[] = {");
		foreach (var stat in Stats)
		{
			sb.AppendLine("\t{");
			sb.AppendLine($"\t\t.Name = \"{stat.Name.MaxLength(16).Escape()}\",");
			sb.AppendLine($"\t\t.Source = {stat.Source.GetDescription()},");
			sb.AppendLine($"\t\t.ValueType = {stat.ValueType.GetDescription()},");
			sb.AppendLine($"\t\t.TrackerType = {stat.TrackerType.GetDescription()},");
			sb.AppendLine($"\t\t.TrackerSave = {stat.TrackerSave.GetDescription()},");
			sb.AppendLine($"\t\t.TrackerSlot = {(int)stat.TrackerSlot},");
			sb.AppendLine($"\t\t.DisplayOnEndGameScoreboard = {(stat.DisplayOnEndGameScoreboard ? 1 : 0)},");
			sb.AppendLine($"\t\t.RoundAggregateType = {stat.RoundAggregateType.GetDescription()},");
			sb.AppendLine("\t},");
		}
		sb.AppendLine("};");
        sb.AppendLine("const int cgmScoreStatsCount = COUNT_OF(cgmScoreStats);");

		return sb.ToString();
	}

	int GetObjectiveStatIndex()
	{
		return GetStatIndexByName(Objective.StatName);
	}

	StatValueType GetObjectiveValueType()
	{
		var statIndex = GetObjectiveStatIndex();
		if (statIndex >= 0)
			return Stats[statIndex].ValueType;

		return StatValueType.Integer;
	}

	public int GetStatIndexByName(string statName)
	{
		if (string.IsNullOrWhiteSpace(statName))
			return -1;

		return Stats.FindIndex(x => string.Equals(x.Name, statName, StringComparison.Ordinal));
	}

	public StatValueType GetStatValueTypeByName(string statName)
	{
		var statIndex = GetStatIndexByName(statName);
		return statIndex >= 0 ? Stats[statIndex].ValueType : StatValueType.Integer;
	}

	public string[] GetStatNames()
	{
		return Stats.Select((x, i) => string.IsNullOrWhiteSpace(x.Name) ? $"Stat {i + 1}" : x.Name).ToArray();
	}

	public void EnsureObjectiveStatName()
	{
		if (Stats.Count <= 0)
		{
			Objective.StatName = "";
			return;
		}

		if (GetStatIndexByName(Objective.StatName) < 0)
			Objective.StatName = Stats[0].Name;
	}

	void ValidateStatNames()
	{
		var emptyStatIndex = Stats.FindIndex(x => string.IsNullOrWhiteSpace(x.Name));
		if (emptyStatIndex >= 0)
			throw new InvalidOperationException($"Score stat {emptyStatIndex + 1} must have a name.");

		var duplicateName = Stats.GroupBy(x => x.Name).FirstOrDefault(x => x.Count() > 1)?.Key;
		if (!string.IsNullOrWhiteSpace(duplicateName))
			throw new InvalidOperationException($"Score stat name '{duplicateName}' is used more than once. Objective stats are stored by name, so names must be unique.");
	}
}
