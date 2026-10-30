#ifndef RAIDS_INTEROP_H
#define RAIDS_INTEROP_H

#include <stddef.h>
#include <libdl/player.h>
#include <libdl/guber.h>

struct MobSpawnEventArgs;
struct MobCreateArgs;
struct MobDamageEventArgs;
enum MobDamageSource;

struct RaidsPlayerStats
{
  float Speed;
  float MaxHealth;
};

// Local calculation only. Damage is quantized once, when the event is sent.
struct RaidsPlayerDamageContext
{
  Player* Attacker;
  Moby* Source;
  Moby* Target;
  int WeaponId;
  int DamageSource; // enum MobDamageSource
  float Damage;
  float CritChance;
  float CritMultiplier;
  int IsCritical;
  u32 DamageFlags;
};

typedef void (*ModePushSnack_func)(char* string, int ticksAlive, int localPlayerIdx);
typedef void (*ModePushDamageBubble_func)(VECTOR position, float randomRadius, float damage, int isLocal, int team);
typedef long (*ModeGetAmmoRefillCost_func)(Player* player);
typedef void (*ModeBeginWorldHop_func)(char* mapFilename, int difficulty, int cost, int delayMs);
typedef void (*ModePopulateSpawnArgs_func)(struct MobSpawnEventArgs* output, struct MobCreateArgs* args);
typedef void (*ModeRegisterNpc_func)(Moby* moby);
typedef int (*ModeOnGuberEvent_func)(Moby* moby, GuberEvent* event);
typedef struct Guber* (*ModeOnGetGuber_func)(Moby* moby);
typedef int (*ModeTryCreateMob_func)(struct MobCreateArgs* args);
typedef void (*ModeRequestPrestigeLoot_func)(int gadgetId);
typedef void (*ModeOnMissionComplete_func)(int cuboidIdx);
typedef void (*ModeOnMissionFail_func)(void);
typedef void (*ModeProcessPlayerDamage_func)(Moby* target, Moby* source, float* damage, u32* damageFlags);
typedef void (*ModeDrawHud_func)(void);

typedef int (*MapOnMobCreate_func)(struct MobCreateArgs* args);
typedef void (*MapOnMobSpawned_func)(Moby* moby);
typedef void (*MapOnMobUpdate_func)(Moby* moby);
typedef void (*MapOnMobDamaged_func)(Moby* moby, struct MobDamageEventArgs* args);
typedef void (*MapOnMobKilled_func)(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
typedef void (*MapOnMobDestroyed_func)(Moby* moby);
typedef void (*MapCreateAmmoDropAt_func)(Moby* moby);
typedef void (*MapFrameTick_func)(void);
typedef void (*MapModifyPlayerStats_func)(Player* player, struct RaidsPlayerStats* stats);
typedef void (*MapOnPlayerUpdate_func)(Player* player);
typedef void (*MapOnPlayerDied_func)(Player* player);
typedef void (*MapOnPlayerRevived_func)(Player* player);
typedef void (*MapModifyPlayerCritChance_func)(struct RaidsPlayerDamageContext* context);
typedef void (*MapModifyPlayerCritDamage_func)(struct RaidsPlayerDamageContext* context);
typedef void (*MapFinalizePlayerDamage_func)(struct RaidsPlayerDamageContext* context);
typedef void (*MapMobAfterDamage_func)(Moby* moby, struct MobDamageEventArgs* args, float appliedDamage);
typedef float (*MapGetAmmoDropChance_func)(Player* player, Moby* moby, float baseChance);
typedef void (*MapMobBlowCorn_func)(Moby* moby, int bangles);

struct RaidsInteropTable
{
  // mode: populated by the shared patch
  ModePushSnack_func ModePushSnackFunc;
  ModePushDamageBubble_func ModePushDamageBubbleFunc;
  ModeGetAmmoRefillCost_func ModeGetAmmoRefillCostFunc;
  ModeBeginWorldHop_func ModeBeginWorldHopFunc;
  ModePopulateSpawnArgs_func ModePopulateSpawnArgsFunc;
  ModeRegisterNpc_func ModeRegisterNpcFunc;
  ModeOnGuberEvent_func ModeOnGuberEventFunc;
  ModeOnGetGuber_func ModeOnGetGuberFunc;
  ModeTryCreateMob_func ModeTryCreateMobFunc;
  ModeRequestPrestigeLoot_func ModeRequestPrestigeLootFunc;
  ModeOnMissionComplete_func ModeOnMissionCompleteFunc;
  ModeOnMissionFail_func ModeOnMissionFailFunc;
  ModeProcessPlayerDamage_func ModeProcessPlayerDamageFunc;
  ModeDrawHud_func ModeDrawHudFunc;

  // map: populated by the loaded map
  MapOnMobCreate_func OnMobCreateFunc;
  MapOnMobSpawned_func OnMobSpawnedFunc;
  MapOnMobUpdate_func OnMobUpdateFunc;
  MapOnMobDamaged_func OnMobDamagedFunc;
  MapOnMobKilled_func OnMobKilledFunc;
  MapOnMobDestroyed_func OnMobDestroyedFunc;
  MapCreateAmmoDropAt_func CreateAmmoDropAtFunc;
  MapFrameTick_func OnFrameTickFunc;
  MapModifyPlayerStats_func ModifyPlayerStatsFunc;
  MapOnPlayerUpdate_func OnPlayerUpdateFunc;
  MapOnPlayerDied_func OnPlayerDiedFunc;
  MapOnPlayerRevived_func OnPlayerRevivedFunc;
  MapModifyPlayerCritChance_func ModifyPlayerCritChanceFunc;
  MapModifyPlayerCritDamage_func ModifyPlayerCritDamageFunc;
  MapFinalizePlayerDamage_func FinalizePlayerDamageFunc;
  MapMobAfterDamage_func MobAfterDamageFunc;
  MapGetAmmoDropChance_func GetAmmoDropChanceFunc;
  MapMobBlowCorn_func MobBlowCornFunc;

  // Reserved for future callbacks.
  void* PlaceholderForFutureUseFuncs[15];
};

typedef char RaidsInteropPointerSizeCheck[sizeof(void*) == 4 ? 1 : -1];
typedef char RaidsInteropSizeCheck[sizeof(struct RaidsInteropTable) == 188 ? 1 : -1];
typedef char RaidsInteropMapOffsetCheck[offsetof(struct RaidsInteropTable, OnMobCreateFunc) == 56 ? 1 : -1];
typedef char RaidsInteropAmmoDropOffsetCheck[offsetof(struct RaidsInteropTable, GetAmmoDropChanceFunc) == 120 ? 1 : -1];
typedef char RaidsInteropPlayerDamageOffsetCheck[offsetof(struct RaidsInteropTable, ModeProcessPlayerDamageFunc) == 48 ? 1 : -1];
typedef char RaidsInteropDrawHudOffsetCheck[offsetof(struct RaidsInteropTable, ModeDrawHudFunc) == 52 ? 1 : -1];
typedef char RaidsInteropMobBlowCornOffsetCheck[offsetof(struct RaidsInteropTable, MobBlowCornFunc) == 124 ? 1 : -1];
typedef char RaidsInteropReservedOffsetCheck[offsetof(struct RaidsInteropTable, PlaceholderForFutureUseFuncs) == 128 ? 1 : -1];

void mapDrawHud(void);
void interopInit(void);
void interopCleanup(void);

#endif // RAIDS_INTEROP_H
