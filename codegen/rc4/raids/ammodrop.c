/***************************************************
 * FILENAME :		ammodrop.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the ammo drops.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/moby.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/collision.h>
#include <libdl/random.h>
#include <libdl/weapon.h>
#include "utils.h"
#include "shared.h"
#include "ammodrop.h"
#include "game.h"
#include "skills.h"
#include "mobs/mob.h"

Moby* ammoDropMobyList[MAX_MOB_AMMO_DROPS];
int ammoDropMobyListRollingIndex = 0;
Moby* ammodropCurrentMobyInUpdate = NULL;
u32 ammoDropProgress = 0;
int ammoDropProgressInitialized = 0;

Moby* ammodropTryCreateAt(Moby* moby);

//--------------------------------------------------------------------------
int ammodropTargetGetGadgetMaxAmmo(GadgetBox* gbox, int gadgetId)
{
  int playerIdx = bankGetPlayerIdxFromGadgetBox(gbox);
  if (playerIdx < 0) return 0;

  Player* player = playerGetAll()[playerIdx];
  if (!playerIsValid(player)) return 0;
  if (!player->IsLocal) return 0; // local only

  return playerGetWeaponMaxAmmo(gbox, gadgetId);
}

//--------------------------------------------------------------------------
int ammodropGetTargetWeapon(Player* player)
{
  int i;
  int bestWeaponId = 0;
  int bestAmmo = 0;
  int bestMaxAmmo = 1;
  if (!skillsPlayerIsReady(player) || !player->IsLocal || playerIsDead(player)
      || player->Health <= 0 || !player->GadgetBox) return 0;

  // iterate player gadgets
  // prefer held weapon, otherwise find weapon with least ammo percentage
  for (i = 1; i < WEAPON_SLOT_COUNT; ++i) {
    int weaponId = weaponSlotToId(i);
    if (player->GadgetBox->Gadgets[weaponId].Level < 0) continue;

    int maxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, weaponId);
    int ammo = player->GadgetBox->Gadgets[weaponId].Ammo;
    if (maxAmmo <= 0 || ammo >= maxAmmo) continue;
    if (weaponId == player->WeaponHeldId) return weaponId;

    if (!bestWeaponId || ammo * bestMaxAmmo < bestAmmo * maxAmmo) {
      bestWeaponId = weaponId;
      bestAmmo = ammo;
      bestMaxAmmo = maxAmmo;
    }
  }

  return bestWeaponId;
}

//--------------------------------------------------------------------------
int ammodropSelectWeapon(int arg, int* weaponId)
{
  // check if called within our custom update loop
  if (!ammodropCurrentMobyInUpdate)
    return ((int (*)(int, int*))0x00500b20)(arg, weaponId);

  // set weapon id to 0 if no weapon needs ammo
  // game ignores the return value
  *weaponId = ammodropGetTargetWeapon(playerGetFromSlot(0));
  return *weaponId != 0;
}

//--------------------------------------------------------------------------
int ammodropCheckPickup(Moby* moby, int arg)
{
  if (ammodropCurrentMobyInUpdate) {

    // check ammo again so full players can pick this up after shooting
    int weaponId = ammodropGetTargetWeapon(playerGetFromSlot(0));
    if (!weaponId || !moby->PVar) return 0;

    // set weapon id before pickup check
    // game can skip weapon selection when the drop is below ground
    *(int*)(moby->PVar + 0x70) = weaponId;
  }

  return ((int (*)(Moby*, int))0x003ac938)(moby, arg);
}

//--------------------------------------------------------------------------
int ammodropGiveAmmo(GadgetBox* gbox, int weaponId, int amount)
{
  // check if called within our custom update loop
  if (!ammodropCurrentMobyInUpdate)
    return ((int (*)(GadgetBox*, int, int))0x006270e0)(gbox, weaponId, amount);

  Player* player = playerGetFromSlot(0);
  if (!skillsPlayerIsReady(player) || !player->IsLocal || player->GadgetBox != gbox) return 0;

  int maxAmmo = playerGetWeaponMaxAmmo(gbox, weaponId);
  if (maxAmmo <= 0) return 0;
  float factor = skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES);
  skillsGiveAmmo(player, weaponId, maxf(1, maxAmmo * AMMODROP_ENEMY_AMMO_FRACTION * factor));
  return 0;
}

//--------------------------------------------------------------------------
void ammodropUpdate(Moby* moby)
{
  // use custom pickup logic during this moby's update
  Moby* previous = ammodropCurrentMobyInUpdate;
  ammodropCurrentMobyInUpdate = moby;

  // keep spawned drops pickup-able
  if (moby->PVar) *(char*)(moby->PVar + 0x5C) = 1;

  // base update function
  ((void (*)(Moby*))0x003ac050)(moby);
  ammodropCurrentMobyInUpdate = previous;
}

//--------------------------------------------------------------------------
Moby* ammodropTryCreateAt(Moby* moby)
{
  if (!moby) return NULL;

  // find free slot
  int i;
  for (i = 0; i < MAX_MOB_AMMO_DROPS; ++i) {
    if (!ammoDropMobyList[i] || mobyIsDestroyed(ammoDropMobyList[i]) || ammoDropMobyList[i]->OClass != MOBY_ID_AMMO) break;
  }

  // use rolling index
  if (i >= MAX_MOB_AMMO_DROPS) {
    i = ammoDropMobyListRollingIndex;
  }

  // check old drop before spawning
  // spawn can reuse a destroyed moby's address
  Moby* previous = ammoDropMobyList[i];
  if (previous && (mobyIsDestroyed(previous) || previous->OClass != MOBY_ID_AMMO))
    previous = NULL;

  // spawn new drop before destroying old one
  // keep old drop if spawn fails
  Moby* ammoMoby = mobySpawn(MOBY_ID_AMMO, 0x100);
  if (!ammoMoby) return NULL;
  if (!ammoMoby->PVar) {
    mobyDestroy(ammoMoby);
    return NULL;
  }

  // destroy old ammo drop
  if (previous)
    mobyDestroy(previous);

  ammoDropMobyList[i] = ammoMoby;
  ammoDropMobyListRollingIndex = (i + 1) % MAX_MOB_AMMO_DROPS;

  // snap to ground
  VECTOR from = {0,0,1,0};
  VECTOR to = {0,0,-10,0};
  vector_copy(ammoMoby->Position, moby->Position);
  vector_add(from, moby->Position, from);
  vector_add(to, moby->Position, to);
  if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL)) {
    vector_copy(ammoMoby->Position, CollLine_Fix_GetHitPosition());
  }

  // update draw
  ammoMoby->Bangles |= 1;
  ammoMoby->DrawDist = 255;
  ammoMoby->UpdateDist = 255;
  ammoMoby->PUpdate = &ammodropUpdate;

  // configure it to be pickup-able
  *(float*)(ammoMoby->PVar + 0x2C) = 2; // radius?
  *(char*)(ammoMoby->PVar + 0x5C) = 1; // state
  return ammoMoby;
}

//--------------------------------------------------------------------------
void ammodropOnMobKilled(Moby* moby, int killedByPlayerId)
{
  if (!MapConfig.State || !moby || !moby->PVar
      || killedByPlayerId < 0 || killedByPlayerId >= GAME_MAX_PLAYERS)
    return;

  Player* recipient = playerGetFromSlot(0);
  Player* killedByPlayer = playerGetAll()[killedByPlayerId];
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // allow local kills and nearby teammate kills
  if (!killedByPlayer || !(killedByPlayer->IsLocal || pvars->MobVars.ClosestDistToLocal < (25*25)))
    return;
  
    if (!skillsPlayerIsReady(recipient) || !recipient->IsLocal
      || playerIsDead(recipient) || recipient->Health <= 0)
    return;

  float frequency = skillsGetAmmoDropChance(recipient, MapConfig.State->AmmoDropChance);
  
  // randomize starting progress once per map
  if (!ammoDropProgressInitialized) {
    ammoDropProgress = rand(AMMODROP_PROGRESS_PER_DROP);
    ammoDropProgressInitialized = 1;
  }

  // use integer progress to avoid float rounding
  u32 credit = (u32)(frequency * AMMODROP_PROGRESS_PER_DROP + 0.5f);
  if (ammoDropProgress <= 0xFFFFFFFFU - credit)
    ammoDropProgress += credit;
  else
    ammoDropProgress = 0xFFFFFFFFU;

  // create one drop and carry over remaining progress
  // only spend progress if spawn succeeds
  if (ammoDropProgress >= AMMODROP_PROGRESS_PER_DROP && ammodropTryCreateAt(moby))
    ammoDropProgress -= AMMODROP_PROGRESS_PER_DROP;
}

//--------------------------------------------------------------------------
void ammodropStart(void)
{
}

//--------------------------------------------------------------------------
void ammodropInit(void)
{
  memset(ammoDropMobyList, 0, sizeof(ammoDropMobyList));
  ammoDropMobyListRollingIndex = 0;
  ammodropCurrentMobyInUpdate = NULL;
  ammoDropProgress = 0;
  ammoDropProgressInitialized = 0;

  // force ammo drops to local players only
  POKE_U32(0x003acc24, 0x8E351A9C);
  HOOK_JAL(0x003aca8c, &ammodropTargetGetGadgetMaxAmmo);
  HOOK_JAL(0x003aca80, &ammodropSelectWeapon);
  HOOK_JAL(0x003ac250, &ammodropCheckPickup);
  HOOK_JAL(0x003ac378, &ammodropGiveAmmo);

  // set ammo amount for map pickups
  POKE_F32(0x003978C0, AMMODROP_AUTHORED_AMMO_FRACTION);

  // disable b6 halving of ammo amount
  POKE_U16(0x003ac2c4, 0x0);
}
