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
#include <libdl/radar.h>
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
#include "maputils.h"
#include "gate.h"

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
int tryPlayerInteract(Moby *moby, Player *player, char *message, char *lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns, int hold)
{
	static int shown[GAME_MAX_LOCALS] = {0, 0};
	VECTOR delta;
	if (!player || !player->PlayerMoby || !player->IsLocal || !isInGame() || playerIsDead(player))
		return 0;

	struct SurvivalPlayer *playerData = NULL;
	int localPlayerIndex = player->LocalPlayerIndex;
	int pIndex = player->PlayerId;

	if (MapConfig.State)
		playerData = &MapConfig.State->PlayerStates[pIndex];

	int canAction = !playerData || !playerData->ActionCooldownTicks;
	if (!canAction)
		return 0;

	vector_subtract(delta, player->PlayerPosition, moby->Position);
	if (vector_sqrmag(delta) < sqrDistance)
	{
		// draw help popup
		snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), message);
		uiShowPopup(player->LocalPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
		shown[localPlayerIndex] = gameGetTime();

		// handle pad input
		int btnDown = padGetAnyButton(localPlayerIndex, btns) > 0;
		int btnDownThisFrame = padGetAnyButtonDown(localPlayerIndex, btns) > 0;
		int btnCheck = hold ? btnDown : btnDownThisFrame;
		if (canAction && btnCheck && (!playerData || (playerData->State.Bolts >= boltCost && playerData->State.CurrentTokens >= tokenCost)))
		{
			if (playerData)
			{
				playerData->ActionCooldownTicks = actionCooldown;
				playerData->MessageCooldownTicks = 2;
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
int playerGetItemCount(Player *player, int itemId)
{
	if (!MapConfig.Functions.GetPlayerItemCountFunc)
		return 0;

	return MapConfig.Functions.GetPlayerItemCountFunc(player, itemId);
}

//--------------------------------------------------------------------------
void playerGiveAlphaMod(Player *player, int alphamod)
{
	if (!playerIsValid(player))
		return;

	if (MapConfig.State)
		MapConfig.State->PlayerStates[player->PlayerId].State.AlphaMods[alphamod] += 1;
	if (player->GadgetBox)
		player->GadgetBox->ModBasic[alphamod - 1]++;
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

//--------------------------------------------------------------------------
void spawnHealthBomb(Player *fromPlayer, VECTOR position, float radius, float healPercent)
{
	int i;
	VECTOR dt;

	// heal / revive
	if (fromPlayer)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			Player *player = playerGetFromIndex(i);
			if (!playerIsValid(player))
				continue;

			vector_subtract(dt, player->PlayerPosition, position);
			if (vector_sqrmag(dt) < (radius * radius))
			{
				if (playerIsDead(player) && player->IsLocal)
				{
					if (MapConfig.Functions.ModeRevivePlayerFunc)
						MapConfig.Functions.ModeRevivePlayerFunc(player, fromPlayer->PlayerId);
				}
				else
				{
					playerSetHealth(player, clamp(player->Health + (player->MaxHealth * healPercent), 0, player->MaxHealth));
				}
			}
		}
	}

	// explode
	u32 color = 0x80800000;
	Moby *expMoby = mobySpawnExplosion(vector_read(position), 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, color, color, color, color, color, color, color, color, color, 0, 0, 0, 0, 2, 0, 0, 0);
	if (expMoby)
		mobyPlaySoundByClass(0, 0, expMoby, MOBY_ID_ARBITER_ROCKET0);

	if (fromPlayer && fromPlayer->IsLocal && fromPlayer->PlayerMoby)
		mobReactToExplosionAt(fromPlayer->PlayerMoby, position, 1, 8, 6);
}

//--------------------------------------------------------------------------
void randomizeWeaponPickups(void)
{
	int i, j;
	GameOptions *gameOptions = gameGetOptions();
	char wepCounts[9];
	char wepEnabled[17];
	int pickupCount = 0;
	int pickupOptionCount = 0;
	memset(wepEnabled, 0, sizeof(wepEnabled));
	memset(wepCounts, 0, sizeof(wepCounts));

	if (gameOptions->WeaponFlags.DualVipers)
	{
		wepEnabled[2] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.MagmaCannon)
	{
		wepEnabled[3] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.Arbiter)
	{
		wepEnabled[4] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.FusionRifle)
	{
		wepEnabled[5] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.MineLauncher)
	{
		wepEnabled[6] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.B6)
	{
		wepEnabled[7] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.Holoshield)
	{
		wepEnabled[16] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.Flail)
	{
		wepEnabled[12] = 1;
		pickupOptionCount++;
	}
	if (gameOptions->WeaponFlags.Chargeboots && gameOptions->GameFlags.MultiplayerGameFlags.SpawnWithChargeboots == 0)
	{
		wepEnabled[13] = 1;
		pickupOptionCount++;
	}

	if (pickupOptionCount > 0)
	{
		Moby *moby = mobyListGetStart();
		Moby *mEnd = mobyListGetEnd();

		while (moby < mEnd)
		{
			if (moby->OClass == MOBY_ID_WEAPON_PICKUP && moby->PVar)
			{

				int target = pickupCount / pickupOptionCount;
				int gadgetId = 1;
				if (target < 3)
				{
					do
					{
						j = rand(pickupOptionCount);
					} while (wepCounts[j] != target);

					++wepCounts[j];

					i = -1;
					do
					{
						++i;
						if (wepEnabled[i])
							--j;
					} while (j >= 0);

					gadgetId = i;
				}

				// set pickup
				((void (*)(Moby *, int))0x0043A370)(moby, gadgetId);

				++pickupCount;
			}

			++moby;
		}
	}
}

//--------------------------------------------------------------------------
void addRadarBlip(Moby *moby, int life, int type, int team)
{
	if (!moby)
		return;

	// draw on radar
	int blipIdx = radarGetBlipIndex(moby);
	if (blipIdx >= 0)
	{
		RadarBlip *blip = radarGetBlips() + blipIdx;
		blip->X = moby->Position[0];
		blip->Y = moby->Position[1];
		blip->Life = life;
		blip->Type = type;
		blip->Team = team;
	}
}