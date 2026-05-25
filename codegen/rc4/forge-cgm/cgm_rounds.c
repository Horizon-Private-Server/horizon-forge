#include <libdl/game.h>
#include <libdl/player.h>
#include <libdl/utils.h>
#include <libdl/string.h>
#include <libdl/time.h>
#include <libdl/team.h>
#include <libdl/net.h>
#include <libdl/stdio.h>
#include "cgm.h"
#include "cgm_rounds.h"
#include "window.h"

int cgmRoundsGetPostRoundRowScore(int index, int teamsEnabled);
int cgmRoundsGetPostRoundRowSortScore(int index, int teamsEnabled);
int cgmRoundsGetPostRoundRowScoreboardSortScore(int index, int teamsEnabled);
int cgmRoundsBuildPostRoundRows(int *rows, int teamsEnabled);
int cgmRoundsBuildPostRoundScoreboardRows(int *rows, int teamsEnabled);
int cgmRoundsBuildPostRoundRowsWithSort(int *rows, int teamsEnabled, int scoreboardSort);
int cgmRoundsFormatPostRoundScoreTarget(char *buf, int bufSize);
void cgmRoundsApplyReset(void);
void cgmRoundsReturnFlags(void);
void cgmRoundsResetNodes(void);
void cgmRoundsDestroyPlayerObjects(void);
void cgmRoundsFreezePlayers(void);
int cgmRoundsGetOnlyTeamLeftAlive(void);

struct CgmRoundsState cgmRoundsState = {};

//--------------------------------------------------------------------------
int cgmRoundsGetRoundNumber(void)
{
	return cgmRoundsState.RoundNumber;
}

//--------------------------------------------------------------------------
int cgmRoundsGetRoundsCompleted(void)
{
	return cgmRoundsState.RoundsCompleted;
}

//--------------------------------------------------------------------------
int cgmRoundsGetRoundElapsedTime(void)
{
	if (!cgmRoundsState.RoundStartTime)
		return 0;

	return gameGetTime() - cgmRoundsState.RoundStartTime;
}

//--------------------------------------------------------------------------
int cgmRoundsGetRoundTimeRemaining(void)
{
	if (cgmRoundsConfig.RoundTimeLimitSeconds <= 0)
		return 0;

	int remainingMs = (cgmRoundsConfig.RoundTimeLimitSeconds * TIME_SECOND) - cgmRoundsGetRoundElapsedTime();
	if (remainingMs < 0)
		remainingMs = 0;

	return remainingMs;
}

//--------------------------------------------------------------------------
int cgmRoundsGetGameElapsedTime(void)
{
	if (!cgmRoundsState.GameStartTime)
		return 0;

	return gameGetTime() - cgmRoundsState.GameStartTime;
}

//--------------------------------------------------------------------------
int cgmRoundsGetRoundPoints(int team)
{
	if (team < 0 || team >= GAME_MAX_PLAYERS)
		return 0;

	return cgmRoundsState.RoundPoints[team];
}

//--------------------------------------------------------------------------
int cgmRoundsGetTeamScore(int team)
{
	return cgmScoreGetStatValueForTeam(team, cgmRoundsConfig.RoundObjectiveStatIndex, 0);
}

//--------------------------------------------------------------------------
enum CgmScoreStatValueType cgmRoundsGetRoundObjectiveValueType(void)
{
	if (cgmRoundsConfig.RoundObjectiveStatIndex >= 0 && cgmRoundsConfig.RoundObjectiveStatIndex < cgmScoreStatsCount)
		return cgmScoreStats[cgmRoundsConfig.RoundObjectiveStatIndex].ValueType;

	return CGM_SCORE_STAT_TYPE_INT;
}

//--------------------------------------------------------------------------
int cgmRoundsGetFormattedTeamScore(int team)
{
	int score = cgmRoundsGetTeamScore(team);
	if (cgmRoundsConfig.ShowLiveAggregateScore)
		score = cgmScoreGetLiveStatValueForTeam(team, cgmRoundsConfig.RoundObjectiveStatIndex, gameGetOptions()->GameFlags.MultiplayerGameFlags.Teamplay);

	return cgmScoreGetFormattedScore(score, cgmRoundsGetRoundObjectiveValueType());
}

//--------------------------------------------------------------------------
int cgmRoundsGetRoundObjectiveTarget(void)
{
	return cgmRoundsConfig.RoundObjectiveTarget;
}

//--------------------------------------------------------------------------
int cgmRoundsGetScoreboardTimerOverride(void)
{
	// no time limit
	if (cgmRoundsConfig.RoundTimeLimitSeconds <= 0)
		return -1;

	return cgmRoundsGetRoundTimeRemaining();
}

//--------------------------------------------------------------------------
void cgmRoundsSetTeamScoreboardOverride(int team, int score)
{
	gameScoreboardSetTeamScore(team, cgmRoundsGetFormattedTeamScore(team));
}

//--------------------------------------------------------------------------
void cgmRoundsSetScoreboardMaxOverride(int max)
{
	gameScoreboardSetScoreMax(cgmScoreGetFormattedScore(cgmRoundsGetRoundObjectiveTarget(), cgmRoundsGetRoundObjectiveValueType()));
}

