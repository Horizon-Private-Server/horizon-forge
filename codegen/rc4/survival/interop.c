#include <libdl/area.h>
#include <libdl/spawnpoint.h>
#include <libdl/stdio.h>
#include "mob.h"
#include "config.h"
#include "interop.h"
#include "utils.h"
#include "maputils.h"

const char *SURVIVAL_PRESTIGE_WEAPON_NEED_V10_MESSAGE = "Your weapon is not powerful enough";
const char *SURVIVAL_PRESTIGE_WEAPON_MAXED_MESSAGE = "Your weapon is too powerful";

//--------------------------------------------------------------------------
void mapOnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId)
{
#if STACKABLES
	stackableOnMobKilled(moby, killedByPlayerId, killedByWeaponId);
#endif

#if SOULCOLLECTOR
	soulcollectorOnSoul(moby->Position, killedByPlayerId);
#endif

#ifdef AMMO_DROP_PROBABILITY
	Player *player = playerGetAll()[killedByPlayerId];
	if (playerIsValid(player) && player->IsLocal && randRange(0, 1) < AMMO_DROP_PROBABILITY)
	{
		ammodropCreateAt(moby);
	}
#endif
}

//--------------------------------------------------------------------------
int mapCanSpawnMobs(void)
{
#if DEBUG_MANUAL_SPAWNING
	return 0;
#endif

	return 1;
}

//--------------------------------------------------------------------------
int mapGetSpawnPoints(int **outSpawnPointIndices)
{
	Area_t area;
	if (mobSpawnPointsAreaIdx < 0 || !areaGetArea(mobSpawnPointsAreaIdx, &area))
	{
		*outSpawnPointIndices = NULL;
		return 0;
	}

	*outSpawnPointIndices = area.Cuboids;
	return area.CuboidCount;
}

//--------------------------------------------------------------------------
int mapConsiderMobSpawnPoint(struct MobSpawnParams *mobSpawnParams, VECTOR position, float yaw, Player *targetPlayer)
{
	if (!targetPlayer || !targetPlayer->PlayerMoby)
		return 1;

	// check if we have a path to
	// if not, don't spawn here
	if (!pathHasRouteFromTo(pathGetClosestNodeIdx(position), pathTargetCacheGetClosestNodeIdx(targetPlayer->PlayerMoby)))
	{
		return 0;
	}

	return 1;
}

//--------------------------------------------------------------------------
float mapGetDifficultyMultiplier(void)
{
	return bakedConfig.Difficulty;
}

//--------------------------------------------------------------------------
float mapGetBoltMultiplier(void)
{
	return bakedConfig.BoltMultiplier;
}

//--------------------------------------------------------------------------
float mapGetXpMultiplier(void)
{
	return bakedConfig.XpMultiplier;
}

//--------------------------------------------------------------------------
int mapGetBoltRankMultiplier(void)
{
	return bakedConfig.BoltRankMultiplier;
}

//--------------------------------------------------------------------------
float mapGetSpawnDistanceMultiplier(void)
{
	return bakedConfig.SpawnDistanceMultiplier;
}

//--------------------------------------------------------------------------
float mapGetWeaponPickupCooldownMultiplier(void)
{
	return bakedConfig.WeaponPickupCooldownMultiplier;
}

//--------------------------------------------------------------------------
struct SurvivalBakedSpawnpoint *mapGetBakedSpawnPoints(int *count)
{
	if (count)
		*count = BAKED_SPAWNPOINT_COUNT;
	return bakedConfig.BakedSpawnPoints;
}

//--------------------------------------------------------------------------
int mapCanPrestigePlayerWeapon(Player *player, int gadgetId, int prestigeNum, char **outMsg)
{
	// cap prestige at max
	if (prestigeNum > bakedConfig.WeaponPrestigeMax)
	{
		if (outMsg)
			*outMsg = SURVIVAL_PRESTIGE_WEAPON_MAXED_MESSAGE;

		return 0;
	}

	// must be v10
	if (player->GadgetBox->Gadgets[gadgetId].Level != VENDOR_MAX_WEAPON_LEVEL)
	{
		if (outMsg)
			*outMsg = SURVIVAL_PRESTIGE_WEAPON_NEED_V10_MESSAGE;

		return 0;
	}

	return 1;
}

//--------------------------------------------------------------------------
u32 mapGetPrestigePlayerWeaponCost(Player *player, int gadgetId, int levelNum)
{
	return bakedConfig.PrestigeCostPerLevel[levelNum];
}

//--------------------------------------------------------------------------
int mapCanUpgradePlayerWeapon(Player *player, int gadgetId, int levelNum)
{
	// only can upgrade weapon gadgets
	switch (gadgetId)
	{
	case WEAPON_ID_VIPERS:
	case WEAPON_ID_MAGMA_CANNON:
	case WEAPON_ID_ARBITER:
	case WEAPON_ID_FUSION_RIFLE:
	case WEAPON_ID_MINE_LAUNCHER:
	case WEAPON_ID_B6:
	case WEAPON_ID_OMNI_SHIELD:
	case WEAPON_ID_FLAIL:
		break;
	default:
		return 0;
	}

	// max v10
	return levelNum < VENDOR_MAX_WEAPON_LEVEL;
}

//--------------------------------------------------------------------------
u32 mapGetUpgradePlayerWeaponCost(Player *player, int gadgetId, int levelNum)
{
	if (!player)
		return 0;

	if (levelNum < 0 || levelNum >= VENDOR_MAX_WEAPON_LEVEL)
		return 0;

	return bakedConfig.VendorCost[levelNum];
}

//--------------------------------------------------------------------------
u32 mapGetXpForNextToken(Player *player, int token)
{
	if (token > SURVIVAL_XP_CURVE_FLATTEN_AFTER_N_TOKENS)
		return (u32)(250 * powf(1.05, SURVIVAL_XP_CURVE_FLATTEN_AFTER_N_TOKENS));

	return (u32)(250 * powf(1.05, token));
}
