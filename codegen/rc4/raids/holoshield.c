#include <float.h>
#include <stddef.h>
#include <libdl/camera.h>
#include <libdl/collision.h>
#include <libdl/game.h>
#include <libdl/gamesettings.h>
#include <libdl/graphics.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/player.h>
#include <libdl/string.h>
#include <libdl/utils.h>
#include <libdl/weapon.h>
#include "common.h"
#include "game.h"
#include "holoshield.h"
#include "laserbeam.h"
#include "npc.h"
#include "skills.h"
#include "utils.h"

#define HOLOSHIELD_NATIVE_PVAR_SIZE (0x80)
#define HOLOSHIELD_VEHICLE_MOB_SHOT_NATIVE_PVAR_SIZE (0xC0)
#define HOLOSHIELD_BAR_DISTANCE (25.0f)

// The native prefix is unchanged. All additional state is local to this client.
struct HoloshieldPVar
{
  u8 Native[HOLOSHIELD_NATIVE_PVAR_SIZE];
  float Capacity;
  float Remaining;
  int OwnerId;
  int OwnerAccountId;
  int NextVisibilityCheckTime;
  u8 Initialized;
  u8 Visible;
};

struct HoloshieldVehicleMobShotPVar
{
  u8 Native[HOLOSHIELD_VEHICLE_MOB_SHOT_NATIVE_PVAR_SIZE];
  int HitShield;
};

typedef char HoloshieldNativePrefixCheck[offsetof(struct HoloshieldPVar, Capacity) == 0x80 ? 1 : -1];
typedef char HoloshieldVehicleMobShotNativePrefixCheck[offsetof(struct HoloshieldVehicleMobShotPVar, HitShield) == 0xC0 ? 1 : -1];

int holoshieldCreatingEnemyVehicleMobShot;
Moby* holoshieldCurrentVehicleMobShot;

void holoshieldUpdate(Moby* moby);
void holoshieldVehicleMobShotUpdate(Moby* moby);

//--------------------------------------------------------------------------
float holoshieldGetCapacity(RaidsInventoryItem_t* item, int impactBonus)
{
  if (!item || item->Type != RAIDS_ITEM_WEAPON || item->WeaponData.GadgetId != WEAPON_ID_OMNI_SHIELD)
    return 0;

  int impact = item->WeaponData.AlphaModCounts[ALPHA_MOD_IMPACT - 1] + impactBonus;
  float capacity = bankGetWeaponDamage(item) * HOLOSHIELD_ABSORPTION_MULTIPLIER
    * (1 + HOLOSHIELD_IMPACT_CAPACITY_FACTOR * maxf(0, impact));
  return capacity > 0 && capacity <= FLT_MAX ? capacity : 0;
}

//--------------------------------------------------------------------------
// Return actual absorbed damage for XP, excluding overkill.
float holoshieldConsumeCapacity(float* remaining, float damage)
{
  if (!remaining || !(*remaining > 0 && *remaining <= FLT_MAX)
      || !(damage > 0 && damage <= FLT_MAX))
    return 0;

  float credited = minf(damage, *remaining);
  *remaining -= credited;
  return credited;
}

//--------------------------------------------------------------------------
int holoshieldIsEnemySource(Moby* source)
{
  // A shared projectile OClass or missing player attribution does not imply enemy fire.
  int i;
  for (i = 0; source && i < 8; ++i, source = source->PParent) {
    if (mobyIsDestroyed(source)) return 0;
    if (mobyIsNpc(source))
      return source->PVar && npcGetPVars(source)->Parameters.IsOnEnemyTeam;
    if (mobyIsMob(source)) return 1;
  }
  return 0;
}

//--------------------------------------------------------------------------
struct HoloshieldPVar* holoshieldGetVars(Moby* moby)
{
  if (!moby || mobyIsDestroyed(moby) || moby->OClass != MOBY_ID_HOLOSHIELD_SHOT
      || moby->PUpdate != &holoshieldUpdate || !moby->PVar)
    return NULL;

  return (struct HoloshieldPVar*)moby->PVar;
}

//--------------------------------------------------------------------------
int holoshieldIsBlocking(Moby* moby)
{
  // Growing shields already have a collider; state 7 starts the deployed timer.
  return moby->State == 2 || moby->State == 3 || moby->State == 4 || moby->State == 7;
}