//--------------------------------------------------------------------------
void cgmRoundsApplyRemoteRoundStarted(struct CgmRoundsRoundStartedMessage *msg)
{
	if (msg->RoundNumber <= 0)
		return;

	if (msg->RoundNumber < cgmRoundsState.RoundNumber)
		return;

	if (msg->RoundNumber == cgmRoundsState.RoundNumber && cgmRoundsState.RoundStarted && !cgmRoundsState.InPostRoundGrace)
		return;

	cgmRoundsState.RoundNumber = msg->RoundNumber;
	cgmRoundsState.RoundStartTime = msg->RoundStartTime;
	cgmRoundsState.RoundsCompleted = msg->RoundsCompleted;
	cgmRoundsState.RoundStarted = 1;
	cgmRoundsState.InPostRoundGrace = 0;
	cgmRoundsState.ForcedRoundComplete = 0;
	cgmRoundsState.EndGameAfterPostRoundGrace = 0;
	cgmRoundsState.RoundCompletePendingStartTime = 0;
	cgmRoundsState.LastRoundCompleteTime = 0;
	cgmRoundsState.PostRoundStartTime = 0;
	cgmRoundsState.LastRoundWinner = -1;
	memcpy(cgmRoundsState.RoundPoints, msg->RoundPoints, sizeof(cgmRoundsState.RoundPoints));
	memset(cgmRoundsState.LastRoundPointDeltas, 0, sizeof(cgmRoundsState.LastRoundPointDeltas));

	cgmRoundsApplyReset();

	if (cgmRoundsConfig.RoundStarted)
		cgmRoundsConfig.RoundStarted(cgmRoundsState.RoundNumber);
}

//--------------------------------------------------------------------------
int cgmRoundsOnRecvRoundStarted(void *connection, void *data)
{
	struct CgmRoundsRoundStartedMessage msg;
	memcpy(&msg, data, sizeof(msg));

	cgmRoundsApplyRemoteRoundStarted(&msg);

	return sizeof(msg);
}

//--------------------------------------------------------------------------
void cgmRoundsBroadcastRoundStarted(void)
{
	void *connection = netGetDmeServerConnection();
	if (!connection)
		return;

	struct CgmRoundsRoundStartedMessage msg;
	memset(&msg, 0, sizeof(msg));
	msg.RoundNumber = cgmRoundsState.RoundNumber;
	msg.RoundStartTime = cgmRoundsState.RoundStartTime;
	msg.RoundsCompleted = cgmRoundsState.RoundsCompleted;
	memcpy(msg.RoundPoints, cgmRoundsState.RoundPoints, sizeof(msg.RoundPoints));

	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CGM_MSG_ID_SEND_ROUND_STARTED, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
int cgmRoundsOnRecvRoundEnded(void *connection, void *data)
{
	struct CgmRoundsRoundEndedMessage msg;
	memcpy(&msg, data, sizeof(msg));

	if (msg.RoundNumber < cgmRoundsState.RoundNumber)
		return sizeof(msg);

	cgmRoundsState.RoundNumber = msg.RoundNumber;
	cgmRoundsState.RoundsCompleted = msg.RoundsCompleted;
	cgmRoundsState.LastRoundCompleteTime = msg.RoundEndTime;
	cgmRoundsState.LastRoundWinner = msg.Winner;
	cgmRoundsState.RoundStarted = 1;
	cgmRoundsState.ForcedRoundComplete = 1;
	cgmRoundsState.RoundCompletePendingStartTime = 0;
	if (msg.NextRoundStartTime > 0)
	{
		cgmRoundsState.PostRoundStartTime = msg.NextRoundStartTime - CGM_ROUNDS_POST_ROUND_GRACE_MS;
		cgmRoundsState.InPostRoundGrace = 1;
		cgmRoundsFreezePlayers();
	}
	else
	{
		cgmRoundsState.PostRoundStartTime = 0;
		cgmRoundsState.InPostRoundGrace = 0;
	}

	memcpy(cgmRoundsState.RoundPoints, msg.RoundPoints, sizeof(cgmRoundsState.RoundPoints));
	memcpy(cgmRoundsState.LastRoundPointDeltas, msg.LastRoundPointDeltas, sizeof(cgmRoundsState.LastRoundPointDeltas));
	cgmScoreFinalizeRoundTimeAliveStats(cgmRoundsState.RoundStartTime, msg.RoundEndTime);
	cgmScoreCommitRoundAggregates();
	cgmScoreBroadcastLocalPlayerRoundAggregates();

	return sizeof(msg);
}

//--------------------------------------------------------------------------
void cgmRoundsBroadcastRoundEnded(int winner, int nextRoundStartTime)
{
	void *connection = netGetDmeServerConnection();
	if (!connection)
		return;

	struct CgmRoundsRoundEndedMessage msg;
	memset(&msg, 0, sizeof(msg));
	msg.RoundNumber = cgmRoundsState.RoundNumber;
	msg.Winner = winner;
	msg.RoundEndTime = cgmRoundsState.LastRoundCompleteTime;
	msg.NextRoundStartTime = nextRoundStartTime;
	msg.RoundsCompleted = cgmRoundsState.RoundsCompleted;
	memcpy(msg.RoundPoints, cgmRoundsState.RoundPoints, sizeof(msg.RoundPoints));
	memcpy(msg.LastRoundPointDeltas, cgmRoundsState.LastRoundPointDeltas, sizeof(msg.LastRoundPointDeltas));

	netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CGM_MSG_ID_SEND_ROUND_ENDED, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
int cgmRoundsGetCurrentWinner(void)
{
	int winningTeam = -1;
	int winningScore = 0;
	int tied = 0;
	int i;

	if (cgmRoundsConfig.SelectRoundWinner)
		return cgmRoundsConfig.SelectRoundWinner();

	int onlyTeamLeftAlive = (cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_ONE_TEAM_LEFT_ALIVE) ? cgmRoundsGetOnlyTeamLeftAlive() : -1;
	if (onlyTeamLeftAlive >= 0)
		return onlyTeamLeftAlive;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		if (!cgmScoreGetTeamHasPlayer(i))
			continue;

		int score = cgmRoundsGetTeamScore(i);
		if (winningTeam < 0 || (!cgmRoundsConfig.RoundObjectiveLowerScoreWins && score > winningScore) || (cgmRoundsConfig.RoundObjectiveLowerScoreWins && score < winningScore))
		{
			winningTeam = i;
			winningScore = score;
			tied = 0;
		}
		else if (score == winningScore)
		{
			tied = 1;
		}
	}

	return tied ? -1 : winningTeam;
}

//--------------------------------------------------------------------------
int cgmRoundsTargetReached(void)
{
	int i;
	if (cgmRoundsConfig.RoundObjectiveTarget <= 0)
		return 0;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		if (!cgmScoreGetTeamHasPlayer(i))
			continue;

		int score = cgmRoundsGetTeamScore(i);
		if (score >= cgmRoundsConfig.RoundObjectiveTarget)
			return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
int cgmRoundsAllPlayersDead(void)
{
	int hadPlayer = 0;
	int i;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!playerIsValid(player))
			continue;

		hadPlayer = 1;
		if (!playerIsDead(player))
			return 0;
	}

	return hadPlayer;
}

