#ifndef MOD_H
#define MOD_H

#include <tamtypes.h>
#include <libdl/game.h>

//--------------------------------------------------------------------------
struct ModState
{
  int ModRunningForTicks;
};

//--------------------------------------------------------------------------
extern struct ModState Mod;

#endif // MOD_H
