#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/player.h>
#include <libdl/vehicle.h>
#include "game.h"
#include "interop.h"
#include "skills.h"
#include "shared.h"
#include "ammodrop.h"
#include "utils.h"
#include "spawner.h"
#include "badges.h"
#include "powerups.h"
#include "mobs/mob.h"

#define WEAPON_MOD_LIGHTFOOT_FACTOR (0.25)

struct RaidsMapData mapData;

extern Moby* mapBossMoby;

//--------------------------------------------------------------------------
void mapDrawHud(void)
{
  if (MapConfig.State && MapConfig.Functions.ModeDrawHudFunc)
    MapConfig.Functions.ModeDrawHudFunc();
}

//--------------------------------------------------------------------------
void mapOnVehicleReinit(void)
{
  Vehicle* vehicle;

	// pointer to vehicle is stored in $s1
  asm volatile ("move %0, $s1" : "=r" (vehicle));
  skillsOnVehicleInitialized(vehicle);
}

//--------------------------------------------------------------------------
void mapOnHealthboxHeal(Player* player, float amount)
{
  skillsOnHealthboxHeal(player, amount);
}

//--------------------------------------------------------------------------
int mapCreateMob(struct MobCreateArgs* args)
{
  if (args->SpawnParamsIdx < 0 || args->SpawnParamsIdx >= MapConfig.MobSpawnParamsCount) {
    DPRINTF("unhandled create spawnParamsIdx %d\n", args->SpawnParamsIdx);
    return 0;
  }

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  if (spawnParams->MobCreate)
    return spawnParams->MobCreate(args);

  DPRINTF("unhandled create spawnParamsIdx %d\n", args->SpawnParamsIdx);
  return 0;
}

//--------------------------------------------------------------------------
void mapOnMobUpdate(Moby* moby)
{
  if (!moby || !moby->PVar) return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (mapBossMoby == moby && (pvars->MobVars.Health <= 0 || pvars->MobVars.Respawn))
    mapBossMoby = NULL;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobUpdate(moby->PParent, moby, pvars->MobVars.Userdata);
  }
}

//--------------------------------------------------------------------------
void mapOnMobDamaged(Moby* moby, struct MobDamageEventArgs* args)
{
  if (!moby || !moby->PVar) return;
}

//--------------------------------------------------------------------------
void mapOnMobKilled(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar) return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int spawnParamsIdx = pvars->MobVars.SpawnParamsIdx;
  if (killedByPlayerId >= 0 && killedByPlayerId < GAME_MAX_PLAYERS
      && source > MOB_DAMAGE_SOURCE_UNKNOWN && source < MOB_DAMAGE_SOURCE_COUNT
      && spawnParamsIdx >= 0 && spawnParamsIdx < MAX_MOB_SPAWN_PARAMS)
    ++MapConfig.MapData->AllKills[killedByPlayerId][source-1][spawnParamsIdx];

  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobKilled(moby->PParent, moby, pvars->MobVars.Userdata, killedByPlayerId, source);
  }

  ammodropOnMobKilled(moby, killedByPlayerId);
  skillsOnMobKilled(moby, killedByPlayerId);

  if (killedByPlayerId >= 0 && killedByPlayerId < GAME_MAX_PLAYERS) {
    Player* killedByPlayer = playerGetAll()[killedByPlayerId];
    int weaponId = getWeaponIdFromDamageSource(source);
    int modStrength = bankGetEquippedWeaponModRarity(killedByPlayerId, weaponId, RAIDS_WEAPON_MOD_WILL_O_WISP) + 1;
    if (killedByPlayer && modStrength > 0) {
      u32 damageFlags = mobAmIOwner(moby) ? 0x00081801 : 0;
      float radius = modStrength * BADGES_EXPLODINGENEMIES_RADIUS_MULT;
      float damage = pvars->MobVars.LastHitByDamage * modStrength * BADGES_EXPLODINGENEMIES_DAMAGE_MULT;
      spawnExplosionDamage(moby->Position, radius, 0x800000C0, killedByPlayer->PlayerMoby, damage, damageFlags);
    }
  }
}

//--------------------------------------------------------------------------
void mapOnMobDestroyed(Moby* moby)
{
  if (mapBossMoby == moby)
    mapBossMoby = NULL;
  if (!moby || !moby->PVar) return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobDestroyed(moby->PParent, moby, pvars->MobVars.Userdata);
  }
}

