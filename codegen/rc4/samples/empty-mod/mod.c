#include <libdl/stdio.h>
#include <libdl/string.h>
#include <libdl/utils.h>
#include "mod.h"

//--------------------------------------------------------------------------
struct ModState Mod = {};

//--------------------------------------------------------------------------
void modDraw(void)
{
	// called every draw frame
	// put draw logic in here
	DPRINTF("modDraw()\n");
}

//--------------------------------------------------------------------------
void modUpdate(void)
{
	// called every game tick
	// put mod logic in here
	DPRINTF("modUpdate()\n");

	Mod.ModRunningForTicks++;
}

//--------------------------------------------------------------------------
void modStart(void)
{
	// called once at the start of the game
	// after the map has loaded and all players are 'ready'
	DPRINTF("modStart()\n");
}

//--------------------------------------------------------------------------
void modCleanup(void)
{
	// called once when map is unloaded
	DPRINTF("modCleanup()\n");
}

//--------------------------------------------------------------------------
void modInit(void)
{
	// called once after map is loaded and mobys are spawned
	DPRINTF("modInit()\n");

	Mod.ModRunningForTicks = 0;
}

//--------------------------------------------------------------------------
void modLoad(void)
{
	// called once when map is loaded
	// install your game hooks here
	DPRINTF("modLoad()\n");
}
