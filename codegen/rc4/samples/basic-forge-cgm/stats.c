#include <libdl/spawnpoint.h>
#include <libdl/math3d.h>
#include "mod.h"
#include "stats.h"
#include "cgm_score.h"

//--------------------------------------------------------------------------
int statsIsInHillCuboid(VECTOR point)
{
	if (!Mod.HillMoby || !Mod.HillMoby->PVar)
		return 0;

	// hill stores current cuboid in pvars at offset 0x80
	int hillCuboidIdx = *(int*)(Mod.HillMoby->PVar + 0x80);
	if (hillCuboidIdx <= 0)
		return 0;

	SpawnPoint* hillCuboid = spawnPointGet(hillCuboidIdx);
	if (!hillCuboid)
		return 0;

	// only works for square hills
	// TODO: add support for circular hills
	return spawnPointIsPointInside(hillCuboid, point, NULL);
}

//--------------------------------------------------------------------------
void statsTrackPlayer(Player* player)
{
	if (!player)
		return;

	GameData* gameData = gameGetData();
	int pidx = player->PlayerId;
	int inHill = statsIsInHillCuboid(player->PlayerPosition);
	struct ModPlayerState *modPlayerState = &Mod.PlayerStates[pidx];

	// track changes in kills stat
	if (gameData->PlayerStats.Kills[pidx] != modPlayerState->LastKills)
	{
		int delta = gameData->PlayerStats.Kills[pidx] - modPlayerState->LastKills;
		if (delta > 0 && inHill) cgmScoreIncCustomPlayerIntStat(pidx, CSTAT_KILLS_IN_HILL, delta, 0);
		modPlayerState->LastKills = gameData->PlayerStats.Kills[pidx];
	}
	
	// track changes in deaths stat
	if (gameData->PlayerStats.Deaths[pidx] != modPlayerState->LastDeaths)
	{
		int delta = gameData->PlayerStats.Deaths[pidx] - modPlayerState->LastDeaths;
		if (delta > 0 && inHill) cgmScoreIncCustomPlayerIntStat(pidx, CSTAT_DEATHS_IN_HILL, delta, 0);
		modPlayerState->LastDeaths = gameData->PlayerStats.Deaths[pidx];
	}
	
	// track distance travelled if not dead/respawning/first pass
	float distance = vector_distance(player->PlayerPosition, modPlayerState->LastPosition);
	vector_copy(modPlayerState->LastPosition, player->PlayerPosition);
	if (!playerIsDead(player) && !playerStateIsDead(modPlayerState->LastState) && modPlayerState->HasFirstPass)
		cgmScoreIncCustomPlayerFloatStat(pidx, CSTAT_DISTANCE, distance, 0);

	// finally update player state for next statsTrackPlayer() call
	modPlayerState->LastState = player->PlayerState;
	modPlayerState->HasFirstPass = 1;
}
