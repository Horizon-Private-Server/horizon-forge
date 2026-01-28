/***************************************************
 * FILENAME :		ammodrop.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the ammo drops.
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
#include <libdl/moby.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/collision.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "ammodrop.h"
#include "game.h"

Moby* ammoDropMobyList[MAX_MOB_AMMO_DROPS];
int ammoDropMobyListRollingIndex = 0;

//--------------------------------------------------------------------------
Player* ammodropFindPlayerFromGadgetBox(GadgetBox* gbox)
{
  Player** players = playerGetAll();

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player)) continue;

    if (player->GadgetBox == gbox) return player;
  }
}

//--------------------------------------------------------------------------
int ammodropTargetGetGadgetMaxAmmo(GadgetBox* gbox, int gadgetId)
{
  Player* player = ammodropFindPlayerFromGadgetBox(gbox);
  if (!playerIsValid(player)) return 0;
  if (!player->IsLocal) return 0; // local only

  return playerGetWeaponMaxAmmo(gbox, gadgetId);
}

//--------------------------------------------------------------------------
void ammodropUpdate(Moby* moby)
{
  // configure it to be pickup-able
  if (moby->PVar) {
    *(char*)(moby->PVar + 0x5C) = 1; // state
  }

  ((void (*)(Moby*))0x003ac050)(moby);
}

//--------------------------------------------------------------------------
void ammodropCreateAt(Moby* moby)
{
  // find free slot
  int i;
  for (i = 0; i < MAX_MOB_AMMO_DROPS; ++i) {
    if (!ammoDropMobyList[i] || mobyIsDestroyed(ammoDropMobyList[i]) || ammoDropMobyList[i]->OClass != MOBY_ID_AMMO) break;
  }

  // use rolling index
  if (i >= MAX_MOB_AMMO_DROPS) {
    i = ammoDropMobyListRollingIndex;
  }

  // destroy old ammo drop
  if (ammoDropMobyList[i] && ammoDropMobyList[i]->OClass == MOBY_ID_AMMO) {
    mobyDestroy(ammoDropMobyList[i]);
  }

  ammoDropMobyListRollingIndex = (i + 1) % MAX_MOB_AMMO_DROPS;
  Moby* ammoMoby = ammoDropMobyList[i] = mobySpawn(MOBY_ID_AMMO, 0x100);
  if (ammoMoby) {
    
    // snap to ground
    VECTOR from = {0,0,1,0};
    VECTOR to = {0,0,-10,0};
    vector_copy(ammoMoby->Position, moby->Position);
    vector_add(from, moby->Position, from);
    vector_add(to, moby->Position, to);
    if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
      vector_copy(ammoMoby->Position, CollLine_Fix_GetHitPosition());
    }

    // update draw
    ammoMoby->Bangles |= 1;
    ammoMoby->DrawDist = 255;
    ammoMoby->UpdateDist = 255;
    ammoMoby->PUpdate = ammodropUpdate;
    
    // configure it to be pickup-able
    if (ammoMoby->PVar) {
      *(float*)(ammoMoby->PVar + 0x2C) = 2; // radius?
      *(char*)(ammoMoby->PVar + 0x5C) = 1; // state
    }
  }
}

//--------------------------------------------------------------------------
void ammodropInit(void)
{
  memset(ammoDropMobyList, 0, sizeof(ammoDropMobyList));
  ammoDropMobyListRollingIndex = 0;

  // force ammo drops to local players only
  POKE_U32(0x003ACC24, 0x8E351A9C);
  HOOK_JAL(0x003aca8c, &ammodropTargetGetGadgetMaxAmmo);

  // double ammo pickup amount
  POKE_F32(0x003978C0, 0.3);

  // disable b6 halving of ammo amount
  POKE_U16(0x003ac2c4, 0x0);
}
