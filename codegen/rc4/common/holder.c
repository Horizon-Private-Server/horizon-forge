/***************************************************
 * FILENAME :		holder.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the holders.
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
#include <libdl/hud.h>
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
#include <libdl/collision.h>
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "controller.h"
#include "holder.h"
#include "common.h"

#if DEBUG
#define DLOG(moby, format, ...) if (((struct HolderPVar*)moby->PVar)->Config.Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#else
#define DLOG(moby, format, ...) 
#endif

//--------------------------------------------------------------------------
void holderUpdatePlayerHolder(Moby* moby)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  Player* player = pvars->State.HeldByPlayer;
  Player** players = playerGetAll();

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* pi = players[i];
    if (!playerIsValid(pi)) continue;
    
    if (pi == player) {
      pi->HeldMoby = moby;
    } else if (pi->HeldMoby == moby) {
      pi->HeldMoby = NULL;
    }
  }
}

//--------------------------------------------------------------------------
void holderOnPickup(Moby* moby, Player* player)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  
  pvars->State.HeldByPlayer = player;
  holderUpdatePlayerHolder(moby);

  // trigger
  if (gameAmIHost() && pvars->Config.OnDropControllerMoby && !mobyIsDestroyed(pvars->Config.OnDropControllerMoby)) {
    controllerSetTriggerMoby(pvars->Config.OnDropControllerMoby, player ? player->PlayerMoby : NULL);
  }

  DLOG(moby, "PICKED UP BY PLAYER %d\n", player ? player->PlayerId : -1);
}

//--------------------------------------------------------------------------
void holderOnDrop(Moby* moby, Player* player)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  
  pvars->State.HeldByPlayer = NULL;
  holderUpdatePlayerHolder(moby);

  // trigger
  if (gameAmIHost() && pvars->Config.OnDropControllerMoby && !mobyIsDestroyed(pvars->Config.OnDropControllerMoby)) {
    controllerSetTriggerMoby(pvars->Config.OnDropControllerMoby, player ? player->PlayerMoby : NULL);
  }

  DLOG(moby, "DROPPED BY PLAYER %d\n", player ? player->PlayerId : -1);
}

//--------------------------------------------------------------------------
Moby* holderGetTargetMoby(Moby* moby)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

  // check if exists
  Moby* targetMoby = pvars->Config.TargetMoby;
  if (!targetMoby || mobyIsDestroyed(targetMoby)) return NULL;

  // check its the same moby
  if (targetMoby->UID != pvars->State.TargetMobyUid) return NULL;

  return targetMoby;
}

//--------------------------------------------------------------------------
void holderBroadcastState(Moby* moby, enum HolderState state)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, HOLDER_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
  }

  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  mobySetState(moby, state, -1);
}

//--------------------------------------------------------------------------
void holderBroadcastPickup(Moby* moby, Player* player)
{
  if (!player) return;

  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, HOLDER_EVENT_PICKUP);
  if (guberEvent) {
    guberEventWrite(guberEvent, &player->PlayerId, 4);
  }

  //holderOnPickup(moby, player);
}

//--------------------------------------------------------------------------
void holderBroadcastDrop(Moby* moby, Player* player)
{
  if (player == NULL) return;

  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

  VECTOR collTestToPos = {0,0,-100,0};
  vector_add(collTestToPos, moby->Position, collTestToPos);
  if (CollLine_Fix(moby->Position, collTestToPos, 0, moby, NULL)) {
    if (CollisionIsLethal(CollLine_Fix_GetHitCollisionId())) {
      holderBroadcastState(moby, HOLDER_STATE_RESET); // reset on lethal
      return;
    }

    vector_copy(moby->Position, CollLine_Fix_GetHitPosition());
  }

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, HOLDER_EVENT_DROP);
  if (guberEvent) {
    guberEventWrite(guberEvent, moby->Position, 12);
    guberEventWrite(guberEvent, &player->PlayerId, 4);
  }

  //holderOnDrop(moby, player);
}

//--------------------------------------------------------------------------
void holderOnStateChanged(Moby* moby)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

  switch (moby->State)
  {
    case HOLDER_STATE_DEACTIVATED:
    {
      // drop on deactivated
      pvars->State.HeldByPlayer = NULL;
      holderUpdatePlayerHolder(moby);
      break;
    }
    case HOLDER_STATE_ACTIVATED:
    {
      break;
    }
    case HOLDER_STATE_RESET:
    {
      holderOnDrop(moby, pvars->State.HeldByPlayer);
      vector_copy(moby->Position, pvars->State.ResetPosition);
      vector_copy(moby->Rotation, pvars->State.ResetRotation);
      mobyUpdateTransform(moby);

      Moby* targetMoby = holderGetTargetMoby(moby);
      if (targetMoby) {
        vector_copy(targetMoby->Position, moby->Position);
        vector_copy(targetMoby->LSphere, moby->LSphere);
        vector_copy(targetMoby->BSphere, moby->BSphere);
        memcpy(targetMoby->M0_03, moby->M0_03, 0x40);
      }

      mobySetState(moby, HOLDER_STATE_ACTIVATED, -1);
      break;
    }
  }
}

//--------------------------------------------------------------------------
void holderHandleDamage(Moby* moby)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

  if (!gameAmIHost()) return;
  if (pvars->State.HeldByPlayer) return;

  // get damager
  int damageIndex = moby->CollDamage;
  MobyColDamage* colDamage = NULL;
  if (damageIndex >= 0) {
    colDamage = mobyGetDamage(moby, 0x80481C41, 0);
  }

  // check damage is valid
  float damage;
  ((void (*)(Moby*, float*, MobyColDamage*))0x005184d0)(moby, &damage, colDamage);
  if (!colDamage || !colDamage->Damager || damage <= 0) return;

  DLOG(moby, "holder %08X hit for %f by %08X\n", (u32)moby, damage, (u32)colDamage->Damager);

  // wrench only
  if (pvars->Config.WrenchOnly && (!colDamage->Damager || colDamage->Damager->OClass != MOBY_ID_WRENCH))
    return;
  
  // get player source
  Player* playerDamager = guberMobyGetPlayerDamager(colDamage->Damager);
  if (!playerIsValid(playerDamager)) return;

  // pickup
  holderBroadcastPickup(moby, playerDamager);
}

//--------------------------------------------------------------------------
void holderCheckForDrop(Moby* moby)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  Player* player = pvars->State.HeldByPlayer;
  if (!player) return;
  
  // player left
  if (gameAmIHost() && !playerIsValid(player)) {
    holderBroadcastDrop(moby, player);
    return;
  }

  // only let local player broadcast when they drop item
  if (!player->IsLocal) return;

  if ((playerIsDead(player) || player->Health <= 0) && pvars->Config.ResetOnDeath) {
    holderBroadcastState(moby, HOLDER_STATE_RESET);
    return;
  }

  if (player->HeldMoby == moby) return;

  // player dropped
  holderBroadcastDrop(moby, player);
}

//--------------------------------------------------------------------------
void holderUpdate(Moby* moby)
{
  int i;
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    holderOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  // if not active, disable
  if (moby->State != HOLDER_STATE_ACTIVATED) {
    moby->ModeBits &= ~(MOBY_MODE_BIT_CAN_BE_DAMAGED);
    moby->CollActive = -1;
    return;
  }
  
  // copy target moby
  // if target moby gone then deactivate
  Moby* targetMoby = holderGetTargetMoby(moby);
  if (!targetMoby) {
    holderBroadcastState(moby, HOLDER_STATE_DEACTIVATED);
    return;
  }

  if (pvars->State.HeldByPlayer) {
    vector_copy(targetMoby->Position, moby->Position);
    vector_copy(targetMoby->LSphere, moby->LSphere);
    vector_copy(targetMoby->BSphere, moby->BSphere);

    MATRIX mRot;
    matrix_unit(mRot);
    matrix_rotate_y(mRot, mRot, 90 * MATH_DEG2RAD);

    MATRIX m;
    matrix_unit(m);
    memcpy(m, moby->M0_03, 3 * sizeof(VECTOR));
    matrix_multiply(m, mRot, m);

    memcpy(targetMoby->M0_03, m, 0x30);
    matrix_toeuler(m, targetMoby->Rotation);

    targetMoby->CollActive = -1;
    moby->CollActive = -1;
  } else {
    vector_copy(moby->Position, targetMoby->Position);
    vector_copy(moby->LSphere, targetMoby->LSphere);
    vector_copy(moby->BSphere, targetMoby->BSphere);
    memcpy(moby->M0_03, targetMoby->M0_03, 0x40);

    moby->CollActive = 0;
    targetMoby->CollActive = -1;
  }

  // copy rest
  moby->ModeBits |= MOBY_MODE_BIT_CAN_BE_DAMAGED;
  moby->ModeBits &= ~MOBY_MODE_BIT_NO_POST_UPDATE;
  moby->Scale = targetMoby->Scale;
  moby->CollData = targetMoby->CollData;
  moby->PClass = targetMoby->PClass;
  moby->MClass = targetMoby->MClass;
  moby->AnimSeq = targetMoby->AnimSeq;
  moby->AnimSeqId = moby->LSeq = targetMoby->AnimSeqId;

  // need collision
  if (!moby->CollData) {
    void* collMobyPtr = mobyGetClassPtr(MOBY_ID_HEALTH_BOX_MULT);
    if (collMobyPtr) moby->CollData = *(void**)((u32)collMobyPtr + 0x10);
  }

  // handle player left/dropped
  holderCheckForDrop(moby);

  // check for player pickup
  holderHandleDamage(moby);
  moby->CollDamage = -1;

  holderUpdatePlayerHolder(moby);
}

//--------------------------------------------------------------------------
void holderOnGuberCreated(Moby* moby)
{
  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;

  moby->PUpdate = &holderUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE | MOBY_MODE_BIT_HAS_SPECIAL_VARS;

  // update moby target ref
  pvars->Config.TargetMoby = mobyGetFromIdxOrNull((int)pvars->Config.TargetMoby);
  pvars->Config.OnPickupControllerMoby = mobyGetFromIdxOrNull((int)pvars->Config.OnPickupControllerMoby);
  pvars->Config.OnDropControllerMoby = mobyGetFromIdxOrNull((int)pvars->Config.OnDropControllerMoby);
  pvars->State.HeldByPlayer = NULL;
  pvars->State.TargetMobyUid = pvars->Config.TargetMoby ? pvars->Config.TargetMoby->UID : 0;

  Moby* initialPosMoby = pvars->Config.TargetMoby ? pvars->Config.TargetMoby : moby;
  vector_copy(pvars->State.ResetPosition, initialPosMoby->Position);
  vector_copy(pvars->State.ResetRotation, initialPosMoby->Rotation);

  pvars->TargetVarsPtr = &pvars->TargetVars;
  pvars->FlashVarsPtr = &pvars->FlashVars;
  pvars->TargetVars.hitPoints = 1;
  pvars->TargetVars.maxHitPoints = 1;
  pvars->TargetVars.targetHeight = 0;
  pvars->TargetVars.targetRadiusIn8ths = 8;
  pvars->TargetVars.team = TEAM_WHITE;

  struct Guber* guber = guberGetObjectByMoby(moby);
  ((GuberMoby*)guber)->TeamNum = pvars->TargetVars.team;

  mobySetState(moby, pvars->Config.DefaultState, -1);

  DLOG(moby, "HOLDER %08X found target %08X\n", (u32)moby, (u32)pvars->Config.TargetMoby);
}

//--------------------------------------------------------------------------
int holderHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  if (!moby || !moby->PVar)
    return 0;

  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &state, 4);
  mobySetState(moby, state, -1);
  DLOG(moby, "RECV STATE %d\n", state);
  return 0;
}

//--------------------------------------------------------------------------
int holderHandleEvent_Pickup(Moby* moby, GuberEvent* event)
{
  int playerId;
  if (!moby || !moby->PVar)
    return 0;

  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &playerId, 4);

  // find pickup player
  Player* player = playerGetAll()[playerId];
  if (playerId < 0 || playerId >= GAME_MAX_PLAYERS || !playerIsValid(player))
    player = NULL;

  // assign to player
  holderOnPickup(moby, player);
  return 0;
}

//--------------------------------------------------------------------------
int holderHandleEvent_Drop(Moby* moby, GuberEvent* event)
{
  VECTOR pos = {0};
  int playerId;
  if (!moby || !moby->PVar)
    return 0;

  struct HolderPVar* pvars = (struct HolderPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, pos, 12);
	guberEventRead(event, &playerId, 4);

  // find drop player
  Player* player = playerGetAll()[playerId];
  if (playerId < 0 || playerId >= GAME_MAX_PLAYERS || !playerIsValid(player))
    player = NULL;

  // drop
  holderOnDrop(moby, player);

  // teleport moby to drop position
  vector_copy(moby->Position, pos);
  Moby* targetMoby = holderGetTargetMoby(moby);
  if (targetMoby)
    vector_copy(targetMoby->Position, pos);

  return 0;
}

//--------------------------------------------------------------------------
struct Guber* holderGetGuber(Moby* moby)
{
	if (moby->OClass == HOLDER_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int holderHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == HOLDER_OCLASS && moby->PVar) {
		u32 eventId = event->NetEvent.EventID;

		switch (eventId)
		{
      case HOLDER_EVENT_SET_STATE: { return holderHandleEvent_SetState(moby, event); }
      case HOLDER_EVENT_PICKUP: { return holderHandleEvent_Pickup(moby, event); }
      case HOLDER_EVENT_DROP: { return holderHandleEvent_Drop(moby, event); }
			default:
			{
				DLOG(moby, "unhandle holder event %d\n", eventId);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void holderStart(void)
{

}

//--------------------------------------------------------------------------
void holderInit(void)
{
  Moby* temp = mobySpawn(HOLDER_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("HOLDER oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for holders
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, HOLDER_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found holder %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        holderOnGuberCreated(moby);
      }
    }

		++moby;
	}

  DPRINTF("holder pvar size %d\n", sizeof(struct HolderPVar));
}
