#include <libdl/ui.h>
#include <libdl/dialog.h>
#include <libdl/random.h>
#include "item.h"
#include "game.h"
#include "maputils.h"
#include "survival_items.h"

#if GATE
#include "gate.h"
#endif

#define ITEM_INVISCLOAK_DURATION (30 * TIME_SECOND)
#define ITEM_INFAMMO_DURATION (30 * TIME_SECOND)
#define ITEM_QUAD_DURATION_TPS (1 * 60 * TPS)
#define ITEM_SHIELD_DURATION_TPS (1 * 60 * TPS)
#define ITEM_EMP_HEALTH_EFFECT_RADIUS (15)
#define ITEM_HEALTHTORNADO_DURATION (10 * TIME_SECOND)
#define ITEM_HEALTHTORNADO_PERIOD_TICKS (TPS * 0.25)
#define ITEM_HEALTHTORNADO_HEAL_PERCENT (0.05)

#define PLAYER_UPGRADE_DAMAGE_FACTOR (0.08)
#define PLAYER_UPGRADE_SPEED_FACTOR (0.03)
#define PLAYER_UPGRADE_HEALTH_FACTOR (5)
#define PLAYER_UPGRADE_CRIT_FACTOR (0.01)

int InvisibilityCloakStopTime[GAME_MAX_PLAYERS] = {};
int HealthTornadoStopTime[GAME_MAX_PLAYERS] = {};
int HealthTornadoActivateTicks[GAME_MAX_PLAYERS] = {};
int InfiniteAmmoStopTime = -1;

//--------------------------------------------------------------------------
void mapOnItemTick_SelfRevive(int defIdx, SurvivalItemDef_t *def)
{
	static int init = 0;
	if (!init && MapConfig.State)
	{
		// solo run start with self revive
		GameSettings *gs = gameGetSettings();
		if (gs->PlayerCount != 1)
			return;
		if (playerGetItemCount(playerGetFromSlot(0), defIdx) > 0)
			return;

		itemBeginAcquire(0, defIdx);
		init = 1;
	}

	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		if (!playerIsDead(player) && player->Health > 0)
			continue;

		// self revive
		if (playerGetItemCount(player, defIdx) > 0)
			itemBeginConsume(player->PlayerId, defIdx);
	}
}

//--------------------------------------------------------------------------
void mapOnItemDraw_SelfRevive(int defIdx, SurvivalItemDef_t *def)
{
}

//--------------------------------------------------------------------------
void mapOnItemInit_SelfRevive(int defIdx, SurvivalItemDef_t *def)
{
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_SelfRevive(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];

	if (!MapConfig.Functions.ModeRevivePlayerFunc || !playerIsValid(player))
		return;

	// revive
	MapConfig.Functions.ModeRevivePlayerFunc(player, playerId);

	// show consumed message
	if (player->IsLocal)
		uiShowPopup(player->LocalPlayerIndex, "Self Revive Used!");
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_UpgradeWeapon(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player) || !player->IsLocal)
		return;

	if (MapConfig.Functions.ModeUpgradePlayerWeaponFunc)
		MapConfig.Functions.ModeUpgradePlayerWeaponFunc(playerId, player->WeaponHeldId);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_RandomizeWeaponPickups(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	randomizeWeaponPickups();
	pushSnack(-1, "Weapon Pickups Randomized!", TPS);
}

//--------------------------------------------------------------------------
void mapOnItemTick_HealthTornado(int defIdx, SurvivalItemDef_t *def)
{
	Player **players = playerGetAll();
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = players[i];
		if (!playerIsValid(player))
			continue;

		// handle health tornado
		if (HealthTornadoStopTime[i] > 0)
		{
			int timeUntilEnd = gameGetTime() - HealthTornadoStopTime[i];
			if (timeUntilEnd < 0)
			{
				if (!HealthTornadoActivateTicks[i])
				{
					spawnHealthBomb(player, player->PlayerPosition, ITEM_EMP_HEALTH_EFFECT_RADIUS, ITEM_HEALTHTORNADO_HEAL_PERCENT);
					HealthTornadoActivateTicks[i] = ITEM_HEALTHTORNADO_PERIOD_TICKS;
				}
				else
				{
					HealthTornadoActivateTicks[i]--;
				}
			}
			else
			{
				HealthTornadoActivateTicks[i] = 0;
				HealthTornadoStopTime[i] = -1;
			}
		}
	}
}

