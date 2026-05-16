#include <libdl/game.h>
#include <libdl/player.h>
#include <libdl/hud.h>
#include <libdl/utils.h>
#include <libdl/net.h>
#include <libdl/stdio.h>
#include <libdl/string.h>
#include "cgm.h"
#include "cgm_score.h"

struct CgmScoreState cgmScoreState = {};

struct CustomPlayerStatMessage
{
	char PlayerId;
	int Values[MAX_CUSTOM_STATS];
};

struct CustomTeamStatMessage
{
	char TeamId;
	int Values[MAX_CUSTOM_STATS];
};

char cgmScoreCustomPlayerStatsDirty[GAME_MAX_PLAYERS] = {};
char cgmScoreCustomTeamStatsDirty[GAME_MAX_PLAYERS] = {};
int cgmScoreBroadcastCustomStatsTicker = 0;

//--------------------------------------------------------------------------
int cgmScoreOnRecvCustomPlayerStats(void *connection, void *data)
{
	struct CustomPlayerStatMessage msg;
	memcpy(&msg, data, sizeof(msg));

	if (msg.PlayerId < 0 || msg.PlayerId >= GAME_MAX_PLAYERS)
		return sizeof(msg);

	int i;
	for (i = 0; i < MAX_CUSTOM_STATS; ++i)
		cgmScoreState.CustomPlayerStats[i][(int)msg.PlayerId] = msg.Values[i];

	cgmScoreCustomPlayerStatsDirty[(int)msg.PlayerId] = 0;
	return sizeof(msg);
}

//--------------------------------------------------------------------------
int cgmScoreOnRecvCustomTeamStats(void *connection, void *data)
{
	struct CustomTeamStatMessage msg;
	memcpy(&msg, data, sizeof(msg));

	if (msg.TeamId < 0 || msg.TeamId >= GAME_MAX_PLAYERS)
		return sizeof(msg);

	int i;
	for (i = 0; i < MAX_CUSTOM_STATS; ++i)
		cgmScoreState.CustomTeamStats[i][(int)msg.TeamId] = msg.Values[i];

	cgmScoreCustomTeamStatsDirty[(int)msg.TeamId] = 0;
	return sizeof(msg);
}

//--------------------------------------------------------------------------
void cgmScoreBroadcastCustomPlayerStats(int playerId)
{
	void *connection = netGetDmeServerConnection();
	if (!connection)
		return;

	struct CustomPlayerStatMessage msg;
	msg.PlayerId = playerId;

	int i;
	for (i = 0; i < MAX_CUSTOM_STATS; ++i)
		msg.Values[i] = cgmScoreGetCustomPlayerIntStat(playerId, i);

	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CGM_MSG_ID_SEND_CUSTOM_PLAYER_STAT, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
void cgmScoreBroadcastCustomTeamStats(int teamId)
{
	void *connection = netGetDmeServerConnection();
	if (!connection)
		return;

	struct CustomTeamStatMessage msg;
	msg.TeamId = teamId;

	int i;
	for (i = 0; i < MAX_CUSTOM_STATS; ++i)
		msg.Values[i] = cgmScoreGetCustomTeamIntStat(teamId, i);

	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CGM_MSG_ID_SEND_CUSTOM_TEAM_STAT, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
void cgmScoreCheckForBroadcastCustomStats(void)
{
	if (cgmScoreBroadcastCustomStatsTicker > 0)
	{
		--cgmScoreBroadcastCustomStatsTicker;
		return;
	}

	// broadcast local player stats
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		if (!cgmScoreCustomPlayerStatsDirty[player->PlayerId])
			continue;

		cgmScoreCustomPlayerStatsDirty[player->PlayerId] = 0;
		cgmScoreBroadcastCustomPlayerStats(player->PlayerId);
	}

	// if host broadcast team stats
	if (gameAmIHost())
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			if (!cgmScoreCustomTeamStatsDirty[i])
				continue;

			cgmScoreCustomTeamStatsDirty[i] = 0;
			cgmScoreBroadcastCustomTeamStats(i);
		}
	}

	// run every 5 seconds
	cgmScoreBroadcastCustomStatsTicker = 60 * 5;
}

