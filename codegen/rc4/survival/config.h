#ifndef SURVIVAL_CONFIG_H
#define SURVIVAL_CONFIG_H

#define BAKED_SPAWNPOINT_COUNT (32)
#define WEAPON_PRESTIGE_MAX (5)
#define VENDOR_MAX_WEAPON_LEVEL (9)

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "game.h"

enum BakedSpawnpointType
{
	BAKED_SPAWNPOINT_NONE = 0,
	BAKED_SPAWNPOINT_UPGRADE = 1,
};

typedef struct SurvivalBakedSpawnpoint
{
	enum BakedSpawnpointType Type;
	int Params;
	float Position[3];
	float Rotation[3];
} SurvivalBakedSpawnpoint_t;

typedef struct SurvivalBakedConfig
{
	float Difficulty;
	float BoltMultiplier;
	float XpMultiplier;
	float SpawnDistanceMultiplier;
	float WeaponPickupCooldownMultiplier;
	int BoltRankMultiplier;
	SurvivalBakedSpawnpoint_t BakedSpawnPoints[BAKED_SPAWNPOINT_COUNT];
	char WeaponPrestigeMax;
	int PrestigeCostPerLevel[WEAPON_PRESTIGE_MAX];
	u32 VendorCost[VENDOR_MAX_WEAPON_LEVEL];
} SurvivalBakedConfig_t;

extern int mobAllowedCuboidIdx;
extern int mobSpawnPointsAreaIdx;
extern SurvivalBakedConfig_t bakedConfig;

void configInit(void);

#endif // SURVIVAL_CONFIG_H
