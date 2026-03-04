#ifndef SURVIVAL_INTEROP_H
#define SURVIVAL_INTEROP_H

// override in Forge with CustomCodeGen define SURVIVAL_XP_CURVE_FLATTEN_AFTER_N_TOKENS=N
#ifndef SURVIVAL_XP_CURVE_FLATTEN_AFTER_N_TOKENS
#define SURVIVAL_XP_CURVE_FLATTEN_AFTER_N_TOKENS (50)
#endif

#include <libdl/moby.h>
#include <libdl/player.h>
#include "config.h"
#include "drop.h"
#include "upgrade.h"

struct MobConfig;
struct MobSpawnEventArgs;
struct MobSpawnParams;
struct SurvivalBakedSpawnpoint;
struct MobDamageEventArgs;

typedef void (*ModeUpgradePlayerWeapon_func)(int playerId, int weaponId);
typedef void (*ModePushSnack_func)(char *string, int ticksAlive, int localPlayerIdx);
typedef void (*ModePushBubble_func)(VECTOR position, float randomRadius, float damage, int isLocal, int team);
typedef void (*ModePopulateSpawnArgs_func)(struct MobSpawnEventArgs *output, struct MobConfig *config, int spawnParamsIdx, int isBaseConfig, int spawnFlags);
typedef int (*ModeCreateMob_func)(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config);
typedef int (*ModeSpawnGetRandomPoint_func)(VECTOR out, struct MobSpawnParams *mob);
typedef void (*ModeMobNuke_func)(int killedByPlayerId);
typedef void (*ModeSetDoublePoints_func)(int isActive);
typedef void (*ModeSetDoubleXP_func)(int isActive);
typedef void (*ModeSetFreezeMobs_func)(int isActive);
typedef void (*ModeRevivePlayer_func)(Player *player, int fromPlayerId);
typedef void (*ModeSendPlayerStats_func)(int playerId);
typedef void (*ModeSendOnPlayerItemAcquired_func)(int playerId, int itemId);
typedef void (*ModeSendOnPlayerItemConsumed_func)(int playerId, int itemId);

typedef struct Guber *(*ModeGetGuber_func)(Moby *moby);
typedef int (*ModeHandleGuberEvent_func)(Moby *moby, GuberEvent *event);

typedef void (*MapOnMobSpawned_func)(Moby *moby);
typedef int (*MapOnMobCreate_func)(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config);
typedef void (*MapOnMobKilled_func)(Moby *moby, int killedByPlayerId, int killedByWeaponId);
typedef int (*MapCanSpawnMobs_func)(void);
typedef int (*MapGetSpawnPoints_func)(int **outSpawnPointIndices);
typedef int (*MapConsiderMobSpawnPoint_func)(struct MobSpawnParams *mobSpawnParams, VECTOR position, float yaw, Player *targetPlayer);
typedef int (*MapOnPlayerGetRes_func)(Player *player, VECTOR outPos, VECTOR outRot, int firstRes);
typedef int (*MapOnPlayerRevived_func)(Player *player, Player *revivedByPlayer);
typedef int (*MapCreateUpgradePickup_func)(int bakedSpawnIdx, int itemIdx);
typedef void (*MapPickupUpgradePickup_func)(Moby *moby, int pickedUpByPlayerId);
typedef int (*MapCreateMobDrop_func)(VECTOR position, int itemIdx, int destroyAtTime, int team);
typedef void (*MapFrameTick_func)(void);
typedef float (*MapGetDifficultyMultiplier_func)(void);
typedef float (*MapGetBoltMultiplier_func)(void);
typedef float (*MapGetXpMultiplier_func)(void);
typedef int (*MapGetBoltRankMultiplier_func)(void);
typedef float (*MapGetSpawnDistanceMultiplier_func)(void);
typedef float (*MapGetWeaponPickupCooldownMultiplier_func)(void);
typedef struct SurvivalBakedSpawnpoint *(*MapGetBakedSpawnPoints_func)(int *count);
typedef int (*MapCanPrestigePlayerWeapon_func)(Player *player, int gadgetId, int prestigeNum, char **outMsg);
typedef u32 (*MapGetPrestigePlayerWeaponCost_func)(Player *player, int gadgetId, int prestigeNum);
typedef int (*MapCanUpgradePlayerWeapon_func)(Player *player, int gadgetId, int levelNum);
typedef u32 (*MapGetUpgradePlayerWeaponCost_func)(Player *player, int gadgetId, int levelNum);
typedef u32 (*MapGetXpForNextToken_func)(Player *player, int token);
typedef float (*MapGetCurrentDifficulty_func)(void);
typedef int (*MapGetDropItemOnMobKilled_func)(Player *killedByPlayer, Moby *mob, int gadgetId);
typedef int (*MapGetRoundTransitionTime_func)(int round);
typedef int (*MapGetPlayerItemCount_func)(Player *player, int itemId);
typedef void (*MapOnPlayerItemAcquired_func)(Player *player, int itemId);
typedef void (*MapOnPlayerItemConsumed_func)(Player *player, int itemId);
typedef void (*MapOnPlayerUpdate_func)(Player *player);
typedef void (*MapOnPlayerDied_func)(Player *player);
typedef void (*MapOnPlayerGetVendorReward_func)(Player *player, int gadgetId, int levelNum);
typedef int (*MapOnBeforeDamageMob_func)(Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args);

