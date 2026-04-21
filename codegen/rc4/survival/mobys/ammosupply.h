#ifndef SURVIVAL_AMMO_SUPPLY_H
#define SURVIVAL_AMMO_SUPPLY_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define AMMO_SUPPLY_OCLASS                      (0x01FF)

struct AmmoSupplyPVar
{
  u32 BaseCost[WEAPON_SLOT_COUNT-1]; // base ammo cost for each weapon
  u32 BaseCostV10[WEAPON_SLOT_COUNT-1]; // base ammo cost for each v10 weapon
};

void ammosupplyInit(void);

#endif // SURVIVAL_AMMO_SUPPLY_H
