/***************************************************
 * FILENAME :		maputils.c
 *
 * DESCRIPTION :
 *
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include "messageid.h"
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/hud.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/collision.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "game.h"
#include "mob.h"
#include "gate.h"

extern char LocalPlayerStrBuffer[2][64];
extern struct SurvivalMapConfig MapConfig;

/*
 * paid sound def
 */
SoundDef PaidSoundDef =
		{
				0.0,	// MinRange
				20.0, // MaxRange
				100,	// MinVolume
				2000, // MaxVolume
				0,		// MinPitch
				0,		// MaxPitch
				0,		// Loop
				0x10, // Flags
				32,		// Index
				3			// Bank
};

//--------------------------------------------------------------------------
void playPaidSound(Player *player)
{
	if (!player)
		return;
	soundPlay(&PaidSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
int tryPlayerInteract(Moby *moby, Player *player, char *message, char *lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns)
{
	static int shown[GAME_MAX_LOCALS] = {0, 0};
	VECTOR delta;
	if (!player || !player->PlayerMoby || !player->IsLocal || !isInGame())
		return 0;

	struct SurvivalPlayer *playerData = NULL;
	int localPlayerIndex = player->LocalPlayerIndex;
	int pIndex = player->PlayerId;

	if (MapConfig.State)
	{
		playerData = &MapConfig.State->PlayerStates[pIndex];
	}

	vector_subtract(delta, player->PlayerPosition, moby->Position);
	if (vector_sqrmag(delta) < sqrDistance)
	{

		// draw help popup
		snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), message);
		uiShowPopup(player->LocalPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
		shown[localPlayerIndex] = gameGetTime();

		// handle pad input
		if (padGetAnyButtonDown(localPlayerIndex, btns) > 0 && (!playerData || (playerData->State.Bolts >= boltCost && playerData->State.CurrentTokens >= tokenCost)))
		{
			if (playerData)
			{
				// playerData->State.Bolts -= boltCost;
				// playerData->State.CurrentTokens -= tokenCost;
				playerData->ActionCooldownTicks = actionCooldown;
				playerData->MessageCooldownTicks = 5;
			}

			return 1;
		}

		// draw lower message
		if (lowerMessage)
		{
			char *a = uiMsgString(0x2415);
			strncpy(a, lowerMessage, 0x40);
			uiShowLowerPopup(0, 0x2415);
		}
	}
	else if (shown[localPlayerIndex] && gameGetTime() > shown[localPlayerIndex])
	{
		hudHidePopup();
		shown[localPlayerIndex] = 0;
	}

	return 0;
}

//--------------------------------------------------------------------------
int mobyIsMob(Moby *moby)
{
	if (!moby)
		return 0;

	int i;
	for (i = 0; i < MapConfig.DefaultSpawnParamsCount; ++i)
	{
		if (MapConfig.DefaultSpawnParams[i].OClass == moby->OClass)
			return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
int playerGetStackableCount(int playerId, int stackable)
{
	if (!MapConfig.State)
		return 0;

	return MapConfig.State->PlayerStates[playerId].State.ItemStackable[stackable];
}

//--------------------------------------------------------------------------
void playerTeleportToSpawn(Player *player, float dealtDamagePercentOfMaxHealth)
{
	VECTOR p, r, o;
	float damage = player->MaxHealth * dealtDamagePercentOfMaxHealth;

	// get spawn point
	if (!MapConfig.Functions.OnPlayerGetResFunc || !MapConfig.Functions.OnPlayerGetResFunc(player, p, r, 0))
		playerGetSpawnpoint(player, p, r, 0);

	// spawn around point, to avoid risk of multiple players spawning on top of one another
	vector_fromyaw(o, (player->PlayerId / (float)GAME_MAX_PLAYERS) * MATH_TAU - MATH_PI);
	vector_scale(o, o, 2.5);
	vector_add(p, p, o);

	// teleport, damage, and set inv timer
	playerSetPosRot(player, p, r);
	playerSetHealth(player, maxf(0, player->Health - damage));
	player->timers.invincibilityTimer = PLAYER_RESPAWN_INV_TICKS;
}

//--------------------------------------------------------------------------
int bakedSpawnGetFirst(int bakedSpawnType, VECTOR outPos, VECTOR outRot)
{
	int i;
	for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i)
	{
		if (bakedConfig.BakedSpawnPoints[i].Type == bakedSpawnType)
		{

			memcpy(outPos, bakedConfig.BakedSpawnPoints[i].Position, 12);
			memcpy(outRot, bakedConfig.BakedSpawnPoints[i].Rotation, 12);
			return 1;
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int localPlayerHasInput(void)
{
	Player *localPlayer = playerGetFromSlot(0);
	if (!localPlayer)
		return 0;

	return !localPlayer->timers.noInput && !gameIsStartMenuOpen(0) && (!MapConfig.State || !MapConfig.State->PlayerStates[localPlayer->PlayerId].IsInWeaponsMenu);
}

//--------------------------------------------------------------------------
Moby *mapOnGuberEventCreateMoby(int oclass, int pvarSize)
{
	if (mobyGetNumSpawnableMobys() < 50)
	{
		Moby *m = mobyFindNextByOClass(mobyListGetStart(), 0x13A1);
		if (!m)
			return NULL;
		if (m)
		{
			mobyDestroy(m);
			m->CollCnt = 0; // lets the moby be reused instantly
		}
	}

	return mobySpawn(oclass, pvarSize);
}

//--------------------------------------------------------------------------
void mapApplyFixes(void)
{
	// HOOK_JAL(0x0061c3ec, &mapOnGuberEventCreateMoby);
}

//--------------------------------------------------------------------------
void mapPrintGambit(int gambit)
{
	char gambitName[32];
	if (!gambit)
		return;

	// read ex data
	char exDataBuf[1024];
	if (PATCH_INTEROP->ReadCustomMapExtraData(PATCH_INTEROP->MapLoaderFilename, exDataBuf, sizeof(exDataBuf), CUSTOM_MODE_SURVIVAL) > 8)
	{
		int gambitCount = *(int *)(exDataBuf + 4);
		char *gambits = (char *)(exDataBuf + 8);

		int i;
		for (i = 0; i < gambitCount; ++i)
		{
			if ((i + 1) == gambit)
			{
				strncpy(gambitName, gambits, sizeof(gambitName));
				break;
			}

			gambits += strlen(gambits) + 1;
			gambits += strlen(gambits) + 1;
		}
	}

	// show user
	snprintf(exDataBuf, sizeof(exDataBuf), "Gambit \x0E%s\x08 Active", gambitName);
	pushSnack(0, exDataBuf, TPS * 10);
	pushSnack(1, exDataBuf, TPS * 10);
}

//--------------------------------------------------------------------------
void mapSendSendGambitCompletedMessage(int gambit)
{
	if (!gambit)
		return;

	void *lobbyConnection = netGetLobbyServerConnection();
	if (!lobbyConnection)
		return;

	UpdateSurvivalGambitCompletedRequest_t msg = {
			.GambitIdx = gambit};

	// read ex data
	char exDataBuf[1024];
	if (PATCH_INTEROP->ReadCustomMapExtraData(PATCH_INTEROP->MapLoaderFilename, exDataBuf, sizeof(exDataBuf), CUSTOM_MODE_SURVIVAL) > 8)
	{
		int gambitCount = *(int *)(exDataBuf + 4);
		char *gambits = (char *)(exDataBuf + 8);

		int i;
		for (i = 0; i < gambitCount; ++i)
		{
			if ((i + 1) == gambit)
			{
				strncpy(msg.GambitName, gambits, sizeof(msg.GambitName));
				break;
			}

			gambits += strlen(gambits) + 1;
			gambits += strlen(gambits) + 1;
		}
	}

	// send request to server
	strncpy(msg.MapFilename, PATCH_INTEROP->MapLoaderFilename, sizeof(msg.MapFilename));
	netSendCustomAppMessage(NET_DELIVERY_CRITICAL, lobbyConnection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_CLIENT_UPDATE_SURVIVAL_GAMBIT_COMPLETED_REQUEST, sizeof(msg), &msg);

	// show user
	snprintf(exDataBuf, sizeof(exDataBuf), "Completed Gambit %s!", msg.GambitName);
	pushSnack(0, exDataBuf, TPS * 10);
	pushSnack(1, exDataBuf, TPS * 10);
}

//--------------------------------------------------------------------------
void mapLocalPlayerEnforceSingleWeaponRestriction(int localPlayerIdx, int weaponId, int hard)
{
	Player *player = playerGetFromSlot(localPlayerIdx);
	if (!playerIsValid(player))
		return;

	playerSetLocalEquipslot(localPlayerIdx, 0, weaponId);
	playerSetLocalEquipslot(localPlayerIdx, 1, 0);
	playerSetLocalEquipslot(localPlayerIdx, 2, 0);

	int s;
	for (s = WEAPON_SLOT_VIPERS; s < WEAPON_SLOT_COUNT; ++s)
	{
		int gadget = weaponSlotToId(s);
		if (gadget == weaponId)
		{
			if (player->GadgetBox->Gadgets[gadget].Level < 0)
				playerGiveWeapon(player->GadgetBox, gadget, 0, 1);
		}
		else
		{
			if (hard)
			{
				player->GadgetBox->Gadgets[gadget].Level = -1;
				player->GadgetBox->Gadgets[gadget].Ammo = 0;
			}

			if (player->WeaponHeldId == gadget)
			{
				player->ChangeWeaponHeldId = weaponId;
			}
		}
	}
}

//--------------------------------------------------------------------------
void mapEnforceSingleWeaponRestriction(int weaponId)
{
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		mapLocalPlayerEnforceSingleWeaponRestriction(i, weaponId, 1);
	}
}
