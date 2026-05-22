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
int cgmRoundsGetRoundWins(int team)
{
	if (team < 0 || team >= GAME_MAX_PLAYERS)
		return 0;

	return cgmRoundsState.RoundWins[team];
}

//--------------------------------------------------------------------------
int cgmRoundsGetRoundLosses(int team)
{
	if (team < 0 || team >= GAME_MAX_PLAYERS)
		return 0;

	return cgmRoundsState.RoundLosses[team];
}

//--------------------------------------------------------------------------
int cgmRoundsGetTeamScore(int team)
{
	return cgmScoreGetTeamScoreForSource(team, cgmRoundsConfig.RoundObjectiveSource);
}

//--------------------------------------------------------------------------
int cgmRoundsGetFormattedTeamScore(int team)
{
	return cgmScoreGetFormattedScore(cgmRoundsGetTeamScore(team), cgmRoundsConfig.RoundObjectiveValueType);
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
	gameScoreboardSetScoreMax(cgmScoreGetFormattedScore(cgmRoundsGetRoundObjectiveTarget(), cgmRoundsConfig.RoundObjectiveValueType));
}

//--------------------------------------------------------------------------
int cgmRoundsOnRecvRoundEnded(void *connection, void *data)
{
	struct CgmRoundsRoundEndedMessage msg;
	memcpy(&msg, data, sizeof(msg));

	cgmRoundsState.RoundNumber = msg.RoundNumber;
	cgmRoundsState.RoundsCompleted = msg.RoundsCompleted;
	cgmRoundsState.LastRoundCompleteTime = msg.RoundEndTime;
	cgmRoundsState.LastRoundWinner = msg.Winner;
	cgmRoundsState.RoundStarted = 1;
	cgmRoundsState.ForcedRoundComplete = 1;
	if (msg.NextRoundStartTime > 0)
	{
		cgmRoundsState.PostRoundStartTime = msg.NextRoundStartTime - CGM_ROUNDS_POST_ROUND_GRACE_MS;
		cgmRoundsState.InPostRoundGrace = 1;
	}
	else
	{
		cgmRoundsState.PostRoundStartTime = 0;
		cgmRoundsState.InPostRoundGrace = 0;
	}

	memcpy(cgmRoundsState.RoundWins, msg.RoundWins, sizeof(cgmRoundsState.RoundWins));
	memcpy(cgmRoundsState.RoundLosses, msg.RoundLosses, sizeof(cgmRoundsState.RoundLosses));

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
	memcpy(msg.RoundWins, cgmRoundsState.RoundWins, sizeof(msg.RoundWins));
	memcpy(msg.RoundLosses, cgmRoundsState.RoundLosses, sizeof(msg.RoundLosses));

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
		if (!playerIsDead(player) && player->Health > 0)
			return 0;
	}

	return hadPlayer;
}

//--------------------------------------------------------------------------
void cgmRoundsResetStats(void)
{
	GameData *gameData = gameGetData();

	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_PLAYER_STATS)
		memset(&gameData->PlayerStats, 0, sizeof(gameData->PlayerStats));

	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_TEAM_STATS)
		memset(&gameData->TeamStats, 0, sizeof(gameData->TeamStats));

	int i;
	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_CUSTOM_PLAYER_STATS)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			int s;
			for (s = 0; s < MAX_CUSTOM_STATS; ++s)
				cgmScoreSetCustomPlayerIntStat(i, s, 0);
		}
	}

	if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_CUSTOM_TEAM_STATS)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			int s;
			for (s = 0; s < MAX_CUSTOM_STATS; ++s)
				cgmScoreSetCustomTeamIntStat(i, s, 0);
		}
	}
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
			playerRespawn(player);

		if (cgmRoundsConfig.ResetFlags & CGM_ROUNDS_RESET_REFILL_HEALTH)
			playerSetHealth(player, player->MaxHealth);
	}
}

//--------------------------------------------------------------------------
void cgmRoundsApplyReset(void)
{
	cgmRoundsResetStats();
	cgmRoundsResetPlayers();

	if (cgmRoundsConfig.ResetRound)
		cgmRoundsConfig.ResetRound(cgmRoundsState.RoundNumber);
}

//--------------------------------------------------------------------------
void cgmRoundsStartNextRound(void)
{
	cgmRoundsState.RoundNumber += 1;
	cgmRoundsState.RoundStartTime = gameGetTime();
	cgmRoundsState.RoundStarted = 1;
	cgmRoundsState.InPostRoundGrace = 0;
	cgmRoundsState.ForcedRoundComplete = 0;
	cgmRoundsState.LastRoundCompleteTime = 0;
	cgmRoundsState.PostRoundStartTime = 0;
	cgmRoundsState.LastRoundWinner = -1;

	cgmRoundsApplyReset();

	if (cgmRoundsConfig.RoundStarted)
		cgmRoundsConfig.RoundStarted(cgmRoundsState.RoundNumber);
}