//--------------------------------------------------------------------------
void holoshieldInitialize(struct HoloshieldPVar* pvars)
{
  if (pvars->Initialized || !MapConfig.State) return;

  int ownerId = pvars->Native[0x16];
  if (ownerId < 0 || ownerId >= GAME_MAX_PLAYERS) return;
  Player* owner = playerGetFromIndex(ownerId);
  GameSettings* gs = gameGetSettings();
  if (!playerIsValid(owner) || !owner->GadgetBox || !gs) return;
  if (owner->IsLocal && !bankGetHasEquippedInventory()) return;

  RaidsInventoryItem_t* item = bankGetEquippedWeaponFromGadgetBox(owner->GadgetBox, WEAPON_ID_OMNI_SHIELD);
  float capacity = holoshieldGetCapacity(item, skillsGetAlphaModBonus(owner, ALPHA_MOD_IMPACT));
  if (!(capacity > 0)) return; // Wait for replicated equipment rather than inventing remote stats.

  pvars->Capacity = pvars->Remaining = capacity;
  pvars->OwnerId = ownerId;
  pvars->OwnerAccountId = gs->PlayerAccountIds[ownerId];
  pvars->NextVisibilityCheckTime = gameGetTime();
  pvars->Initialized = 1;
}

//--------------------------------------------------------------------------
void holoshieldRemoveFromCache(Moby* moby, int ownerId)
{
  Moby** cache = (Moby**)0x003360E0 + ownerId * 4;
  int i, count = 0;
  for (i = 0; i < 4; ++i) {
    if (cache[i] && cache[i] != moby)
      cache[count++] = cache[i];
  }
  while (count < 4) cache[count++] = NULL;
}

//--------------------------------------------------------------------------
void holoshieldDamage(Moby* shield, float damage)
{
  struct HoloshieldPVar* pvars = holoshieldGetVars(shield);
  if (!pvars || !holoshieldIsBlocking(shield)) return;
  holoshieldInitialize(pvars);
  if (!pvars->Initialized) return;

  float credited = holoshieldConsumeCapacity(&pvars->Remaining, damage);
  if (!(credited > 0)) return;

  // The final projectile is still blocked, but the next collision must not enable this shield.
  if (pvars->Remaining <= 0) {
    holoshieldRemoveFromCache(shield, pvars->OwnerId);
    shield->CollData = NULL;
    mobySetState(shield, 5, -1); // Native 20-tick collapse, including light/sound cleanup.
  }

  Player* owner = playerGetFromIndex(pvars->OwnerId);
  GameSettings* gs = gameGetSettings();
  if (playerIsValid(owner) && owner->IsLocal && gs && bankGetHasAccount()
      && pvars->OwnerAccountId > 0 && gs->PlayerAccountIds[pvars->OwnerId] == pvars->OwnerAccountId)
    bankAddWeaponXP((double)credited * HOLOSHIELD_XP_WEIGHT, WEAPON_ID_OMNI_SHIELD);
}

//--------------------------------------------------------------------------
u32 holoshieldGetBarColor(struct HoloshieldPVar* pvars, u32 color)
{
  float health = pvars->Remaining / pvars->Capacity;
  if (health <= 0.25f) return (color & 0xFF000000) | 0x003030FF;
  if (health <= 0.50f) return (color & 0xFF000000) | 0x0000A5FF;
  return color;
}

