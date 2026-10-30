#ifndef RAIDS_HOLOSHIELD_H
#define RAIDS_HOLOSHIELD_H

#include <libdl/moby.h>
#include "bank.h"

// Capacity uses permanent weapon damage and the effective Impact mod count.
#define HOLOSHIELD_IMPACT_CAPACITY_FACTOR (0.10f)
#define HOLOSHIELD_ABSORPTION_MULTIPLIER (10.0f)
#define HOLOSHIELD_XP_WEIGHT (0.10)

float holoshieldGetCapacity(RaidsInventoryItem_t* item, int impactBonus);
float holoshieldConsumeCapacity(float* remaining, float damage);
int holoshieldIsEnemySource(Moby* source);
void holoshieldDamage(Moby* shield, float damage);
Moby* holoshieldSpawnEnemyVehicleMobShot(float trailLength, float damage, VECTOR position, VECTOR velocity, Moby* creator, Moby* source);
void holoshieldDamageBeam(Moby* beam);
void holoshieldDraw(void);
void holoshieldInit(void);

#endif // RAIDS_HOLOSHIELD_H