//--------------------------------------------------------------------------
int cgmScoreGetCustomPlayerIntStat(int playerId, int statId)
{
	return cgmScoreState.CustomPlayerStats[statId][playerId];
}

//--------------------------------------------------------------------------
int cgmScoreSetCustomPlayerIntStat(int playerId, int statId, int value)
{
	cgmScoreCustomPlayerStatsDirty[playerId] |= cgmScoreState.CustomPlayerStats[statId][playerId] != value;
	return cgmScoreState.CustomPlayerStats[statId][playerId] = value;
}

//--------------------------------------------------------------------------
int cgmScoreIncCustomPlayerIntStat(int playerId, int statId, int amount)
{
	cgmScoreCustomPlayerStatsDirty[playerId] |= amount != 0;
	return cgmScoreState.CustomPlayerStats[statId][playerId] += amount;
}

//--------------------------------------------------------------------------
float cgmScoreGetCustomPlayerFloatStat(int playerId, int statId)
{
	return cgmScoreState.CustomPlayerStats[statId][playerId] / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreSetCustomPlayerFloatStat(int playerId, int statId, float value)
{
	return cgmScoreSetCustomPlayerIntStat(playerId, statId, value * SCORE_FLOAT_PRECISION) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreIncCustomPlayerFloatStat(int playerId, int statId, float amount)
{
	return cgmScoreIncCustomPlayerIntStat(playerId, statId, amount * SCORE_FLOAT_PRECISION) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
int cgmScoreGetCustomTeamIntStat(int teamId, int statId)
{
	return cgmScoreState.CustomTeamStats[statId][teamId];
}

//--------------------------------------------------------------------------
int cgmScoreSetCustomTeamIntStat(int teamId, int statId, int value)
{
	cgmScoreCustomTeamStatsDirty[teamId] |= cgmScoreState.CustomTeamStats[statId][teamId] != value;
	return cgmScoreState.CustomTeamStats[statId][teamId] = value;
}

//--------------------------------------------------------------------------
int cgmScoreIncCustomTeamIntStat(int teamId, int statId, int amount)
{
	cgmScoreCustomTeamStatsDirty[teamId] |= amount != 0;
	return cgmScoreState.CustomTeamStats[statId][teamId] += amount;
}

//--------------------------------------------------------------------------
float cgmScoreGetCustomTeamFloatStat(int teamId, int statId)
{
	return cgmScoreState.CustomTeamStats[statId][teamId] / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreSetCustomTeamFloatStat(int teamId, int statId, float value)
{
	return cgmScoreSetCustomTeamIntStat(teamId, statId, value * SCORE_FLOAT_PRECISION) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreIncCustomTeamFloatStat(int teamId, int statId, float amount)
{
	return cgmScoreIncCustomTeamIntStat(teamId, statId, amount * SCORE_FLOAT_PRECISION) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
int cgmScoreGetGameEndReasonScoreReached(void)
{
	GameOptions *gameOptions = gameGetOptions();
	GameSettings *gameSettings = gameGetSettings();

	switch (gameSettings->GameRules)
	{
	case GAMERULE_CQ:
		return gameOptions->GameFlags.MultiplayerGameFlags.UNK_1C
							 ? GAME_END_REASON_TARGET_NODE_CAPS_REACHED
							 : GAME_END_REASON_TARGET_BOLTS_REACHED;
	case GAMERULE_CTF:
		return GAME_END_REASON_TARGET_FLAG_CAPS_REACHED;
	case GAMERULE_KOTH:
		return GAME_END_REASON_TARGET_HILL_TIME_REACHED;
	case GAMERULE_DM:
	case GAMERULE_JUGGY:
		return GAME_END_REASON_TARGET_KILLS_REACHED;
	}

	return GAME_END_REASON_TARGET_KILLS_REACHED;
}

//--------------------------------------------------------------------------
int cgmScoreGetTeamHasPlayer(int team)
{
	GameData *gameData = gameGetData();
	return gameData->TeamCaptain[team] != -1;
}

//--------------------------------------------------------------------------
int cgmScoreGetPlayerStat(int playerIdx, enum CgmScoreStatSource source)
{
	GameData *gameData = gameGetData();
	GameSettings *gameSettings = gameGetSettings();
	Player *player = playerGetFromIndex(playerIdx);
	int team = player ? playerGetJuggSafeTeam(player) : gameSettings->PlayerTeams[playerIdx];

	switch (source)
	{
		// handle team stats represented by sum of team's player stats
	case CGM_SCORE_STAT_KILLS:
		return gameData->PlayerStats.Kills[playerIdx];
	case CGM_SCORE_STAT_DEATHS:
		return gameData->PlayerStats.Deaths[playerIdx];
	case CGM_SCORE_STAT_SUICIDES:
		return gameData->PlayerStats.Suicides[playerIdx];
	case CGM_SCORE_STAT_KILLS_MINUS_SUICIDES:
		return gameData->PlayerStats.Kills[playerIdx] - gameData->PlayerStats.Suicides[playerIdx];
	case CGM_SCORE_STAT_HILL_TIME:
		return (int)gameData->PlayerStats.KingHillHoldTime[playerIdx];
	case CGM_SCORE_STAT_JUGGERNAUT_TIME:
		return (int)gameData->PlayerStats.JuggernautTime[playerIdx];
	case CGM_SCORE_STAT_CAPS:
		return gameData->PlayerStats.CtfFlagsCaptures[playerIdx];
	case CGM_SCORE_STAT_SAVES:
		return gameData->PlayerStats.CtfFlagsSaved[playerIdx];
	case CGM_SCORE_STAT_NODES:
		return gameData->PlayerStats.ConquestNodesCaptured[playerIdx];
	case CGM_SCORE_STAT_POINTS:
		return gameData->PlayerStats.TicketScore[playerIdx];

		// custom player stats
	case CGM_SCORE_STAT_PLAYER_1:
		return cgmScoreState.CustomPlayerStats[0][playerIdx];
	case CGM_SCORE_STAT_PLAYER_2:
		return cgmScoreState.CustomPlayerStats[1][playerIdx];
	case CGM_SCORE_STAT_PLAYER_3:
		return cgmScoreState.CustomPlayerStats[2][playerIdx];
	case CGM_SCORE_STAT_PLAYER_4:
		return cgmScoreState.CustomPlayerStats[3][playerIdx];

		// custom team stats
	case CGM_SCORE_STAT_TEAM_1:
		return cgmScoreState.CustomTeamStats[0][team];
	case CGM_SCORE_STAT_TEAM_2:
		return cgmScoreState.CustomTeamStats[1][team];
	case CGM_SCORE_STAT_TEAM_3:
		return cgmScoreState.CustomTeamStats[2][team];
	case CGM_SCORE_STAT_TEAM_4:
		return cgmScoreState.CustomTeamStats[3][team];
	}

	return 0;
}

//--------------------------------------------------------------------------
int cgmScoreGetTeamScore(int team)
{
	GameData *gameData = gameGetData();

	switch (cgmScoreTarget.Source)
	{
		// return raw team stat
	case CGM_SCORE_STAT_POINTS:
		return gameData->TeamStats.TeamTicketScore[team];
	case CGM_SCORE_STAT_CAPS:
		return gameData->TeamStats.FlagCaptureCounts[team];
	case CGM_SCORE_STAT_TEAM_1:
		return cgmScoreState.CustomTeamStats[0][team];
	case CGM_SCORE_STAT_TEAM_2:
		return cgmScoreState.CustomTeamStats[1][team];
	case CGM_SCORE_STAT_TEAM_3:
		return cgmScoreState.CustomTeamStats[2][team];
	case CGM_SCORE_STAT_TEAM_4:
		return cgmScoreState.CustomTeamStats[3][team];

		// fall through to sum of team's player's stats
	default:
		break;
	}

	int score = 0;
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!player)
			continue;

		if (playerGetJuggSafeTeam(player) == team)
			score += cgmScoreGetPlayerStat(i, cgmScoreTarget.Source);
	}

	return score;
}

//--------------------------------------------------------------------------
int cgmScoreGetFormattedScore(int score, enum CgmScoreStatValueType type)
{
	GameData *gameData = gameGetData();

	switch (type)
	{
	case CGM_SCORE_STAT_TYPE_INT:
		return score;
	case CGM_SCORE_STAT_TYPE_TIME_SECONDS:
		return score;
	case CGM_SCORE_STAT_TYPE_FLOAT:
		return score / SCORE_FLOAT_PRECISION;
	}

	return score;
}

//--------------------------------------------------------------------------
int cgmScoreGetFormattedTeamScore(int team)
{
	GameData *gameData = gameGetData();
	int rawValue = cgmScoreGetTeamScore(team);
	return cgmScoreGetFormattedScore(rawValue, cgmScoreTarget.ValueType);
}

//--------------------------------------------------------------------------
int cgmScoreGetTargetScore(void)
{
	int score = 0;
	GameData *gameData = gameGetData();

	switch (cgmScoreTarget.Target)
	{
	// we copy score target from game flags into custom target at the start of the game
	// case CGM_SCORE_TARGET_KILLS_TO_WIN:
	// 	return gameOptions->GameFlags.MultiplayerGameFlags.KillsToWin;
	// case CGM_SCORE_TARGET_BOLTS_TO_WIN:
	// 	return gameOptions->GameFlags.MultiplayerGameFlags.BoltsToWin * 10;
	// case CGM_SCORE_TARGET_HILL_TIME_TO_WIN:
	// 	return gameOptions->GameFlags.MultiplayerGameFlags.HillTimeToWin * 30;
	// case CGM_SCORE_TARGET_CAPS_TO_WIN:
	// 	return gameOptions->GameFlags.MultiplayerGameFlags.CapsToWin;
	case CGM_SCORE_TARGET_CAPS_TO_WIN:
	case CGM_SCORE_TARGET_HILL_TIME_TO_WIN:
	case CGM_SCORE_TARGET_BOLTS_TO_WIN:
	case CGM_SCORE_TARGET_KILLS_TO_WIN:
	case CGM_SCORE_TARGET_CUSTOM:
		return cgmScoreTarget.CustomTarget;
	case CGM_SCORE_TARGET_NODES_TO_WIN:
		return (gameData->NumNodes + 1) / 2;
	}

	return score;
}

//--------------------------------------------------------------------------
void cgmScoreSetTeamScoreboardOverride(int team, int score)
{
	gameScoreboardSetTeamScore(team, cgmScoreGetFormattedTeamScore(team));
}

//--------------------------------------------------------------------------
void cgmScoreSetScoreboardMaxOverride(int max)
{
	gameScoreboardSetScoreMax(cgmScoreGetFormattedScore(cgmScoreGetTargetScore(), cgmScoreTarget.ValueType));
}

//--------------------------------------------------------------------------
void cgmScoreSendSerializedStatsUpstreamToMode(void)
{
	struct CgmStats stats;
	GameData *gameData = gameGetData();

	// no func ptr
	if (!MapConfig.ModeFunctions.UpdateStats)
		return;

	memset(&stats, 0, sizeof(stats));

	stats.WinningTeam = gameData->WinningPlayer >= 0 ? gameData->WinningPlayer : gameData->WinningTeam;

	// fill team scores
	int i;
	stats.TeamScoreType = cgmScoreTarget.ValueType;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		stats.TeamScores[i] = cgmScoreGetTeamScore(i);

	// fill placements
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		stats.TeamPlacements[i] = -1;

		// team must have had one player at start of game
		if (!cgmScoreGetTeamHasPlayer(i))
			continue;

		// count number of teams with greater score
		int j;
		int score = stats.TeamScores[i];
		int placement = 0;
		for (j = 0; j < GAME_MAX_PLAYERS; ++j)
		{
			// either greater or same but team id is lower
			// ties like this don't matter here just need unique placements that are correct at a glance
			if (stats.TeamScores[j] > score)
				placement++;
			else if (stats.TeamScores[j] == score && j < i)
				placement++;
		}

		stats.TeamPlacements[i] = placement;
	}

	// fill player stats
	int s = 0;
	for (i = 0; i < MAX_SCOREBOARD_STATS; ++i)
	{
		// find next stat to display on end game scoreboard
		while (s < MAX_CUSTOM_STATS && cgmScoreStats[s].DisplayOnEndGameScoreboard == 0)
			++s;

		if (s >= MAX_CUSTOM_STATS || s >= cgmScoreStatsCount)
			break;

		safe_strcpy(stats.PlayerStatNames[i], cgmScoreStats[s].Name, sizeof(stats.PlayerStatNames[i]));
		stats.PlayerStatTypes[i] = cgmScoreStats[s].ValueType;
		int p;
		for (p = 0; p < GAME_MAX_PLAYERS; ++p)
			stats.PlayerStatValues[p][i] = cgmScoreGetPlayerStat(p, cgmScoreStats[s].Source);

		++s;
	}

	// pass upstream
	MapConfig.ModeFunctions.UpdateStats(&stats);
}

//--------------------------------------------------------------------------
int cgmScoreUpdateCurrentWinner(int force)
{
	GameOptions *gameOptions = gameGetOptions();
	int targetScore = cgmScoreGetTargetScore();
	int winningIndex = -1;
	int winningScore = 0;
	int numWithWinningScore = 0;
	int teamSize[GAME_MAX_PLAYERS] = {};

	// count num players per team
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!player)
			continue;

		int team = playerGetJuggSafeTeam(player);
		if (team >= 0)
			teamSize[team] += 1;
	}

	// find team with highest/lowest score
	// count number of teams with same highest score in case of draw
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		// skip empty teams
		if (teamSize[i] <= 0)
			continue;

		int teamScore = cgmScoreGetTeamScore(i);
		if (winningIndex < 0 || (!cgmScoreTarget.SortDescending && teamScore > winningScore) || (cgmScoreTarget.SortDescending && teamScore < winningScore))
		{
			winningIndex = i;
			numWithWinningScore = 1;
			winningScore = teamScore;
		}
		else if (teamScore == winningScore)
		{
			winningIndex = i;
			numWithWinningScore++;
		}
	}

	// set game winner if target reached or forced
	// even when winner is determined by team/player with LEAST score
	int hasWinner = winningScore >= targetScore && targetScore > 0;
	int isDraw = numWithWinningScore != 1;
	int winner = isDraw ? -1 : winningIndex;
	if (cgmScoreState.HasWinnerOverride)
		winner = cgmScoreState.WinnerOverride;

	if (hasWinner || force)
	{
		gameSetWinner(winner, gameOptions->GameFlags.MultiplayerGameFlags.Teamplay);
		cgmScoreSendSerializedStatsUpstreamToMode();
	}

	// return whether we have a winner
	return hasWinner;
}

