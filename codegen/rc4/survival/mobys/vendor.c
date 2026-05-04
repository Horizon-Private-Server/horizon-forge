/***************************************************
 * FILENAME :		vendor.c
 *
 * DESCRIPTION :
 * 		Handles logic for the weapon vendor.
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
#include "mobys/vendor.h"
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "maputils.h"

#define WEAPON_VENDOR_MAX_DIST (3)
#define WEAPON_UPGRADE_COOLDOWN_TICKS (15)

const char *SURVIVAL_UPGRADE_MESSAGE = "\x11 Upgrade [\x0E%s\x08]";

//--------------------------------------------------------------------------
void vendorUpdate(Moby *moby)
{
	int i;

	// check disabled
	if (moby->State != VENDOR_STATE_ACTIVATED || moby->DrawDist <= 0)
		return;

	// add blip
	addRadarBlip(moby, 31, 4, TEAM_GREEN);

	// check for any locals that are nearby
	// prompt to open menu
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		if (!localPlayerHasInput())
			continue;

		struct SurvivalPlayer *playerData = &MapConfig.State->PlayerStates[player->PlayerId];

		// check if can upgrade weapon
		int heldWeapon = player->WeaponHeldId;
		GadgetBox *gBox = player->GadgetBox;
		int level = gBox->Gadgets[(int)heldWeapon].Level;
		if (!MapConfig.Functions.CanUpgradePlayerWeaponFunc || !MapConfig.Functions.CanUpgradePlayerWeaponFunc(player, heldWeapon, level))
			continue;

		if (vector_sqrdistance(moby->Position, player->PlayerPosition) > (WEAPON_VENDOR_MAX_DIST * WEAPON_VENDOR_MAX_DIST))
			continue;

		// get upgrade cost
		int cost = 0;
		if (MapConfig.Functions.GetUpgradePlayerWeaponCostFunc)
			cost = MapConfig.Functions.GetUpgradePlayerWeaponCostFunc(player, heldWeapon, level);

		// prompt for payment if near
		char buf[64];
		char costBuf[32];
		uiPrintCommaNumber(costBuf, sizeof(costBuf), cost, 0);
		snprintf(buf, sizeof(buf), SURVIVAL_UPGRADE_MESSAGE, costBuf);
		if (!tryPlayerInteract(moby, player, buf, NULL, cost, 0, WEAPON_UPGRADE_COOLDOWN_TICKS, WEAPON_VENDOR_MAX_DIST * WEAPON_VENDOR_MAX_DIST, PAD_CIRCLE, 1))
			continue;

		// charge
		playerData->State.Bolts -= cost;
		playerData->MessageCooldownTicks = 0;
		uiShowPopup(i, uiMsgString(0x2330));
		playPaidSound(player);

		// upgrade
		if (MapConfig.Functions.ModeUpgradePlayerWeaponFunc)
			MapConfig.Functions.ModeUpgradePlayerWeaponFunc(player->PlayerId, heldWeapon);

		// give reward
		if (MapConfig.Functions.OnPlayerGetVendorRewardFunc)
			MapConfig.Functions.OnPlayerGetVendorRewardFunc(player, heldWeapon, level + 1);
	}
}

//--------------------------------------------------------------------------
void vendorInit(void)
{
	// update vendor update function
	Moby *moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, VENDOR_MOBY_OCLASS)))
	{
		if (!mobyIsDestroyed(moby))
		{
			struct VendorPVar *pvars = (struct VendorPVar *)moby->PVar;
			DPRINTF("found vendor %08X\n", (u32)moby);
			moby->PUpdate = &vendorUpdate;
			moby->ModeBits = 0x50;
			moby->Bangles = 0x8;
			moby->UpdateDist = -1;

			if (pvars)
				mobySetState(moby, pvars->DefaultState, -1);
		}

		++moby;
	}
}
