#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include "game.h"
#include "messageid.h"
#include "utils.h"
#include "maputils.h"

int stackableExtraJumpJumpCount[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
int stackableHoverbootTicker[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
int stackableHoverbootState[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};

//--------------------------------------------------------------------------
void stackableOnMobKilled(Moby* moby, int killedByPlayerId, int killedByWeaponId) {
  if (killedByPlayerId >= 0 && killedByWeaponId >= 0) {
    Player* killedByPlayer = playerGetAll()[killedByPlayerId];
    if (!killedByPlayer->IsLocal) return; // only process dmg if local

    float willOWispStrength = playerGetStackableCount(killedByPlayerId, STACKABLE_ITEM_EXPLODING_ENEMIES);
    if (killedByPlayer && willOWispStrength > 0) {
      u32 damageFlags = mobAmIOwner(moby) ? 0x00081801 : 0;
      float radius = ITEM_STACKABLE_EXPLODINGENEMIES_RADIUS;
      float damage = willOWispStrength * ITEM_STACKABLE_EXPLODINGENEMIES_DAMAGE;
      u32 color = TEAM_COLORS[killedByPlayer->Team];
      spawnExplosion(moby->Position, radius, color);

      // damage
      if (MapConfig.State && MapConfig.State->AllMobsSorted) {
        Moby** allMobsSorted = MapConfig.State->AllMobsSorted;
        int i;
        for (i = 0; i < MAX_MOBS_ALIVE; ++i) {
          Moby* mob = allMobsSorted[i];
          if (!mob || mobyIsDestroyed(mob) || !mob->PVar || !mobyIsMob(mob)) continue;
          if (vector_sqrdistance(mob->Position, moby->Position) > (radius*radius)) continue;

          // create event
          MobyColDamageIn in = {
            .DamageFlags = damageFlags,
            .DamageHp = damage,
            .Damager = killedByPlayer->PlayerMoby,
            .DamageClass = 0,
            .DamageStrength = 1,
            .DamageIndex = 0,
            .Flags = 1,
            .Momentum = 0
          };
          mobyCollDamageDirect(mob, &in);

	        // Guber* guber = guberGetObjectByMoby(mob);
          // GuberEvent * guberEvent = guberEventCreateEvent(guber, MOB_EVENT_DAMAGE, 0, 0);
          // if (guberEvent) {
          //   struct MobDamageEventArgs args;
          //   args.SourceUID = killedByPlayer->Guber.Id.UID;
          //   args.SourceOClass = 0;
          //   args.DamageQuarters = damage*4;
          //   args.DamageFlags = damageFlags;
          //   args.Knockback.Angle = 0;
          //   args.Knockback.Ticks = 0;
          //   args.Knockback.Power = 0;
          //   args.Knockback.Force = 0;
          //   guberEventWrite(guberEvent, &args, sizeof(struct MobDamageEventArgs));
          // }
        }
      }

      //spawnExplosionDamage(moby->Position, radius, 0x800000C0, killedByPlayer->PlayerMoby, damage, damageFlags);
    }
  }
}

//--------------------------------------------------------------------------
Player* stackableFindPlayerByGadgetBox(GadgetBox* gadgetBox) {
  // find player
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !playerIsConnected(player)) continue;

    if (player->GadgetBox == gadgetBox) return players[i];
  }

  return NULL;
}