//--------------------------------------------------------------------------
int cgmRoundsGetOnlyTeamLeftAlive(void)
{
	int teamsWithPlayers[GAME_MAX_PLAYERS] = {};
	int teamsAlive[GAME_MAX_PLAYERS] = {};
	int teamCount = 0;
	int aliveTeamCount = 0;
	int aliveTeam = -1;
	int i;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!playerIsValid(player))
			continue;

		int team = playerGetJuggSafeTeam(player);
		if (team < 0 || team >= GAME_MAX_PLAYERS)
			continue;

		if (!teamsWithPlayers[team])
		{
			teamsWithPlayers[team] = 1;
			teamCount += 1;
		}

		if (!playerIsDead(player) && player->Health > 0 && !teamsAlive[team])
		{
			teamsAlive[team] = 1;
			aliveTeamCount += 1;
			aliveTeam = team;
		}
	}

	if (teamCount > 1 && aliveTeamCount == 1)
		return aliveTeam;

	return -1;
}

//--------------------------------------------------------------------------
int cgmRoundsOneTeamLeftAlive(void)
{
	return cgmRoundsGetOnlyTeamLeftAlive() >= 0;
}

//--------------------------------------------------------------------------
void cgmRoundsResetStats(void)
{
	cgmScoreResetRoundStats();
}

//--------------------------------------------------------------------------
void cgmRoundsRespawnPlayer(Player *player)
{
	// Kick from vehicle
	if (player->Vehicle)
		vehicleRemovePlayer(player->Vehicle, player);

	// Respawn (first res patch)
	POKE_U32(0x205e2d48, 0x24070001);
	playerRespawn(player);
	POKE_U32(0x205e2d48, 0x0000382d);
}

//--------------------------------------------------------------------------
void cgmRoundsResetPlayers(void)
{
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!playerIsValid(player))
			continue;

		if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_RESPAWN_PLAYERS)
			cgmRoundsRespawnPlayer(player);

		if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_REFILL_HEALTH)
			playerSetHealth(player, player->MaxHealth);
	}

	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_DESTROY_PLAYER_OBJECTS)
		cgmRoundsDestroyPlayerObjects();
}

//--------------------------------------------------------------------------
void cgmRoundsReturnFlags(void)
{
	// find and reset flags
	Moby *moby = mobyListGetStart();
	Moby *mEnd = mobyListGetEnd();
	while (moby < mEnd)
	{
		if (!mobyIsDestroyed(moby) &&
				(moby->OClass == MOBY_ID_BLUE_FLAG ||
				 moby->OClass == MOBY_ID_RED_FLAG ||
				 moby->OClass == MOBY_ID_GREEN_FLAG ||
				 moby->OClass == MOBY_ID_ORANGE_FLAG))
		{
			*(u16 *)(moby->PVar + 0x10) = 0xFFFF;
			vector_copy((float *)&moby->Position, (float *)moby->PVar);
		}

		++moby;
	}
}

//--------------------------------------------------------------------------
void cgmRoundsResetNodes(void)
{
}