//--------------------------------------------------------------------------
void mapOnItemDraw_HealthTornado(int defIdx, SurvivalItemDef_t *def)
{
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_HealthTornado(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	// start
	HealthTornadoStopTime[playerId] = gameGetTime() + ITEM_HEALTHTORNADO_DURATION;
	HealthTornadoActivateTicks[playerId] = 0;

	// show consumed message
	Player *player = playerGetAll()[playerId];
	if (playerIsValid(player) && player->IsLocal)
		uiShowPopup(player->LocalPlayerIndex, "Health Tornado Activated!");
}

//--------------------------------------------------------------------------
void mapOnItemTick_InvisibilityCloak(int defIdx, SurvivalItemDef_t *def)
{
	Player **players = playerGetAll();
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = players[i];
		if (!playerIsValid(player))
			continue;

		// handle invisibility
		if (InvisibilityCloakStopTime[i] > 0)
		{
			int timeUntilEnd = gameGetTime() - InvisibilityCloakStopTime[i];
			if (timeUntilEnd < 0)
			{
				player->SkinMoby->Opacity = 0x20;
			}
			else
			{
				player->SkinMoby->Opacity = 0x80;
				InvisibilityCloakStopTime[i] = -1;
			}
		}
	}
}

//--------------------------------------------------------------------------
void mapOnItemDraw_InvisibilityCloak(int defIdx, SurvivalItemDef_t *def)
{
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_InvisibilityCloak(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	// start
	InvisibilityCloakStopTime[playerId] = gameGetTime() + ITEM_INVISCLOAK_DURATION;

	// show consumed message
	Player *player = playerGetAll()[playerId];
	if (playerIsValid(player) && player->IsLocal)
		uiShowPopup(player->LocalPlayerIndex, "Invisibility Cloak Equipped!");
}

//--------------------------------------------------------------------------
void mapOnItemTick_GlobalInfiniteAmmo(int defIdx, SurvivalItemDef_t *def)
{
	// handle inf ammo
	if (InfiniteAmmoStopTime > 0)
	{
		int timeUntilEnd = gameGetTime() - InfiniteAmmoStopTime;
		if (timeUntilEnd < 0)
		{
			gameGetOptions()->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 1;
		}
		else
		{
			gameGetOptions()->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 0;
			InfiniteAmmoStopTime = -1;

			// give everyone max ammo
			int i;
			Player **players = playerGetAll();
			for (i = 0; i < GAME_MAX_PLAYERS; ++i)
			{
				Player *player = players[i];
				if (!playerIsValid(player) || !player->GadgetBox)
					continue;

				int j = 0;
				for (j = 0; j < (WEAPON_SLOT_COUNT - 1); ++j)
				{
					int weaponId = weaponSlotToId(j + 1);
					struct GadgetEntry *gadgetEntry = &player->GadgetBox->Gadgets[weaponId];
					if (gadgetEntry->Level >= 0)
					{
						gadgetEntry->Ammo = playerGetWeaponMaxAmmo(player->GadgetBox, weaponId);
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalInfiniteAmmo(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	InfiniteAmmoStopTime = gameGetTime() + ITEM_INFAMMO_DURATION;
	pushSnack(-1, "Infinite Ammo!", TPS);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalShield(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	int i;
	Player **players = playerGetAll();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = players[i];
		if (!playerIsValid(player))
			continue;

		// increase duration by player pickup cooldown upgrade
		short duration = ITEM_SHIELD_DURATION_TPS;

		player->timers.armorLevelTimer = duration;
		player->ArmorLevel = 3;
	}

	pushSnack(-1, "Shield!", TPS);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalQuad(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	int i;
	Player **players = playerGetAll();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = players[i];
		if (!playerIsValid(player))
			continue;

		short duration = ITEM_QUAD_DURATION_TPS;
		player->timers.damageMuliplierTimer = duration;
		player->DamageMultiplier = 4;
	}

	pushSnack(-1, "Quad!", TPS);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_ResetRandomGate(int defIdx, SurvivalItemDef_t *def, int playerId)
{
#if GATE
	pushSnack(-1, "Gate reset!", 60);

	if (gameAmIHost())
	{
		gateResetRandomGate();
	}
#endif
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_MysteryboxVox(int defIdx, SurvivalItemDef_t *def, int playerId)
{
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_DreadToken(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	if (!MapConfig.State)
		return;

	MapConfig.State->PlayerStates[playerId].State.TotalTokens += 1;
	MapConfig.State->PlayerStates[playerId].State.CurrentTokens += 1;
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_Alphamod(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];

#if ITEM_ALPHAMOD_SPEED
	if (defIdx == ITEM_ALPHAMOD_SPEED)
	{
		playerGiveAlphaMod(player, ALPHA_MOD_SPEED);
	}
#endif

#if ITEM_ALPHAMOD_AREA
	if (defIdx == ITEM_ALPHAMOD_AREA)
	{
		playerGiveAlphaMod(player, ALPHA_MOD_AREA);
	}
#endif

#if ITEM_ALPHAMOD_AMMO
	if (defIdx == ITEM_ALPHAMOD_AMMO)
	{
		playerGiveAlphaMod(player, ALPHA_MOD_AMMO);
	}
#endif

#if ITEM_ALPHAMOD_IMPACT
	if (defIdx == ITEM_ALPHAMOD_IMPACT)
	{
		playerGiveAlphaMod(player, ALPHA_MOD_IMPACT);
	}
#endif

#if ITEM_ALPHAMOD_JACKPOT
	if (defIdx == ITEM_ALPHAMOD_JACKPOT)
	{
		playerGiveAlphaMod(player, ALPHA_MOD_JACKPOT);
	}
#endif

#if ITEM_ALPHAMOD_XP
	if (defIdx == ITEM_ALPHAMOD_XP)
	{
		playerGiveAlphaMod(player, ALPHA_MOD_XP);
	}
#endif
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalNuke(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	pushSnack(-1, "Nuke Activated!", TPS);
	if (MapConfig.Functions.ModeMobNukeFunc)
		MapConfig.Functions.ModeMobNukeFunc(playerId);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalAmmo(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	int i;
	Player **players = playerGetAll();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *p = players[i];
		if (playerIsValid(p))
		{
			int j;
			for (j = 0; j <= 8; ++j)
			{
				int gadgetId = weaponSlotToId(j);
				if (p->GadgetBox->Gadgets[gadgetId].Level >= 0)
					p->GadgetBox->Gadgets[gadgetId].Ammo = playerGetWeaponMaxAmmo(p->GadgetBox, gadgetId);
			}
		}
	}

	pushSnack(-1, "You Got Ammo!", TPS);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalDoublePoints(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	pushSnack(-1, "Double Bolts!", TPS);
	if (MapConfig.Functions.ModeSetDoublePointsFunc)
		MapConfig.Functions.ModeSetDoublePointsFunc(1);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalDoubleXp(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	pushSnack(-1, "Double XP!", TPS);
	if (MapConfig.Functions.ModeSetDoubleXPFunc)
		MapConfig.Functions.ModeSetDoubleXPFunc(1);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalFreeze(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	pushSnack(-1, "Freeze Activated!", TPS);
	if (MapConfig.Functions.ModeSetFreezeMobsFunc)
		MapConfig.Functions.ModeSetFreezeMobsFunc(1);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_GlobalHealth(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	int i;
	Player **players = playerGetAll();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *p = players[i];
		if (playerIsValid(p))
		{
			if (!playerIsDead(p) && p->Health > 0)
			{
				playerSetHealth(p, p->MaxHealth);
			}
			else if (MapConfig.Functions.ModeRevivePlayerFunc && MapConfig.State && MapConfig.State->PlayerStates[i].ReviveCooldownTicks)
			{
				MapConfig.Functions.ModeRevivePlayerFunc(p, playerId);
			}
		}
	}

	pushSnack(-1, "You Got Health!", TPS);
}

//--------------------------------------------------------------------------
int mapItem_WeaponUpgrade_GetWeaponIdFromItem(int defIdx)
{
	int weaponId = 0;

#if ITEM_IMMEDIATE_UPGRADE_DUAL_VIPERS
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_DUAL_VIPERS)
		weaponId = WEAPON_ID_VIPERS;
#endif

#if ITEM_IMMEDIATE_UPGRADE_MAGMA_CANNON
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_MAGMA_CANNON)
		weaponId = WEAPON_ID_MAGMA_CANNON;
#endif

#if ITEM_IMMEDIATE_UPGRADE_ARBITER
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_ARBITER)
		weaponId = WEAPON_ID_ARBITER;
#endif

#if ITEM_IMMEDIATE_UPGRADE_FUSION_RIFLE
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_FUSION_RIFLE)
		weaponId = WEAPON_ID_FUSION_RIFLE;
#endif

#if ITEM_IMMEDIATE_UPGRADE_MINE_LAUNCHER
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_MINE_LAUNCHER)
		weaponId = WEAPON_ID_MINE_LAUNCHER;
#endif

#if ITEM_IMMEDIATE_UPGRADE_B6_OBLITERATOR
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_B6_OBLITERATOR)
		weaponId = WEAPON_ID_B6;
#endif

#if ITEM_IMMEDIATE_UPGRADE_HOLOSHIELD
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_HOLOSHIELD)
		weaponId = WEAPON_ID_OMNI_SHIELD;
#endif

#if ITEM_IMMEDIATE_UPGRADE_SCORPION_FLAIL
	if (defIdx == ITEM_IMMEDIATE_UPGRADE_SCORPION_FLAIL)
		weaponId = WEAPON_ID_FLAIL;
#endif

	return weaponId;
}

//--------------------------------------------------------------------------
int mapOnItemCanBuyInStore_WeaponUpgrade(int defIdx, struct SurvivalItemDef *def, Moby *storeMoby, int playerId, int numTimesPurchased)
{
	int weaponId = mapItem_WeaponUpgrade_GetWeaponIdFromItem(defIdx);
	if (weaponId <= 0)
		return 0;

	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player))
		return 0;

	int weaponLevel = player->GadgetBox->Gadgets[weaponId].Level;
	if (MapConfig.Functions.CanUpgradePlayerWeaponFunc && !MapConfig.Functions.CanUpgradePlayerWeaponFunc(player, weaponId, weaponLevel))
		return 0;

	// check min/max
	if (weaponLevel < 0 || weaponLevel >= 9)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
u32 mapOnItemGetStoreCost_WeaponUpgrade(int defIdx, struct SurvivalItemDef *def, Moby *storeMoby, int playerId, int numTimesPurchased)
{
	int weaponId = mapItem_WeaponUpgrade_GetWeaponIdFromItem(defIdx);
	if (weaponId <= 0)
		return 0;

	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player))
		return 0;

	int weaponLevel = player->GadgetBox->Gadgets[weaponId].Level;
	if (weaponLevel < 0 || weaponLevel >= 9)
		return 0;

	if (MapConfig.Functions.CanUpgradePlayerWeaponFunc && !MapConfig.Functions.CanUpgradePlayerWeaponFunc(player, weaponId, weaponLevel))
		return 0;

	return MapConfig.Functions.GetUpgradePlayerWeaponCostFunc(player, weaponId, weaponLevel);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_WeaponUpgrade(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	int weaponId = mapItem_WeaponUpgrade_GetWeaponIdFromItem(defIdx);

	// upgrade
	if (weaponId > 0 && MapConfig.Functions.ModeUpgradePlayerWeaponFunc)
		MapConfig.Functions.ModeUpgradePlayerWeaponFunc(playerId, weaponId);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_PlayerSpeed(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player) || !player->IsLocal)
		return;

	pushSnack(player->LocalPlayerIndex, "Speed Upgraded!", 10);
}

//--------------------------------------------------------------------------
void mapOnItemApply_PlayerSpeed(int defIdx, SurvivalItemDef_t *def, Player *player)
{
	player->Speed += (PLAYER_UPGRADE_SPEED_FACTOR * playerGetItemCount(player, defIdx));
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_PlayerHealth(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player) || !player->IsLocal)
		return;

	pushSnack(player->LocalPlayerIndex, "Health Upgraded!", 10);
}

//--------------------------------------------------------------------------
void mapOnItemApply_PlayerHealth(int defIdx, SurvivalItemDef_t *def, Player *player)
{
	player->MaxHealth += (PLAYER_UPGRADE_HEALTH_FACTOR * playerGetItemCount(player, defIdx));
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_PlayerDamage(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player) || !player->IsLocal)
		return;

	pushSnack(player->LocalPlayerIndex, "Damage Upgraded!", 10);
}

//--------------------------------------------------------------------------
void mapOnItemApply_PlayerDamage(int defIdx, SurvivalItemDef_t *def, Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args)
{
	if (!player->IsLocal)
		return;

	float damage = args->DamageQuarters / 4.0;
	int count = playerGetItemCount(player, defIdx);
	damage *= 1 + (PLAYER_UPGRADE_DAMAGE_FACTOR * count);
	args->DamageQuarters = (u32)(damage * 4);
}

//--------------------------------------------------------------------------
void mapOnItemConsumed_PlayerCrit(int defIdx, SurvivalItemDef_t *def, int playerId)
{
	Player *player = playerGetAll()[playerId];
	if (!playerIsValid(player) || !player->IsLocal)
		return;

	pushSnack(player->LocalPlayerIndex, "Critical Hit Upgraded!", 10);
}

//--------------------------------------------------------------------------
void mapOnItemApply_PlayerCrit(int defIdx, SurvivalItemDef_t *def, Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args)
{
	if (!player->IsLocal)
		return;

	float damage = args->DamageQuarters / 4.0;
	float critProbability = playerGetItemCount(player, defIdx) * PLAYER_UPGRADE_CRIT_FACTOR;
	float r = randRange(0, 1);
	if (r < critProbability)
	{
		damage *= 3;
		args->DamageFlags |= 0x20000000;
	}

	args->DamageQuarters = (u32)(damage * 4);
}