//--------------------------------------------------------------------------
void mapOnMobSpawned(Moby* moby)
{
  if (!moby || !moby->PVar) return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobSpawned(moby->PParent, moby, pvars->MobVars.Userdata);
  }
}

//--------------------------------------------------------------------------
void mapMobAfterDamage(Moby* moby, struct MobDamageEventArgs* args, float appliedDamage)
{
  powerupsOnMobDamaged(moby, args, appliedDamage);
}

//--------------------------------------------------------------------------
void mapModifyPlayerStats(Player* player, struct RaidsPlayerStats* stats)
{
  if (!playerIsValid(player) || !stats) return;

  int lightfootStrength = bankGetEquippedWeaponModRarity(player->PlayerId, player->WeaponHeldId, RAIDS_WEAPON_MOD_LIGHTFOOT) + 1;
  stats->Speed *= 1 + (WEAPON_MOD_LIGHTFOOT_FACTOR * lightfootStrength);
  skillsModifyPlayerStats(player, stats);
}

//--------------------------------------------------------------------------
void mapOnPlayerUpdate(Player* player)
{
  skillsOnPlayerUpdate(player);
}

//--------------------------------------------------------------------------
void mapOnPlayerDied(Player* player)
{
  skillsResetPlayer(player);
}

//--------------------------------------------------------------------------
void mapOnPlayerRevived(Player* player)
{
  skillsResetPlayer(player);
}

//--------------------------------------------------------------------------
void mapModifyPlayerCritChance(struct RaidsPlayerDamageContext* context)
{
  skillsModifyPlayerCritChance(context);
}

//--------------------------------------------------------------------------
void mapModifyPlayerCritDamage(struct RaidsPlayerDamageContext* context)
{
  skillsModifyPlayerCritDamage(context);
}

//--------------------------------------------------------------------------
void mapFinalizePlayerDamage(struct RaidsPlayerDamageContext* context)
{
  skillsFinalizePlayerDamage(context);
}

//--------------------------------------------------------------------------
void interopInit(void)
{
  MapConfig.MapData = &mapData;
  memset(MapConfig.MapData, 0, sizeof(*MapConfig.MapData));

  HOOK_J(0x004546EC, &mapOnVehicleReinit); // puma
  HOOK_J(0x00465244, &mapOnVehicleReinit); // hoverbike
  HOOK_J(0x0047DA18, &mapOnVehicleReinit); // landstalker
  HOOK_J(0x0046ED14, &mapOnVehicleReinit); // hovership
  HOOK_JAL(0x004130F4, &mapOnHealthboxHeal);

  MapConfig.Functions.OnMobCreateFunc = &mapCreateMob;
  MapConfig.Functions.OnMobSpawnedFunc = &mapOnMobSpawned;
  MapConfig.Functions.OnMobUpdateFunc = &mapOnMobUpdate;
  MapConfig.Functions.OnMobDamagedFunc = &mapOnMobDamaged;
  MapConfig.Functions.OnMobKilledFunc = &mapOnMobKilled;
  MapConfig.Functions.OnMobDestroyedFunc = &mapOnMobDestroyed;
  MapConfig.Functions.ModifyPlayerStatsFunc = &mapModifyPlayerStats;
  MapConfig.Functions.OnPlayerUpdateFunc = &mapOnPlayerUpdate;
  MapConfig.Functions.OnPlayerDiedFunc = &mapOnPlayerDied;
  MapConfig.Functions.OnPlayerRevivedFunc = &mapOnPlayerRevived;
  MapConfig.Functions.ModifyPlayerCritChanceFunc = &mapModifyPlayerCritChance;
  MapConfig.Functions.ModifyPlayerCritDamageFunc = &mapModifyPlayerCritDamage;
  MapConfig.Functions.FinalizePlayerDamageFunc = &mapFinalizePlayerDamage;
  MapConfig.Functions.MobAfterDamageFunc = &mapMobAfterDamage;
  MapConfig.Functions.MobBlowCornFunc = &mobBlowCorn;
}

//--------------------------------------------------------------------------
void interopCleanup(void)
{
  // Only clear map callbacks. Mode callbacks belong to the shared runtime.
  u32 start = offsetof(struct RaidsInteropTable, OnMobCreateFunc);
  u32 end = offsetof(struct RaidsInteropTable, PlaceholderForFutureUseFuncs);
  memset((char*)&MapConfig.Functions + start, 0, end - start);
}