//--------------------------------------------------------------------------
int cgmRoundsShouldCompleteRound(void)
{
	if (cgmRoundsState.ForcedRoundComplete)
		return 1;

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_TIME_REACHED) && cgmRoundsConfig.RoundTimeLimitSeconds > 0)
	{
		if (cgmRoundsGetRoundElapsedTime() >= (cgmRoundsConfig.RoundTimeLimitSeconds * TIME_SECOND))
			return 1;
	}

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_TARGET_REACHED) && cgmRoundsTargetReached())
		return 1;

	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_ALL_PLAYERS_DEAD) && cgmRoundsAllPlayersDead())
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

	if (cgmRoundsConfig.MaxRoundWins > 0)
	{
		int i;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			if (cgmRoundsState.RoundWins[i] >= cgmRoundsConfig.MaxRoundWins)
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

	int winner = winnerOverride >= -1 ? winnerOverride : cgmRoundsGetCurrentWinner();
	cgmRoundsState.LastRoundWinner = winner;
	cgmRoundsState.RoundsCompleted += 1;
	if (winner >= 0 && winner < GAME_MAX_PLAYERS)
	{
		cgmRoundsState.RoundWins[winner] += 1;

		int i;
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			if (i == winner || !cgmScoreGetTeamHasPlayer(i))
				continue;

			cgmRoundsState.RoundLosses[i] += 1;
		}
	}

	cgmRoundsState.LastRoundCompleteTime = gameGetTime();
	if (cgmRoundsConfig.RoundCompleted)
		cgmRoundsConfig.RoundCompleted(cgmRoundsState.RoundNumber);

	if (cgmRoundsShouldEndGame())
	{
		cgmRoundsBroadcastRoundEnded(winner, -1);
		cgmScoreEndGameEarly();
	}
	else
		cgmRoundsStartPostRoundGrace(winner);

	cgmRoundsState.CompletingRound = 0;
}

//--------------------------------------------------------------------------
void cgmRoundsCompleteRoundWithCurrentWinner(void)
{
	cgmRoundsCompleteRound(-2);
}

//--------------------------------------------------------------------------
void cgmRoundsTick(void)
{
	GameData *gameData = gameGetData();

	if (gameData->GameIsOver || !gameAmIHost())
		return;

	// update score hud timer
	((void (*)(int))0x00540508)(cgmRoundsGetRoundElapsedTime());

	if (!cgmRoundsState.RoundStarted)
	{
		cgmRoundsStartNextRound();
		return;
	}

	if (cgmRoundsState.InPostRoundGrace)
	{
		if (cgmRoundsPostRoundGraceComplete())
			cgmRoundsStartNextRound();
		return;
	}

	if (cgmRoundsShouldCompleteRound())
		cgmRoundsCompleteRound(-2);
}

