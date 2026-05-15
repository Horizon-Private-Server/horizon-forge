#ifndef FORGE_CGM_H
#define FORGE_CGM_H

#include <tamtypes.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "common.h"
#include "messageid.h"

#define MAP_CONFIG_MAGIC (0xDEADBEEF)
#define MAX_SCOREBOARD_STATS (4)
#define MAX_TRACKED_STATS (8)
#define MAX_CUSTOM_STATS (16)
#define SCORE_FLOAT_PRECISION (1024.0f)

enum CgmMessageIds
{
	CGM_MSG_ID_SEND_GAME_STATS = CUSTOM_MSG_ID_GAME_MODE_START,
	CGM_MSG_ID_SEND_CUSTOM_PLAYER_STAT,
	CGM_MSG_ID_SEND_CUSTOM_TEAM_STAT,
};

struct CgmStats;

typedef void (*Stub_func)(void);
typedef void (*Tick_func)(PatchStateContainer_t *gameState);
typedef void (*UpdateGameState_func)(PatchStateContainer_t *gameState);
typedef void (*UpdateStats_func)(struct CgmStats *stats);

struct CgmMapFunctions
{
	Tick_func TickFrame;
	Tick_func TickGame;
	UpdateGameState_func UpdateGameState;
};

struct CgmModeFunctions
{
	UpdateStats_func UpdateStats;
};

struct CgmMapConfig
{
	u32 Magic;

	// 64 mode functions
	union
	{
		struct CgmModeFunctions ModeFunctions;
		Stub_func _padding[64];
	};

	// 64 map functions
	union
	{
		struct CgmMapFunctions MapFunctions;
		Stub_func _padding[64];
	};
};

struct CgmStats
{
	int WinningTeam;
	int TeamScores[GAME_MAX_PLAYERS];
	char PlayerStatNames[MAX_SCOREBOARD_STATS][16];
	int PlayerStatValues[GAME_MAX_PLAYERS][MAX_SCOREBOARD_STATS];
	char PlayerStatTypes[MAX_SCOREBOARD_STATS];
	char TeamScoreType;
	char TeamPlacements[GAME_MAX_PLAYERS];
};

struct CgmTrackedStat
{
	char Name[16];
	char ValueType;
	char TrackerType;
	char TrackerSlot;
	char TrackerSave;
};

struct CgmCustomGameStats
{
	int Version;
	int RuntimeMs;
	char Name[32];
	char SharedRankCode[32];
	char MinTeamsForRank;
	char MinTeamsForStats;
	char PADDING[2];
	int TeamScores[GAME_MAX_PLAYERS];
	char PlayerTeams[GAME_MAX_PLAYERS];
	char TeamsEnabled;
	char OrderScoreByAscending; // if non-zero, indicates a lower score is better
	struct CgmTrackedStat TrackedStats[MAX_TRACKED_STATS];
	int TrackedStatValues[MAX_TRACKED_STATS][GAME_MAX_PLAYERS];
};

extern struct CgmMapConfig MapConfig;

int cgmGetParameter(int paramIdx);

#endif // FORGE_CGM_H
