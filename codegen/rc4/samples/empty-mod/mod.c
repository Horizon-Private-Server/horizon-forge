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
void modCleanup(void)
{
	// called once when map is unloaded
	DPRINTF("modCleanup()\n");
}

//--------------------------------------------------------------------------
void modInit(void)
{
	// called once when map is loaded
	DPRINTF("modInit()\n");

	Mod.ModRunningForTicks = 0;
}
