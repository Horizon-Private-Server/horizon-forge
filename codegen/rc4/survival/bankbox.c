/***************************************************
 * FILENAME :		bankbox.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the bank box.
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
#include <libdl/random.h>
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
#include "bankbox.h"
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "maputils.h"

//--------------------------------------------------------------------------
void bboxUpdate(Moby* moby)
{
  // update mode reference
  if (MapConfig.State) {
    MapConfig.State->Bankbox = moby;
  }
}

//--------------------------------------------------------------------------
void bboxInit(void)
{
  Moby* temp = mobySpawn(BANK_BOX_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("BANKBOX oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, (u32)mobyFunctionsPtr->GetGuberObject, (u32)mobyFunctionsPtr->MobyEventHandler);
  }
  mobyDestroy(temp);
  
  // create gubers for bank boxes
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, BANK_BOX_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DPRINTF("found bank box %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        moby->PUpdate = &bboxUpdate;
        moby->ModeBits = 0x40;
      }
    }

		++moby;
	}
}
