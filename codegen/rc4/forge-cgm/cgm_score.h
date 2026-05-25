#ifndef FORGE_CGM_SCORE_H
#define FORGE_CGM_SCORE_H

#include <libdl/game.h>
#include "cgm.h"

enum CgmScoreScoreboardType
{
	CGM_SCORE_BOARD_TYPE_NORMAL,
	CGM_SCORE_BOARD_TYPE_HIDDEN,
};

enum CgmScoreStatValueType
{
	CGM_SCORE_STAT_TYPE_INT,
	CGM_SCORE_STAT_TYPE_TIME_SECONDS,
	CGM_SCORE_STAT_TYPE_FLOAT,
	CGM_SCORE_STAT_TYPE_TIME_MILLISECONDS,
};

enum CgmScoreStatTrackerType
{
	CGM_SCORE_STAT_TRACK_ADD,
	CGM_SCORE_STAT_TRACK_MAX,
	CGM_SCORE_STAT_TRACK_MIN,
	CGM_SCORE_STAT_TRACK_SET,
};

enum CgmScoreStatTrackerSave
{
	CGM_SCORE_STAT_TRACK_SAVE_WITH_STATS,
	CGM_SCORE_STAT_TRACK_SAVE_WITH_RANK,
};

enum CgmScoreRoundAggregateType
{
	CGM_SCORE_ROUND_AGGREGATE_NONE,
	CGM_SCORE_ROUND_AGGREGATE_SUM,
	CGM_SCORE_ROUND_AGGREGATE_MAX,
	CGM_SCORE_ROUND_AGGREGATE_MIN,
	CGM_SCORE_ROUND_AGGREGATE_AVERAGE,
	CGM_SCORE_ROUND_AGGREGATE_LATEST,
};

enum CgmScoreStatTargetSource
{
	CGM_SCORE_TARGET_KILLS_TO_WIN,
	CGM_SCORE_TARGET_BOLTS_TO_WIN,
	CGM_SCORE_TARGET_HILL_TIME_TO_WIN,
	CGM_SCORE_TARGET_CAPS_TO_WIN,
	CGM_SCORE_TARGET_NODES_TO_WIN,
	CGM_SCORE_TARGET_CUSTOM,
};

enum CgmScoreStatSource
{
	CGM_SCORE_STAT_KILLS,
	CGM_SCORE_STAT_DEATHS,
	CGM_SCORE_STAT_SUICIDES,
	CGM_SCORE_STAT_KILLS_MINUS_SUICIDES,
	CGM_SCORE_STAT_HILL_TIME,
	CGM_SCORE_STAT_JUGGERNAUT_TIME,
	CGM_SCORE_STAT_CAPS,
	CGM_SCORE_STAT_SAVES,
	CGM_SCORE_STAT_NODES,
	CGM_SCORE_STAT_POINTS,
	CGM_SCORE_STAT_PLAYER_1,
	CGM_SCORE_STAT_PLAYER_2,
	CGM_SCORE_STAT_PLAYER_3,
	CGM_SCORE_STAT_PLAYER_4,
	CGM_SCORE_STAT_TEAM_1,
	CGM_SCORE_STAT_TEAM_2,
	CGM_SCORE_STAT_TEAM_3,
	CGM_SCORE_STAT_TEAM_4,
	CGM_SCORE_STAT_DISTANCE_TRAVELLED,
	CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS,
	CGM_SCORE_STAT_TIME_ALIVE_SECONDS = CGM_SCORE_STAT_TIME_ALIVE_MILLISECONDS,
	CGM_SCORE_STAT_ROUNDS_COMPLETED,
	CGM_SCORE_STAT_ROUND_POINTS,
	CGM_SCORE_STAT_SOURCE_COUNT,
};

struct CgmScoreTarget
{
	enum CgmScoreStatTargetSource Target;
	enum CgmScoreScoreboardType Scoreboard;
	int StatIndex;
	int SortAscending;
	int CustomTarget;
};

struct CgmScoreStat
{
	char Name[16];
	enum CgmScoreStatSource Source;
	enum CgmScoreStatValueType ValueType;
	enum CgmScoreStatTrackerType TrackerType;
	enum CgmScoreStatTrackerSave TrackerSave;
	char TrackerSlot;
	char DisplayOnEndGameScoreboard;
	char RoundAggregateType;
};

