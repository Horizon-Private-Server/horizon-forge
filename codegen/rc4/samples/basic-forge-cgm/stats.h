#ifndef STATS_H
#define STATS_H

#include <tamtypes.h>
#include <libdl/player.h>

//--------------------------------------------------------------------------
enum ModCustomStatIds
{
	CSTAT_KILLS_IN_HILL,
	CSTAT_DEATHS_IN_HILL,
	CSTAT_DISTANCE,
};

//--------------------------------------------------------------------------
void statsTrackPlayer(Player* player);

#endif // STATS_H