//--------------------------------------------------------------------------
int stackableGetAlphaModCount(GadgetBox* gadgetBox, int weaponId, int alphaModId) {
  if (!gadgetBox) return 0;
  if (alphaModId > 8) return 0;
  if (alphaModId <= 0) return 0;

  // count alpha mods
  int i, count = 0;
  for (i = 0; i < 10; ++i) {
    if (gadgetBox->Gadgets[weaponId].AlphaMods[i] == alphaModId) {
      ++count;
    }
  }

  // add stackables
  int stackableId = -1;
  switch (alphaModId)
  {
    case ALPHA_MOD_AMMO: stackableId = STACKABLE_ITEM_ALPHA_MOD_AMMO; break;
    case ALPHA_MOD_AREA: stackableId = STACKABLE_ITEM_ALPHA_MOD_AREA; break;
    case ALPHA_MOD_IMPACT: stackableId = STACKABLE_ITEM_ALPHA_MOD_IMPACT; break;
    case ALPHA_MOD_SPEED: stackableId = STACKABLE_ITEM_ALPHA_MOD_SPEED; break;
  }

  if (stackableId >= 0) {

    Player* player = stackableFindPlayerByGadgetBox(gadgetBox);
    if (player && MapConfig.State && !MapConfig.State->PlayerStates[player->PlayerId].IsInWeaponsMenu) {
      count += ITEM_STACKABLE_ALPHA_MOD_AMT * playerGetStackableCount(player->PlayerId, stackableId);

      // cap at 10 total alpha mods
      //if (count > 10) count = 10;
    }
  }

  return count;
}

//--------------------------------------------------------------------------
void stackableOnDecrementAmmo(GadgetBox* gadgetBox, int weaponId, int amount) {

  if (amount == -1) {
    Player* player = stackableFindPlayerByGadgetBox(gadgetBox);
    if (player) {
      int multishotCount = playerGetStackableCount(player->PlayerId, STACKABLE_ITEM_EXTRA_SHOT);
      if (multishotCount > 0) {
        amount *= 1 + multishotCount;
      }
    }
  }

  ((void (*)(GadgetBox*, int, int))0x006270E0)(gadgetBox, weaponId, amount);
}

//--------------------------------------------------------------------------
int stackableOnPlayerCanPickupHealth(Player* player, Moby* healthMoby) {

  // vampire disables health pickups
  if (player && playerGetStackableCount(player->PlayerId, STACKABLE_ITEM_VAMPIRE) > 0)
    return 0;

  // return base
  return ((int (*)(Player*, Moby*))0x00411c20)(player, healthMoby);
}

//--------------------------------------------------------------------------
void stackablePlayerOnDoubleJump(Player* player, int stateId, int a2, int a3, int t0) {
  PlayerVTable* vtable = playerGetVTable(player);
  if (!vtable) return;

  int featherCount = playerGetStackableCount(player->PlayerId, STACKABLE_ITEM_EXTRA_JUMP);
  if (featherCount > 0
      && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP)
      && (stackableExtraJumpJumpCount[player->PlayerId] < featherCount || player->timers.state < 6)) {
        // don't let the player flip until after they've used their jumps
        POKE_U8((u32)player + 0x1CA0 + 0xFF, 0);
  } else {
    stackableExtraJumpJumpCount[player->PlayerId] = 0;
    vtable->UpdateState(player, stateId, a2, a3, t0);
  }
}

//--------------------------------------------------------------------------
void stackableProcessPlayer_ExtraJumps(Player* player, struct SurvivalPlayer* playerData) {

  // trigger extra jump on JUMP button
  int featherCount = playerGetStackableCount(player->PlayerId, STACKABLE_ITEM_EXTRA_JUMP);
  int jumpBits = padGetMappedPad(0x40, player->PlayerId) << 8;
  PlayerVTable* vtable = playerGetVTable(player);
  if (featherCount > 0
      && vtable
      && playerPadGetAnyButtonDown(player, jumpBits)
      && player->timers.state > 6
      && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP)) {

    // trigger another jump or indicate if the player can now double jump
    if (stackableExtraJumpJumpCount[player->PlayerId] < featherCount) {
      stackableExtraJumpJumpCount[player->PlayerId] += 1;
      vtable->UpdateState(player, PLAYER_STATE_JUMP, 1, 1, 1);
      POKE_U8((u32)player + 0x1CA0 + 0xFF, 0);
    } else {
      POKE_U8((u32)player + 0x1CA0 + 0xFF, 1);
    }
  }

  // reset extra jump counter when not in first jump state
  if (player->PlayerState != PLAYER_STATE_JUMP && player->PlayerState != PLAYER_STATE_RUN_JUMP)
    stackableExtraJumpJumpCount[player->PlayerId] = 0;
}

