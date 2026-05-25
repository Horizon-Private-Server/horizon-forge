#include <libdl/game.h>
#include <libdl/player.h>
#include <libdl/hud.h>
#include <libdl/math3d.h>
#include <libdl/utils.h>
#include <libdl/net.h>
#include <libdl/stdio.h>
#include <libdl/string.h>
#include <libdl/time.h>
#include "cgm.h"
#include "cgm_score.h"

#ifdef FORGE_CGM_ROUNDS
#include "cgm_rounds.h"
#endif

//--------------------------------------------------------------------------
struct CgmScoreState cgmScoreState = {};

//--------------------------------------------------------------------------
struct CustomPlayerStatMessage
{
	char PlayerId;
	int Values[MAX_CUSTOM_STATS];
};

//--------------------------------------------------------------------------
struct CustomTeamStatMessage
{
	char TeamId;
	int Values[MAX_CUSTOM_STATS];
};

//--------------------------------------------------------------------------
struct PlayerRoundAggregateMessage
{
	char PlayerId;
	int RoundsCompleted;
	int Values[MAX_CUSTOM_STATS];
};

//--------------------------------------------------------------------------
char cgmScoreCustomPlayerStatsDirty[GAME_MAX_PLAYERS] = {};
char cgmScoreCustomTeamStatsDirty[GAME_MAX_PLAYERS] = {};
int cgmScoreBroadcastCustomStatsTicker = 0;

//--------------------------------------------------------------------------
void cgmScoreSendSerializedStatsUpstreamToMode(void);
int cgmScoreGetRoundAggregateCurrentValue(int row, int statIndex, int teamsEnabled);

//--------------------------------------------------------------------------
void cgmScoreRefreshSerializedStatsUpstreamIfGameOver(void)
{
	GameData *gameData = gameGetData();
	if (gameData->GameIsOver)
		cgmScoreSendSerializedStatsUpstreamToMode();
}

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
	cgmScoreRefreshSerializedStatsUpstreamIfGameOver();
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
	cgmScoreRefreshSerializedStatsUpstreamIfGameOver();
	return sizeof(msg);
}

//--------------------------------------------------------------------------
int cgmScoreOnRecvPlayerRoundAggregates(void *connection, void *data)
{
	struct PlayerRoundAggregateMessage msg;
	memcpy(&msg, data, sizeof(msg));

	if (msg.PlayerId < 0 || msg.PlayerId >= GAME_MAX_PLAYERS)
		return sizeof(msg);

#ifdef FORGE_CGM_ROUNDS
	if (msg.RoundsCompleted < cgmRoundsGetRoundsCompleted())
		return sizeof(msg);
#endif

	int i;
	for (i = 0; i < MAX_CUSTOM_STATS; ++i)
		cgmScoreState.RoundPlayerAggregateValues[i][(int)msg.PlayerId] = msg.Values[i];

	cgmScoreRefreshSerializedStatsUpstreamIfGameOver();
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
void cgmScoreBroadcastPlayerRoundAggregates(int playerId)
{
	void *connection = netGetDmeServerConnection();
	if (!connection)
		return;

	struct PlayerRoundAggregateMessage msg;
	msg.PlayerId = playerId;
	msg.RoundsCompleted = cgmScoreState.RoundAggregateCount;

#ifdef FORGE_CGM_ROUNDS
	msg.RoundsCompleted = cgmRoundsGetRoundsCompleted();
#endif

	int i;
	for (i = 0; i < MAX_CUSTOM_STATS; ++i)
		msg.Values[i] = cgmScoreState.RoundPlayerAggregateValues[i][playerId];

	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CGM_MSG_ID_SEND_PLAYER_ROUND_AGGREGATES, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
void cgmScoreBroadcastLocalPlayerRoundAggregates(void)
{
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		cgmScoreBroadcastPlayerRoundAggregates(player->PlayerId);
	}
}

//--------------------------------------------------------------------------
void cgmScoreCheckForBroadcastCustomStats(void)
{
	cgmScoreUpdateManualStatSources();
	cgmScoreUpdateRoundTrackedStats();

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

	// run every quarter second
	cgmScoreBroadcastCustomStatsTicker = 15;
}

//--------------------------------------------------------------------------
int cgmScoreCanUpdateCustomStat(int force)
{
	if (force)
		return 1;

	GameData *gameData = gameGetData();
	if (gameData->GameIsOver)
		return 0;

#ifdef FORGE_CGM_ROUNDS
	if (cgmRoundsState.InPostRoundGrace)
		return 0;
#endif

	return 1;
}

//--------------------------------------------------------------------------
int cgmScoreIsPlayerAliveForManualStats(Player *player)
{
	return player && playerIsValid(player) && !playerIsDead(player) && player->Health > 0;
}

//--------------------------------------------------------------------------
void cgmScorePrimeManualStatSourceForPlayer(int playerIdx, Player *player)
{
	if (!player || !playerIsValid(player))
	{
		cgmScoreState.ManualPlayerHasLastPosition[playerIdx] = 0;
		cgmScoreState.ManualPlayerWasAlive[playerIdx] = 0;
		return;
	}

	vector_copy(cgmScoreState.ManualPlayerLastPosition[playerIdx], player->PlayerPosition);
	cgmScoreState.ManualPlayerHasLastPosition[playerIdx] = 1;
	cgmScoreState.ManualPlayerWasAlive[playerIdx] = cgmScoreIsPlayerAliveForManualStats(player);
}

//--------------------------------------------------------------------------
void cgmScorePrimeManualStatSources(void)
{
	cgmScoreState.ManualPlayerLastUpdateTime = gameGetTime();
	cgmScoreState.ManualPlayerLastDistanceSampleTime = cgmScoreState.ManualPlayerLastUpdateTime;

	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		cgmScorePrimeManualStatSourceForPlayer(i, playerGetFromIndex(i));
}

//--------------------------------------------------------------------------
void cgmScoreUpdateManualStatSources(void)
{
	int now = gameGetTime();
	if (cgmScoreState.ManualPlayerLastUpdateTime == now)
		return;

	int elapsedMs = cgmScoreState.ManualPlayerLastUpdateTime > 0 ? now - cgmScoreState.ManualPlayerLastUpdateTime : 0;
	if (elapsedMs < 0)
		elapsedMs = 0;

	cgmScoreState.ManualPlayerLastUpdateTime = now;
	int shouldSampleDistance = now - cgmScoreState.ManualPlayerLastDistanceSampleTime >= TIME_SECOND;
	if (shouldSampleDistance)
		cgmScoreState.ManualPlayerLastDistanceSampleTime = now;

	int canAccumulate = cgmScoreCanUpdateCustomStat(0);
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		int alive = cgmScoreIsPlayerAliveForManualStats(player);

		if (!canAccumulate || !player || !playerIsValid(player))
		{
			cgmScorePrimeManualStatSourceForPlayer(i, player);
			continue;
		}

		if (alive && (!cgmScoreState.ManualPlayerWasAlive[i] || !cgmScoreState.ManualPlayerHasLastPosition[i]))
		{
			cgmScorePrimeManualStatSourceForPlayer(i, player);
			continue;
		}

		if (alive && cgmScoreState.ManualPlayerWasAlive[i] && cgmScoreState.ManualPlayerHasLastPosition[i])
		{
			float distance = shouldSampleDistance ? vector_distance(player->PlayerPosition, cgmScoreState.ManualPlayerLastPosition[i]) : 0;
			if (distance > 0.1)
				cgmScoreState.ManualPlayerDistanceTravelled[i] += distance * SCORE_FLOAT_PRECISION;

			cgmScoreState.ManualPlayerTimeAliveMs[i] += elapsedMs;
		}

		if (shouldSampleDistance)
			cgmScorePrimeManualStatSourceForPlayer(i, player);
		else
			cgmScoreState.ManualPlayerWasAlive[i] = alive;
	}
}

