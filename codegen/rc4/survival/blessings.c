#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include "game.h"
#include "messageid.h"
#include "maputils.h"

char blessingQuadJumpJumpCount[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
short blessingHealthRegenTickers[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
short blessingAmmoRegenTickers[GAME_MAX_PLAYERS] = {0,0,0,0,0,0,0,0,0,0};
VECTOR blessingLastPosition[GAME_MAX_PLAYERS] = {};

//--------------------------------------------------------------------------
void playerDecHitpointHooked(Player* player, float amount)
{
  float factor = 1;

  if (playerHasBlessing(player->PlayerId, BLESSING_ITEM_THORNS))
    factor = 1 - ITEM_BLESSING_THORN_DAMAGE_FACTOR;

  playerDecHealth(player, amount * factor);
}

//--------------------------------------------------------------------------
void headbuttDamage(float hitpoints, Moby* hitMoby, Moby* sourceMoby, int damageFlags, VECTOR fromPos, VECTOR t0)
{
  if (!MapConfig.State) return 0;

  // allow damaging healthbox
  // otherwise check if player can headbutt mob, and deny if not
  if (hitMoby && hitMoby->OClass != MOBY_ID_HEALTH_BOX_MULT) {
    Player * sourcePlayer = guberMobyGetPlayerDamager(sourceMoby);
    if (!sourcePlayer || !mobyIsMob(hitMoby))
      return;

    // only headbutt if bull blessing
    if (!playerHasBlessing(sourcePlayer->PlayerId, BLESSING_ITEM_BULL))
      return;

    hitpoints = 10 * (1 + (PLAYER_UPGRADE_DAMAGE_FACTOR * MapConfig.State->PlayerStates[sourcePlayer->PlayerId].State.Upgrades[UPGRADE_DAMAGE]));
    hitpoints *= sourcePlayer->DamageMultiplier;
#if LOG_STATS2
    DPRINTF("headbutt %08X %04X with %f and %X\n", (u32)hitMoby, hitMoby->OClass, hitpoints, damageFlags);
#endif
  }

  ((void (*)(float, Moby*, Moby*, int, VECTOR, VECTOR))0x00503500)(hitpoints, hitMoby, sourceMoby, damageFlags, fromPos, t0);
}

//--------------------------------------------------------------------------
void playerStateUpdate(Player* player, int stateId, int a2, int a3, int t0) {
  PlayerVTable* vtable = playerGetVTable(player);
  if (!vtable) return;

  // if player has bull blessing, then don't let walls stop them from cbooting
  if (playerHasBlessing(player->PlayerId, BLESSING_ITEM_BULL) && stateId == PLAYER_STATE_JUMP_BOUNCE) {
    return;
  }

  vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void playerBlessingOnDoubleJump(Player* player, int stateId, int a2, int a3, int t0) {
  PlayerVTable* vtable = playerGetVTable(player);
  if (!vtable) return;

  if (playerHasBlessing(player->PlayerId, BLESSING_ITEM_MULTI_JUMP)
      && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP)
      && blessingQuadJumpJumpCount[player->PlayerId] < ITEM_BLESSING_MULTI_JUMP_COUNT) {
        // don't let the player flip until after they've used their jumps
  } else {
    blessingQuadJumpJumpCount[player->PlayerId] = 0;
    vtable->UpdateState(player, stateId, a2, a3, t0);
  }
}

//--------------------------------------------------------------------------
int playerBlessingGetAmmoRegenAmount(int weaponId) {
	
  switch (weaponId)
  {
    case WEAPON_ID_VIPERS: return 5;
    case WEAPON_ID_MAGMA_CANNON:
    case WEAPON_ID_ARBITER:
    case WEAPON_ID_MINE_LAUNCHER:
    case WEAPON_ID_FUSION_RIFLE:
    case WEAPON_ID_OMNI_SHIELD:
    case WEAPON_ID_FLAIL:
    case WEAPON_ID_B6:
      return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
void processPlayerBlessings(Player* player) {
  if (!player) return;
  if (!MapConfig.State) return;

  struct SurvivalPlayer* playerData = &MapConfig.State->PlayerStates[player->PlayerId];

  // 
  int jumpBits = padGetMappedPad(0x40, player->PlayerId) << 8;
  PlayerVTable* vtable = playerGetVTable(player);
  if (playerHasBlessing(player->PlayerId, BLESSING_ITEM_MULTI_JUMP)
      && vtable
      && playerPadGetAnyButtonDown(player, jumpBits)
      && player->timers.state > 6
      && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP)
      && blessingQuadJumpJumpCount[player->PlayerId] < ITEM_BLESSING_MULTI_JUMP_COUNT) {
    blessingQuadJumpJumpCount[player->PlayerId] += 1;
    vtable->UpdateState(player, PLAYER_STATE_JUMP, 1, 1, 1);
  }

  // reset quad jump counter when not in first jump state
  if (player->PlayerState != PLAYER_STATE_JUMP && player->PlayerState != PLAYER_STATE_RUN_JUMP)
    blessingQuadJumpJumpCount[player->PlayerId] = 0;

  int i;
  for (i = 0; i < playerData->State.BlessingSlots && i < PLAYER_MAX_BLESSINGS; ++i) {
    switch (playerData->State.ItemBlessings[i])
    {
      case BLESSING_ITEM_BULL:
      {
        if (player->PlayerState == PLAYER_STATE_CHARGE && player->timers.state > 55 && playerPadGetButton(player, PAD_L2) > 0) {
          player->timers.state = 55;
        }
        break;
      }
      case BLESSING_ITEM_ELEM_IMMUNITY:
      {
        player->timers.acidTimer = 0;
        player->timers.freezeTimer = 0;
        break;
      }
      case BLESSING_ITEM_HEALTH_REGEN:
      {
        if (!player->IsLocal) break;
        if (playerData->TicksSinceHealthChanged < (TPS * 10)) break;
        if (playerIsDead(player)) break;
        if (blessingHealthRegenTickers[player->PlayerId] > 0) {
          --blessingHealthRegenTickers[player->PlayerId];
          break;
        }
        if (player->Health >= player->MaxHealth) break;

        playerData->LastHealth = player->Health = clamp(player->Health + 4, 0, player->MaxHealth);
        if (player->pNetPlayer) player->pNetPlayer->pNetPlayerData->hitPoints = player->Health;
        blessingHealthRegenTickers[player->PlayerId] = ITEM_BLESSING_HEALTH_REGEN_RATE_TPS;
        break;
      }
      case BLESSING_ITEM_AMMO_REGEN:
      {
        if (!player->IsLocal) break;
        if (blessingAmmoRegenTickers[player->PlayerId] > 0) {
          --blessingAmmoRegenTickers[player->PlayerId];
        } else {

          VECTOR dt;
          vector_subtract(dt, blessingLastPosition[player->PlayerId], player->PlayerPosition);
          if (vector_sqrmag(dt) > (2*2)) {
            struct GadgetEntry* gadgetEntry = &player->GadgetBox->Gadgets[player->WeaponHeldId];
            int maxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, player->WeaponHeldId);
            int ammo = gadgetEntry->Ammo;
            int ammoInc = (int)ceilf(maxAmmo * 0.1); //playerBlessingGetAmmoRegenAmount(player->WeaponHeldId);
            if (ammoInc > 0 && ammo < maxAmmo) {
              gadgetEntry->Ammo = (ammo + ammoInc) > maxAmmo ? maxAmmo : (ammo + ammoInc);
            }
          }

          vector_copy(blessingLastPosition[player->PlayerId], player->PlayerPosition);
          blessingAmmoRegenTickers[player->PlayerId] = ITEM_BLESSING_AMMO_REGEN_RATE_TPS;
        }
        break;
      }
    }
  }
}

//--------------------------------------------------------------------------
void blessingsTick(void) {
  
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !playerIsConnected(player)) continue;

    // check for blessing slot
    struct SurvivalPlayer* playerData = &MapConfig.State->PlayerStates[i];
    if (playerData->State.BlessingSlots) {
      processPlayerBlessings(player);
    }
  }
}

void blessingsInit(void) {
  
  // hook blessings
  HOOK_JAL(0x0060C660, &playerBlessingOnDoubleJump);
  HOOK_JAL(0x0060CD44, &playerStateUpdate);

	// enable headbutt
	*(u32*)0x005F98D0 = 0x24020001;
	*(u32*)0x005f9920 = 0x0C000000 | ((u32)&headbuttDamage >> 2);

  // hook when player takes damage
  HOOK_JAL(0x00605820, &playerDecHitpointHooked);
  POKE_U32(0x005E1870, 0);
}
