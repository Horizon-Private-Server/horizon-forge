#ifndef RAIDS_BADGES_H
#define RAIDS_BADGES_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "game.h"

#define BADGES_HEALTH_REGEN_AMOUNT                  (5)
#define BADGES_HEALTH_REGEN_COOLDOWN_TICKS          (2*TPS)

#define BADGES_AMMO_REGEN_COOLDOWN_TICKS            (5*TPS)

#define BADGES_BERSERKER_RANGED_DAMAGE_MULT         (2.0)

void badgesStart(void);
void badgesInit(void);

#endif // RAIDS_BADGES_H
