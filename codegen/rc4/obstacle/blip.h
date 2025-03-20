#ifndef OBSTACLE_BLIP_H
#define OBSTACLE_BLIP_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define BLIP_OCLASS                           (0x400C)

struct BlipPVar
{
  char DefaultState;
  char BlipType;
  char BlipTeam;
  Moby* AttachToMoby;
};

void blipInit(void);

#endif // OBSTACLE_BLIP_H