//--------------------------------------------------------------------------
void holoshieldDrawBar(Moby* moby)
{
  struct HoloshieldPVar* pvars = holoshieldGetVars(moby);
  GameCamera* camera = cameraGetGameCamera(0);
  if (!pvars || !pvars->Initialized || !camera || !holoshieldIsBlocking(moby)) return;

  VECTOR delta, position;
  vector_subtract(delta, moby->Position, camera->pos);
  if (vector_sqrmag(delta) > HOLOSHIELD_BAR_DISTANCE * HOLOSHIELD_BAR_DISTANCE) return;
  vector_add(position, moby->Position, moby->M2_03);
  position[2] += 0.3f;
  int x, y;
  if (!gfxWorldSpaceToScreenSpace(position, &x, &y)) return;

  // Bound the additional raycasts when several players deploy shields together.
  int now = gameGetTime();
  if ((s32)((u32)now - (u32)pvars->NextVisibilityCheckTime) >= 0) {
    pvars->Visible = !CollLine_Fix(camera->pos, moby->Position, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL);
    pvars->NextVisibilityCheckTime = now + TIME_SECOND / 8;
  }
  if (!pvars->Visible) return;

  float health = clamp(pvars->Remaining / pvars->Capacity, 0, 1);
  u32 color = holoshieldGetBarColor(pvars, moby->GlowRGBA | 0x80000000);
  // gfxResetGsRegisters();
  gfxHelperDrawBox_WS(position, 52, 7, 0x80101010, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox_WS(position, 50, 5, 0x80000000, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox_WS(position, 50 * health, 5, color, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void holoshieldUpdate(Moby* moby)
{
  struct HoloshieldPVar* pvars = holoshieldGetVars(moby);
  if (!pvars) return;
  holoshieldInitialize(pvars);

  ((void (*)(Moby*))0x004002B8)(moby);
}

//--------------------------------------------------------------------------
void holoshieldDraw(void)
{
  if (!MapConfig.State || MapConfig.State->MenuOpen != RAIDS_CUSTOM_MENU_NONE || shouldCloseMenu()) return;

  // Draw 2D bars in the HUD pass, never between native transparent FX draws.
  // The 2D helpers change GS state without updating the FX renderer's state cache.
  Moby** cache = (Moby**)0x003360E0;
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS * 4; ++i) {
    if (cache[i]) holoshieldDrawBar(cache[i]);
  }
}

//--------------------------------------------------------------------------
Moby* holoshieldAllocate(int oclass, int pvarSize)
{
  Moby* moby = mobySpawn(oclass, sizeof(struct HoloshieldPVar));
  if (moby) {
    memset((u8*)moby->PVar + HOLOSHIELD_NATIVE_PVAR_SIZE, 0, sizeof(struct HoloshieldPVar) - HOLOSHIELD_NATIVE_PVAR_SIZE);
    moby->PUpdate = &holoshieldUpdate;
  }
  return moby;
}

//--------------------------------------------------------------------------
// Only the shared vehicle/mob shot, MOBY_ID_MAGMA_ROCK_VEHICLE_SHOTS (0x1B35).
// Keep mobySpawn's arguments to match the native allocation call being hooked.
Moby* holoshieldAllocateVehicleMobShot(int oclass, int pvarSize)
{
  if (!holoshieldCreatingEnemyVehicleMobShot) return mobySpawn(oclass, pvarSize);

  Moby* moby = mobySpawn(oclass, sizeof(struct HoloshieldVehicleMobShotPVar));
  if (moby) {
    ((struct HoloshieldVehicleMobShotPVar*)moby->PVar)->HitShield = 0;
    moby->PUpdate = &holoshieldVehicleMobShotUpdate;
  }
  return moby;
}

//--------------------------------------------------------------------------
Moby* holoshieldSpawnEnemyVehicleMobShot(float trailLength, float damage, VECTOR position, VECTOR velocity, Moby* creator, Moby* source)
{
  int previous = holoshieldCreatingEnemyVehicleMobShot;
  holoshieldCreatingEnemyVehicleMobShot = holoshieldIsEnemySource(source);
  Moby* moby = ((Moby* (*)(float, float, VECTOR, VECTOR, Moby*, int, int, int, int))0x0045D598)
    (trailLength, damage, position, velocity, creator, 1, 0x222124, -1, 0);
  holoshieldCreatingEnemyVehicleMobShot = previous;
  return moby;
}

//--------------------------------------------------------------------------
void holoshieldVehicleMobShotUpdate(Moby* moby)
{
  Moby* previous = holoshieldCurrentVehicleMobShot;
  holoshieldCurrentVehicleMobShot = moby;
  ((void (*)(Moby*))0x0045D7C0)(moby);
  holoshieldCurrentVehicleMobShot = previous;
}

//--------------------------------------------------------------------------
int holoshieldVehicleMobShotCollision(VECTOR from, VECTOR to, u64 flags, Moby* source, MobyColDamageIn* damage)
{
  int hit = CollLine_Fix(from, to, flags, source, damage);
  Moby* shot = holoshieldCurrentVehicleMobShot;
  if (hit && damage && shot && shot->PUpdate == &holoshieldVehicleMobShotUpdate) {
    struct HoloshieldVehicleMobShotPVar* pvars = (struct HoloshieldVehicleMobShotPVar*)shot->PVar;
    Moby* shield = CollLine_Fix_GetHitMoby();
    if (!pvars->HitShield && holoshieldGetVars(shield)) {
      pvars->HitShield = 1;
      holoshieldDamage(shield, damage->DamageHp);
    }
  }
  return hit;
}

//--------------------------------------------------------------------------
// Called once per Leviathan simulation update, never from a draw callback.
void holoshieldDamageBeam(Moby* beam)
{
  if (!beam || !beam->PVar || !holoshieldIsEnemySource(beam->PParent)) return;
  struct LaserbeamPVar* pvars = (struct LaserbeamPVar*)beam->PVar;
  if (!(pvars->Damage > 0)) return;

  VECTOR to;
  vector_scale(to, pvars->Direction, pvars->MaxLength);
  vector_add(to, to, beam->Position);
  weaponTurnOnHoloshields(-1);
  int hit = CollLine_Fix(beam->Position, to, COLLISION_FLAG_IGNORE_NONE, beam->PParent, NULL);
  Moby* shield = hit ? CollLine_Fix_GetHitMoby() : NULL;
  weaponTurnOffHoloshields();
  if (shield) holoshieldDamage(shield, pvars->Damage * MATH_DT);
}

//--------------------------------------------------------------------------
void holoshieldInit(void)
{
  // All three sites belong to the map overlay and reset when the map unloads.
  HOOK_JAL(0x003FF1DC, &holoshieldAllocate);
  HOOK_JAL(0x0045D5EC, &holoshieldAllocateVehicleMobShot);
  HOOK_JAL(0x0045DB98, &holoshieldVehicleMobShotCollision);
}
