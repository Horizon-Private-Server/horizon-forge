/***************************************************
 * FILENAME :		hackerorb.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the hackerorbs.
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
#include "hackerorb.h"

//--------------------------------------------------------------------------
void hackerorbBroadcastState(Moby* moby)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, HACKERORB_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &moby->State, 1);
  }
}

//--------------------------------------------------------------------------
void hackerorbUpdate(Moby* moby)
{
  // detect when state was changed
  if ((moby->Triggers & 1) == 0 && moby->Mission != moby->State) {
    if (moby->PrevState != 0 && (moby->State == HACKERORB_STATE_CAPTURED || moby->State == HACKERORB_STATE_UNCAPTURED)) {
      DPRINTF("hackerorb %08X send state %d => %d\n", (u32)moby, moby->PrevState, moby->State);
      hackerorbBroadcastState(moby);
    }
    moby->Mission = moby->State;
  }

  // call base update
  ((void (*)(Moby*))0x00426680)(moby);
}

//--------------------------------------------------------------------------
int hackerorbHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  char state;
  if (!moby || !moby->PVar)
    return 0;

	// read event
	guberEventRead(event, &state, 1);
  if (moby->State != state) {
    DPRINTF("hackerorb %08X recv state %d => %d\n", (u32)moby, moby->State, state);
    mobySetState(moby, state, -1);
    moby->Mission = moby->State;
  }
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* hackerorbGetGuber(Moby* moby)
{
	if (moby->OClass == MOBY_ID_HACKER_ORB && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int hackerorbHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == MOBY_ID_HACKER_ORB && moby->PVar) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
      case HACKERORB_EVENT_SET_STATE: { return hackerorbHandleEvent_SetState(moby, event); }
			default:
			{
				DPRINTF("unhandle hacker orb event %d\n", upgradeEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void hackerorbInit(void)
{
  Moby* temp = mobySpawn(MOBY_ID_HACKER_ORB, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("HACKERORB oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for hacker orbs
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, MOBY_ID_HACKER_ORB)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DPRINTF("found hacker orb %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        moby->PUpdate = &hackerorbUpdate;
      }
    }

		++moby;
	}
}
