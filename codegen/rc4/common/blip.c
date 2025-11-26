/***************************************************
 * FILENAME :		blip.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the blips.
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
#include <libdl/radar.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "blip.h"

//--------------------------------------------------------------------------
void blipUpdate(Moby* moby)
{
  struct BlipPVar* pvars = (struct BlipPVar*)moby->PVar;

  // attach to moby
  if (pvars->AttachToMoby && !mobyIsDestroyed(pvars->AttachToMoby)) {
    vector_copy(moby->Position, pvars->AttachToMoby->Position);
  }

  // draw on radar
  if (moby->State) {
    int blipIdx = radarGetBlipIndex(moby);
    if (blipIdx >= 0) {
      RadarBlip* blip = radarGetBlips() + blipIdx;
      blip->X = moby->Position[0];
      blip->Y = moby->Position[1];
      blip->Life = 0x1F;
      blip->Type = pvars->BlipType;
      blip->Team = pvars->BlipTeam;
      blip->Moby = moby;
    }
  }
}

//--------------------------------------------------------------------------
void blipInit(void)
{
  // set update function for blips
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, BLIP_OCLASS)))
	{
		if (!mobyIsDestroyed(moby)) {
      struct BlipPVar* pvars = (struct BlipPVar*)moby->PVar;
      DPRINTF("found blip %08X\n", (u32)moby);

      pvars->AttachToMoby = mobyGetFromIdxOrNull((int)pvars->AttachToMoby);
      moby->PUpdate = blipUpdate;
      moby->ModeBits &= ~MOBY_MODE_BIT_NO_UPDATE;
      mobySetState(moby, pvars->DefaultState, -1);
    }

		++moby;
	}
}
