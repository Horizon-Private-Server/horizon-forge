/***************************************************
 * FILENAME :		badges.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the badges.
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
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "badges.h"
#include "game.h"

int badgesPlayerCooldown[GAME_MAX_PLAYERS] = {0};
int badgesPlayerTimeLastLastHit[GAME_MAX_PLAYERS] = {0};
int badgesAmmoRegenAmount[WEAPON_SLOT_COUNT] = {
  [WEAPON_SLOT_VIPERS] 5,
  [WEAPON_SLOT_MAGMA_CANNON] 2,
  [WEAPON_SLOT_ARBITER] 1,
  [WEAPON_SLOT_FUSION_RIFLE] 1,
  [WEAPON_SLOT_MINE_LAUNCHER] 1,
  [WEAPON_SLOT_B6] 1,
  [WEAPON_SLOT_OMNI_SHIELD] 1,
  [WEAPON_SLOT_FLAIL] 2,
};
int badgesFlinchResistanceAmount[RAIDS_ITEM_RARITY_COUNT] = {
  [RAIDS_ITEM_RARITY_COMMON] 10,
  [RAIDS_ITEM_RARITY_UNCOMMON] 25,
  [RAIDS_ITEM_RARITY_RARE] 40,
  [RAIDS_ITEM_RARITY_LEGENDARY] 70,
  [RAIDS_ITEM_RARITY_MYTHIC] 100
};
float badgesBerserkerMeleeMult[RAIDS_ITEM_RARITY_COUNT] = {
  [RAIDS_ITEM_RARITY_COMMON] 0.75,
  [RAIDS_ITEM_RARITY_UNCOMMON] 0.60,
  [RAIDS_ITEM_RARITY_RARE] 0.50,
  [RAIDS_ITEM_RARITY_LEGENDARY] 0.40,
  [RAIDS_ITEM_RARITY_MYTHIC] 0.20
};

//--------------------------------------------------------------------------
void badgesOnPlayerGetHit(Player* player, int stateId, int a2, int a3, int t0) {
  PlayerVTable* vtable = playerGetVTable(player);
  if (!vtable) return;

  // 
  if (stateId == PLAYER_STATE_GET_HIT) {
    RaidsInventoryItem_t* badge = &MapConfig.State->PlayerStates[player->PlayerId].Inventory.Badge;
    if (bankItemIsBadge(badge) && badge->BadgeType == RAIDS_BADGE_TYPE_EXPLOSIVE_WRENCH) {
      if (player->PlayerState == PLAYER_STATE_JUMP_ATTACK && player->PlayerMoby->AnimSeqId == 43) {
        return;
      }
    }
  }

  vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void badgesUpdate_HealthRegen(Player* player, int badgeLevel)
{
  if (playerIsDead(player) || player->Health <= 0) return;
  
  int delayMs = (TIME_SECOND * 5);
  int timeSinceLastHitMs = gameGetTime() - (badgesPlayerTimeLastLastHit[player->PlayerId] + delayMs);
  if (timeSinceLastHitMs < 0) return;

  timeSinceLastHitMs *= 1 + (0.5 * badgeLevel);
  int cooldown = BADGES_HEALTH_REGEN_COOLDOWN_TICKS;
  if (timeSinceLastHitMs < (TIME_SECOND * 15))
    cooldown *= 5;
  else if (timeSinceLastHitMs < (TIME_SECOND * 30))
    cooldown *= 3;
  else if (timeSinceLastHitMs < (TIME_SECOND * 60))
    cooldown *= 2;

  float newHealth = clamp(player->Health + BADGES_HEALTH_REGEN_AMOUNT, 0, player->MaxHealth);
  if (newHealth != player->Health) {
    playerSetHealth(player, newHealth);
    //mobyPlaySoundByClass(1, 0, player->PlayerMoby, MOBY_ID_HEALTH_BOX_MULT);
    badgesPlayerCooldown[player->PlayerId] = cooldown;
  }
}

//--------------------------------------------------------------------------
void badgesUpdate_AmmoRegen(Player* player, int badgeLevel)
{
  if (playerIsDead(player)) return;

  int equippedGadgetId = player->WeaponHeldId;
  int gadgetSlotId = weaponIdToSlot(equippedGadgetId);
  if (!gadgetSlotId) return;

  if (equippedGadgetId > 0) {
    int equippedGadgetMaxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, equippedGadgetId);
    if (equippedGadgetMaxAmmo) {
      int equippedGadgetAmmo = player->GadgetBox->Gadgets[equippedGadgetId].Ammo;
      float newAmmo = equippedGadgetAmmo + badgesAmmoRegenAmount[gadgetSlotId]*(badgeLevel+1);
      if (newAmmo > equippedGadgetMaxAmmo) newAmmo = equippedGadgetMaxAmmo;
      if (newAmmo != equippedGadgetAmmo) {
        player->GadgetBox->Gadgets[equippedGadgetId].Ammo = newAmmo;
        badgesPlayerCooldown[player->PlayerId] = BADGES_AMMO_REGEN_COOLDOWN_TICKS;
      }
    }
  }
}

//--------------------------------------------------------------------------
void badgesUpdate_FlinchResistance(Player* player, int badgeLevel)
{
  if (playerIsDead(player)) return;

  if (player->timers.postHitInvinc == 47) {
    player->timers.postHitInvinc += badgesFlinchResistanceAmount[badgeLevel];
    badgesPlayerCooldown[player->PlayerId] = player->timers.postHitInvinc - 1;
  }
}

//--------------------------------------------------------------------------
void badgesUpdate_Berserker(Player* player, int badgeLevel)
{
  if (playerIsDead(player)) return;
  
  if (player->PlayerMoby->CollDamage >= 0) {
    u32 meleeFlags = 0x00081801;
    u32 rangedFlags = 0x1;
    MobyColDamage* colDamage = mobyGetDamage(player->PlayerMoby, meleeFlags | rangedFlags, 0);
    if (colDamage) {
      if ((colDamage->DamageFlags & meleeFlags) == meleeFlags) {
        colDamage->DamageHp *= badgesBerserkerMeleeMult[badgeLevel];
      } else if ((colDamage->DamageFlags & rangedFlags) == rangedFlags) {
        colDamage->DamageHp *= BADGES_BERSERKER_RANGED_DAMAGE_MULT;
      }
    }
  }
}

//--------------------------------------------------------------------------
void badgesUpdate_ExplosiveWrench(Player* player, int badgeLevel)
{
  if (playerIsDead(player)) return;
  
  if (player->PlayerState == PLAYER_STATE_JUMP_ATTACK && player->PlayerMoby->AnimSeqId == 43) {
    if (player->PlayerMoby->AnimSeqT > 12) {
          
      // spawn explosion
      u128 vPos = vector_read(player->PlayerPosition);
      float damage = 50;
      float radius = 5 * (badgeLevel + 1);
      mobySpawnExplosion
            (vPos, 1, 0x0, 0x0, 0x0, 0x10, 0x10, 0x0, 0, 0, 0, 0,
            1, 0, 0x80080840, 0, 0x801040C0, 0x801010C0, 0x801010C0, 0x801010C0, 0x801010C0, 0x801010C0, 0x801010C0, 0x801010C0,
            0x801040C0, 1, player->PlayerMoby, 0, vPos, radius/4, 0, damage, radius);
      
      // play explosion sound
      mobyPlaySoundByClass(0, 0, player->PlayerMoby, MOBY_ID_ARBITER_ROCKET0);

      badgesPlayerCooldown[player->PlayerId] = 10;
    }
  }
}

//--------------------------------------------------------------------------
void badgesUpdatePlayer(Player* player, enum RaidsBadgeType badgeType, int badgeLevel)
{
  if (!player) return;

  // update time last had full health
  if (player->PlayerState == PLAYER_STATE_GET_HIT)
    badgesPlayerTimeLastLastHit[player->PlayerId] = gameGetTime();

  u32 cooldown = decTimerU32(&badgesPlayerCooldown[player->PlayerId]);
  if (cooldown) return;

  switch (badgeType)
  {
    case RAIDS_BADGE_TYPE_HEALTH_REGEN: badgesUpdate_HealthRegen(player, badgeLevel); break;
    case RAIDS_BADGE_TYPE_AMMO_REGEN: badgesUpdate_AmmoRegen(player, badgeLevel); break;
    case RAIDS_BADGE_TYPE_SHARPSHOOTER: break; // handled by gamemode
    case RAIDS_BADGE_TYPE_BERSERKER: badgesUpdate_Berserker(player, badgeLevel); break;
    case RAIDS_BADGE_TYPE_FLINCH_RESISTANCE: badgesUpdate_FlinchResistance(player, badgeLevel); break;
    case RAIDS_BADGE_TYPE_EXPLOSIVE_WRENCH: badgesUpdate_ExplosiveWrench(player, badgeLevel); break;
    default: break;
  }
}

//--------------------------------------------------------------------------
void badgesStart(void)
{
  if (!MapConfig.State) return;

  Player** players = playerGetAll();
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player))
      continue;

    RaidsInventoryItem_t* badge = &MapConfig.State->PlayerStates[i].Inventory.Badge;
    if (!badge->BadgeType)
      continue;

    badgesUpdatePlayer(player, badge->BadgeType, bankGetRarityFromQuality(badge->Quality));
  }
}

//--------------------------------------------------------------------------
void badgesInit(void)
{
  // hook blessings
  HOOK_JAL(0x005E1DEC, &badgesOnPlayerGetHit);
}