//--------------------------------------------------------------------------
int cgmScoreGetCustomPlayerIntStat(int playerId, int statId)
{
	if (statId < 0 || statId >= MAX_CUSTOM_PLAYER_STATS)
		return 0;

	return cgmScoreState.CustomPlayerStats[statId][playerId];
}

//--------------------------------------------------------------------------
int cgmScoreSetCustomPlayerIntStat(int playerId, int statId, int value, int force)
{
	if (!cgmScoreCanUpdateCustomStat(force))
		return cgmScoreState.CustomPlayerStats[statId][playerId];

	cgmScoreCustomPlayerStatsDirty[playerId] |= cgmScoreState.CustomPlayerStats[statId][playerId] != value;
	return cgmScoreState.CustomPlayerStats[statId][playerId] = value;
}

//--------------------------------------------------------------------------
int cgmScoreIncCustomPlayerIntStat(int playerId, int statId, int amount, int force)
{
	if (!cgmScoreCanUpdateCustomStat(force))
		return cgmScoreState.CustomPlayerStats[statId][playerId];

	cgmScoreCustomPlayerStatsDirty[playerId] |= amount != 0;
	return cgmScoreState.CustomPlayerStats[statId][playerId] += amount;
}

//--------------------------------------------------------------------------
float cgmScoreGetCustomPlayerFloatStat(int playerId, int statId)
{
	if (statId < 0 || statId >= MAX_CUSTOM_PLAYER_STATS)
		return 0;

	return cgmScoreState.CustomPlayerStats[statId][playerId] / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreSetCustomPlayerFloatStat(int playerId, int statId, float value, int force)
{
	return cgmScoreSetCustomPlayerIntStat(playerId, statId, value * SCORE_FLOAT_PRECISION, force) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreIncCustomPlayerFloatStat(int playerId, int statId, float amount, int force)
{
	return cgmScoreIncCustomPlayerIntStat(playerId, statId, amount * SCORE_FLOAT_PRECISION, force) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
int cgmScoreGetCustomTeamIntStat(int teamId, int statId)
{
	if (statId < 0 || statId >= MAX_CUSTOM_TEAM_STATS)
		return 0;

	return cgmScoreState.CustomTeamStats[statId][teamId];
}

//--------------------------------------------------------------------------
int cgmScoreSetCustomTeamIntStat(int teamId, int statId, int value, int force)
{
	if (!cgmScoreCanUpdateCustomStat(force))
		return cgmScoreState.CustomTeamStats[statId][teamId];

	cgmScoreCustomTeamStatsDirty[teamId] |= cgmScoreState.CustomTeamStats[statId][teamId] != value;
	return cgmScoreState.CustomTeamStats[statId][teamId] = value;
}

//--------------------------------------------------------------------------
int cgmScoreIncCustomTeamIntStat(int teamId, int statId, int amount, int force)
{
	if (!cgmScoreCanUpdateCustomStat(force))
		return cgmScoreState.CustomTeamStats[statId][teamId];

	cgmScoreCustomTeamStatsDirty[teamId] |= amount != 0;
	return cgmScoreState.CustomTeamStats[statId][teamId] += amount;
}

//--------------------------------------------------------------------------
float cgmScoreGetCustomTeamFloatStat(int teamId, int statId)
{
	if (statId < 0 || statId >= MAX_CUSTOM_TEAM_STATS)
		return 0;

	return cgmScoreState.CustomTeamStats[statId][teamId] / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreSetCustomTeamFloatStat(int teamId, int statId, float value, int force)
{
	return cgmScoreSetCustomTeamIntStat(teamId, statId, value * SCORE_FLOAT_PRECISION, force) / SCORE_FLOAT_PRECISION;
}

//--------------------------------------------------------------------------
float cgmScoreIncCustomTeamFloatStat(int teamId, int statId, float amount, int force)
{
	return cgmScoreIncCustomTeamIntStat(teamId, statId, amount * SCORE_FLOAT_PRECISION, force) / SCORE_FLOAT_PRECISION;
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
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!playerIsValid(player))
			continue;

		if (playerGetJuggSafeTeam(player) == team)
			return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
int cgmScoreGetRawPlayerStat(int playerIdx, enum CgmScoreStatSource source)
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
	case CGM_SCORE_STAT_DISTANCE_TRAVELLED:
		return cgmScoreState.ManualPlayerDistanceTravelled[playerIdx];
	case CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS:
		return cgmScoreState.ManualPlayerTimeAliveMs[playerIdx];

#ifdef FORGE_CGM_ROUNDS
	case CGM_SCORE_STAT_ROUNDS_COMPLETED:
		return cgmRoundsGetRoundsCompleted();
	case CGM_SCORE_STAT_ROUND_POINTS:
		return cgmRoundsGetRoundPoints(team);
#endif

	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int cgmScoreGetRawTeamScoreForSource(int team, enum CgmScoreStatSource source)
{
	GameData *gameData = gameGetData();

	switch (source)
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

#ifdef FORGE_CGM_ROUNDS
	case CGM_SCORE_STAT_ROUNDS_COMPLETED:
		return cgmRoundsGetRoundsCompleted();
	case CGM_SCORE_STAT_ROUND_POINTS:
		return cgmRoundsGetRoundPoints(team);
#endif

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
			score += cgmScoreGetRawPlayerStat(i, source);
	}

	return score;
}

//--------------------------------------------------------------------------
int cgmScoreStatSourceIsTeamStat(enum CgmScoreStatSource source)
{
	switch (source)
	{
	case CGM_SCORE_STAT_POINTS:
	case CGM_SCORE_STAT_CAPS:
	case CGM_SCORE_STAT_TEAM_1:
	case CGM_SCORE_STAT_TEAM_2:
	case CGM_SCORE_STAT_TEAM_3:
	case CGM_SCORE_STAT_TEAM_4:
		return 1;
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int cgmScoreStatSourceIsCustomTeamStat(enum CgmScoreStatSource source)
{
	switch (source)
	{
	case CGM_SCORE_STAT_TEAM_1:
	case CGM_SCORE_STAT_TEAM_2:
	case CGM_SCORE_STAT_TEAM_3:
	case CGM_SCORE_STAT_TEAM_4:
		return 1;
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int cgmScoreGetCustomPlayerStatSlot(enum CgmScoreStatSource source)
{
	switch (source)
	{
	case CGM_SCORE_STAT_PLAYER_1:
		return 0;
	case CGM_SCORE_STAT_PLAYER_2:
		return 1;
	case CGM_SCORE_STAT_PLAYER_3:
		return 2;
	case CGM_SCORE_STAT_PLAYER_4:
		return 3;
	default:
		return -1;
	}
}

//--------------------------------------------------------------------------
int cgmScoreGetCustomTeamStatSlot(enum CgmScoreStatSource source)
{
	switch (source)
	{
	case CGM_SCORE_STAT_TEAM_1:
		return 0;
	case CGM_SCORE_STAT_TEAM_2:
		return 1;
	case CGM_SCORE_STAT_TEAM_3:
		return 2;
	case CGM_SCORE_STAT_TEAM_4:
		return 3;
	default:
		return -1;
	}
}

//--------------------------------------------------------------------------
int cgmScoreStatSourceCanUseRoundTrackedValue(enum CgmScoreStatSource source)
{
#ifndef FORGE_CGM_ROUNDS
	return 0;
#else
	return source >= 0 && source < CGM_SCORE_STAT_ROUNDS_COMPLETED;
#endif
}

//--------------------------------------------------------------------------
int cgmScoreGetFirstStatIndexForSource(enum CgmScoreStatSource source)
{
	int statIndex;
	for (statIndex = 0; statIndex < cgmScoreStatsCount && statIndex < MAX_CUSTOM_STATS; ++statIndex)
	{
		if (cgmScoreStats[statIndex].Source == source)
			return statIndex;
	}

	return -1;
}

//--------------------------------------------------------------------------
int cgmScoreGetFirstStatIndexForStat(int statIndex)
{
	if (statIndex < 0 || statIndex >= cgmScoreStatsCount || statIndex >= MAX_CUSTOM_STATS)
		return -1;

	return cgmScoreGetFirstStatIndexForSource(cgmScoreStats[statIndex].Source);
}

//--------------------------------------------------------------------------
int cgmScoreGetRoundTrackedStatIndex(enum CgmScoreStatSource source)
{
#ifndef FORGE_CGM_ROUNDS
	return -1;
#else
	if (!cgmScoreStatSourceCanUseRoundTrackedValue(source))
		return -1;

	return cgmScoreGetFirstStatIndexForSource(source);
#endif
}

//--------------------------------------------------------------------------
int cgmScoreGetPlayerStat(int playerIdx, enum CgmScoreStatSource source)
{
	int statIndex = cgmScoreGetRoundTrackedStatIndex(source);
	if (statIndex >= 0)
	{
		GameSettings *gameSettings = gameGetSettings();
		Player *player = playerGetFromIndex(playerIdx);
		int team = player ? playerGetJuggSafeTeam(player) : gameSettings->PlayerTeams[playerIdx];
		if (cgmScoreStatSourceIsCustomTeamStat(source))
			return cgmScoreState.RoundTeamStats[statIndex][team];

		return cgmScoreState.RoundPlayerStats[statIndex][playerIdx];
	}

	return cgmScoreGetRawPlayerStat(playerIdx, source);
}

//--------------------------------------------------------------------------
int cgmScoreGetTeamScoreForSource(int team, enum CgmScoreStatSource source)
{
	int statIndex = cgmScoreGetRoundTrackedStatIndex(source);
	if (statIndex >= 0)
	{
		if (cgmScoreStatSourceIsTeamStat(source))
			return cgmScoreState.RoundTeamStats[statIndex][team];

		int score = 0;
		int i;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			Player *player = playerGetFromIndex(i);
			if (!player)
				continue;

			if (playerGetJuggSafeTeam(player) == team)
				score += cgmScoreState.RoundPlayerStats[statIndex][i];
		}

		return score;
	}

	return cgmScoreGetRawTeamScoreForSource(team, source);
}

//--------------------------------------------------------------------------
int cgmScoreGetPlayerStatSortValue(int playerIdx, enum CgmScoreStatSource source)
{
	if (source == CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS)
	{
		int statIndex = cgmScoreGetRoundTrackedStatIndex(source);
		if (statIndex >= 0 && cgmScoreState.ManualPlayerTimeAliveFinalized[playerIdx])
			return cgmScoreState.RoundPlayerStats[statIndex][playerIdx];

		int roundMs = cgmScoreState.ManualPlayerTimeAliveMs[playerIdx] - cgmScoreState.ManualPlayerTimeAliveRoundStartMs[playerIdx];
		return roundMs > 0 ? roundMs : 0;
	}

	return cgmScoreGetPlayerStat(playerIdx, source);
}

//--------------------------------------------------------------------------
int cgmScoreGetTeamScoreSortValueForSource(int team, enum CgmScoreStatSource source)
{
	if (source == CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS)
	{
		int score = 0;
		int i;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			Player *player = playerGetFromIndex(i);
			if (!playerIsValid(player))
				continue;

			if (playerGetJuggSafeTeam(player) == team)
				score += cgmScoreGetPlayerStatSortValue(i, source);
		}

		return score;
	}

	return cgmScoreGetTeamScoreForSource(team, source);
}

//--------------------------------------------------------------------------
int cgmScoreGetStatHasRoundAggregate(int statIndex)
{
#ifndef FORGE_CGM_ROUNDS
	return 0;
#else
	int firstStatIndex = cgmScoreGetFirstStatIndexForStat(statIndex);
	return firstStatIndex >= 0 &&
				 cgmScoreStats[firstStatIndex].RoundAggregateType != CGM_SCORE_ROUND_AGGREGATE_NONE;
#endif
}

//--------------------------------------------------------------------------
int cgmScoreGetRoundAggregateCount(void)
{
#ifdef FORGE_CGM_ROUNDS
	int roundsCompleted = cgmRoundsGetRoundsCompleted();
	return cgmScoreState.RoundAggregateCount > roundsCompleted ? cgmScoreState.RoundAggregateCount : roundsCompleted;
#else
	return cgmScoreState.RoundAggregateCount;
#endif
}

//--------------------------------------------------------------------------
int cgmScoreGetRoundAggregateValue(int row, int statIndex, int playerAggregate)
{
	if (!cgmScoreGetStatHasRoundAggregate(statIndex) || row < 0 || row >= GAME_MAX_PLAYERS)
		return 0;

	int firstStatIndex = cgmScoreGetFirstStatIndexForStat(statIndex);
	if (cgmScoreStats[firstStatIndex].RoundAggregateType == CGM_SCORE_ROUND_AGGREGATE_AVERAGE)
	{
		int aggregateCount = cgmScoreGetRoundAggregateCount();
		if (aggregateCount <= 0)
			return 0;

		return (playerAggregate ? cgmScoreState.RoundPlayerAggregateValues[firstStatIndex][row] : cgmScoreState.RoundAggregateValues[firstStatIndex][row]) / aggregateCount;
	}

	return playerAggregate ? cgmScoreState.RoundPlayerAggregateValues[firstStatIndex][row] : cgmScoreState.RoundAggregateValues[firstStatIndex][row];
}

//--------------------------------------------------------------------------
int cgmScoreHasActiveRoundForLiveAggregate(void)
{
#ifdef FORGE_CGM_ROUNDS
	return cgmRoundsState.RoundStarted && !cgmRoundsState.InPostRoundGrace;
#else
	return 0;
#endif
}

//--------------------------------------------------------------------------
int cgmScoreCombineLiveRoundAggregate(int statIndex, int committedValue, int committedCount, int currentValue)
{
	switch (cgmScoreStats[statIndex].RoundAggregateType)
	{
	case CGM_SCORE_ROUND_AGGREGATE_SUM:
		return committedValue + currentValue;
	case CGM_SCORE_ROUND_AGGREGATE_AVERAGE:
		return ((committedValue * committedCount) + currentValue) / (committedCount + 1);
	case CGM_SCORE_ROUND_AGGREGATE_MAX:
		return committedCount <= 0 || currentValue > committedValue ? currentValue : committedValue;
	case CGM_SCORE_ROUND_AGGREGATE_MIN:
		return committedCount <= 0 || currentValue < committedValue ? currentValue : committedValue;
	case CGM_SCORE_ROUND_AGGREGATE_LATEST:
		return currentValue;
	case CGM_SCORE_ROUND_AGGREGATE_NONE:
	default:
		return committedValue;
	}
}

//--------------------------------------------------------------------------
int cgmScoreGetLiveRoundAggregateValue(int row, int statIndex, int playerAggregate, int teamsEnabled)
{
	if (statIndex < 0 || statIndex >= cgmScoreStatsCount || row < 0 || row >= GAME_MAX_PLAYERS)
		return 0;

	int firstStatIndex = cgmScoreGetFirstStatIndexForStat(statIndex);
	if (firstStatIndex < 0 || !cgmScoreGetStatHasRoundAggregate(firstStatIndex))
		return 0;

	int usePlayerRow = playerAggregate || !teamsEnabled;
	if (!cgmScoreHasActiveRoundForLiveAggregate())
	{
		if (usePlayerRow)
			return cgmScoreGetStatValueForPlayer(row, firstStatIndex, 1);

		return cgmScoreGetStatValueForTeam(row, firstStatIndex, 1);
	}

	cgmScoreUpdateRoundTrackedStats();

	int committedCount = cgmScoreGetRoundAggregateCount();
	int committedValue = usePlayerRow ? cgmScoreGetStatValueForPlayer(row, firstStatIndex, 1) : cgmScoreGetStatValueForTeam(row, firstStatIndex, 1);
	int currentValue = usePlayerRow ? cgmScoreGetStatValueForPlayer(row, firstStatIndex, 0) : cgmScoreGetStatValueForTeam(row, firstStatIndex, 0);
	return cgmScoreCombineLiveRoundAggregate(firstStatIndex, committedValue, committedCount, currentValue);
}

//--------------------------------------------------------------------------
int cgmScoreGetStatValueForPlayer(int playerIdx, int statIndex, int useRoundAggregate)
{
	cgmScoreUpdateManualStatSources();

	if (statIndex < 0 || statIndex >= cgmScoreStatsCount)
		return 0;

	if (useRoundAggregate && cgmScoreGetStatHasRoundAggregate(statIndex))
		return cgmScoreGetRoundAggregateValue(playerIdx, statIndex, 1);

	return cgmScoreGetPlayerStat(playerIdx, cgmScoreStats[statIndex].Source);
}

//--------------------------------------------------------------------------
int cgmScoreGetStatValueForTeam(int team, int statIndex, int useRoundAggregate)
{
	cgmScoreUpdateManualStatSources();

	if (statIndex < 0 || statIndex >= cgmScoreStatsCount)
		return 0;

	if (useRoundAggregate && cgmScoreGetStatHasRoundAggregate(statIndex))
		return cgmScoreGetRoundAggregateValue(team, statIndex, 0);

	return cgmScoreGetTeamScoreForSource(team, cgmScoreStats[statIndex].Source);
}

//--------------------------------------------------------------------------
int cgmScoreGetLiveStatValueForTeam(int team, int statIndex, int teamsEnabled)
{
	if (statIndex < 0 || statIndex >= cgmScoreStatsCount)
		return 0;

	if (cgmScoreGetStatHasRoundAggregate(statIndex))
	{
		GameData *gameData = gameGetData();
		if (gameData->GameIsOver)
		{
			if (!teamsEnabled)
				return cgmScoreGetStatValueForPlayer(team, statIndex, 1);

			return cgmScoreGetStatValueForTeam(team, statIndex, 1);
		}

		if (cgmScoreHasActiveRoundForLiveAggregate())
			return cgmScoreGetLiveRoundAggregateValue(team, statIndex, 0, teamsEnabled);

		if (!teamsEnabled)
			return cgmScoreGetStatValueForPlayer(team, statIndex, 1);

		return cgmScoreGetStatValueForTeam(team, statIndex, 1);
	}

	return cgmScoreGetStatValueForTeam(team, statIndex, 0);
}

//--------------------------------------------------------------------------
int cgmScoreGetTeamScore(int team)
{
	return cgmScoreGetTargetTeamScore(team);
}

//--------------------------------------------------------------------------
int cgmScoreGetTargetTeamScore(int team)
{
	if (cgmScoreTarget.StatIndex >= 0 && cgmScoreTarget.StatIndex < cgmScoreStatsCount)
		return cgmScoreGetStatValueForTeam(team, cgmScoreTarget.StatIndex, 1);

	return 0;
}

//--------------------------------------------------------------------------
enum CgmScoreStatValueType cgmScoreGetTargetValueType(void)
{
	if (cgmScoreTarget.StatIndex >= 0 && cgmScoreTarget.StatIndex < cgmScoreStatsCount)
		return cgmScoreStats[cgmScoreTarget.StatIndex].ValueType;

	return CGM_SCORE_STAT_TYPE_INT;
}

//--------------------------------------------------------------------------
int cgmScoreGetRoundAggregateCurrentValue(int row, int statIndex, int teamsEnabled)
{
	if (teamsEnabled)
		return cgmScoreGetTeamScoreForSource(row, cgmScoreStats[statIndex].Source);

	return cgmScoreGetPlayerStat(row, cgmScoreStats[statIndex].Source);
}

//--------------------------------------------------------------------------
void cgmScoreFinalizeRoundTimeAliveStats(int roundStartTime, int roundEndTime)
{
#ifdef FORGE_CGM_ROUNDS
	cgmScoreUpdateRoundTrackedStats();

	int statIndex = cgmScoreGetRoundTrackedStatIndex(CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS);
	if (statIndex < 0)
		return;

	int fullRoundMs = roundEndTime - roundStartTime;
	if (fullRoundMs < 0)
		fullRoundMs = 0;

	int row;
	for (row = 0; row < GAME_MAX_PLAYERS; ++row)
	{
		int roundMs = cgmScoreState.ManualPlayerTimeAliveMs[row] - cgmScoreState.ManualPlayerTimeAliveRoundStartMs[row];
		if (roundMs < 0)
			roundMs = 0;

		Player *player = playerGetFromIndex(row);
		if (cgmScoreIsPlayerAliveForManualStats(player))
			roundMs = fullRoundMs;

		cgmScoreState.RoundPlayerStats[statIndex][row] = roundMs;
		cgmScoreState.LastPlayerStats[statIndex][row] = cgmScoreGetRawPlayerStat(row, CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS);
		cgmScoreState.ManualPlayerTimeAliveFinalized[row] = 1;
	}
#endif
}

//--------------------------------------------------------------------------
void cgmScoreUpdateRoundTrackedStats(void)
{
#ifdef FORGE_CGM_ROUNDS
	cgmScoreUpdateManualStatSources();

	int statIndex;
	for (statIndex = 0; statIndex < cgmScoreStatsCount && statIndex < MAX_CUSTOM_STATS; ++statIndex)
	{
		enum CgmScoreStatSource source = cgmScoreStats[statIndex].Source;
		if (statIndex != cgmScoreGetFirstStatIndexForSource(source) || !cgmScoreStatSourceCanUseRoundTrackedValue(source))
			continue;

		int row;
		for (row = 0; row < GAME_MAX_PLAYERS; ++row)
		{
			if (!cgmScoreStatSourceIsCustomTeamStat(source))
			{
				int currentValue = cgmScoreGetRawPlayerStat(row, source);
				if (source == CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS && cgmScoreState.ManualPlayerTimeAliveFinalized[row])
				{
					cgmScoreState.LastPlayerStats[statIndex][row] = currentValue;
					continue;
				}

				int delta = currentValue - cgmScoreState.LastPlayerStats[statIndex][row];
				if (delta)
					cgmScoreState.RoundPlayerStats[statIndex][row] += delta;

				cgmScoreState.LastPlayerStats[statIndex][row] = currentValue;
			}

			if (cgmScoreStatSourceIsTeamStat(source))
			{
				int currentValue = cgmScoreGetRawTeamScoreForSource(row, source);
				int delta = currentValue - cgmScoreState.LastTeamStats[statIndex][row];
				if (delta)
					cgmScoreState.RoundTeamStats[statIndex][row] += delta;

				cgmScoreState.LastTeamStats[statIndex][row] = currentValue;
			}
		}
	}
#endif
}

//--------------------------------------------------------------------------
void cgmScoreResetRawCustomStatsForRoundAggregate(enum CgmScoreStatSource source)
{
#ifdef FORGE_CGM_ROUNDS
	int customPlayerStatSlot = cgmScoreGetCustomPlayerStatSlot(source);
	int customTeamStatSlot = cgmScoreGetCustomTeamStatSlot(source);
	int row;

	if (customPlayerStatSlot >= 0)
	{
		for (row = 0; row < GAME_MAX_PLAYERS; ++row)
			cgmScoreSetCustomPlayerIntStat(row, customPlayerStatSlot, 0, 1);
	}

	if (customTeamStatSlot >= 0)
	{
		for (row = 0; row < GAME_MAX_PLAYERS; ++row)
			cgmScoreSetCustomTeamIntStat(row, customTeamStatSlot, 0, 1);
	}
#endif
}

//--------------------------------------------------------------------------
void cgmScoreResetRoundStats(void)
{
#ifdef FORGE_CGM_ROUNDS
	int row;
	for (row = 0; row < GAME_MAX_PLAYERS; ++row)
	{
		cgmScoreState.ManualPlayerTimeAliveRoundStartMs[row] = cgmScoreState.ManualPlayerTimeAliveMs[row];
		cgmScoreState.ManualPlayerTimeAliveFinalized[row] = 0;
	}

	int statIndex;
	for (statIndex = 0; statIndex < cgmScoreStatsCount && statIndex < MAX_CUSTOM_STATS; ++statIndex)
	{
		enum CgmScoreStatSource source = cgmScoreStats[statIndex].Source;
		if (statIndex != cgmScoreGetFirstStatIndexForSource(source) || !cgmScoreStatSourceCanUseRoundTrackedValue(source) || !cgmScoreGetStatHasRoundAggregate(statIndex))
			continue;

		cgmScoreResetRawCustomStatsForRoundAggregate(source);

		if (cgmScoreStatSourceIsTeamStat(source))
		{
			for (row = 0; row < GAME_MAX_PLAYERS; ++row)
			{
				cgmScoreState.RoundTeamStats[statIndex][row] = 0;
				cgmScoreState.LastTeamStats[statIndex][row] = cgmScoreGetRawTeamScoreForSource(row, source);
			}
		}

		if (!cgmScoreStatSourceIsCustomTeamStat(source))
		{
			for (row = 0; row < GAME_MAX_PLAYERS; ++row)
			{
				cgmScoreState.RoundPlayerStats[statIndex][row] = 0;
				cgmScoreState.LastPlayerStats[statIndex][row] = cgmScoreGetRawPlayerStat(row, source);
			}
		}
	}
#endif
}

//--------------------------------------------------------------------------
void cgmScoreCommitRoundAggregateValue(int statIndex, int row, int roundValue, int playerAggregate)
{
	switch (cgmScoreStats[statIndex].RoundAggregateType)
	{
	case CGM_SCORE_ROUND_AGGREGATE_SUM:
	case CGM_SCORE_ROUND_AGGREGATE_AVERAGE:
		if (playerAggregate)
			cgmScoreState.RoundPlayerAggregateValues[statIndex][row] += roundValue;
		else
			cgmScoreState.RoundAggregateValues[statIndex][row] += roundValue;
		break;
	case CGM_SCORE_ROUND_AGGREGATE_MAX:
		if (playerAggregate && (cgmScoreState.RoundAggregateCount <= 0 || roundValue > cgmScoreState.RoundPlayerAggregateValues[statIndex][row]))
			cgmScoreState.RoundPlayerAggregateValues[statIndex][row] = roundValue;
		else if (!playerAggregate && (cgmScoreState.RoundAggregateCount <= 0 || roundValue > cgmScoreState.RoundAggregateValues[statIndex][row]))
			cgmScoreState.RoundAggregateValues[statIndex][row] = roundValue;
		break;
	case CGM_SCORE_ROUND_AGGREGATE_MIN:
		if (playerAggregate && (cgmScoreState.RoundAggregateCount <= 0 || roundValue < cgmScoreState.RoundPlayerAggregateValues[statIndex][row]))
			cgmScoreState.RoundPlayerAggregateValues[statIndex][row] = roundValue;
		else if (!playerAggregate && (cgmScoreState.RoundAggregateCount <= 0 || roundValue < cgmScoreState.RoundAggregateValues[statIndex][row]))
			cgmScoreState.RoundAggregateValues[statIndex][row] = roundValue;
		break;
	case CGM_SCORE_ROUND_AGGREGATE_LATEST:
		if (playerAggregate)
			cgmScoreState.RoundPlayerAggregateValues[statIndex][row] = roundValue;
		else
			cgmScoreState.RoundAggregateValues[statIndex][row] = roundValue;
		break;
	case CGM_SCORE_ROUND_AGGREGATE_NONE:
	default:
		return;
	}
}

//--------------------------------------------------------------------------
void cgmScoreCommitRoundAggregatesToCount(int targetAggregateCount)
{
#ifdef FORGE_CGM_ROUNDS
	if (targetAggregateCount <= 0 || cgmScoreState.RoundAggregateCount >= targetAggregateCount)
		return;

	cgmScoreUpdateRoundTrackedStats();

	GameOptions *gameOptions = gameGetOptions();
	int teamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	int statIndex;
	for (statIndex = 0; statIndex < cgmScoreStatsCount && statIndex < MAX_CUSTOM_STATS; ++statIndex)
	{
		if (statIndex != cgmScoreGetFirstStatIndexForStat(statIndex) || !cgmScoreGetStatHasRoundAggregate(statIndex))
			continue;

		if (gameAmIHost())
		{
			int row;
			for (row = 0; row < GAME_MAX_PLAYERS; ++row)
			{
				int playerRoundValue = cgmScoreGetPlayerStat(row, cgmScoreStats[statIndex].Source);
				cgmScoreCommitRoundAggregateValue(statIndex, row, playerRoundValue, 1);
			}
		}
		else
		{
			int localIdx;
			for (localIdx = 0; localIdx < GAME_MAX_LOCALS; ++localIdx)
			{
				Player *player = playerGetFromSlot(localIdx);
				if (!playerIsValid(player))
					continue;

				int playerRoundValue = cgmScoreGetPlayerStat(player->PlayerId, cgmScoreStats[statIndex].Source);
				cgmScoreCommitRoundAggregateValue(statIndex, player->PlayerId, playerRoundValue, 1);
			}
		}

		if (!gameAmIHost())
			continue;

		int row;
		for (row = 0; row < GAME_MAX_PLAYERS; ++row)
		{
			int teamRoundValue = cgmScoreGetRoundAggregateCurrentValue(row, statIndex, teamsEnabled);
			cgmScoreCommitRoundAggregateValue(statIndex, row, teamRoundValue, 0);
		}
	}

	cgmScoreState.RoundAggregateCount = targetAggregateCount;
#endif
}

//--------------------------------------------------------------------------
void cgmScoreCommitRoundAggregates(void)
{
#ifndef FORGE_CGM_ROUNDS
	cgmScoreUpdateRoundTrackedStats();

	GameOptions *gameOptions = gameGetOptions();
	int teamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	int statIndex;
	for (statIndex = 0; statIndex < cgmScoreStatsCount && statIndex < MAX_CUSTOM_STATS; ++statIndex)
	{
		if (statIndex != cgmScoreGetFirstStatIndexForStat(statIndex) || !cgmScoreGetStatHasRoundAggregate(statIndex))
			continue;

		int row;
		for (row = 0; row < GAME_MAX_PLAYERS; ++row)
		{
			int playerRoundValue = cgmScoreGetPlayerStat(row, cgmScoreStats[statIndex].Source);
			int teamRoundValue = cgmScoreGetRoundAggregateCurrentValue(row, statIndex, teamsEnabled);
			cgmScoreCommitRoundAggregateValue(statIndex, row, playerRoundValue, 1);
			cgmScoreCommitRoundAggregateValue(statIndex, row, teamRoundValue, 0);
		}
	}

	cgmScoreState.RoundAggregateCount += 1;
#else
	cgmScoreCommitRoundAggregatesToCount(cgmRoundsGetRoundsCompleted());
#endif
}

//--------------------------------------------------------------------------
void cgmScoreFinalizeActiveRoundAggregatesForGameEnd(void)
{
#ifdef FORGE_CGM_ROUNDS
	if (!cgmRoundsState.RoundStarted || cgmRoundsState.InPostRoundGrace)
		return;

	int targetAggregateCount = cgmRoundsGetRoundsCompleted() + 1;
	if (cgmScoreState.RoundAggregateCount >= targetAggregateCount)
		return;

	cgmScoreFinalizeRoundTimeAliveStats(cgmRoundsState.RoundStartTime, gameGetTime());
	cgmScoreCommitRoundAggregatesToCount(targetAggregateCount);
	cgmScoreBroadcastLocalPlayerRoundAggregates();
#endif
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
	case CGM_SCORE_STAT_TYPE_TIME_MILLISECONDS:
		return score / TIME_SECOND;
	case CGM_SCORE_STAT_TYPE_FLOAT:
		return score / SCORE_FLOAT_PRECISION;
	}

	return score;
}

//--------------------------------------------------------------------------
int cgmScoreGetFormattedTeamScore(int team)
{
	GameData *gameData = gameGetData();
	int rawValue = cgmScoreGetTargetTeamScore(team);
	return cgmScoreGetFormattedScore(rawValue, cgmScoreGetTargetValueType());
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
	gameScoreboardSetScoreMax(cgmScoreGetFormattedScore(cgmScoreGetTargetScore(), cgmScoreGetTargetValueType()));
}

//--------------------------------------------------------------------------
int cgmScoreSetCustomTarget(int target)
{
	if (target == cgmScoreTarget.CustomTarget)
		return cgmScoreTarget.CustomTarget;

	cgmScoreTarget.CustomTarget = target;
	cgmScoreSetScoreboardMaxOverride(0);
	return cgmScoreTarget.CustomTarget;
}

//--------------------------------------------------------------------------
void cgmScoreSendSerializedStatsUpstreamToMode(void)
{
	struct CgmStats stats;
	GameData *gameData = gameGetData();

	// no func ptr
	if (!MapConfig.ModeFunctions.UpdateStats)
		return;

	cgmScoreUpdateManualStatSources();

	memset(&stats, 0, sizeof(stats));

	stats.WinningTeam = gameData->WinningPlayer >= 0 ? gameData->WinningPlayer : gameData->WinningTeam;

	// fill team scores
	int i;
	stats.TeamScoreType = cgmScoreGetTargetValueType();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		stats.TeamScores[i] = cgmScoreGetTargetTeamScore(i);

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
			stats.PlayerStatValues[p][i] = cgmScoreGetStatValueForPlayer(p, s, 1);

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

		int teamScore = cgmScoreGetTargetTeamScore(i);
		if (winningIndex < 0 || (!cgmScoreTarget.SortAscending && teamScore > winningScore) || (cgmScoreTarget.SortAscending && teamScore < winningScore))
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
		cgmScoreFinalizeActiveRoundAggregatesForGameEnd();
		cgmScoreSendSerializedStatsUpstreamToMode();
	}

	// return whether we have a winner
	return hasWinner;
}

//--------------------------------------------------------------------------
void cgmScoreGameEndedUpdateWinnerOverride(void)
{
	GameData *gameData = gameGetData();
	cgmScoreFinalizeActiveRoundAggregatesForGameEnd();

	// reassert the provided game winning team
	if (gameData->WinningPlayer >= 0)
		gameSetWinner(gameData->WinningPlayer, 0);
	else
		gameSetWinner(gameData->WinningTeam, 1);

	// if game reached time limit, determine who won and change the GameEndReason accordingly
	if (gameData->GameEndReason <= GAME_END_REASON_TIME_LIMIT_REACHED)
	{
		gameData->GameEndReason = cgmScoreGetGameEndReasonScoreReached();

		// only host can update who won as the game is ending abruptly
		if (gameAmIHost())
			cgmScoreUpdateCurrentWinner(1);
	}
}

//--------------------------------------------------------------------------
int cgmScoreBroadcastGameEndedOverride(int transport, void *connection, int toClientIndex, int msgId, int payloadSize, void *payload)
{
	// trigger winner override early so we can broadcast the result
	cgmScoreGameEndedUpdateWinnerOverride();

	// patch winner
	GameData *gameData = gameGetData();
	POKE_U32(payload + 0x2C, gameData->WinningTeam);
	POKE_U32(payload + 0x30, gameData->WinningPlayer);

	// send
	return netBroadcastMediusAppMessage(transport, connection, msgId, payloadSize, payload);
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
		int score = cgmScoreGetStatValueForPlayer(i, cgmScoreTarget.StatIndex, 1);
		playerPoints[i] = score;
		if (score > largestScore)
			largestScore = score;
	}

	// invert points to flip sort order
	if (cgmScoreTarget.SortAscending)
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

	// #ifdef FORGE_CGM_ROUNDS
	// 	if (cgmRoundsState.InPostRoundGrace)
	// 		return;
	// #endif

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

	cgmScoreUpdateRoundTrackedStats();

	gameState->CustomGameStatsSize = sizeof(struct CgmCustomGameStats);
	struct CgmCustomGameStats *sGameData = (struct CgmCustomGameStats *)gameState->CustomGameStats->Payload;
	sGameData->Version = 0x00000001;
	sGameData->TeamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	sGameData->OrderScoreByAscending = cgmScoreTarget.SortAscending;

	// set team scores
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		if (cgmScoreTarget.StatIndex >= 0 && cgmScoreTarget.StatIndex < cgmScoreStatsCount && cgmScoreGetStatHasRoundAggregate(cgmScoreTarget.StatIndex))
			sGameData->TeamScores[i] = cgmScoreGetLiveRoundAggregateValue(i, cgmScoreTarget.StatIndex, 0, sGameData->TeamsEnabled);
		else
			sGameData->TeamScores[i] = cgmScoreGetTargetTeamScore(i);
	}

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
		{
			if (cgmScoreGetStatHasRoundAggregate(s))
				sGameData->TrackedStatValues[i][p] = cgmScoreGetLiveRoundAggregateValue(p, s, 1, sGameData->TeamsEnabled);
			else
				sGameData->TrackedStatValues[i][p] = cgmScoreGetStatValueForPlayer(p, s, 1);
		}

		++s;
	}
}