//--------------------------------------------------------------------------
void cgmRoundsDestroyPlayerObjects(void)
{
	Moby *moby = mobyListGetStart();
	Moby *mEnd = mobyListGetEnd();
	while (moby < mEnd)
	{
		if (!mobyIsDestroyed(moby))
		{
			switch (moby->OClass)
			{
			case MOBY_ID_MINE_LAUNCHER_MINE:
			{
				moby->State = 3; // destroy
				break;
			}
			case MOBY_ID_HOLOSHIELD_SHOT:
			{
				// todo
				break;
			}
			}
		}

		++moby;
	}
}

//--------------------------------------------------------------------------
void cgmRoundsApplyReset(void)
{
	cgmRoundsResetStats();
	if (cgmRoundsState.RoundNumber > 1 || cgmRoundsState.RoundsCompleted > 0)
		cgmRoundsResetPlayers();

	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_RETURN_FLAGS)
		cgmRoundsReturnFlags();

	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_NODES)
		cgmRoundsResetNodes();

	if (cgmRoundsConfig.ResetRound)
		cgmRoundsConfig.ResetRound(cgmRoundsState.RoundNumber);

	cgmScorePrimeManualStatSources();
}

//--------------------------------------------------------------------------
void cgmRoundsFreezePlayers(void)
{
	// freeze players
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!player)
			continue;

		player->timers.noInput = 3;
		player->timers.allowQuickSelect = 3;
		player->timers.noCamInputTimer = 3;
		player->timers.invincibilityTimer = 3;
		player->timers.noExternalRot = 3;
		player->timers.noJumps = 3;
		player->timers.noSwing = 3;
	}
}

//--------------------------------------------------------------------------
void cgmRoundsStartNextRound(void)
{
	cgmRoundsState.RoundNumber += 1;
	cgmRoundsState.RoundStartTime = gameGetTime();
	cgmRoundsState.RoundStarted = 1;
	cgmRoundsState.InPostRoundGrace = 0;
	cgmRoundsState.ForcedRoundComplete = 0;
	cgmRoundsState.EndGameAfterPostRoundGrace = 0;
	cgmRoundsState.RoundCompletePendingStartTime = 0;
	cgmRoundsState.LastRoundCompleteTime = 0;
	cgmRoundsState.PostRoundStartTime = 0;
	cgmRoundsState.LastRoundWinner = -1;
	memset(cgmRoundsState.LastRoundPointDeltas, 0, sizeof(cgmRoundsState.LastRoundPointDeltas));

	cgmRoundsApplyReset();

	if (cgmRoundsConfig.RoundStarted)
		cgmRoundsConfig.RoundStarted(cgmRoundsState.RoundNumber);

	cgmRoundsBroadcastRoundStarted();
}

//--------------------------------------------------------------------------
int cgmRoundsShouldCompleteRound(void)
{
	if (cgmRoundsState.ForcedRoundComplete)
		return 1;

	if (cgmRoundsConfig.RoundCompleteCheckDelaySeconds > 0 && cgmRoundsGetRoundElapsedTime() < (cgmRoundsConfig.RoundCompleteCheckDelaySeconds * TIME_SECOND))
		return 0;

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_TIME_REACHED) && cgmRoundsConfig.RoundTimeLimitSeconds > 0)
	{
		if (cgmRoundsGetRoundElapsedTime() >= (cgmRoundsConfig.RoundTimeLimitSeconds * TIME_SECOND))
			return 1;
	}

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_TARGET_REACHED) && cgmRoundsTargetReached())
		return 1;

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_ALL_PLAYERS_DEAD) && cgmRoundsAllPlayersDead())
		return 1;

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_ONE_TEAM_LEFT_ALIVE) && cgmRoundsOneTeamLeftAlive())
		return 1;

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_CUSTOM) && cgmRoundsConfig.CustomRoundCompleteCondition)
		return cgmRoundsConfig.CustomRoundCompleteCondition();

	return 0;
}

//--------------------------------------------------------------------------
void cgmRoundsStartPostRoundGrace(int winner)
{
	cgmRoundsState.InPostRoundGrace = 1;
	cgmRoundsState.PostRoundStartTime = gameGetTime();
	cgmRoundsState.LastRoundWinner = winner;

	cgmRoundsFreezePlayers();
	if (cgmRoundsConfig.PostRoundStarted)
		cgmRoundsConfig.PostRoundStarted(cgmRoundsState.RoundNumber);

	cgmRoundsBroadcastRoundEnded(winner, cgmRoundsState.PostRoundStartTime + CGM_ROUNDS_POST_ROUND_GRACE_MS);
}

//--------------------------------------------------------------------------
int cgmRoundsPostRoundGraceComplete(void)
{
	return cgmRoundsState.InPostRoundGrace && ((gameGetTime() - cgmRoundsState.PostRoundStartTime) >= CGM_ROUNDS_POST_ROUND_GRACE_MS);
}

