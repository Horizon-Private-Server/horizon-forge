/***************************************************
 * FILENAME :		soulcollector.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the soulcollectors.
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
#include "soulcollector.h"
#include "common.h"

#define MAX_SOULCOLLECTORS      (20)

int soulcollectorCount = 0;
Moby* soulcollectorCache[MAX_SOULCOLLECTORS];

#if DEBUG
#define DLOG(moby, format, ...) if (((struct SoulCollectorPVar*)moby->PVar)->Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#else
#define DLOG(moby, format, ...) 
#endif

//--------------------------------------------------------------------------
int soulcollectorHasPoint(Moby* moby, VECTOR point)
{
  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;

  int i;
  int count = 0;
  for (i = 0; i < 4; ++i) {
    int cuboidIdx = pvars->Cuboid[i];
    if (cuboidIdx < 0) continue;

    ++count;
    SpawnPoint* sp = spawnPointGet(cuboidIdx);
    if (spawnPointIsPointInside(sp, point, NULL))
      return 1;
  }

  return count == 0;
}

//--------------------------------------------------------------------------
void soulcollectorOnSoul(VECTOR position, int collectedByPlayerId)
{
  if (collectedByPlayerId < 0) return;
  if (!soulcollectorCount) return;

  Player* player = playerGetAll()[collectedByPlayerId];
  if (!player || !playerIsConnected(player)) return;

  int i;
  for (i = 0; i < soulcollectorCount; ++i) {
    Moby* sc = soulcollectorCache[i];
    if (!sc || mobyIsDestroyed(sc) || sc->OClass != SOULCOLLECTOR_OCLASS) continue;

    if (!soulcollectorHasPoint(sc, player->PlayerPosition)) continue;

    struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)sc->PVar;
    pvars->Value++;
    pvars->TicksSinceLastSoul = 0;
  }
}

//--------------------------------------------------------------------------
void soulcollectorUpdateTargetMoby(Moby* moby)
{
  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;
  if (pvars->TargetMoby && !mobyIsDestroyed(pvars->TargetMoby)) {
    int state = moby->State == SOULCOLLECTOR_STATE_TARGET_REACHED ? pvars->TargetMobyOnState : pvars->TargetMobyOffState;
    if (pvars->TargetMoby->State != state) {
      //printf("%08X %d: %08X set state %d=>%d\n", (u32)moby, moby->UID, (u32)pvars->TargetMoby, pvars->TargetMoby->State, state);
      mobySetState(pvars->TargetMoby, state, -1);
    }
  }
}

//--------------------------------------------------------------------------
void soulcollectorActivateAll(void)
{
  if (!soulcollectorCount) return;

  int i;
  for (i = 0; i < soulcollectorCount; ++i) {
    Moby* sc = soulcollectorCache[i];
    if (!sc || mobyIsDestroyed(sc) || sc->OClass != SOULCOLLECTOR_OCLASS) continue;

    struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)sc->PVar;
    pvars->TicksSinceLastSoul = (int)maxf(0, (int)(pvars->TimeBeforeDecay * TPS) - 2);
    if (sc->State != SOULCOLLECTOR_STATE_TARGET_REACHED) {
      pvars->Value = pvars->Target;
      mobySetState(sc, SOULCOLLECTOR_STATE_TARGET_REACHED, -1);
      soulcollectorUpdateTargetMoby(sc);
    }
  }
}

//--------------------------------------------------------------------------
void soulcollectorBroadcastState(Moby* moby, enum SoulCollectorState state, int value)
{
  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, SOULCOLLECTOR_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
    guberEventWrite(guberEvent, &value, 4);
    guberEventWrite(guberEvent, &pvars->TicksSinceLastSoul, 4);
  }

  pvars->Value = value;
  mobySetState(moby, state, -1);
}

//--------------------------------------------------------------------------
void soulcollectorOnStateChanged(Moby* moby)
{
  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;

  switch (moby->State)
  {
    case SOULCOLLECTOR_STATE_DEACTIVATED:
    case SOULCOLLECTOR_STATE_RESET:
    {
      // reset on deactivated
      pvars->Value = 0;
      break;
    }
    case SOULCOLLECTOR_STATE_ACTIVATED:
    {
      break;
    }
  }

}

//--------------------------------------------------------------------------
void soulcollectorUpdate(Moby* moby)
{
  int i;
  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    soulcollectorOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  // 
  if (moby->State == SOULCOLLECTOR_STATE_DEACTIVATED)
    return;

  // state changes, host only
  if (gameAmIHost()) {

    // check if completed
    if (moby->State == SOULCOLLECTOR_STATE_ACTIVATED && pvars->Value >= pvars->Target) {
      DLOG(moby, "soulcollector %08X reached target\n", (u32)moby);
      soulcollectorBroadcastState(moby, SOULCOLLECTOR_STATE_TARGET_REACHED, pvars->Value);
    }

    // check if not completed
    if (moby->State == SOULCOLLECTOR_STATE_TARGET_REACHED && pvars->Value < pvars->Target) {
      DLOG(moby, "soulcollector %08X not reached target\n", (u32)moby);
      soulcollectorBroadcastState(moby, SOULCOLLECTOR_STATE_ACTIVATED, pvars->Value);
    }

    if (moby->State == SOULCOLLECTOR_STATE_RESET) {
      if (gameAmIHost()) soulcollectorBroadcastState(moby, SOULCOLLECTOR_STATE_ACTIVATED, 0);
      return;
    }

    // sync periodically
    if (gameAmIHost() && (moby->State == SOULCOLLECTOR_STATE_ACTIVATED || moby->State == SOULCOLLECTOR_STATE_TARGET_REACHED) && pvars->SyncTicker == 0) {
      soulcollectorBroadcastState(moby, moby->State, pvars->Value);
      pvars->SyncTicker = 3 * TPS;
    }

    // 
    if (pvars->SyncTicker) --pvars->SyncTicker;
  }

  // decay
  ++pvars->TicksSinceLastSoul;
  if (pvars->Value > 0 && pvars->DecayRate > 0 && (pvars->TimeBeforeDecay * TPS) < pvars->TicksSinceLastSoul) {
    pvars->DecayCounter += pvars->DecayRate * MATH_DT;
    if (pvars->DecayCounter >= 1) {
      int amt = (int)pvars->DecayCounter;
      pvars->Value -= amt;
      pvars->DecayCounter -= amt;
      if (pvars->Value <= 0) {
        pvars->Value = 0;
        pvars->DecayCounter = 0;
      }
      if (gameAmIHost()) {
        soulcollectorBroadcastState(moby, moby->State, pvars->Value);
        pvars->SyncTicker = 3 * TPS;
      }
    }
  }
}

//--------------------------------------------------------------------------
void soulcollectorOnGuberCreated(Moby* moby)
{
  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;

  moby->PUpdate = &soulcollectorUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
  pvars->TargetMoby = mobyGetFromIdxOrNull((int)pvars->TargetMoby);
  pvars->DecayCounter = 0;
  pvars->TicksSinceLastSoul = 0;

  if (soulcollectorCount < MAX_SOULCOLLECTORS) {
    soulcollectorCache[soulcollectorCount++] = moby;
  } else {
    DPRINTF("REACHED MAX SOULCOLLECTORS uid:%d %08X\n", moby->UID, (u32)moby);
  }

  DLOG(moby, "FOUND TARGET %08X\n", pvars->TargetMoby);
  mobySetState(moby, pvars->DefaultState, -1);
}

//--------------------------------------------------------------------------
int soulcollectorHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  if (!moby || !moby->PVar)
    return 0;

  struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &state, 4);
	guberEventRead(event, &pvars->Value, 4);
	guberEventRead(event, &pvars->TicksSinceLastSoul, 4);
  mobySetState(moby, state, -1);
  soulcollectorUpdateTargetMoby(moby);
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* soulcollectorGetGuber(Moby* moby)
{
	if (moby->OClass == SOULCOLLECTOR_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int soulcollectorHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == SOULCOLLECTOR_OCLASS && moby->PVar) {
		u32 eventId = event->NetEvent.EventID;

		switch (eventId)
		{
      case SOULCOLLECTOR_EVENT_SET_STATE: { return soulcollectorHandleEvent_SetState(moby, event); }
			default:
			{
				DLOG(moby, "unhandle soulcollector event %d\n", eventId);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void soulcollectorFrameUpdate(void)
{
  int i,j;
  for (i = 0; i < soulcollectorCount; ++i) {
    Moby* sc = soulcollectorCache[i];

    if (sc && !mobyIsDestroyed(sc) && sc->OClass == SOULCOLLECTOR_OCLASS && sc->State != SOULCOLLECTOR_STATE_DEACTIVATED) {
          
      // print
      struct SoulCollectorPVar* pvars = (struct SoulCollectorPVar*)sc->PVar;
      if (pvars->PrintCuboid >= 0) {
        SpawnPoint* sp = spawnPointGet(pvars->PrintCuboid);
        
        for (j = 0; j < GAME_MAX_LOCALS; ++j) {
          Player* player = playerGetFromSlot(j);
          if (!player) continue;
          if (gameIsStartMenuOpen(j)) continue;

          if (spawnPointIsPointInside(sp, player->PlayerPosition, NULL)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Souls Captured \x0E%d\x08/%d", pvars->Value, pvars->Target);
            gfxHelperDrawText(SCREEN_WIDTH / 2, 60, 0, -10, 1, 0x80FFFFFF, buf, -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
          }
        }
      }

    }
  }
}

//--------------------------------------------------------------------------
void soulcollectorStart(void)
{

}

//--------------------------------------------------------------------------
void soulcollectorInit(void)
{
  Moby* temp = mobySpawn(SOULCOLLECTOR_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("SOULCOLLECTOR oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for soulcollectors
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, SOULCOLLECTOR_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found soulcollector %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        soulcollectorOnGuberCreated(moby);
      }
    }

		++moby;
	}

  DPRINTF("soulcollector pvar size %d\n", sizeof(struct SoulCollectorPVar));
}
