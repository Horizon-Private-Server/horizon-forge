#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include "game.h"
#include "messageid.h"
#include "utils.h"
#include "mobs/mob.h"
#include "maputils.h"

#define ITEM_STACKABLE_HOVERBOOTS_DUR_TPS (2 * TPS)
#define ITEM_STACKABLE_HOVERBOOTS_SPEED_BUF (0.1)
#define ITEM_STACKABLE_LOW_HEALTH_DMG_BUF_FAC (0.75)
#define ITEM_STACKABLE_LOW_HEALTH_DMG_BUF_RAMP (0.5)
#define ITEM_STACKABLE_ALPHA_MOD_AMT (1)
#define ITEM_STACKABLE_VAMPIRE_HEALTH_AMT (3)
#define ITEM_STACKABLE_EXPLODINGENEMIES_DAMAGE (20)
#define ITEM_STACKABLE_EXPLODINGENEMIES_RADIUS (5.0)

int stackableExtraJumpJumpCount[GAME_MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int stackableHoverbootTicker[GAME_MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int stackableHoverbootState[GAME_MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//--------------------------------------------------------------------------
Player *stackableFindPlayerByGadgetBox(GadgetBox *gadgetBox)
{
	// find player
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!playerIsValid(player))
			continue;

		if (player->GadgetBox == gadgetBox)
			return player;
	}

	return NULL;
}

//--------------------------------------------------------------------------
int stackableGetAlphaModCount(GadgetBox *gadgetBox, int weaponId, int alphaModId)
{
	if (!gadgetBox)
		return 0;
	if (alphaModId > 8)
		return 0;
	if (alphaModId <= 0)
		return 0;

	// count alpha mods
	int i, count = 0;
	for (i = 0; i < 10; ++i)
	{
		if (gadgetBox->Gadgets[weaponId].AlphaMods[i] == alphaModId)
		{
			++count;
		}
	}

	// add stackables
	int itemId = -1;
	switch (alphaModId)
	{
#ifdef ITEM_PASSIVE_ALPHA_MOD_AMMO
	case ALPHA_MOD_AMMO:
		itemId = ITEM_PASSIVE_ALPHA_MOD_AMMO;
		break;
#endif
#ifdef ITEM_PASSIVE_ALPHA_MOD_AREA
	case ALPHA_MOD_AREA:
		itemId = ITEM_PASSIVE_ALPHA_MOD_AREA;
		break;
#endif
#ifdef ITEM_PASSIVE_ALPHA_MOD_IMPACT
	case ALPHA_MOD_IMPACT:
		itemId = ITEM_PASSIVE_ALPHA_MOD_IMPACT;
		break;
#endif
#ifdef ITEM_PASSIVE_ALPHA_MOD_SPEED
	case ALPHA_MOD_SPEED:
		itemId = ITEM_PASSIVE_ALPHA_MOD_SPEED;
		break;
#endif
	}

	if (itemId >= 0)
	{
		Player *player = stackableFindPlayerByGadgetBox(gadgetBox);
		if (player && MapConfig.State && !MapConfig.State->PlayerStates[player->PlayerId].IsInWeaponsMenu)
		{
			count += ITEM_STACKABLE_ALPHA_MOD_AMT * playerGetItemCount(player, itemId);
		}
	}

	return count;
}

//--------------------------------------------------------------------------
void stackablesItemInit_Alphamod(int defIdx, struct SurvivalItemDef *def)
{
	HOOK_J(0x006299A8, &stackableGetAlphaModCount);
	POKE_U32(0x006299CC, 0);
}

//--------------------------------------------------------------------------
void stackableOnDecrementAmmo(GadgetBox *gadgetBox, int weaponId, int amount)
{
#ifdef ITEM_PASSIVE_EXTRA_SHOT
	if (amount == -1)
	{
		Player *player = stackableFindPlayerByGadgetBox(gadgetBox);
		if (player)
		{
			int multishotCount = playerGetItemCount(player, ITEM_PASSIVE_EXTRA_SHOT);
			if (multishotCount > 0)
			{
				amount *= 1 + multishotCount;
			}
		}
	}
#endif

	((void (*)(GadgetBox *, int, int))0x006270E0)(gadgetBox, weaponId, amount);
}

//--------------------------------------------------------------------------
void stackablesItemInit_ExtraShot(int defIdx, struct SurvivalItemDef *def)
{
	HOOK_JAL(0x00627188, &stackableOnDecrementAmmo);
}

//--------------------------------------------------------------------------
int stackableOnPlayerCanPickupHealth(Player *player, Moby *healthMoby)
{
#ifdef ITEM_PASSIVE_VAMPIRE
	// vampire disables health pickups
	if (player && playerGetItemCount(player, ITEM_PASSIVE_VAMPIRE) > 0)
		return 0;
#endif

	// return base
	return ((int (*)(Player *, Moby *))0x00411c20)(player, healthMoby);
}

//--------------------------------------------------------------------------
void stackablesItemInit_Vampire(int defIdx, struct SurvivalItemDef *def)
{
	HOOK_JAL(0x004121C0, &stackableOnPlayerCanPickupHealth);
	HOOK_JAL(0x00412080, &stackableOnPlayerCanPickupHealth);
	HOOK_JAL(0x004128B0, &stackableOnPlayerCanPickupHealth);
}

//--------------------------------------------------------------------------
void stackablePlayerOnDoubleJump(Player *player, int stateId, int a2, int a3, int t0)
{
	PlayerVTable *vtable = playerGetVTable(player);
	if (!vtable)
		return;

#ifdef ITEM_PASSIVE_EXTRA_JUMP
	int featherCount = playerGetItemCount(player, ITEM_PASSIVE_EXTRA_JUMP);
	if (featherCount > 0 && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP) && (stackableExtraJumpJumpCount[player->PlayerId] < featherCount || player->timers.state < 6))
	{
		// don't let the player flip until after they've used their jumps
		POKE_U8((u32)player + 0x1CA0 + 0xFF, 0);
		return;
	}
#endif

	stackableExtraJumpJumpCount[player->PlayerId] = 0;
	vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void stackableProcessPlayer_ExtraJumps(Player *player)
{
#ifdef ITEM_PASSIVE_EXTRA_JUMP

	if (!MapConfig.State)
		return;
	if (!playerIsValid(player))
		return;

	// trigger extra jump on JUMP button
	int featherCount = playerGetItemCount(player, ITEM_PASSIVE_EXTRA_JUMP);
	int jumpBits = padGetMappedPad(0x40, player->PlayerId) << 8;
	PlayerVTable *vtable = playerGetVTable(player);
	if (featherCount > 0 && vtable && playerPadGetAnyButtonDown(player, jumpBits) && player->timers.state > 6 && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP))
	{
		// trigger another jump or indicate if the player can now double jump
		if (stackableExtraJumpJumpCount[player->PlayerId] < featherCount)
		{
			stackableExtraJumpJumpCount[player->PlayerId] += 1;
			vtable->UpdateState(player, PLAYER_STATE_JUMP, 1, 1, 1);
			POKE_U8((u32)player + 0x1CA0 + 0xFF, 0);
		}
		else
		{
			POKE_U8((u32)player + 0x1CA0 + 0xFF, 1);
		}
	}

	// reset extra jump counter when not in first jump state
	if (player->PlayerState != PLAYER_STATE_JUMP && player->PlayerState != PLAYER_STATE_RUN_JUMP)
		stackableExtraJumpJumpCount[player->PlayerId] = 0;

#endif
}