//--------------------------------------------------------------------------
void cgmScoreGameEndedUpdateWinnerOverride(void)
{
	GameData *gameData = gameGetData();

	// if game reached time limit, determine who won and change the GameEndReason accordingly
	if (gameData->GameEndReason == GAME_END_REASON_TIME_LIMIT_REACHED)
	{
		cgmScoreUpdateCurrentWinner(1);
		gameData->GameEndReason = cgmScoreGetGameEndReasonScoreReached();
		return;
	}

	// otherwise assert the provided game winning team
	if (gameData->WinningPlayer >= 0)
		gameSetWinner(gameData->WinningPlayer, 0);
	else
		gameSetWinner(gameData->WinningTeam, 1);
}

//--------------------------------------------------------------------------
void cgmScoreAfterGameOverDataUpdated(void)
{
	void *gameOverData = (void *)0x001e0d78;

	int *playerPoints = (int *)(gameOverData + 0x7c0);
	int largestScore = 0;
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		int score = cgmScoreGetPlayerStat(i, cgmScoreTarget.Source);
		playerPoints[i] = score;
		if (score > largestScore)
			largestScore = score;
	}

	// invert points to flip sort order
	if (cgmScoreTarget.SortDescending)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			playerPoints[i] = largestScore - playerPoints[i];
		}
	}
}

