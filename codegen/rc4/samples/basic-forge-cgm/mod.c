#include <libdl/stdio.h>
#include <libdl/string.h>
#include <libdl/utils.h>
#include "mod.h"
#include "stats.h"

//--------------------------------------------------------------------------
struct ModState Mod = {};

//--------------------------------------------------------------------------
void modDraw(void)
{
	// called every draw frame
	// put draw logic in here
}

//--------------------------------------------------------------------------
void modUpdate(void)
{
	// called every game tick
	// put mod logic in here
	if (!Mod.HillMoby) return;

	// track player stats
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		statsTrackPlayer(playerGetFromIndex(i));
}

//--------------------------------------------------------------------------
void modCleanup(void)
{
	// called once when map is unloaded
}

//--------------------------------------------------------------------------
void modInit(void)
{
	// called once when map is loaded
	// find hill moby
	Mod.HillMoby = mobyFindNextByOClass(mobyListGetStart(), 9732);
}