//--------------------------------------------------------------------------
int cgmRoundsShouldEndGame(void)
{
	if (cgmRoundsConfig.MaxRounds > 0 && cgmRoundsState.RoundsCompleted >= cgmRoundsConfig.MaxRounds)
		return 1;

	if (cgmRoundsConfig.MaxRoundPoints > 0)
	{
		int i;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			if (cgmRoundsState.RoundPoints[i] >= cgmRoundsConfig.MaxRoundPoints)
				return 1;
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void cgmRoundsCompleteRound(int winnerOverride)
{
	if (cgmRoundsState.CompletingRound || cgmRoundsState.InPostRoundGrace)
		return;

	cgmRoundsState.CompletingRound = 1;
	cgmRoundsState.ForcedRoundComplete = 1;
	cgmRoundsState.RoundCompletePendingStartTime = 0;

	int roundEndTime = gameGetTime();
	cgmScoreUpdateRoundTrackedStats();
	cgmScoreFinalizeRoundTimeAliveStats(cgmRoundsState.RoundStartTime, roundEndTime);
	int winner = winnerOverride >= -1 ? winnerOverride : cgmRoundsGetCurrentWinner();
	cgmRoundsState.LastRoundWinner = winner;
	cgmRoundsState.RoundsCompleted += 1;
	int rows[GAME_MAX_PLAYERS] = {};
	int teamsEnabled = gameGetOptions()->GameFlags.MultiplayerGameFlags.Teamplay;
	int rowCount = cgmRoundsBuildPostRoundRows(rows, teamsEnabled);
	int placement = 0;
	int lastScore = 0;
	int i;
	memset(cgmRoundsState.LastRoundPointDeltas, 0, sizeof(cgmRoundsState.LastRoundPointDeltas));
	for (i = 0; i < rowCount; ++i)
	{
		int score = cgmRoundsGetPostRoundRowSortScore(rows[i], teamsEnabled);
		if (i == 0 || score != lastScore)
			placement = i;

		lastScore = score;

		if (placement >= 0 && placement < GAME_MAX_PLAYERS)
		{
			int delta = cgmRoundsConfig.RoundPlacementPoints[placement];
			cgmRoundsState.LastRoundPointDeltas[rows[i]] = delta;
			cgmRoundsState.RoundPoints[rows[i]] += delta;
		}
	}

	cgmRoundsState.LastRoundCompleteTime = roundEndTime;
	cgmScoreCommitRoundAggregates();
	cgmScoreBroadcastLocalPlayerRoundAggregates();
	if (cgmRoundsConfig.RoundCompleted)
		cgmRoundsConfig.RoundCompleted(cgmRoundsState.RoundNumber);

	if (cgmRoundsShouldEndGame())
	{
		cgmRoundsState.EndGameAfterPostRoundGrace = 1;
		cgmRoundsStartPostRoundGrace(winner);
	}
	else
	{
		cgmRoundsState.EndGameAfterPostRoundGrace = 0;
		cgmRoundsStartPostRoundGrace(winner);
	}

	cgmRoundsState.CompletingRound = 0;
}

//--------------------------------------------------------------------------
void cgmRoundsCompleteRoundWithCurrentWinner(void)
{
	cgmRoundsCompleteRound(-2);
}

//--------------------------------------------------------------------------
void cgmRoundsCheckDelayedRoundComplete(void)
{
	if (cgmRoundsState.RoundCompletePendingStartTime > 0)
	{
		if ((gameGetTime() - cgmRoundsState.RoundCompletePendingStartTime) >= CGM_ROUNDS_COMPLETE_DELAY_MS)
			cgmRoundsCompleteRound(-2);

		return;
	}

	if (cgmRoundsShouldCompleteRound())
		cgmRoundsState.RoundCompletePendingStartTime = gameGetTime();
}

//--------------------------------------------------------------------------
void cgmRoundsTick(void)
{
	GameData *gameData = gameGetData();

	// update score hud timer
	if (cgmRoundsConfig.RoundTimeLimitSeconds > 0)
	{
		((void (*)(int))0x00540508)(cgmRoundsGetRoundElapsedTime());
	}

	if (gameData->GameIsOver)
		return;

	if (cgmRoundsState.InPostRoundGrace)
	{
		cgmRoundsFreezePlayers();

		if (!gameAmIHost())
			return;

		if (cgmRoundsPostRoundGraceComplete())
		{
			if (cgmRoundsState.EndGameAfterPostRoundGrace)
			{
				cgmScoreEndGameEarly();
				return;
			}

			cgmRoundsStartNextRound();
		}
		return;
	}

	if (!gameAmIHost())
		return;

	if (!cgmRoundsState.RoundStarted)
	{
		cgmRoundsStartNextRound();
		return;
	}

	cgmRoundsCheckDelayedRoundComplete();
}

//--------------------------------------------------------------------------
void cgmRoundsCleanup(void)
{
	netUninstallCustomMsgHandler(CGM_MSG_ID_SEND_ROUND_STARTED, &cgmRoundsOnRecvRoundStarted);
	netUninstallCustomMsgHandler(CGM_MSG_ID_SEND_ROUND_ENDED, &cgmRoundsOnRecvRoundEnded);
}

//--------------------------------------------------------------------------
void cgmRoundsInit(void)
{
	memset(&cgmRoundsState, 0, sizeof(cgmRoundsState));
	cgmRoundsState.GameStartTime = gameGetTime();
	cgmRoundsState.LastRoundWinner = -1;

	// hook net messages
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_ROUND_STARTED, &cgmRoundsOnRecvRoundStarted);
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_ROUND_ENDED, &cgmRoundsOnRecvRoundEnded);

	// prevent survivor from ending the game when it is a round end condition
	if ((cgmRoundsConfig.RoundCompleteFlags & (CGM_ROUNDS_COMPLETE_ALL_PLAYERS_DEAD | CGM_ROUNDS_COMPLETE_ONE_TEAM_LEFT_ALIVE)))
	{
		POKE_U32(0x006219B8, 0);
		POKE_U32(0x00621A10, 0);
	}

	// display round time in score timer if round has time limit
	if (cgmRoundsConfig.RoundTimeLimitSeconds > 0)
	{
		HOOK_JAL(0x0055b968, cgmRoundsGetScoreboardTimerOverride);
	}

	if (cgmRoundsConfig.DisplayRoundTargetInScoreboardHud)
	{
		HOOK_JAL(0x005404f0, cgmRoundsSetTeamScoreboardOverride);
		HOOK_JAL(0x005404d0, cgmRoundsSetScoreboardMaxOverride);
		cgmRoundsSetScoreboardMaxOverride(0);

		if (cgmRoundsConfig.RoundObjectiveLowerScoreWins)
		{
			POKE_U32(0x00542640, 0x15400044); // bne t2,zero,0x00542754
			POKE_U32(0x005426D0, 0x1040005D); // beq v0,zero,0x00542848
		}
		else
		{
			POKE_U32(0x00542640, 0x11400044); // beq t2,zero,0x00542754
			POKE_U32(0x005426D0, 0x1440005D); // bne v0,zero,0x00542848
		}
	}
}

