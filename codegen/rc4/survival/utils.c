/***************************************************
 * FILENAME :		maputils.c
 * 
 * DESCRIPTION :
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include "messageid.h"
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/hud.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/collision.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "game.h"
#include "mob.h"
#include "gate.h"

extern char LocalPlayerStrBuffer[2][64];
extern struct SurvivalMapConfig MapConfig;

/* 
 * paid sound def
 */
SoundDef PaidSoundDef =
{
	0.0,	// MinRange
	20.0,	// MaxRange
	100,		// MinVolume
	2000,		// MaxVolume
	0,			// MinPitch
	0,			// MaxPitch
	0,			// Loop
	0x10,		// Flags
	32,		  // Index
	3			  // Bank
};


//--------------------------------------------------------------------------
void playPaidSound(Player* player)
{
  if (!player) return;
  soundPlay(&PaidSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
int tryPlayerInteract(Moby* moby, Player* player, char* message, char* lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns)
{
  static int shown[GAME_MAX_LOCALS] = {0,0};
  VECTOR delta;
  if (!player || !player->PlayerMoby || !player->IsLocal || !isInGame())
    return 0;

  struct SurvivalPlayer* playerData = NULL;
  int localPlayerIndex = player->LocalPlayerIndex;
  int pIndex = player->PlayerId;

  if (MapConfig.State) {
    playerData = &MapConfig.State->PlayerStates[pIndex];
  }
  
  vector_subtract(delta, player->PlayerPosition, moby->Position);
  if (vector_sqrmag(delta) < sqrDistance) {

    // draw help popup
    snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), message);
    uiShowPopup(player->LocalPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
    shown[localPlayerIndex] = gameGetTime();

    // handle pad input
    if (padGetAnyButtonDown(localPlayerIndex, btns) > 0 && (!playerData || (playerData->State.Bolts >= boltCost && playerData->State.CurrentTokens >= tokenCost))) {
      if (playerData) {
        //playerData->State.Bolts -= boltCost;
        //playerData->State.CurrentTokens -= tokenCost;
        playerData->ActionCooldownTicks = actionCooldown;
        playerData->MessageCooldownTicks = 5;
      }

      return 1;
    }
    
    // draw lower message
    if (lowerMessage) {
      char* a = uiMsgString(0x2415);
      strncpy(a, lowerMessage, 0x40);
      uiShowLowerPopup(0, 0x2415);
    }
  } else if (shown[localPlayerIndex] && gameGetTime() > shown[localPlayerIndex]) {
    hudHidePopup();
    shown[localPlayerIndex] = 0;
  }

  return 0;
}

//--------------------------------------------------------------------------
int mobyIsMob(Moby* moby)
{
  if (!moby) return 0;

  int i;
  for (i = 0; i < MapConfig.DefaultSpawnParamsCount; ++i) {
    if (MapConfig.DefaultSpawnParams[i].OClass == moby->OClass)
      return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int playerHasBlessing(int playerId, int blessing)
{
  if (!MapConfig.State) return 0;

  int i;
  for (i = 0; i < PLAYER_MAX_BLESSINGS; ++i) {
    if (MapConfig.State->PlayerStates[playerId].State.ItemBlessings[i] == blessing) return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int playerGetStackableCount(int playerId, int stackable)
{
  if (!MapConfig.State) return 0;

  return MapConfig.State->PlayerStates[playerId].State.ItemStackable[stackable];
}

//--------------------------------------------------------------------------
int bakedSpawnGetFirst(int bakedSpawnType, VECTOR outPos, VECTOR outRot) {
  if (!MapConfig.BakedConfig) return 0;

  int i;
  for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {
    if (MapConfig.BakedConfig->BakedSpawnPoints[i].Type == bakedSpawnType) {

      memcpy(outPos, MapConfig.BakedConfig->BakedSpawnPoints[i].Position, 12);
      memcpy(outRot, MapConfig.BakedConfig->BakedSpawnPoints[i].Rotation, 12);
      return 1;
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
Moby* mapOnGuberEventCreateMoby(int oclass, int pvarSize)
{
  if (mobyGetNumSpawnableMobys() < 50) {
    Moby* m = mobyFindNextByOClass(mobyListGetStart(), 0x13A1);
    if (!m) return NULL;
    if (m) {
      mobyDestroy(m);
      m->CollCnt = 0; // lets the moby be reused instantly
    }
  }

  return mobySpawn(oclass, pvarSize);
}

//--------------------------------------------------------------------------
void mapApplyFixes(void)
{
  HOOK_JAL(0x0061c3ec, &mapOnGuberEventCreateMoby);
}

//--------------------------------------------------------------------------
void mapPrintGambit(int gambit)
{
  char gambitName[32];
  if (!gambit) return;
  
  // read ex data
  char exDataBuf[1024];
  if (PATCH_INTEROP->ReadCustomMapExtraData(PATCH_INTEROP->MapLoaderFilename, exDataBuf, sizeof(exDataBuf), CUSTOM_MODE_SURVIVAL) > 8) {
    int gambitCount = *(int*)(exDataBuf + 4);
    char* gambits = (char*)(exDataBuf + 8);

    int i;
    for (i = 0; i < gambitCount; ++i) {
      if ((i+1) == gambit) {
        strncpy(gambitName, gambits, sizeof(gambitName));
        break;
      }

      gambits += strlen(gambits) + 1;
      gambits += strlen(gambits) + 1;
    }
  }

  // show user
  snprintf(exDataBuf, sizeof(exDataBuf), "Gambit \x0E%s\x08 Active", gambitName);
  pushSnack(0, exDataBuf, TPS * 10);
  pushSnack(1, exDataBuf, TPS * 10);
}

//--------------------------------------------------------------------------
void mapSendSendGambitCompletedMessage(int gambit)
{
  if (!gambit) return;
  
  void* lobbyConnection = netGetLobbyServerConnection();
  if (!lobbyConnection) return;

  UpdateSurvivalGambitCompletedRequest_t msg = {
    .GambitIdx = gambit
  };

  // read ex data
  char exDataBuf[1024];
  if (PATCH_INTEROP->ReadCustomMapExtraData(PATCH_INTEROP->MapLoaderFilename, exDataBuf, sizeof(exDataBuf), CUSTOM_MODE_SURVIVAL) > 8) {
    int gambitCount = *(int*)(exDataBuf + 4);
    char* gambits = (char*)(exDataBuf + 8);

    int i;
    for (i = 0; i < gambitCount; ++i) {
      if ((i+1) == gambit) {
        strncpy(msg.GambitName, gambits, sizeof(msg.GambitName));
        break;
      }

      gambits += strlen(gambits) + 1;
      gambits += strlen(gambits) + 1;
    }
  }

  // send request to server
  strncpy(msg.MapFilename, PATCH_INTEROP->MapLoaderFilename, sizeof(msg.MapFilename));
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, lobbyConnection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_UPDATE_SURVIVAL_GAMBIT_COMPLETED_REQUEST, sizeof(msg), &msg);

  // show user
  snprintf(exDataBuf, sizeof(exDataBuf), "Completed Gambit %s!", msg.GambitName);
  pushSnack(0, exDataBuf, TPS * 10);
  pushSnack(1, exDataBuf, TPS * 10);
}

//--------------------------------------------------------------------------
void mapLocalPlayerEnforceSingleWeaponRestriction(int localPlayerIdx, int weaponId, int hard)
{
  Player* player = playerGetFromSlot(localPlayerIdx);
  if (!playerIsValid(player)) return;

  playerSetLocalEquipslot(localPlayerIdx, 0, weaponId);
  playerSetLocalEquipslot(localPlayerIdx, 1, 0);
  playerSetLocalEquipslot(localPlayerIdx, 2, 0);
  
  int s;
  for (s = WEAPON_SLOT_VIPERS; s < WEAPON_SLOT_COUNT; ++s) {
    int gadget = weaponSlotToId(s);
    if (gadget == weaponId) {
      if (player->GadgetBox->Gadgets[gadget].Level < 0)
        playerGiveWeapon(player->GadgetBox, gadget, 0, 1);
    } else {
      if (hard) {
        player->GadgetBox->Gadgets[gadget].Level = -1;
        player->GadgetBox->Gadgets[gadget].Ammo = 0;
      }

      if (player->WeaponHeldId == gadget) {
        player->ChangeWeaponHeldId = weaponId;
      }
    }
  }
}

//--------------------------------------------------------------------------
void mapEnforceSingleWeaponRestriction(int weaponId)
{
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    mapLocalPlayerEnforceSingleWeaponRestriction(i, weaponId, 1);
  }
}

//--------------------------------------------------------------------------
extern Moby* gateMobies[GATE_MAX_COUNT];
void gateResetRandomGate(void)
{
  int i = 0;
  int r = 1 + rand(GATE_MAX_COUNT);
  int loops = 0;
  Moby* gateToReset = NULL;

  while (r > 0) {
    gateToReset = gateMobies[i];

    // skip mobies that are NULL, don't have pvars, or gates that aren't deactivated yet
    // we only want to reset an open gate
    if (!gateToReset || !gateToReset->PVar || gateToReset->State != GATE_STATE_DEACTIVATED) {
      i = (i+1) % GATE_MAX_COUNT;

      // we've looped through the entire list without finding
      // a gate that we can reset
      // so stop
      if (loops == 0 && i == 0) {
        return;
      }

      continue;
    }

    // increment gate index
    // and decrement random number
    i = (i+1) % GATE_MAX_COUNT;
    --r;

    ++loops;
  }
  
  // create event set cost event
  if (gateToReset) {
    struct GatePVar* pvars = (struct GatePVar*)gateToReset->PVar;
    GuberEvent * guberEvent = guberCreateEvent(gateToReset, GATE_EVENT_SET_COST);
    if (guberEvent) {
      guberEventWrite(guberEvent, &pvars->InitialCost, 4);
    }
  }
}
