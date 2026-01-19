#ifndef RAIDS_COLLECTIBLE_H
#define RAIDS_COLLECTIBLE_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "mob.h"
#include "game.h"

#define COLLECTIBLE_OCLASS                        (0x400E)
#define COLLECTIBLE_REWARD                        (100000)

struct CollectiblePVar
{
  char Index;
};

void collectibleInit(void);

#endif // RAIDS_COLLECTIBLE_H