//--------------------------------------------------------------------------
void stackablesItemTick_ExtraJump(int defIdx, struct SurvivalItemDef *def)
{
	int i;
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!playerIsValid(player))
			continue;

		stackableProcessPlayer_ExtraJumps(player);
	}
}

//--------------------------------------------------------------------------
void stackablesItemInit_ExtraJump(int defIdx, struct SurvivalItemDef *def)
{
	HOOK_JAL(0x0060C660, &stackablePlayerOnDoubleJump);
}

//--------------------------------------------------------------------------
void stackablePlayerOnChargeboot(Player *player, int stateId, int a2, int a3, int t0)
{
	PlayerVTable *vtable = playerGetVTable(player);
	if (!vtable)
		return;

#ifdef ITEM_PASSIVE_HOVERBOOTS
	// prevent cbooting with hoverboots
	if (playerGetItemCount(player, ITEM_PASSIVE_HOVERBOOTS) > 0)
		return;
#endif

	// base
	vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void stackableProcessPlayer_Hoverboots(Player *player)
{
#ifdef ITEM_PASSIVE_HOVERBOOTS

	if (!MapConfig.State)
		return;
	if (!playerIsValid(player))
		return;

	int playerId = player->PlayerId;
	int canHover = (player->PlayerState == PLAYER_STATE_WALK || player->PlayerState == PLAYER_STATE_IDLE || player->PlayerState == PLAYER_STATE_FALL || player->PlayerState == PLAYER_STATE_GET_HIT) && !player->Ground.onGood;
	int count = playerGetItemCount(player, ITEM_PASSIVE_HOVERBOOTS);
	int hoverDuration = /*playerGetItemCount(player, ITEM_PASSIVE_HOVERBOOTS) * */ ITEM_STACKABLE_HOVERBOOTS_DUR_TPS;

	// hover
	if (canHover && count > 0 && stackableHoverbootTicker[playerId] >= 0 && stackableHoverbootTicker[playerId] < hoverDuration)
	{
		player->Ground.offGood = 0;
		stackableHoverbootTicker[playerId]++;

		if ((stackableHoverbootTicker[playerId] % 10) == 0)
		{
			mobySpawnExplosion(vector_read(player->PlayerPosition), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0x80003080, 0, NULL, NULL, 0, 0.5, 0, 0, 0);
		}
	}

	// increase speed
	player->Speed += playerGetItemCount(player, ITEM_PASSIVE_HOVERBOOTS) * ITEM_STACKABLE_HOVERBOOTS_SPEED_BUF;

	// replace cboots with magnet boots
	if (count > 0)
	{
		player->GadgetBox->Gadgets[17].Level = 0;

		if (player->Gadgets[1].pMoby)
			player->Gadgets[1].pMoby->Bangles = 4;
		if (player->Gadgets[1].pMoby2)
			player->Gadgets[1].pMoby2->Bangles = 4;
	}
	else if (player->GadgetBox->Gadgets[17].Level >= 0)
	{

		if (player->Gadgets[1].pMoby)
			player->Gadgets[1].pMoby->Bangles = 2;
		if (player->Gadgets[1].pMoby2)
			player->Gadgets[1].pMoby2->Bangles = 2;
	}

	// reset extra jump counter when not in first jump state
	if (!canHover)
		stackableHoverbootTicker[playerId] = player->Ground.onGood ? 0 : -1;

#endif
}

//--------------------------------------------------------------------------
void stackablesItemInit_Hoverboots(int defIdx, struct SurvivalItemDef *def)
{
	HOOK_JAL(0x0060DC6C, &stackablePlayerOnChargeboot);
	POKE_U32(0x005D9948, 0); // disable cboot bangles write
}

//--------------------------------------------------------------------------
void stackableOnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId)
{
	Player *killedByPlayer = playerGetFromIndex(killedByPlayerId);
  if (!playerIsValid(killedByPlayer))
    return;

#ifdef ITEM_PASSIVE_VAMPIRE
  int vampCount = playerGetItemCount(killedByPlayer, ITEM_PASSIVE_VAMPIRE);
  if (vampCount > 0)
  {
    playerSetHealth(killedByPlayer, minf(killedByPlayer->MaxHealth, killedByPlayer->Health + (vampCount * ITEM_STACKABLE_VAMPIRE_HEALTH_AMT)));
  }
#endif

#ifdef ITEM_PASSIVE_EXPLODING_ENEMIES
	if (killedByWeaponId >= 0)
	{
		if (!killedByPlayer->IsLocal)
			return; // only process dmg if local

		int itemCount = playerGetItemCount(killedByPlayer, ITEM_PASSIVE_EXPLODING_ENEMIES);
		if (killedByPlayer && itemCount > 0)
		{
			u32 damageFlags = mobAmIOwner(moby) ? 0x00081801 : 0;
			float radius = ITEM_STACKABLE_EXPLODINGENEMIES_RADIUS;
			float damage = itemCount * ITEM_STACKABLE_EXPLODINGENEMIES_DAMAGE;
			u32 color = TEAM_COLORS[killedByPlayer->Team];
			spawnExplosion(moby->Position, radius, color);

			// damage
			if (MapConfig.State && MapConfig.State->AllMobsSorted)
			{
				Moby **allMobsSorted = MapConfig.State->AllMobsSorted;
				int i;
				for (i = 0; i < MAX_MOBS_ALIVE; ++i)
				{
					Moby *mob = allMobsSorted[i];
					if (!mob || mobyIsDestroyed(mob) || !mob->PVar || !mobyIsMob(mob))
						continue;
					if (vector_sqrdistance(mob->Position, moby->Position) > (radius * radius))
						continue;

					// create event
					MobyColDamageIn in = {
							.DamageFlags = damageFlags,
							.DamageHp = damage,
							.Damager = killedByPlayer->PlayerMoby,
							.DamageClass = 0,
							.DamageStrength = 1,
							.DamageIndex = 0,
							.Flags = 1,
							.Momentum = {0}};
					mobyCollDamageDirect(mob, &in);
				}
			}
		}
	}
#endif
}

