#ifndef OBSTACLE_PVARPOKE_H
#define OBSTACLE_PVARPOKE_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define PVARPOKE_OCLASS                         (0x400B)

enum PVarPokeState {
	PVARPOKE_TYPE_SET,
	PVARPOKE_TYPE_ADD,
	PVARPOKE_TYPE_MULT,
	PVARPOKE_TYPE_DIV,
};

enum PVarPokeType {
	PVARPOKE_STATE_DEACTIVATED,
	PVARPOKE_STATE_ACTIVATED,
};

struct PVarPokeRuntimeState
{
  int TimeActivated;
};

struct PVarPokePVar
{
  char DefaultState;
  char Log;
  Moby* Target;
  short PVarOffset;
  short DataSize;
  u8 MobyDerefs[4];
  char Buffer[0];
};

void pvarpokeStart(void);
void pvarpokeInit(void);

#endif // OBSTACLE_PVARPOKE_H
