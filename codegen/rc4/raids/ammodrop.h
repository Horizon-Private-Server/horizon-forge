#ifndef RAIDS_AMMODROP_H
#define RAIDS_AMMODROP_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "game.h"

#define AMMODROP_AUTHORED_AMMO_FRACTION (0.3f)
#define AMMODROP_ENEMY_AMMO_FRACTION (0.15f)
#define AMMODROP_PROGRESS_PER_DROP (10000)

void ammodropOnMobKilled(Moby* moby, int killedByPlayerId);
void ammodropStart(void);
void ammodropInit(void);

#endif // RAIDS_AMMODROP_H
