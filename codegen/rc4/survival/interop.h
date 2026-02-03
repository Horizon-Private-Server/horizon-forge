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

typedef void (*ModeUpgradePlayerWeapon_func)(int playerId, int weaponId, int giveAlphaMod);
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

typedef struct GuberMoby *(*ModeGetGuber_func)(Moby *moby);
typedef int (*ModeHandleGuberEvent_func)(Moby *moby, GuberEvent *event);

typedef void (*MapOnMobSpawned_func)(Moby *moby);
typedef int (*MapOnMobCreate_func)(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config);
typedef void (*MapOnMobKilled_func)(Moby *moby, int killedByPlayerId, int killedByWeaponId);
typedef int (*MapCanSpawnMobs_func)(void);
typedef int (*MapGetSpawnPoints_func)(int **outSpawnPointIndices);
typedef int (*MapConsiderMobSpawnPoint_func)(struct MobSpawnParams *mobSpawnParams, VECTOR position, float yaw, Player *targetPlayer);
typedef int (*MapOnPlayerGetRes_func)(Player *player, VECTOR outPos, VECTOR outRot, int firstRes);
typedef int (*MapOnPlayerRevived_func)(Player *player, Player *revivedByPlayer);
typedef int (*MapCreateUpgradePickup_func)(VECTOR position, VECTOR rotation, enum UpgradeType upgradeType);
typedef void (*MapPickupUpgradePickup_func)(Moby *moby, int pickedUpByPlayerId);
typedef int (*MapCreateMobDrop_func)(VECTOR position, enum DropType dropType, int destroyAtTime, int team);
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
typedef int (*MapGetDropTypeOnMobKilled_func)(Player *killedByPlayer, Moby *mob, int gadgetId);
typedef int (*MapGetRoundTransitionTime_func)(int round);
typedef int (*MapGetRandomAlphamodForPlayer_func)(Player *player, int gadgetIdOrEmpty);

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
	MapGetDropTypeOnMobKilled_func GetDropTypeOnMobKilledFunc;
	MapGetRoundTransitionTime_func GetRoundTransitionTimeFunc;
	MapGetRandomAlphamodForPlayer_func GetRandomAlphamodForPlayerFunc;
};

#endif // SURVIVAL_INTEROP_H
