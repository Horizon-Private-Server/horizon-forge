
/***************************************************
 * FILENAME :		ammosupply.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the ammo supply machine.
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
#include <libdl/radar.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "utils.h"
#include "maputils.h"
#include "ammosupply.h"

//--------------------------------------------------------------------------
int ammosupplyGetCost(Moby* moby, Player* player, int gadgetId)
{
  if (!moby || !player)
    return 0;

  struct AmmoSupplyPVar* pvars = (struct AmmoSupplyPVar*)moby->PVar;
  int maxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, gadgetId);
  int currentAmmo = player->GadgetBox->Gadgets[gadgetId].Ammo;
  int purchaseAmmoAmt = maxAmmo - currentAmmo;
  if (purchaseAmmoAmt <= 0) return 0;
  
  // convert gadget id to index in cost array
  int slotId = weaponIdToSlot(gadgetId) - 1;
  if (slotId < 0 || slotId > (WEAPON_SLOT_COUNT-1)) return 0;

  // get base cost per ammo
  int baseCost = pvars->BaseCost[slotId];
  if (player->GadgetBox->Gadgets[gadgetId].Level >= 9)
    baseCost = pvars->BaseCostV10[slotId];

  return baseCost * purchaseAmmoAmt;
}

//--------------------------------------------------------------------------
void ammosupplyUpdate(Moby* moby)
{
  int i;
  char buf[48];
  if (!moby || !moby->PVar)
    return;
    
  struct AmmoSupplyPVar* pvars = (struct AmmoSupplyPVar*)moby->PVar;

  // find local players to activate
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;

    int weaponId = player->WeaponHeldId;
    int cost = ammosupplyGetCost(moby, player, weaponId);
    if (cost <= 0) continue;

    snprintf(buf, sizeof(buf), "\x11 Refill Ammo [\x0E%'d\x08]", cost);
    if (tryPlayerInteract(moby, player, buf, NULL, cost, 0, 30, 9, PAD_CIRCLE)) {
      player->GadgetBox->Gadgets[weaponId].Ammo = playerGetWeaponMaxAmmo(player->GadgetBox, weaponId);
      MapConfig.State->PlayerStates[player->PlayerId].State.Bolts -= cost;
      playPaidSound(player);
      break;
    }
  }
}

//--------------------------------------------------------------------------
void ammosupplyInit(void)
{
  // set update func
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, AMMO_SUPPLY_OCLASS)))
	{
		if (!mobyIsDestroyed(moby)) {
      moby->PUpdate = &ammosupplyUpdate;
      moby->ModeBits &= ~MOBY_MODE_BIT_NO_UPDATE;
      DPRINTF("ammosupply found %08X\n", (u32)moby);
    }

		++moby;
	}
}
