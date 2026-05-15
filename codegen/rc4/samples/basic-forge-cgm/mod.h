#ifndef MOD_H
#define MOD_H

#include <tamtypes.h>
#include <libdl/game.h>
#include <libdl/moby.h>

struct ModPlayerState
{
  VECTOR LastPosition;
  int HasFirstPass;
  int LastState;
  short LastKills;
  short LastDeaths;
};

//--------------------------------------------------------------------------
struct ModState
{
	Moby* HillMoby;
	struct ModPlayerState PlayerStates[GAME_MAX_PLAYERS];
};

//--------------------------------------------------------------------------
extern struct ModState Mod;

#endif // MOD_H
