#ifndef OBSTACLE_CONFIG_H
#define OBSTACLE_CONFIG_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

typedef void (*SetLocalPlayerReachedEnd_t)(void);
typedef void (*SetLocalPlayerReachedCheckpoint_t)(Moby* checkpoint);

struct ObstacleMapConfig
{
  u32 Magic;
  SetLocalPlayerReachedEnd_t SetLocalPlayerReachedEnd;
  SetLocalPlayerReachedCheckpoint_t SetLocalPlayerReachedCheckpoint;
};

extern struct ObstacleMapConfig MapConfig;

#endif // OBSTACLE_CONFIG_H