struct CgmScoreState
{
	// Custom player stats set by map/custom mode code. Indexed by custom stat slot, then player id.
	int CustomPlayerStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Custom team stats set by map/custom mode code. Indexed by custom stat slot, then team id.
	int CustomTeamStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Resettable, round-visible player stat mirror. Indexed by first configured custom stat slot for each source.
	int RoundPlayerStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Resettable, round-visible team stat mirror. Indexed by first configured custom stat slot for each source.
	int RoundTeamStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Last raw player stat sample used to compute deltas into RoundPlayerStats.
	int LastPlayerStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Last raw team stat sample used to compute deltas into RoundTeamStats.
	int LastTeamStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Built-in manual player stats. These are local tracking values and are not directly synced.
	int ManualPlayerDistanceTravelled[GAME_MAX_PLAYERS];
	int ManualPlayerTimeAliveMs[GAME_MAX_PLAYERS];
	int ManualPlayerTimeAliveRoundStartMs[GAME_MAX_PLAYERS];
	char ManualPlayerTimeAliveFinalized[GAME_MAX_PLAYERS];
	VECTOR ManualPlayerLastPosition[GAME_MAX_PLAYERS];
	char ManualPlayerHasLastPosition[GAME_MAX_PLAYERS];
	char ManualPlayerWasAlive[GAME_MAX_PLAYERS];
	int ManualPlayerLastDistanceSampleTime;
	int ManualPlayerLastUpdateTime;

	// Match-level aggregate values for individual players. Used by player scoreboard/stat export rows.
	int RoundPlayerAggregateValues[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Match-level aggregate values for teams. Used by final team scoring and team scoreboard rows.
	int RoundAggregateValues[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];

	// Local guard for the last round committed into aggregate tables.
	int RoundAggregateCount;

	// Optional explicit winner override for the score module's final game winner decision.
	char HasWinnerOverride;
	char WinnerOverride;
};

int cgmScoreGetCustomPlayerIntStat(int playerId, int statId);
int cgmScoreSetCustomPlayerIntStat(int playerId, int statId, int value, int force);
int cgmScoreIncCustomPlayerIntStat(int playerId, int statId, int amount, int force);

float cgmScoreGetCustomPlayerFloatStat(int playerId, int statId);
float cgmScoreSetCustomPlayerFloatStat(int playerId, int statId, float value, int force);
float cgmScoreIncCustomPlayerFloatStat(int playerId, int statId, float value, int force);

int cgmScoreGetCustomTeamIntStat(int teamId, int statId);
int cgmScoreSetCustomTeamIntStat(int teamId, int statId, int value, int force);
int cgmScoreIncCustomTeamIntStat(int teamId, int statId, int amount, int force);

float cgmScoreGetCustomTeamFloatStat(int teamId, int statId);
float cgmScoreSetCustomTeamFloatStat(int teamId, int statId, float value, int force);
float cgmScoreIncCustomTeamFloatStat(int teamId, int statId, float amount, int force);

void cgmScoreEndGameEarly(void);
void cgmScoreEndGameEarlyWithWinner(int winnerOverride);

int cgmScoreGetTargetScore(void);
// Sets cgmScoreTarget.CustomTarget and refreshes the scoreboard HUD max.
// This overrides the max set by the rounds module when rounds displays its target on the scoreboard HUD.
int cgmScoreSetCustomTarget(int target);
int cgmScoreGetTeamHasPlayer(int team);
int cgmScoreGetPlayerStat(int playerIdx, enum CgmScoreStatSource source);
int cgmScoreGetTeamScoreForSource(int team, enum CgmScoreStatSource source);
int cgmScoreGetPlayerStatSortValue(int playerIdx, enum CgmScoreStatSource source);
int cgmScoreGetTeamScoreSortValueForSource(int team, enum CgmScoreStatSource source);
int cgmScoreGetStatValueForPlayer(int playerIdx, int statIndex, int useRoundAggregate);
int cgmScoreGetStatValueForTeam(int team, int statIndex, int useRoundAggregate);
int cgmScoreGetLiveStatValueForTeam(int team, int statIndex, int teamsEnabled);
int cgmScoreGetTeamScore(int team);
int cgmScoreGetTargetTeamScore(int team);
int cgmScoreGetFormattedScore(int score, enum CgmScoreStatValueType type);
void cgmScoreResetRoundStats(void);
void cgmScorePrimeManualStatSources(void);
void cgmScoreUpdateManualStatSources(void);
void cgmScoreUpdateRoundTrackedStats(void);
void cgmScoreFinalizeRoundTimeAliveStats(int roundStartTime, int roundEndTime);
void cgmScoreCommitRoundAggregates(void);
void cgmScoreBroadcastLocalPlayerRoundAggregates(void);
void cgmScoreUpdateGameState(PatchStateContainer_t *gameState);
void cgmScoreCleanup(void);
void cgmScoreInit(void);
void cgmScoreCheckTargetScoreReached(void);
void cgmScoreCheckForBroadcastCustomStats(void);

extern struct CgmScoreTarget cgmScoreTarget;
extern struct CgmScoreStat cgmScoreStats[];
extern struct CgmScoreState cgmScoreState;
extern const int cgmScoreStatsCount;

#endif // FORGE_CGM_SCORE_H
