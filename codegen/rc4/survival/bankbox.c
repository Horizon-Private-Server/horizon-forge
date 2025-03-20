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
int bboxCreate(VECTOR position, VECTOR rotation)
{
  Moby* moby = mobySpawn(BANK_BOX_OCLASS, sizeof(struct BankBoxPVar));
  if (!moby) return 0;

  struct BankBoxPVar* pvars = (struct BankBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  vector_copy(moby->Position, position);
  vector_copy(moby->Rotation, rotation);

	// set update
	moby->PUpdate = NULL;
  moby->DrawDist = 0x40;
  moby->UpdateDist = 0x40;
  moby->ModeBits = 0x40;
  moby->Scale = 0.08;

  // update mode reference
  if (MapConfig.State) MapConfig.State->Bankbox = moby;

  DPRINTF("bbox spawned %08X\n", (u32)moby);
  return 1;
}

//--------------------------------------------------------------------------
void bboxInit(void)
{
  
}
