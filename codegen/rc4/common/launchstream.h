#ifndef COMMON_LAUNCHSTREAM_H
#define COMMON_LAUNCHSTREAM_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include <libdl/spawnpoint.h>

#define LAUNCHSTREAM_OCLASS                           (0x4011)

struct LaunchStreamPlayerState
{
  float Velocity[3];
  short Ticks;
  char Active;
  //short SplineIdx;
  //float SplineIdxT;
};

struct LaunchStreamPVar
{
  char DefaultState;
  char Log;
  char Omnidirectional;
  int CuboidIdx;
  Moby* TriggerMoby;
  float Angle;
  float Speed;
  struct LaunchStreamPlayerState PlayerStates[GAME_MAX_PLAYERS];
};

void launchstreamInit(void);

#endif // COMMON_LAUNCHSTREAM_H