//--------------------------------------------------------------------------
void cgmScoreEndGameEarly(void)
{
	// update winner and end game
	cgmScoreUpdateCurrentWinner(1);
	gameEnd(cgmScoreGetGameEndReasonScoreReached());
}

//--------------------------------------------------------------------------
void cgmScoreEndGameEarlyWithWinner(int winnerOverride)
{
	cgmScoreState.WinnerOverride = winnerOverride;
	cgmScoreState.HasWinnerOverride = 1;
	cgmScoreEndGameEarly();
}

//--------------------------------------------------------------------------
void cgmScoreCheckTargetScoreReached(void)
{
	GameData *gameData = gameGetData();

	// game already over
	if (gameData->GameIsOver || !gameAmIHost())
		return;

	// get current winner
	// if we have a winner trigger game end
	if (cgmScoreUpdateCurrentWinner(0))
		gameEnd(cgmScoreGetGameEndReasonScoreReached());
}

//--------------------------------------------------------------------------
void cgmScoreUpdateGameState(PatchStateContainer_t *gameState)
{
	GameSettings *gameSettings = gameGetSettings();
	GameOptions *gameOptions = gameGetOptions();

	// post custom game stats
	if (!gameState->UpdateCustomGameStats || !gameState->CustomGameStats)
		return;

	gameState->CustomGameStatsSize = sizeof(struct CgmCustomGameStats);
	struct CgmCustomGameStats *sGameData = (struct CgmCustomGameStats *)gameState->CustomGameStats->Payload;
	sGameData->Version = 0x00000001;
	sGameData->TeamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	sGameData->OrderScoreByAscending = 0; // not yet implemented

	// set team scores
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		sGameData->TeamScores[i] = cgmScoreGetTeamScore(i);

	// set teams
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		// prefer using player for team
		// but if someone leaves their team should still be tracked
		Player *player = playerGetFromIndex(i);
		if (player)
			sGameData->PlayerTeams[i] = playerGetJuggSafeTeam(player);
		else
			sGameData->PlayerTeams[i] = sGameData->TeamsEnabled ? gameSettings->PlayerTeams[i] : i;
	}

	// set tracked stats
	int s = 0;
	for (i = 0; i < MAX_TRACKED_STATS; ++i)
	{
		// find next tracked stat
		while (s < cgmScoreStatsCount && cgmScoreStats[s].TrackerSlot == 0)
			++s;

		if (s >= cgmScoreStatsCount)
			break;

		// copy stat def into buffer
		safe_strcpy(sGameData->TrackedStats[i].Name, cgmScoreStats[s].Name, sizeof(sGameData->TrackedStats[i].Name));
		sGameData->TrackedStats[i].ValueType = cgmScoreStats[s].ValueType;
		sGameData->TrackedStats[i].TrackerType = cgmScoreStats[s].TrackerType;
		sGameData->TrackedStats[i].TrackerSave = cgmScoreStats[s].TrackerSave;
		sGameData->TrackedStats[i].TrackerSlot = cgmScoreStats[s].TrackerSlot - 1;

		// copy player stat values for tracked stat into buffer
		int p;
		for (p = 0; p < GAME_MAX_PLAYERS; ++p)
			sGameData->TrackedStatValues[i][p] = cgmScoreGetPlayerStat(p, cgmScoreStats[s].Source);

		++s;
	}
}

