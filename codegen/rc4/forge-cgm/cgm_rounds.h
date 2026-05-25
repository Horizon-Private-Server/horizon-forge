#ifndef FORGE_CGM_ROUNDS_H
#define FORGE_CGM_ROUNDS_H

#ifndef FORGE_CGM_SCORE
#error "FORGE_CGM_ROUNDS requires FORGE_CGM_SCORE"
#endif

#include <libdl/game.h>
#include <libdl/time.h>
#include "cgm.h"
#include "cgm_score.h"

#define CGM_ROUNDS_POST_ROUND_GRACE_MS (5 * TIME_SECOND)
#define CGM_ROUNDS_COMPLETE_DELAY_MS (TIME_SECOND / 4)

enum CgmRoundsResetFlags
{
	CGM_ROUNDS_RESET_NONE = 0,
	CGM_ROUNDS_RESET_RESPAWN_PLAYERS = 1 << 4,
	CGM_ROUNDS_RESET_REFILL_HEALTH = 1 << 5,
	CGM_ROUNDS_RESET_RETURN_FLAGS = 1 << 6,
	CGM_ROUNDS_RESET_NODES = 1 << 7,
	CGM_ROUNDS_RESET_DESTROY_PLAYER_OBJECTS = 1 << 8,
};

enum CgmRoundsCompleteFlags
{
	CGM_ROUNDS_COMPLETE_NONE = 0,
	CGM_ROUNDS_COMPLETE_TIME_REACHED = 1 << 0,
	CGM_ROUNDS_COMPLETE_TARGET_REACHED = 1 << 1,
	CGM_ROUNDS_COMPLETE_ALL_PLAYERS_DEAD = 1 << 2,
	CGM_ROUNDS_COMPLETE_ONE_TEAM_LEFT_ALIVE = 1 << 3,
	CGM_ROUNDS_COMPLETE_CUSTOM = 1 << 4,
};

typedef void (*CgmRoundsEvent_func)(int roundNumber);
typedef int (*CgmRoundsCondition_func)(void);
typedef int (*CgmRoundsSelectWinner_func)(void);

struct CgmRoundsConfig
{
	int ResetFlags;
	int RoundCompleteFlags;
	int RoundCompleteCheckDelaySeconds;
	int RoundTimeLimitSeconds;
	int MaxRounds;
	int MaxRoundPoints;
	int RoundPlacementPoints[GAME_MAX_PLAYERS];
	int RoundObjectiveTarget;
	int RoundObjectiveStatIndex;
	char RoundObjectiveLowerScoreWins;
	char DisplayRoundTargetInScoreboardHud;
	char ShowLiveAggregateScore;
	CgmRoundsEvent_func ResetRound;
	CgmRoundsEvent_func RoundStarted;
	CgmRoundsEvent_func RoundCompleted;
	CgmRoundsEvent_func PostRoundStarted;
	CgmRoundsCondition_func CustomRoundCompleteCondition;
	CgmRoundsSelectWinner_func SelectRoundWinner;
};

struct CgmRoundsState
{
	int RoundNumber;
	int RoundStartTime;
	int GameStartTime;
	int RoundCompletePendingStartTime;
	int LastRoundCompleteTime;
	int PostRoundStartTime;
	int RoundsCompleted;
	int RoundPoints[GAME_MAX_PLAYERS];
	int LastRoundPointDeltas[GAME_MAX_PLAYERS];
	int LastRoundWinner;
	char RoundStarted;
	char CompletingRound;
	char InPostRoundGrace;
	char ForcedRoundComplete;
	char EndGameAfterPostRoundGrace;
};

struct CgmRoundsRoundEndedMessage
{
	int RoundNumber;
	int Winner;
	int RoundEndTime;
	int NextRoundStartTime;
	int RoundsCompleted;
	int RoundPoints[GAME_MAX_PLAYERS];
	int LastRoundPointDeltas[GAME_MAX_PLAYERS];
};

struct CgmRoundsRoundStartedMessage
{
	int RoundNumber;
	int RoundStartTime;
	int RoundsCompleted;
	int RoundPoints[GAME_MAX_PLAYERS];
};

extern struct CgmRoundsConfig cgmRoundsConfig;
extern struct CgmRoundsState cgmRoundsState;

int cgmRoundsGetRoundNumber(void);
int cgmRoundsGetRoundsCompleted(void);
int cgmRoundsGetRoundElapsedTime(void);
int cgmRoundsGetGameElapsedTime(void);
int cgmRoundsGetRoundPoints(int team);
int cgmRoundsGetTeamScore(int team);
int cgmRoundsGetFormattedTeamScore(int team);
int cgmRoundsGetRoundObjectiveTarget(void);
int cgmRoundsGetCurrentWinner(void);
int cgmRoundsTargetReached(void);
int cgmRoundsOneTeamLeftAlive(void);

void cgmRoundsStartNextRound(void);
void cgmRoundsCompleteRoundWithCurrentWinner(void);
void cgmRoundsCompleteRound(int winnerOverride);
void cgmRoundsCleanup(void);
void cgmRoundsInit(void);
void cgmRoundsTick(void);
void cgmRoundsDraw(void);

#endif // FORGE_CGM_ROUNDS_H
