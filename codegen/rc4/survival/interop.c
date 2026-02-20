#include <libdl/area.h>
#include <libdl/spawnpoint.h>
#include <libdl/stdio.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include "mob.h"
#include "config.h"
#include "stackables.h"
#include "survival_items.h"
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
void mapConsiderSpawnDrop(Moby *moby, int killedByPlayerId, int killedByWeaponId)
{
	if (!MapConfig.State)
		return;

	// have drop funcs
	if (!MapConfig.Functions.GetDropItemOnMobKilledFunc || !MapConfig.Functions.CreateMobDropFunc)
		return;

	int roundIsSpecial = MapConfig.State->RoundIsSpecial;
	int disableDrops = MapConfig.SpecialRoundParams[MapConfig.State->RoundSpecialIdx].DisableDrops;
	if (!roundIsSpecial || !disableDrops)
	{
		Player *killedByPlayer = playerGetFromIndex(killedByPlayerId);
		if (playerIsValid(killedByPlayer) && killedByPlayer->IsLocal)
		{
			int itemIdx = MapConfig.Functions.GetDropItemOnMobKilledFunc(killedByPlayer, moby, killedByWeaponId);
			if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
			{
				MapConfig.Functions.CreateMobDropFunc(moby->Position, itemIdx, gameGetTime() + DROP_DURATION, killedByPlayer->Team);
			}
		}
	}
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
	Player *player = playerGetFromIndex(killedByPlayerId);
	if (playerIsValid(player) && player->IsLocal && randRange(0, 1) < AMMO_DROP_PROBABILITY)
	{
		ammodropCreateAt(moby);
	}
#endif

	mapConsiderSpawnDrop(moby, killedByPlayerId, killedByWeaponId);
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
u32 mapGetPrestigePlayerWeaponCost(Player *player, int gadgetId, int prestigeNum)
{
	if (prestigeNum <= 0)
		return 0;

	return bakedConfig.PrestigeCostPerLevel[prestigeNum - 1];
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
int mapGetDropItemOnMobKilled(Player *killedByPlayer, Moby *mob, int gadgetId)
{
#ifdef MOB_DROP_PROBABILITY
	float randomValue = randRange(0.0, 1.0);
	float probability = MOB_DROP_PROBABILITY;
	if (probability <= 0)
		return -1;

	// wait for drop cooldown
	if (MapConfig.State && MapConfig.State->DropCooldownTicks > 0)
		return -1;

	// return negative to not spawn
	if (randomValue >= probability)
		return -1;

	// return random drop type
	return dropGetRandomItem(mob, killedByPlayer->PlayerId, gadgetId);
#else
	// no drops defined
	return -1;
#endif
}

//--------------------------------------------------------------------------
int mapGetRoundTransitionTime(int round)
{
	// return negative value for unlimited round time
	// return 0 for no round time

	// solo players want to be able to take breaks more frequently
	// should infinite post round time just be the default?
	if (MapConfig.State && MapConfig.State->ActivePlayerCount == 1)
		return -1;

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
int mapGetPlayerItemCount(Player *player, int itemId)
{
	if (!MapConfig.State)
		return 0;

	if (!playerIsValid(player))
		return 0;

	if (itemId < 0 || itemId >= MapConfig.ItemDefCount)
		return 0;

	return MapConfig.State->PlayerStates[player->PlayerId].State.ItemCounts[itemId];
}

//--------------------------------------------------------------------------
void mapOnPlayerUpdate(Player *player)
{
	// pass to stackables
	stackablesProcessPlayer(player);

	// pass to items
#ifdef ITEM_IMMEDIATE_PLAYER_HEALTH_UPGRADE
	mapOnItemApply_PlayerHealth(ITEM_IMMEDIATE_PLAYER_HEALTH_UPGRADE, &MapConfig.ItemDefs[ITEM_IMMEDIATE_PLAYER_HEALTH_UPGRADE], player);
#endif
#ifdef ITEM_IMMEDIATE_PLAYER_SPEED_UPGRADE
	mapOnItemApply_PlayerSpeed(ITEM_IMMEDIATE_PLAYER_SPEED_UPGRADE, &MapConfig.ItemDefs[ITEM_IMMEDIATE_PLAYER_SPEED_UPGRADE], player);
#endif
}

//--------------------------------------------------------------------------
void mapOnPlayerDied(Player *player)
{
}

//--------------------------------------------------------------------------
void mapOnPlayerGetVendorReward(Player *player, int gadgetId, int levelNum)
{
	if (!playerIsValid(player) || !player->IsLocal)
		return;

	int i;
	int itemIdxs[MAX_ITEM_COUNT];
	int count = 0;
	float sumWeight = 0;

	// collect all possible items
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		SurvivalItemDef_t *itemDef = &MapConfig.ItemDefs[i];
		float itemWeight = itemGetVendorRewardChanceWeight(i, itemDef, player->PlayerId);
		int canAcquire = itemCanAcquire(player, i);
		if (itemWeight <= 0 || !canAcquire)
			continue;

		sumWeight += itemWeight;
		itemIdxs[count] = i;
		++count;
	}

	// none found
	if (!count)
		return;

	// generate random value within range of weights
	// grab the first item where r > last
	float r = randRange(0, sumWeight);
	for (i = 0; i < (count - 1); ++i)
	{
		int itemIdx = itemIdxs[i];
		SurvivalItemDef_t *itemDef = &MapConfig.ItemDefs[itemIdx];
		float itemWeight = itemGetVendorRewardChanceWeight(i, itemDef, player->PlayerId);

		// found item
		if (r < itemWeight)
			break;

		r -= itemWeight;
	}

	// reward
	int selectedRewardItemIdx = itemIdxs[i];
	itemBeginAcquire(player->PlayerId, selectedRewardItemIdx);
	// itemShowMessage(player->LocalPlayerIndex, selectedRewardItemIdx, "Got %s!", 60);
}

//--------------------------------------------------------------------------
int mapOnBeforeDamageMob(Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args)
{
	float damage = args->DamageQuarters / 4.0;

	// no changes
	if (!player)
		return 1;

	// pass to stackables
	stackablesOnBeforeDamage(player, &damage);

	// update damage
	args->DamageQuarters = (u32)(damage * 4);

	// pass to items
#ifdef ITEM_IMMEDIATE_PLAYER_CRIT_UPGRADE
	mapOnItemApply_PlayerCrit(ITEM_IMMEDIATE_PLAYER_CRIT_UPGRADE, &MapConfig.ItemDefs[ITEM_IMMEDIATE_PLAYER_CRIT_UPGRADE], player, sourceMoby, mobMoby, args);
#endif
#ifdef ITEM_IMMEDIATE_PLAYER_DAMAGE_UPGRADE
	mapOnItemApply_PlayerDamage(ITEM_IMMEDIATE_PLAYER_DAMAGE_UPGRADE, &MapConfig.ItemDefs[ITEM_IMMEDIATE_PLAYER_DAMAGE_UPGRADE], player, sourceMoby, mobMoby, args);
#endif

	return 1;
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
	MapConfig.Functions.GetDropItemOnMobKilledFunc = &mapGetDropItemOnMobKilled;
	MapConfig.Functions.GetRoundTransitionTimeFunc = &mapGetRoundTransitionTime;
	MapConfig.Functions.GetRandomAlphamodForPlayerFunc = &mapGetRandomAlphamodForPlayer;
	MapConfig.Functions.GetPlayerItemCountFunc = &mapGetPlayerItemCount;
	MapConfig.Functions.GetOnPlayerItemAcquiredFunc = &itemOnAcquireTriggered;
	MapConfig.Functions.GetOnPlayerItemConsumedFunc = &itemOnConsumeTriggered;
	MapConfig.Functions.OnPlayerUpdateFunc = &mapOnPlayerUpdate;
	MapConfig.Functions.OnPlayerDiedFunc = &mapOnPlayerDied;
	MapConfig.Functions.OnPlayerGetVendorRewardFunc = &mapOnPlayerGetVendorReward;
	MapConfig.Functions.OnBeforeDamageMobFunc = &mapOnBeforeDamageMob;
}