//--------------------------------------------------------------------------
void cgmRoundsInit(void)
{
	memset(&cgmRoundsState, 0, sizeof(cgmRoundsState));
	cgmRoundsState.GameStartTime = gameGetTime();
	cgmRoundsState.LastRoundWinner = -1;

	// hook net messages
	netInstallCustomMsgHandler(CGM_MSG_ID_SEND_ROUND_ENDED, &cgmRoundsOnRecvRoundEnded);

	// prevent survivor from ending the game when it is a round end condition
	if ((cgmRoundsConfig.RoundCompleteFlags & CGM_ROUNDS_COMPLETE_ALL_PLAYERS_DEAD))
	{
		POKE_U32(0x006219B8, 0);
	}

	HOOK_JAL(0x0055b968, cgmRoundsGetScoreboardTimerOverride);
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
	gfxHelperDrawText(x, y, 0, 0, 1.0 * scale, 0x80FFFFFF, buf, -1, TEXT_ALIGN_MIDDLERIGHT, COMMON_DZO_DRAW_NORMAL);

	float yOffset = lineHeight;
	if (cgmRoundsConfig.MaxRounds > 0)
	{
		snprintf(buf, sizeof(buf), "of %d", cgmRoundsConfig.MaxRounds);
		gfxHelperDrawText(x, y, 0, yOffset, 0.8 * scale, 0x80FFFFFF, buf, -1, TEXT_ALIGN_MIDDLERIGHT, COMMON_DZO_DRAW_NORMAL);
		yOffset += lineHeight;
	}
	else if (cgmRoundsConfig.MaxRoundWins > 0)
	{
		snprintf(buf, sizeof(buf), "first to %d wins", cgmRoundsConfig.MaxRoundWins);
		gfxHelperDrawText(x, y, 0, yOffset, 0.8 * scale, 0x80FFFFFF, buf, -1, TEXT_ALIGN_MIDDLERIGHT, COMMON_DZO_DRAW_NORMAL);
		yOffset += lineHeight;
	}
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundRowScore(int index, int teamsEnabled)
{
	if (teamsEnabled)
		return cgmRoundsGetTeamScore(index);

	return cgmScoreGetPlayerStat(index, cgmRoundsConfig.RoundObjectiveSource);
}

//--------------------------------------------------------------------------
int cgmRoundsGetPostRoundRowStat(int index, int teamsEnabled, struct CgmScoreStat *stat)
{
	if (teamsEnabled)
		return cgmScoreGetTeamScoreForSource(index, stat->Source);

	return cgmScoreGetPlayerStat(index, stat->Source);
}

//--------------------------------------------------------------------------
int cgmRoundsPostRoundRowHasPlayer(int index, int teamsEnabled)
{
	if (teamsEnabled)
		return cgmScoreGetTeamHasPlayer(index);

	return playerIsValid(playerGetFromIndex(index));
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
int cgmRoundsGetPostRoundScoreboardStats(struct CgmScoreStat **stats)
{
	int count = 0;
	int i;
	for (i = 0; i < cgmScoreStatsCount && count < MAX_SCOREBOARD_STATS; ++i)
	{
		if (!cgmScoreStats[i].DisplayOnEndGameScoreboard)
			continue;

		stats[count++] = &cgmScoreStats[i];
	}

	return count;
}

//--------------------------------------------------------------------------
int cgmRoundsBuildPostRoundRows(int *rows, int teamsEnabled)
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
			int aScore = cgmRoundsGetPostRoundRowScore(rows[a], teamsEnabled);
			int bScore = cgmRoundsGetPostRoundRowScore(rows[b], teamsEnabled);
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
	struct CgmScoreStat *stats[MAX_SCOREBOARD_STATS] = {};
	int rowCount = cgmRoundsBuildPostRoundRows(rows, teamsEnabled);
	int statCount = cgmRoundsGetPostRoundScoreboardStats(stats);

	if (rowCount <= 0)
		return;

	float padding = 4;
	float labelWidth = 70;
	float statWidth = statCount > 0 ? 58 : 0;
	float rowHeight = 13;
	float contentWidth = labelWidth + (statCount * statWidth);
	float windowWidth = contentWidth + (padding * 2);
	float windowHeight = rowHeight * (rowCount + 1);
	float y = (SCREEN_HEIGHT / 2) - 60;
	float scale = 0.55;
	u32 color = 0x80FFFFFF;
	char buf[32];

	Window_t window;
	windowCreate(&window, SCREEN_WIDTH / 2, y, 0, 0, windowWidth, windowHeight, TEXT_ALIGN_TOPCENTER);

	windowDrawText(&window, TEXT_ALIGN_TOPLEFT, padding, rowHeight / 2, scale, color, teamsEnabled ? "Team" : "Player", -1, TEXT_ALIGN_MIDDLELEFT);
	int s;
	for (s = 0; s < statCount; ++s)
		windowDrawText(&window, TEXT_ALIGN_TOPLEFT, padding + labelWidth + (s * statWidth), rowHeight / 2, scale, color, stats[s]->Name, -1, TEXT_ALIGN_MIDDLELEFT);

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
		safe_strcpy(buf, teamsEnabled ? teamName : playerName, sizeof(buf));
		windowDrawText(&windowRow, TEXT_ALIGN_MIDDLELEFT, padding, 2, scale, color, buf, -1, TEXT_ALIGN_BOTTOMLEFT);

		for (s = 0; s < statCount; ++s)
		{
			int value = cgmRoundsGetPostRoundRowStat(index, teamsEnabled, stats[s]);
			cgmRoundsFormatPostRoundStat(buf, sizeof(buf), value, stats[s]->ValueType);
			windowDrawText(&windowRow, TEXT_ALIGN_MIDDLELEFT, padding + labelWidth + (s * statWidth), 2, scale, color, buf, -1, TEXT_ALIGN_BOTTOMLEFT);
		}
	}
}

//--------------------------------------------------------------------------
int cgmRoundsDidLocalPlayerWinLastRound(void)
{
	if (cgmRoundsState.LastRoundWinner < 0)
		return 0;

	GameOptions *gameOptions = gameGetOptions();
	int teamsEnabled = gameOptions->GameFlags.MultiplayerGameFlags.Teamplay;
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		if (teamsEnabled && playerGetJuggSafeTeam(player) == cgmRoundsState.LastRoundWinner)
			return 1;

		if (!teamsEnabled && player->PlayerId == cgmRoundsState.LastRoundWinner)
			return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
void cgmRoundsDrawPostRound(void)
{
	if (!cgmRoundsState.InPostRoundGrace)
		return;

	char buf[32];
	int won = cgmRoundsDidLocalPlayerWinLastRound();
	gfxHelperDrawText(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 2) - 98, 0, 0, 1.4, won ? 0x8000FF00 : 0x800000FF, won ? "VICTORY" : "FAILURE", -1, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

	snprintf(buf, sizeof(buf), "Round %d Results", cgmRoundsState.RoundNumber);
	gfxHelperDrawText(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 2) - 70, 0, 0, 0.8, 0x80FFFFFF, buf, -1, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
	cgmRoundsDrawPostRoundScoreboard();
}

//--------------------------------------------------------------------------
void cgmRoundsDraw(void)
{
	GameData *gameData = gameGetData();
	if (gameData->GameIsOver)
		return;

	if (gameIsAnyStartMenuOpen())
		return;

	if (PATCH_POINTERS_PATCHMENU)
		return;

	cgmRoundsDrawStatus();
	cgmRoundsDrawPostRound();
}
