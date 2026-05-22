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
	CGM_SCORE_STAT_ROUNDS_COMPLETED,
	CGM_SCORE_STAT_ROUNDS_WON,
	CGM_SCORE_STAT_ROUNDS_LOST,
};

struct CgmScoreTarget
{
	enum CgmScoreStatSource Source;
	enum CgmScoreStatValueType ValueType;
	enum CgmScoreStatTargetSource Target;
	enum CgmScoreScoreboardType Scoreboard;
	int SortDescending;
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
};

struct CgmScoreState
{
	int CustomPlayerStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];
	int CustomTeamStats[MAX_CUSTOM_STATS][GAME_MAX_PLAYERS];
	char HasWinnerOverride;
	char WinnerOverride;
};

int cgmScoreGetCustomPlayerIntStat(int playerId, int statId);
int cgmScoreSetCustomPlayerIntStat(int playerId, int statId, int value);
int cgmScoreIncCustomPlayerIntStat(int playerId, int statId, int amount);

float cgmScoreGetCustomPlayerFloatStat(int playerId, int statId);
float cgmScoreSetCustomPlayerFloatStat(int playerId, int statId, float value);
float cgmScoreIncCustomPlayerFloatStat(int playerId, int statId, float value);

int cgmScoreGetCustomTeamIntStat(int teamId, int statId);
int cgmScoreSetCustomTeamIntStat(int teamId, int statId, int value);
int cgmScoreIncCustomTeamIntStat(int teamId, int statId, int amount);

float cgmScoreGetCustomTeamFloatStat(int teamId, int statId);
float cgmScoreSetCustomTeamFloatStat(int teamId, int statId, float value);
float cgmScoreIncCustomTeamFloatStat(int teamId, int statId, float amount);

void cgmScoreEndGameEarly(void);
void cgmScoreEndGameEarlyWithWinner(int winnerOverride);

int cgmScoreGetTargetScore(void);
int cgmScoreGetTeamHasPlayer(int team);
int cgmScoreGetPlayerStat(int playerIdx, enum CgmScoreStatSource source);
int cgmScoreGetTeamScoreForSource(int team, enum CgmScoreStatSource source);
int cgmScoreGetTeamScore(int team);
int cgmScoreGetFormattedScore(int score, enum CgmScoreStatValueType type);
void cgmScoreUpdateGameState(PatchStateContainer_t *gameState);
void cgmScoreInit(void);
void cgmScoreCheckTargetScoreReached(void);
void cgmScoreCheckForBroadcastCustomStats(void);

extern struct CgmScoreTarget cgmScoreTarget;
extern struct CgmScoreStat cgmScoreStats[];
extern const int cgmScoreStatsCount;

#endif // FORGE_CGM_SCORE_H