//--------------------------------------------------------------------------
void stackablePlayerOnChargeboot(Player* player, int stateId, int a2, int a3, int t0) {
  PlayerVTable* vtable = playerGetVTable(player);
  if (!vtable) return;

  // prevent cbooting with hoverboots
  if (playerGetStackableCount(player->PlayerId, STACKABLE_ITEM_HOVERBOOTS) > 0) return;

  // base
  vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void stackableProcessPlayer_Hoverboots(Player* player, struct SurvivalPlayer* playerData) {

  int playerId = player->PlayerId;
  int canHover = (player->PlayerState == PLAYER_STATE_WALK || player->PlayerState == PLAYER_STATE_IDLE
              || player->PlayerState == PLAYER_STATE_FALL || player->PlayerState == PLAYER_STATE_GET_HIT)
              && !player->Ground.onGood;
  int count = playerGetStackableCount(playerId, STACKABLE_ITEM_HOVERBOOTS);
  int hoverDuration = /*playerGetStackableCount(playerId, STACKABLE_ITEM_HOVERBOOTS) * */ ITEM_STACKABLE_HOVERBOOTS_DUR_TPS;

  // hover
  if (canHover && count > 0 && stackableHoverbootTicker[playerId] >= 0 && stackableHoverbootTicker[playerId] < hoverDuration) {
    player->Ground.offGood = 0;
    stackableHoverbootTicker[playerId]++;

    if ((stackableHoverbootTicker[playerId] % 10) == 0) {
      mobySpawnExplosion(vector_read(player->PlayerPosition), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0x80003080, 0, NULL, NULL, 0, 0.5, 0, 0, 0);
    }
  }

  // replace cboots with magnet boots
  if (count > 0) {
    player->GadgetBox->Gadgets[17].Level = 0;
    
    if (player->Gadgets[1].pMoby) player->Gadgets[1].pMoby->Bangles = 4;
    if (player->Gadgets[1].pMoby2) player->Gadgets[1].pMoby2->Bangles = 4;
  } else if (player->GadgetBox->Gadgets[17].Level >= 0) {
    
    if (player->Gadgets[1].pMoby) player->Gadgets[1].pMoby->Bangles = 2;
    if (player->Gadgets[1].pMoby2) player->Gadgets[1].pMoby2->Bangles = 2;
  }

  // reset extra jump counter when not in first jump state
  if (!canHover)
    stackableHoverbootTicker[playerId] = player->Ground.onGood ? 0 : -1;
}

//--------------------------------------------------------------------------
void stackableProcessPlayer(Player* player) {
  if (!player) return;
  if (!MapConfig.State) return;

  struct SurvivalPlayer* playerData = &MapConfig.State->PlayerStates[player->PlayerId];

  stackableProcessPlayer_ExtraJumps(player, playerData);
  stackableProcessPlayer_Hoverboots(player, playerData);
}

//--------------------------------------------------------------------------
void stackableTick(void) {
  
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !playerIsConnected(player)) continue;

    struct SurvivalPlayer* playerData = &MapConfig.State->PlayerStates[i];
    stackableProcessPlayer(player);
  }
}

//--------------------------------------------------------------------------
void stackableInit(void) {

  // hook
  HOOK_JAL(0x0060C660, &stackablePlayerOnDoubleJump);
  HOOK_JAL(0x0060DC6C, &stackablePlayerOnChargeboot);
  HOOK_J(0x006299A8, &stackableGetAlphaModCount);
  POKE_U32(0x006299CC, 0);
  HOOK_JAL(0x00627188, &stackableOnDecrementAmmo);
  HOOK_JAL(0x004121C0, &stackableOnPlayerCanPickupHealth);
  HOOK_JAL(0x00412080, &stackableOnPlayerCanPickupHealth);
  HOOK_JAL(0x004128B0, &stackableOnPlayerCanPickupHealth);

  POKE_U32(0x005D9948, 0); // disable cboot bangles write
}