struct SurvivalInteropTable
{
	// mode
	ModeSpawnGetRandomPoint_func ModeSpawnGetRandomPointFunc;
	ModeUpgradePlayerWeapon_func ModeUpgradePlayerWeaponFunc;
	ModePushSnack_func ModePushSnackFunc;
	ModePushBubble_func ModePushBubbleFunc;
	ModePopulateSpawnArgs_func ModePopulateSpawnArgsFunc;
	ModeCreateMob_func ModeCreateMobFunc;
	ModeMobNuke_func ModeMobNukeFunc;
	ModeSetDoublePoints_func ModeSetDoublePointsFunc;
	ModeSetDoubleXP_func ModeSetDoubleXPFunc;
	ModeSetFreezeMobs_func ModeSetFreezeMobsFunc;
	ModeRevivePlayer_func ModeRevivePlayerFunc;
	ModeGetGuber_func ModeOnGetGuberFunc;
	ModeHandleGuberEvent_func ModeOnGuberEventFunc;
	ModeSendPlayerStats_func ModeSendPlayerStatsFunc;
	ModeSendOnPlayerItemAcquired_func ModeSendOnPlayerItemAcquiredFunc;
	ModeSendOnPlayerItemConsumed_func ModeSendOnPlayerItemConsumedFunc;

	// map
	MapOnMobCreate_func OnMobCreateFunc;
	MapOnMobSpawned_func OnMobSpawnedFunc;
	MapOnMobKilled_func OnMobKilledFunc;
	MapCanSpawnMobs_func CanSpawnMobsFunc;
	MapGetSpawnPoints_func GetSpawnPointsFunc;
	MapConsiderMobSpawnPoint_func ConsiderMobSpawnPointFunc;
	MapOnPlayerGetRes_func OnPlayerGetResFunc;
	MapOnPlayerRevived_func OnPlayerRevivedFunc;
	MapCreateUpgradePickup_func CreateUpgradePickupFunc;
	MapPickupUpgradePickup_func PickupUpgradeFunc;
	MapCreateMobDrop_func CreateMobDropFunc;
	MapFrameTick_func OnFrameTickFunc;
	MapGetDifficultyMultiplier_func GetDifficultyMultiplierFunc;
	MapGetBoltMultiplier_func GetBoltMultiplierFunc;
	MapGetXpMultiplier_func GetXpMultiplierFunc;
	MapGetBoltRankMultiplier_func GetBoltRankMultiplierFunc;
	MapGetSpawnDistanceMultiplier_func GetSpawnDistanceMultiplierFunc;
	MapGetWeaponPickupCooldownMultiplier_func GetWeaponPickupCooldownMultiplierFunc;
	MapGetBakedSpawnPoints_func GetBakedSpawnPointsFunc;
	MapCanPrestigePlayerWeapon_func CanPrestigePlayerWeaponFunc;
	MapGetPrestigePlayerWeaponCost_func GetPrestigePlayerWeaponCostFunc;
	MapCanUpgradePlayerWeapon_func CanUpgradePlayerWeaponFunc;
	MapGetUpgradePlayerWeaponCost_func GetUpgradePlayerWeaponCostFunc;
	MapGetXpForNextToken_func GetXpForNextTokenFunc;
	MapGetCurrentDifficulty_func GetCurrentDifficultyFunc;
	MapGetDropItemOnMobKilled_func GetDropItemOnMobKilledFunc;
	MapGetRoundTransitionTime_func GetRoundTransitionTimeFunc;
	MapGetPlayerItemCount_func GetPlayerItemCountFunc;
	MapOnPlayerItemAcquired_func GetOnPlayerItemAcquiredFunc;
	MapOnPlayerItemConsumed_func GetOnPlayerItemConsumedFunc;
	MapOnPlayerUpdate_func OnPlayerUpdateFunc;
	MapOnPlayerDied_func OnPlayerDiedFunc;
	MapOnPlayerGetVendorReward_func OnPlayerGetVendorRewardFunc;
	MapOnBeforeDamageMob_func OnBeforeDamageMobFunc;

	// creates extra empty function slots
	// so that when adding new ones, old maps at least have a nullptr
	// instead of garbage pointing to a random, invalid piece of memory
	void *PlaceholderForFutureUseFuncs[16];
};

#endif // SURVIVAL_INTEROP_H
