#ifndef SURVIVAL_AMMODROP_H
#define SURVIVAL_AMMODROP_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "game.h"

#define MAX_MOB_AMMO_DROPS                    (10)

void ammodropCreateAt(Moby* moby);
void ammodropInit(void);

#endif // SURVIVAL_AMMODROP_H