//--------------------------------------------------------------------------
void cgmRoundsDrawStatus(void)
{
	if (!cgmRoundsState.RoundStarted)
		return;

	float x = SCREEN_WIDTH - 97;
	float y = 34;
	float scale = 0.8;
	float lineHeight = 14;

	char buf[32];
	snprintf(buf, sizeof(buf), "Round %d", cgmRoundsState.RoundNumber);
	gfxHelperDrawText(15, SCREEN_HEIGHT - 15, 0, 0, 0.8, 0x80FFFFFF, buf, -1, TEXT_ALIGN_BOTTOMLEFT, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundRowScore(int index, int teamsEnabled)
{
	if (teamsEnabled)
		return cgmRoundsGetTeamScore(index);

	return cgmScoreGetStatValueForPlayer(index, cgmRoundsConfig.RoundObjectiveStatIndex, 0);
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundRowSortScore(int index, int teamsEnabled)
{
	enum CgmScoreStatSource source = cgmScoreStats[cgmRoundsConfig.RoundObjectiveStatIndex].Source;
	if (teamsEnabled)
		return cgmScoreGetTeamScoreSortValueForSource(index, source);

	return cgmScoreGetPlayerStatSortValue(index, source);
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundRowScoreboardSortScore(int index, int teamsEnabled)
{
	int statIndex = cgmRoundsConfig.RoundObjectiveStatIndex;
	if (cgmScoreGetStatHasRoundAggregate(statIndex))
	{
		if (teamsEnabled)
			return cgmScoreGetStatValueForTeam(index, statIndex, 1);

		return cgmScoreGetStatValueForPlayer(index, statIndex, 1);
	}

	return cgmRoundsGetPostRoundRowSortScore(index, teamsEnabled);
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundRowStat(int index, int teamsEnabled, int statIndex)
{
	if (teamsEnabled)
		return cgmScoreGetStatValueForTeam(index, statIndex, 1);

	return cgmScoreGetStatValueForPlayer(index, statIndex, 1);
}

//--------------------------------------------------------------------------
int cgmRoundsPostRoundRowHasPlayer(int index, int teamsEnabled)
{
	GameSettings *gameSettings = gameGetSettings();

	if (teamsEnabled)
	{
		int i;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
			if (gameSettings->PlayerTeams[i] == index && gameSettings->PlayerNames[i][0])
				return 1;

		return 0;
	}

	return gameSettings->PlayerNames[index][0];
}

//--------------------------------------------------------------------------
void cgmRoundsFormatPostRoundStat(char *buf, int bufSize, int value, enum CgmScoreStatValueType valueType)
{
	switch (valueType)
	{
	case CGM_SCORE_STAT_TYPE_TIME_SECONDS:
	{
		int seconds = value;
		if (seconds < 0)
			seconds = 0;

		snprintf(buf, bufSize, "%d:%02d", seconds / 60, seconds % 60);
		break;
	}
	case CGM_SCORE_STAT_TYPE_TIME_MILLISECONDS:
	{
		int seconds = value / TIME_SECOND;
		if (seconds < 0)
			seconds = 0;

		snprintf(buf, bufSize, "%d:%02d", seconds / 60, seconds % 60);
		break;
	}
	case CGM_SCORE_STAT_TYPE_FLOAT:
	{
		int whole = value / SCORE_FLOAT_PRECISION;
		int frac = value % (int)SCORE_FLOAT_PRECISION;
		if (frac < 0)
			frac = -frac;

		snprintf(buf, bufSize, "%d.%02d", whole, (frac * 100) / (int)SCORE_FLOAT_PRECISION);
		break;
	}
	case CGM_SCORE_STAT_TYPE_INT:
	default:
		snprintf(buf, bufSize, "%d", value);
		break;
	}
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundScoreboardStats(int *statIndexes)
{
	int count = 0;
	int i;
	for (i = 0; i < cgmScoreStatsCount && count < MAX_SCOREBOARD_STATS; ++i)
	{
		if (!cgmScoreStats[i].DisplayOnEndGameScoreboard)
			continue;

		statIndexes[count++] = i;
	}

	return count;
}

//--------------------------------------------------------------------------
int cgmRoundsBuildPostRoundRows(int *rows, int teamsEnabled)
{
	return cgmRoundsBuildPostRoundRowsWithSort(rows, teamsEnabled, 0);
}

//--------------------------------------------------------------------------
int cgmRoundsBuildPostRoundScoreboardRows(int *rows, int teamsEnabled)
{
	return cgmRoundsBuildPostRoundRowsWithSort(rows, teamsEnabled, 1);
}

//--------------------------------------------------------------------------
int cgmRoundsBuildPostRoundRowsWithSort(int *rows, int teamsEnabled, int scoreboardSort)
{
	int count = 0;
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		if (!cgmRoundsPostRoundRowHasPlayer(i, teamsEnabled))
			continue;

		rows[count++] = i;
	}

	int a;
	for (a = 0; a < count; ++a)
	{
		int b;
		for (b = a + 1; b < count; ++b)
		{
			int aScore;
			int bScore;
			if (scoreboardSort)
			{
				aScore = cgmRoundsGetPostRoundRowScoreboardSortScore(rows[a], teamsEnabled);
				bScore = cgmRoundsGetPostRoundRowScoreboardSortScore(rows[b], teamsEnabled);
			}
			else
			{
				aScore = cgmRoundsGetPostRoundRowSortScore(rows[a], teamsEnabled);
				bScore = cgmRoundsGetPostRoundRowSortScore(rows[b], teamsEnabled);
			}

			int shouldSwap = cgmRoundsConfig.RoundObjectiveLowerScoreWins ? bScore < aScore : bScore > aScore;
			if (!shouldSwap && aScore == bScore)
				shouldSwap = rows[b] < rows[a];

			if (shouldSwap)
			{
				int tmp = rows[a];
				rows[a] = rows[b];
				rows[b] = tmp;
			}
		}
	}

	return count;
}

//--------------------------------------------------------------------------
void cgmRoundsDrawPostRoundScoreboard(void)
{
	GameOptions *gameOptions = gameGetOptions();
	GameSettings *gameSettings = gameGetSettings();
	int teamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	int rows[GAME_MAX_PLAYERS] = {};
	int statIndexes[MAX_SCOREBOARD_STATS] = {};
	int rowCount = cgmRoundsBuildPostRoundScoreboardRows(rows, teamsEnabled);
	int statCount = cgmRoundsGetPostRoundScoreboardStats(statIndexes);
	int hasRoundPointsStat = 0;

	int s;
	for (s = 0; s < statCount; ++s)
	{
		if (cgmScoreStats[statIndexes[s]].Source == CGM_SCORE_STAT_ROUND_POINTS)
		{
			hasRoundPointsStat = 1;
			break;
		}
	}

	if (rowCount <= 0)
		return;

	float padding = 4;
	float placementWidth = 16;
	float labelWidth = 70;
	float statWidth = statCount > 0 ? (hasRoundPointsStat ? 70 : 58) : 0;
	float rowHeight = 13;
	float contentWidth = placementWidth + labelWidth + (statCount * statWidth);
	float windowWidth = contentWidth + (padding * 2);
	float windowHeight = rowHeight * (rowCount + 1);
	float y = (SCREEN_HEIGHT / 2) - (cgmScoreGetTargetScore() > 0 ? 45 : 60);
	float scale = 0.55;
	u32 color = 0x80FFFFFF;
	char buf[32];

	Window_t window;
	windowCreate(&window, SCREEN_WIDTH / 2, y, 0, 0, windowWidth, windowHeight, TEXT_ALIGN_TOPCENTER);

	windowDrawText(&window, TEXT_ALIGN_TOPLEFT, padding, rowHeight / 2, scale, color, "#", -1, TEXT_ALIGN_MIDDLELEFT);
	windowDrawText(&window, TEXT_ALIGN_TOPLEFT, padding + placementWidth, rowHeight / 2, scale, color, teamsEnabled ? "Team" : "Player", -1, TEXT_ALIGN_MIDDLELEFT);
	for (s = 0; s < statCount; ++s)
	{
		if (cgmScoreGetTargetScore() > 0 && statIndexes[s] == cgmScoreTarget.StatIndex)
		{
			snprintf(buf, sizeof(buf), "%s*", cgmScoreStats[statIndexes[s]].Name);
			windowDrawText(&window, TEXT_ALIGN_TOPLEFT, padding + placementWidth + labelWidth + (s * statWidth), rowHeight / 2, scale, color, buf, -1, TEXT_ALIGN_MIDDLELEFT);
		}
		else
		{
			windowDrawText(&window, TEXT_ALIGN_TOPLEFT, padding + placementWidth + labelWidth + (s * statWidth), rowHeight / 2, scale, color, cgmScoreStats[statIndexes[s]].Name, -1, TEXT_ALIGN_MIDDLELEFT);
		}
	}

	int r;
	for (r = 0; r < rowCount; ++r)
	{
		int index = rows[r];
		char *teamName = ((char *(*)(int))0x0061c160)(index);
		char *playerName = gameSettings->PlayerNames[index];
		Player *player = playerGetFromIndex(index);
		int team = teamsEnabled ? index : (player ? playerGetJuggSafeTeam(player) : gameSettings->PlayerTeams[index]);
		u32 teamColor = (team >= 0 && team < GAME_MAX_PLAYERS) ? TEAM_COLORS[team] : 0x80202020;
		u32 rowColor = (teamColor & 0x00FFFFFF) | 0x30000000;
		float rowY = (r + 1) * rowHeight;

		Window_t windowRow;
		windowCreateFrom(&windowRow, &window, 0, rowY, windowWidth, rowHeight, TEXT_ALIGN_TOPLEFT);
		windowDrawBox(&windowRow, TEXT_ALIGN_TOPLEFT, 0, 0, windowWidth, rowHeight - 1, rowColor, TEXT_ALIGN_TOPLEFT);
		snprintf(buf, sizeof(buf), "%d", r + 1);
		windowDrawText(&windowRow, TEXT_ALIGN_MIDDLELEFT, padding, 2, scale, color, buf, -1, TEXT_ALIGN_BOTTOMLEFT);
		safe_strcpy(buf, teamsEnabled ? teamName : playerName, sizeof(buf));
		windowDrawText(&windowRow, TEXT_ALIGN_MIDDLELEFT, padding + placementWidth, 2, scale, color, buf, -1, TEXT_ALIGN_BOTTOMLEFT);

		for (s = 0; s < statCount; ++s)
		{
			int value = cgmRoundsGetPostRoundRowStat(index, teamsEnabled, statIndexes[s]);
			if (cgmScoreStats[statIndexes[s]].Source == CGM_SCORE_STAT_ROUND_POINTS && cgmRoundsState.LastRoundPointDeltas[index] > 0)
				snprintf(buf, sizeof(buf), "%d  (+%d)", value, cgmRoundsState.LastRoundPointDeltas[index]);
			else
				cgmRoundsFormatPostRoundStat(buf, sizeof(buf), value, cgmScoreStats[statIndexes[s]].ValueType);

			windowDrawText(&windowRow, TEXT_ALIGN_MIDDLELEFT, padding + placementWidth + labelWidth + (s * statWidth), 2, scale, color, buf, -1, TEXT_ALIGN_BOTTOMLEFT);
		}
	}
}

//--------------------------------------------------------------------------
int cgmRoundsFormatPostRoundScoreTarget(char *buf, int bufSize)
{
	int target = cgmScoreGetTargetScore();
	int statIndex = cgmScoreTarget.StatIndex;
	if (target <= 0 || statIndex < 0 || statIndex >= cgmScoreStatsCount)
		return 0;

	int displayTarget = cgmScoreGetFormattedScore(target, cgmScoreStats[statIndex].ValueType);
	snprintf(buf, bufSize, "First to %d", displayTarget);
	return 1;
}

//--------------------------------------------------------------------------
int cgmRoundsDidLocalPlayerWinLastRound(void)
{
	GameOptions *gameOptions = gameGetOptions();
	int teamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	int rows[GAME_MAX_PLAYERS] = {};
	int rowCount = cgmRoundsBuildPostRoundRows(rows, teamsEnabled);
	if (rowCount <= 0)
		return 0;

	int winningScore = cgmRoundsGetPostRoundRowSortScore(rows[0], teamsEnabled);
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		int row = teamsEnabled ? playerGetJuggSafeTeam(player) : player->PlayerId;
		if (row < 0 || row >= GAME_MAX_PLAYERS)
			continue;

		if (cgmRoundsGetPostRoundRowSortScore(row, teamsEnabled) == winningScore)
			return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
void cgmRoundsDrawPostRound(void)
{
	// draw post round scoreboard during game end phase
	if (gameHasEnded())
	{
		cgmRoundsDrawPostRoundScoreboard();
		return;
	}

	if (!cgmRoundsState.InPostRoundGrace)
		return;

	char buf[48];
	int won = cgmRoundsDidLocalPlayerWinLastRound();
	gfxHelperDrawText(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 2) - 98, 0, 0, 2.0, won ? 0x8000FF00 : 0x800000FF, won ? "ROUND WIN" : "ROUND LOSS", -1, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

	snprintf(buf, sizeof(buf), "Round %d Results", cgmRoundsState.RoundNumber);
	gfxHelperDrawText(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 2) - 70, 0, 0, 0.8, 0x80FFFFFF, buf, -1, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
	if (cgmRoundsFormatPostRoundScoreTarget(buf, sizeof(buf)))
		gfxHelperDrawText(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 2) - 56, 0, 0, 0.65, 0x80FFFFFF, buf, -1, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

	cgmRoundsDrawPostRoundScoreboard();
}

//--------------------------------------------------------------------------
void cgmRoundsDraw(void)
{
	if (gameIsAnyStartMenuOpen())
		return;

	if (PATCH_POINTERS_PATCHMENU)
		return;

	cgmRoundsDrawStatus();
	cgmRoundsDrawPostRound();
}
