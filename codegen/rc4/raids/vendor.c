/***************************************************
 * FILENAME :		vendor.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the vendors.
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
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "vendor.h"
#include "game.h"

extern char LocalPlayerStrBuffer[GAME_MAX_LOCALS][64];

//--------------------------------------------------------------------------
void vendorUpdate(Moby* moby)
{
  if (!MapConfig.State)
    return;

  VECTOR dt;
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player) || playerIsDead(player))
      continue;
    
	  struct RaidsPlayer * playerData = &MapConfig.State->PlayerStates[player->PlayerId];
    if (playerData->ActionCooldownTicks == 0) {
      vector_subtract(dt, player->PlayerPosition, moby->Position);
      if (vector_sqrmag(dt) < (VENDOR_INTERACT_RADIUS * VENDOR_INTERACT_RADIUS)) {
        
        // get cost
        int cost = getAmmoRefillCost(player);
        if (cost >= 0) {

          // draw help popup
          snprintf(LocalPlayerStrBuffer[i], sizeof(LocalPlayerStrBuffer[i]), "\x11 Refill Ammo \x0E%'d", cost);
          uiShowPopup(i, LocalPlayerStrBuffer[i]);
          playerData->MessageCooldownTicks = 3;

          if (padGetButtonDown(i, PAD_CIRCLE) > 0 && bankTryChargeLocalAccount(player, cost)) {
            playerData->ActionCooldownTicks = TPS;
            replenishAmmo(player);
          }
        }
      }
    }

  }
}

//--------------------------------------------------------------------------
void vendorStart(void)
{
  
}

//--------------------------------------------------------------------------
void vendorInit(void)
{
  // set update function for vendors
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, VENDOR_OCLASS)))
	{
		if (!mobyIsDestroyed(moby)) {
      DPRINTF("found vendor %08X\n", (u32)moby);

      moby->PUpdate = vendorUpdate;
      moby->ModeBits &= ~MOBY_MODE_BIT_NO_UPDATE;
    }

		++moby;
	}
}
