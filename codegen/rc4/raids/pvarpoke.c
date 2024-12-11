/***************************************************
 * FILENAME :		pvarpoke.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the pvarpokes.
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
#include "shared.h"
#include "spawner.h"
#include "gate.h"
#include "mover.h"
#include "pvarpoke.h"
#include "mob.h"
#include "game.h"

#define DLOG(moby, format, ...) if (((struct PVarPokePVar*)moby->PVar)->Log) { DPRINTF(format, ##__VA_ARGS__); }

//--------------------------------------------------------------------------
void pvarpokeOnStateChanged(Moby* moby)
{

}

//--------------------------------------------------------------------------
void pvarpokeUpdate(Moby* moby)
{
  int i;
  struct PVarPokePVar* pvars = (struct PVarPokePVar*)moby->PVar;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    pvarpokeOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  if (moby->State != PVARPOKE_STATE_ACTIVATED)
    return;
 
  // run once
  DLOG(moby, "%08X at +%04X (count=%d)\n", pvars->Target, pvars->PVarOffset, pvars->DataSize);

  if (pvars->DataSize > 0 && pvars->Target && pvars->Target->PVar) {
    memcpy(pvars->Target->PVar + pvars->PVarOffset, pvars->Buffer, pvars->DataSize);
  }

  mobySetState(moby, PVARPOKE_STATE_DEACTIVATED, -1);
}

//--------------------------------------------------------------------------
void pvarpokeStart(void)
{
  
}

//--------------------------------------------------------------------------
void pvarpokeInit(void)
{
  int i;

  // set update functions
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, PVARPOKE_OCLASS)))
	{
    struct PVarPokePVar* pvars = (struct PVarPokePVar*)moby->PVar;
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      DLOG(moby, "found pvarpoke %08X\n", (u32)moby);
      
      pvars->Target = mobyGetFromIdxOrNull((int)pvars->Target);
      if (pvars->MobyDerefs[0]) {
        int* buf = (int*)&pvars->Buffer[(pvars->MobyDerefs[0]-1) * 4];
        *(Moby**)buf = mobyGetFromIdxOrNull(*buf);
      }

      moby->PUpdate = &pvarpokeUpdate;
      moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
      mobySetState(moby, pvars->DefaultState, -1);
    }

		++moby;
	}

  DPRINTF("pvarpoke pvar size %d\n", sizeof(struct PVarPokePVar));
}