//--------------------------------------------------------------------------
void cgmScoreCleanup(void)
{
	netUninstallCustomMsgHandler(CGM_MSG_ID_SEND_CUSTOM_PLAYER_STAT, &cgmScoreOnRecvCustomPlayerStats);
	netUninstallCustomMsgHandler(CGM_MSG_ID_SEND_CUSTOM_TEAM_STAT, &cgmScoreOnRecvCustomTeamStats);
	netUninstallCustomMsgHandler(CGM_MSG_ID_SEND_PLAYER_ROUND_AGGREGATES, &cgmScoreOnRecvPlayerRoundAggregates);
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
	HOOK_JAL(0x00622E34, cgmScoreBroadcastGameEndedOverride);
	HOOK_JAL(0x00623434, cgmScoreGameEndedUpdateWinnerOverride);
	POKE_U32(0x0062343c, 0x100001fd); // b 0x00623c3c
	POKE_U32(0x00623440, 0x27C4D600); // addiu a0,fp,-0x2A00

	// invert hud scoreboard sort
	if (cgmScoreTarget.SortAscending)
	{
		POKE_U32(0x00542640, 0x15400044); // bne t2,zero,0x00542754
		POKE_U32(0x005426D0, 0x1040005D); // beq v0,zero,0x00542848
	}

	// hook net msgs
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_CUSTOM_PLAYER_STAT, &cgmScoreOnRecvCustomPlayerStats);
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_CUSTOM_TEAM_STAT, &cgmScoreOnRecvCustomTeamStats);
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_PLAYER_ROUND_AGGREGATES, &cgmScoreOnRecvPlayerRoundAggregates);

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

	cgmScorePrimeManualStatSources();

	init = 1;
}
