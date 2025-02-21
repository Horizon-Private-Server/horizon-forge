/***************************************************
 * FILENAME :		collectible.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the gold bolts.
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
#include "collectible.h"

//--------------------------------------------------------------------------
void collectibleCollect(Moby* moby)
{
  struct CollectiblePVar* pvars = (struct CollectiblePVar*)moby->PVar;
  int index = pvars->Index;
  int hasCollected = (MapConfig.State->CurrentMapStats.CollectiblesMask & (1 << index)) != 0;

  if (!hasCollected) {

    // mark collected
    MapConfig.State->CurrentMapStats.CollectiblesMask |= (1 << index);

    // play sound
    mobyPlaySoundByClass(1, 0, moby, MOBY_ID_WEAPON_PICKUP);

    // award bolts
    bankAddBolts(COLLECTIBLE_REWARD);

    // popup
    char strBuf[64];
    int count = countBits(MapConfig.State->CurrentMapStats.CollectiblesMask);
    snprintf(strBuf, sizeof(strBuf), "You found a Gold Bolt!\x01 Collected %d/%d", count, MapConfig.State->CurrentMapStats.CollectiblesCount);
    pushSnack(0, strBuf, 120);

    // send to server
    bankSendMapStats(&MapConfig.State->CurrentMapStats);
  }

  mobyDestroy(moby);
}

//--------------------------------------------------------------------------
void collectibleUpdate(Moby* moby)
{
  if (!MapConfig.State) return;
  if (MapConfig.State->CurrentMapStats.Invalid) return;

  struct CollectiblePVar* pvars = (struct CollectiblePVar*)moby->PVar;
  int index = pvars->Index;
  
  // check if collected
  int hasCollected = (MapConfig.State->CurrentMapStats.CollectiblesMask & (1 << index)) != 0;
  if (hasCollected) {
    moby->Opacity = 0x20;
    moby->State = 1;
  }

  // spin
  moby->Rotation[1] = -MATH_PI / 4;
  moby->Rotation[2] = clampAngle(moby->Rotation[2] + MATH_DT*MATH_PI*0.5);

  // bob
  moby->Position[2] = moby->AnimSpeed + 0.25*sinf(gameGetTime() / 500.0);

  // check if player has reached bolt
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* local = playerGetFromSlot(i);
    if (!local) break;

    if (vector_sqrdistance(moby->Position, local->PlayerPosition) < 6.25) {
      collectibleCollect(moby);
    }
  }
}

//--------------------------------------------------------------------------
void collectibleInit(void)
{
  void* boltMobyClass = mobyGetClassPtr(13);

  // set update
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, COLLECTIBLE_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      DPRINTF("found collectible %08X\n", (u32)moby);
      moby->PUpdate = &collectibleUpdate;
      moby->PClass = boltMobyClass;
      moby->CollData = *(int*)((u32)boltMobyClass + 0x10);
      moby->MClass = *(u8*)(0x0024a110 + 13);
      moby->AnimSeq = *(void**)((u32)boltMobyClass + 0x48);
      moby->AnimSeqId = 0;
      moby->AnimSpeed = 1;
      moby->JointCnt = *(char*)((u32)boltMobyClass + 0x08);
      moby->Scale = 0.03125;
      moby->ModeBits = 0x0050;
      //((void* (*)(Moby*))0x004fb910)(moby); // init joint cache

      moby->AnimSpeed = moby->Position[2];
      moby->GlowRGBA = 0x80808080;
    }

		++moby;
	}

  DPRINTF("collectible pvar size %d\n", sizeof(struct CollectiblePVar));
}