//--------------------------------------------------------------------------
void cgmScoreInit(void)
{
	static int init = 0;
	if (init)
		return;

	// hook update scoreboard
	HOOK_JAL(0x005404f0, cgmScoreSetTeamScoreboardOverride);
	HOOK_JAL(0x005404d0, cgmScoreSetScoreboardMaxOverride);

	// hook game over data update to save our custom "points" for end scoreboard sorting
	HOOK_J(0x00623d2c, &cgmScoreAfterGameOverDataUpdated);

	// override game ended who won
	HOOK_JAL(0x00623434, cgmScoreGameEndedUpdateWinnerOverride);
	POKE_U32(0x0062343c, 0x100001fd); // b 0x00623c3c
	POKE_U32(0x00623440, 0x27C4D600); // addiu a0,fp,-0x2A00

	// invert hud scoreboard sort
	if (cgmScoreTarget.SortDescending)
	{
		POKE_U32(0x00542640, 0x15400044); // bne t2,zero,0x00542754
		POKE_U32(0x005426D0, 0x1040005D); // beq v0,zero,0x00542848
	}

	// hook net msgs
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_CUSTOM_PLAYER_STAT, &cgmScoreOnRecvCustomPlayerStats);
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_CUSTOM_TEAM_STAT, &cgmScoreOnRecvCustomTeamStats);

	// reset winner override
	cgmScoreState.WinnerOverride = -1;

	// set scoreboard hud
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		PlayerHUDFlags *hud = hudGetPlayerFlags(i);

		switch (cgmScoreTarget.Scoreboard)
		{
		case CGM_SCORE_BOARD_TYPE_NORMAL:
			// do nothing
			break;
		case CGM_SCORE_BOARD_TYPE_HIDDEN:
			hud->Flags.ConquestScoreboard = 0;
			hud->Flags.NormalScoreboard = 0;
			break;
		}
	}

	// initialize scoreboard max value
	cgmScoreSetScoreboardMaxOverride(0);

	// copy score config from game settings into custom before overwriting values
	GameOptions *gameOptions = gameGetOptions();
	switch (cgmScoreTarget.Target)
	{
	case CGM_SCORE_TARGET_KILLS_TO_WIN:
		cgmScoreTarget.CustomTarget = gameOptions->GameFlags.MultiplayerGameFlags.KillsToWin;
		break;
	case CGM_SCORE_TARGET_BOLTS_TO_WIN:
		cgmScoreTarget.CustomTarget = gameOptions->GameFlags.MultiplayerGameFlags.BoltsToWin * 10;
		break;
	case CGM_SCORE_TARGET_HILL_TIME_TO_WIN:
		cgmScoreTarget.CustomTarget = gameOptions->GameFlags.MultiplayerGameFlags.HillTimeToWin * 30;
		break;
	case CGM_SCORE_TARGET_CAPS_TO_WIN:
		cgmScoreTarget.CustomTarget = gameOptions->GameFlags.MultiplayerGameFlags.CapsToWin;
		break;

	// do nothing
	case CGM_SCORE_TARGET_NODES_TO_WIN:
	case CGM_SCORE_TARGET_CUSTOM:
		break;
	}

	// disable default game end by score
	gameOptions->GameFlags.MultiplayerGameFlags.KillsToWin = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.BoltsToWin = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.CapsToWin = 0;
	gameOptions->GameFlags.MultiplayerGameFlags.HillTimeToWin = 0;

	init = 1;
}
