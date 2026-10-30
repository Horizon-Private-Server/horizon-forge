#ifndef RAIDS_AMMODROP_H
#define RAIDS_AMMODROP_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "game.h"

#define AMMODROP_AMMO_FRACTION (0.3f)

float ammodropGetPickupAmount(Player* player, int weaponId);
void ammodropCreateAt(Moby* moby);
void ammodropStart(void);
void ammodropInit(void);

#endif // RAIDS_AMMODROP_H
