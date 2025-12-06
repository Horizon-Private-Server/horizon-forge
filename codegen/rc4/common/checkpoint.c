/***************************************************
 * FILENAME :		checkpoint.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the checkpoints.
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
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "common.h"
#include "maputils.h"
#include "gate.h"
#include "mover.h"
#include "controller.h"
#include "checkpoint.h"

#if OBSTACLE || RAIDS
#include "config.h"
#endif

#if DEBUG
#define DLOG_CHPT(moby, format, ...) if (((struct CheckpointPVar*)moby->PVar)->Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#define DLOG_MNGR(moby, format, ...) if (((struct CheckpointManagerPVar*)moby->PVar)->Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#else
#define DLOG_CHPT(moby, format, ...) 
#define DLOG_MNGR(moby, format, ...) 
#endif

void checkpointManagerUpdate(Moby* moby);


Moby* checkpointManagerMoby = NULL;


//--------------------------------------------------------------------------
void checkpointOnStateChanged(Moby* moby)
{
  struct CheckpointPVar* pvars = (struct CheckpointPVar*)moby->PVar;

  if (!gameAmIHost()) return;

  DLOG_CHPT(moby, "CHECKPOINT STATE %d (%08X %08X)\n", moby->State, (u32)pvars->OnActivateControllerMoby, (u32)pvars->OnDeactivateControllerMoby);

  if (moby->State == CHECKPOINT_ACTIVE) {
    if (pvars->OnActivateControllerMoby && pvars->OnActivateControllerMoby->OClass == CONTROLLER_OCLASS) {
      controllerBroadcastNewState(pvars->OnActivateControllerMoby, CONTROLLER_STATE_ACTIVATED);
    }
  } else if (moby->State == CHECKPOINT_NOT_ACTIVE) {
    if (pvars->OnDeactivateControllerMoby && pvars->OnDeactivateControllerMoby->OClass == CONTROLLER_OCLASS) {
      controllerBroadcastNewState(pvars->OnDeactivateControllerMoby, CONTROLLER_STATE_ACTIVATED);
    }
  }
}

//--------------------------------------------------------------------------
void checkpointSetCuboid(Moby* moby)
{
  // update cuboid
  int i;
  int spCount = spawnPointGetCount();
  for (i = 0; i < spCount; ++i) {
    if (!spawnPointIsPlayer(i)) continue;

    SpawnPoint* sp = spawnPointGet(i);
    vector_copy(&sp->M0[12], moby->Position);
    vector_copy(&sp->M1[12], moby->Rotation);
    break;
  }
}

//--------------------------------------------------------------------------
void checkpointUpdate(Moby* moby)
{
  // wait for all clients to be ready before triggering anything
  if (!allClientsReady()) return;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    checkpointOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  if (moby->State != CHECKPOINT_ACTIVE) return;

  // update cuboid
  checkpointSetCuboid(moby);
}

//--------------------------------------------------------------------------
int checkpointSetActive(Moby* checkpointMoby)
{
  if (!checkpointManagerMoby) return 0;

  struct CheckpointManagerPVar* pvars = (struct CheckpointManagerPVar*)checkpointManagerMoby->PVar;
  struct CheckpointPVar* checkpointPvars = (struct CheckpointPVar*)checkpointMoby->PVar;

  // get index of checkpoint
  int idx = 0;
  for (idx = 0; idx < CHECKPOINT_MAX_CHECKPOINTS; ++idx) {
    if (pvars->CheckpointMobys[idx] == checkpointMoby) {
      break;
    }
  }

  if (idx >= CHECKPOINT_MAX_CHECKPOINTS) return 0;
  if (checkpointManagerMoby->State == idx) return 0;

  DLOG_MNGR(checkpointManagerMoby, "Activate checkpoint %08X => %d\n", (u32)checkpointMoby, idx);
  DLOG_CHPT(checkpointMoby, "Activate checkpoint %08X => %d\n", (u32)checkpointMoby, idx);
  
  // update checkpoint locally
  mobySetState(checkpointManagerMoby, idx, -1);
  checkpointManagerUpdate(checkpointManagerMoby);
  checkpointUpdate(checkpointMoby);

#if OBSTACLE
  if (MapConfig.SetLocalPlayerReachedCheckpoint && checkpointPvars->Save) {
    MapConfig.SetLocalPlayerReachedCheckpoint(checkpointMoby);
    uiShowPopup(0, "Saved Checkpoint");
  } else {
    uiShowPopup(0, "Checkpoint");
  }
#else
  uiShowPopup(0, "Checkpoint");
#endif

	// create event
  // if (gameAmIHost()) {
  //   GuberEvent * guberEvent = guberCreateEvent(checkpointManagerMoby, CHECKPOINT_EVENT_SET_STATE);
  //   if (guberEvent) {
  //     guberEventWrite(guberEvent, &idx, 4);
  //   }
  // }

  return 1;
}

//--------------------------------------------------------------------------
void checkpointManagerUpdate(Moby* moby)
{
  struct CheckpointManagerPVar* pvars = (struct CheckpointManagerPVar*)moby->PVar;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    Moby* selectedCheckpoint = pvars->CheckpointMobys[(int)moby->State];
    Moby* lastCheckpoint = pvars->CheckpointMobys[(int)pvars->LastCheckpoint];
    if (selectedCheckpoint) {
      if (pvars->LastCheckpoint >= 0 && lastCheckpoint) {
        mobySetState(lastCheckpoint, 0, -1);
      }

      mobySetState(selectedCheckpoint, 1, -1);
      pvars->LastCheckpoint = moby->State;
    }
    moby->Triggers |= 1;
  }
}

//--------------------------------------------------------------------------
int checkpointManagerHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  if (!moby || !moby->PVar)
    return 0;

	// read event
	guberEventRead(event, &state, 4);
  mobySetState(moby, state, -1);
  return 0;
}

//--------------------------------------------------------------------------
void checkpointManagerOnGuberCreated(Moby* moby)
{
  struct CheckpointManagerPVar* pvars = (struct CheckpointManagerPVar*)moby->PVar;

  moby->PUpdate = &checkpointManagerUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;

  // set default state
  pvars->DefaultCheckpointMoby = mobyGetFromIdxOrNull((int)pvars->DefaultCheckpointMoby);
  pvars->LastCheckpoint = -1;
  mobySetState(moby, 0, -1);
}

//--------------------------------------------------------------------------
struct Guber* checkpointGetGuber(Moby* moby)
{
	if (moby->OClass == CHECKPOINT_MANAGER_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int checkpointHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == CHECKPOINT_MANAGER_OCLASS && moby->PVar) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
      case CHECKPOINT_EVENT_SET_STATE: { return checkpointManagerHandleEvent_SetState(moby, event); }
			default:
			{
				DLOG_MNGR(moby, "unhandle checkpoint manager event %d\n", upgradeEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void checkpointStart(void)
{
  
}

//--------------------------------------------------------------------------
void checkpointInit(void)
{
  struct CheckpointManagerPVar* managerPvars = NULL;
  int checkpointCount = 0;
  checkpointManagerMoby = NULL;

  Moby* temp = mobySpawn(CHECKPOINT_MANAGER_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("CHECKPOINT MANAGER oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // set update functions
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, CHECKPOINT_MANAGER_OCLASS)))
	{
    struct CheckpointManagerPVar* pvars = (struct CheckpointManagerPVar*)moby->PVar;
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      managerPvars = pvars;
      checkpointManagerMoby = moby;
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG_MNGR(moby, "found checkpoint manager %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        checkpointManagerOnGuberCreated(moby);
      }

      break;
    }

		++moby;
	}

  // warn no manager
  if (!managerPvars) {
    DPRINTF("unable to find checkpoint manager\n");
  }

  // set update functions
  moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, CHECKPOINT_OCLASS)))
	{
    struct CheckpointPVar* pvars = (struct CheckpointPVar*)moby->PVar;
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      DLOG_CHPT(moby, "found checkpoint %08X\n", (u32)moby);
      moby->PUpdate = &checkpointUpdate;
      moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
      pvars->OnActivateControllerMoby = mobyGetFromIdxOrNull((int)pvars->OnActivateControllerMoby);
      pvars->OnDeactivateControllerMoby = mobyGetFromIdxOrNull((int)pvars->OnDeactivateControllerMoby);
      mobySetState(moby, CHECKPOINT_NOT_ACTIVE, -1);

      if (managerPvars && checkpointCount < CHECKPOINT_MAX_CHECKPOINTS) {

        managerPvars->CheckpointMobys[checkpointCount] = moby;
        
        // set default state
        if (managerPvars->DefaultCheckpointMoby == moby) {
          DLOG_MNGR(checkpointManagerMoby, "set as default checkpoint %08X %d\n", (u32)moby, checkpointCount);
          DLOG_CHPT(moby, "set as default checkpoint %08X %d\n", (u32)moby, checkpointCount);
          //checkpointSetActive(moby);
          checkpointSetCuboid(moby);
          mobySetState(checkpointManagerMoby, checkpointCount, -1);
          mobySetState(moby, CHECKPOINT_ACTIVE, -1);
          //checkpointUpdate(moby);
        }

        checkpointCount++;
      }
    }

		++moby;
	}

  DPRINTF("checkpoint manager pvar size %d\n", sizeof(struct CheckpointManagerPVar));
  DPRINTF("checkpoint pvar size %d\n", sizeof(struct CheckpointPVar));
}