//--------------------------------------------------------------------------
void stackablesOnBeforeDamage(Player *player, float *damage)
{
#ifdef ITEM_PASSIVE_EXTRA_SHOT
	// multishot
	int multishotCount = playerGetItemCount(player, ITEM_PASSIVE_EXTRA_SHOT);
	if (multishotCount > 0)
	{
		*damage *= 1 + multishotCount;
	}
#endif

#ifdef ITEM_PASSIVE_LOW_HEALTH_DAMAGE_BUFF
	// low health dmg buf
	int lowHealthDmgBufCount = playerGetItemCount(player, ITEM_PASSIVE_LOW_HEALTH_DAMAGE_BUFF);
	if (lowHealthDmgBufCount > 0)
	{
		float healthFactor = player->Health / player->MaxHealth;
		float healthCurve = 1 - powf(healthFactor, ITEM_STACKABLE_LOW_HEALTH_DMG_BUF_RAMP);
		float dmgBuf = healthCurve * ITEM_STACKABLE_LOW_HEALTH_DMG_BUF_FAC * lowHealthDmgBufCount;
		*damage *= 1 + dmgBuf;
	}
#endif
}

//--------------------------------------------------------------------------
void stackablesProcessPlayer(Player *player)
{
	stackableProcessPlayer_Hoverboots(player);
}
