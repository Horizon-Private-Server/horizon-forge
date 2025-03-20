#ifndef OBSTACLE_CONFIG_H
#define OBSTACLE_CONFIG_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

typedef void (*SetLocalPlayerReachedEnd_t)(void);

struct ObstacleMapConfig
{
  u32 Magic;
  SetLocalPlayerReachedEnd_t SetLocalPlayerReachedEnd;
};

#endif // OBSTACLE_CONFIG_H
