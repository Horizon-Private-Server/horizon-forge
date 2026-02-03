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
int mapOnMobCreate(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config)
{
	if (spawnParamsIdx < 0 || spawnParamsIdx >= MapConfig.DefaultSpawnParamsCount)
	{
		DPRINTF("unhandled create spawnParamsIdx %d\\n", spawnParamsIdx);
		return 0;
	}

	return mobCreate(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);
}

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

//--------------------------------------------------------------------------
float mapGetCurrentDifficulty(void)
{
	if (!MapConfig.State)
		return 0;

	float difficultyBySpawnedMobs = MapConfig.State->MobStats.TotalSpawned / 100;
	float difficultyBump = 0.6 + powf(1.5, MapConfig.State->RoundNumber / 25) * 0.4;
	float difficulty = difficultyBySpawnedMobs * difficultyBump * MapConfig.State->Difficulty;
	return difficulty;
}

//--------------------------------------------------------------------------
int mapGetDropTypeOnMobKilled(Player *killedByPlayer, Moby *mob, int gadgetId)
{
	float randomValue = randRange(0.0, 1.0);
	float probability = MOB_HAS_DROP_PROBABILITY;

	// return negative to not spawn
	if (randomValue >= probability)
		return -1;

	// return random drop type
	return randRangeInt(0, DROP_COUNT - 1);
}

//--------------------------------------------------------------------------
int mapGetRoundTransitionTime(int round)
{
	// return negative value for unlimited round time
	// return 0 for no round time

	// by default give infinite post round every 25 rounds
	if ((round % 25) == 0)
		return -1;

	return ROUND_TRANSITION_DELAY_MS;
}

//--------------------------------------------------------------------------
int mapGetRandomAlphamodForPlayer(Player *player, int gadgetIdOrEmpty)
{
	return AlphaModsEnabled[rand(AlphaModsEnabledCount)];
}

//--------------------------------------------------------------------------
void interopInit(void)
{
	MapConfig.Functions.OnMobCreateFunc = &mapOnMobCreate;
	MapConfig.Functions.OnMobKilledFunc = &mapOnMobKilled;
	MapConfig.Functions.CanSpawnMobsFunc = &mapCanSpawnMobs;
	MapConfig.Functions.GetSpawnPointsFunc = &mapGetSpawnPoints;
	MapConfig.Functions.ConsiderMobSpawnPointFunc = &mapConsiderMobSpawnPoint;
	MapConfig.Functions.GetDifficultyMultiplierFunc = &mapGetDifficultyMultiplier;
	MapConfig.Functions.GetBoltMultiplierFunc = &mapGetBoltMultiplier;
	MapConfig.Functions.GetXpMultiplierFunc = &mapGetXpMultiplier;
	MapConfig.Functions.GetBoltRankMultiplierFunc = &mapGetBoltRankMultiplier;
	MapConfig.Functions.GetSpawnDistanceMultiplierFunc = &mapGetSpawnDistanceMultiplier;
	MapConfig.Functions.GetWeaponPickupCooldownMultiplierFunc = &mapGetWeaponPickupCooldownMultiplier;
	MapConfig.Functions.GetBakedSpawnPointsFunc = &mapGetBakedSpawnPoints;
	MapConfig.Functions.CanPrestigePlayerWeaponFunc = &mapCanPrestigePlayerWeapon;
	MapConfig.Functions.GetPrestigePlayerWeaponCostFunc = &mapGetPrestigePlayerWeaponCost;
	MapConfig.Functions.CanUpgradePlayerWeaponFunc = &mapCanUpgradePlayerWeapon;
	MapConfig.Functions.GetUpgradePlayerWeaponCostFunc = &mapGetUpgradePlayerWeaponCost;
	MapConfig.Functions.GetXpForNextTokenFunc = &mapGetXpForNextToken;
	MapConfig.Functions.GetCurrentDifficultyFunc = &mapGetCurrentDifficulty;
	MapConfig.Functions.GetDropTypeOnMobKilledFunc = &mapGetDropTypeOnMobKilled;
	MapConfig.Functions.GetRoundTransitionTimeFunc = &mapGetRoundTransitionTime;
	MapConfig.Functions.GetRandomAlphamodForPlayerFunc = &mapGetRandomAlphamodForPlayer;
}
