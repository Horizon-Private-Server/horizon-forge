#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include "game.h"
#include "mob.h"
#include "blessings.h"
#include "messageid.h"
#include "maputils.h"

struct PlayerBlessingState
{
	int Slots;
	char Blessings[PLAYER_MAX_BLESSINGS];
};

char blessingQuadJumpJumpCount[GAME_MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
short blessingHealthRegenTickers[GAME_MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
short blessingAmmoRegenTickers[GAME_MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
VECTOR blessingLastPosition[GAME_MAX_PLAYERS] = {};
struct PlayerBlessingState BlessingStates[GAME_MAX_PLAYERS] = {0};

const char blessingTexIds[] = {
		[BLESSING_ITEM_MULTI_JUMP] 111 - 3,
		[BLESSING_ITEM_LUCK] 112 - 3,
		[BLESSING_ITEM_BULL] 113 - 3,
		[BLESSING_ITEM_ELEM_IMMUNITY] 114 - 3,
		[BLESSING_ITEM_HEALTH_REGEN] 115 - 3,
		[BLESSING_ITEM_AMMO_REGEN] 116 - 3,
		[BLESSING_ITEM_THORNS] 117 - 3,
};

const char blessingTexDims[] = {
		[BLESSING_ITEM_MULTI_JUMP] 32,
		[BLESSING_ITEM_LUCK] 32,
		[BLESSING_ITEM_BULL] 32,
		[BLESSING_ITEM_ELEM_IMMUNITY] 32,
		[BLESSING_ITEM_HEALTH_REGEN] 32,
		[BLESSING_ITEM_AMMO_REGEN] 32,
		[BLESSING_ITEM_THORNS] 32,
};

//--------------------------------------------------------------------------
int blessingsPlayerHasBlessing(int playerId, int blessing)
{
	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return 0;

	int i;
	for (i = 0; i < PLAYER_MAX_BLESSINGS; ++i)
		if (BlessingStates[playerId].Blessings[i] == blessing)
			return 1;

	return 0;
}

//--------------------------------------------------------------------------
void blessingsSetPlayerBlessingAt(int playerId, int slot, int blessing)
{
	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return BLESSING_ITEM_NONE;

	if (slot < 0 || slot >= PLAYER_MAX_BLESSINGS)
		return BLESSING_ITEM_NONE;

	BlessingStates[playerId].Blessings[slot] = blessing;
}

//--------------------------------------------------------------------------
int blessingsGetPlayerBlessingAt(int playerId, int slot)
{
	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return BLESSING_ITEM_NONE;

	if (slot < 0 || slot >= PLAYER_MAX_BLESSINGS)
		return BLESSING_ITEM_NONE;

	return BlessingStates[playerId].Blessings[slot];
}

//--------------------------------------------------------------------------
void blessingsSetPlayerSlots(int playerId, int slots)
{
	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return;

	// clamp between 0 and max blessings
	if (slots < 0)
		slots = 0;
	if (slots > PLAYER_MAX_BLESSINGS)
		slots = PLAYER_MAX_BLESSINGS;

	BlessingStates[playerId].Slots = slots;
}

//--------------------------------------------------------------------------
int blessingsGetPlayerSlots(int playerId)
{
	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return 0;

	return BlessingStates[playerId].Slots;
}

//--------------------------------------------------------------------------
void playerDecHitpointHooked(Player *player, float amount)
{
	float factor = 1;

	if (blessingsPlayerHasBlessing(player->PlayerId, BLESSING_ITEM_THORNS))
		factor = 1 - ITEM_BLESSING_THORN_DAMAGE_FACTOR;

	playerDecHealth(player, amount * factor);
}

//--------------------------------------------------------------------------
void headbuttDamage(float hitpoints, Moby *hitMoby, Moby *sourceMoby, int damageFlags, VECTOR fromPos, VECTOR t0)
{
	if (!MapConfig.State)
		return 0;

	// allow damaging healthbox
	// otherwise check if player can headbutt mob, and deny if not
	if (hitMoby && hitMoby->OClass != MOBY_ID_HEALTH_BOX_MULT)
	{
		Player *sourcePlayer = guberMobyGetPlayerDamager(sourceMoby);
		if (!sourcePlayer || !mobyIsMob(hitMoby))
			return;

		// only headbutt if bull blessing
		if (!blessingsPlayerHasBlessing(sourcePlayer->PlayerId, BLESSING_ITEM_BULL))
			return;

		hitpoints = 10 * (1 + (PLAYER_UPGRADE_DAMAGE_FACTOR * MapConfig.State->PlayerStates[sourcePlayer->PlayerId].State.Upgrades[UPGRADE_DAMAGE]));
		hitpoints *= sourcePlayer->DamageMultiplier;
#if LOG_STATS2
		DPRINTF("headbutt %08X %04X with %f and %X\n", (u32)hitMoby, hitMoby->OClass, hitpoints, damageFlags);
#endif
	}

	((void (*)(float, Moby *, Moby *, int, VECTOR, VECTOR))0x00503500)(hitpoints, hitMoby, sourceMoby, damageFlags, fromPos, t0);
}

//--------------------------------------------------------------------------
void playerStateUpdate(Player *player, int stateId, int a2, int a3, int t0)
{
	PlayerVTable *vtable = playerGetVTable(player);
	if (!vtable)
		return;

	// if player has bull blessing, then don't let walls stop them from cbooting
	if (blessingsPlayerHasBlessing(player->PlayerId, BLESSING_ITEM_BULL) && stateId == PLAYER_STATE_JUMP_BOUNCE)
	{
		return;
	}

	vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void playerBlessingOnDoubleJump(Player *player, int stateId, int a2, int a3, int t0)
{
	PlayerVTable *vtable = playerGetVTable(player);
	if (!vtable)
		return;

	if (blessingsPlayerHasBlessing(player->PlayerId, BLESSING_ITEM_MULTI_JUMP) && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP) && blessingQuadJumpJumpCount[player->PlayerId] < ITEM_BLESSING_MULTI_JUMP_COUNT)
	{
		// don't let the player flip until after they've used their jumps
	}
	else
	{
		blessingQuadJumpJumpCount[player->PlayerId] = 0;
		vtable->UpdateState(player, stateId, a2, a3, t0);
	}
}

//--------------------------------------------------------------------------
int playerBlessingGetAmmoRegenAmount(int weaponId)
{

	switch (weaponId)
	{
	case WEAPON_ID_VIPERS:
		return 5;
	case WEAPON_ID_MAGMA_CANNON:
	case WEAPON_ID_ARBITER:
	case WEAPON_ID_MINE_LAUNCHER:
	case WEAPON_ID_FUSION_RIFLE:
	case WEAPON_ID_OMNI_SHIELD:
	case WEAPON_ID_FLAIL:
	case WEAPON_ID_B6:
		return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
void blessingsMobReactToThorns(Moby *moby, float damage, int byPlayerId)
{
	if (byPlayerId < 0)
		return;
	if (!mobAmIOwner(moby))
		return;

	int i;
	VECTOR delta;
	struct MobDamageEventArgs args;
	Player **players = playerGetAll();
	Player *player = players[byPlayerId];
	Guber *guber = guberGetObjectByMoby(moby);

	if (!guber)
		return;
	if (!player || !player->SkinMoby)
		return;

	// take percentage of damage dealt
	damage *= ITEM_BLESSING_THORN_DAMAGE_FACTOR;

	// get angle
	vector_subtract(delta, moby->Position, player->PlayerPosition);
	float dist = vector_length(delta);
	float angle = atan2f(delta[1] / dist, delta[0] / dist);

	// create event
	GuberEvent *guberEvent = mobCreateEvent(moby, MOB_EVENT_DAMAGE);
	if (guberEvent)
	{
		args.SourceUID = player->Guber.Id.UID;
		args.SourceOClass = 0;
		args.DamageQuarters = damage * 4;
		args.DamageFlags = 0;
		args.Knockback.Angle = (short)(angle * 1000);
		args.Knockback.Ticks = 5;
		args.Knockback.Power = rand(4);
		args.Knockback.Force = args.Knockback.Power > 0;
		guberEventWrite(guberEvent, &args, sizeof(struct MobDamageEventArgs));
	}
}

//--------------------------------------------------------------------------
void processPlayerBlessings(Player *player)
{
	if (!player)
		return;
	if (!MapConfig.State)
		return;

	struct SurvivalPlayer *playerData = &MapConfig.State->PlayerStates[player->PlayerId];
	struct PlayerBlessingState *blessingsState = &BlessingStates[player->PlayerId];

	//
	int jumpBits = padGetMappedPad(0x40, player->PlayerId) << 8;
	PlayerVTable *vtable = playerGetVTable(player);
	if (blessingsPlayerHasBlessing(player->PlayerId, BLESSING_ITEM_MULTI_JUMP) && vtable && playerPadGetAnyButtonDown(player, jumpBits) && player->timers.state > 6 && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP) && blessingQuadJumpJumpCount[player->PlayerId] < ITEM_BLESSING_MULTI_JUMP_COUNT)
	{
		blessingQuadJumpJumpCount[player->PlayerId] += 1;
		vtable->UpdateState(player, PLAYER_STATE_JUMP, 1, 1, 1);
	}

	// reset quad jump counter when not in first jump state
	if (player->PlayerState != PLAYER_STATE_JUMP && player->PlayerState != PLAYER_STATE_RUN_JUMP)
		blessingQuadJumpJumpCount[player->PlayerId] = 0;

	int i;
	for (i = 0; i < blessingsState->Slots && i < PLAYER_MAX_BLESSINGS; ++i)
	{
		switch (blessingsState->Blessings[i])
		{
		case BLESSING_ITEM_BULL:
		{
			if (player->PlayerState == PLAYER_STATE_CHARGE && player->timers.state > 55 && playerPadGetButton(player, PAD_L2) > 0)
			{
				player->timers.state = 55;
			}
			break;
		}
		case BLESSING_ITEM_ELEM_IMMUNITY:
		{
			player->timers.acidTimer = 0;
			player->timers.freezeTimer = 0;
			break;
		}
		case BLESSING_ITEM_HEALTH_REGEN:
		{
			if (!player->IsLocal)
				break;
			if (playerData->TicksSinceHealthChanged < (TPS * 10))
				break;
			if (playerIsDead(player))
				break;
			if (blessingHealthRegenTickers[player->PlayerId] > 0)
			{
				--blessingHealthRegenTickers[player->PlayerId];
				break;
			}
			if (player->Health >= player->MaxHealth)
				break;

			playerData->LastHealth = player->Health = clamp(player->Health + 4, 0, player->MaxHealth);
			if (player->pNetPlayer)
				player->pNetPlayer->pNetPlayerData->hitPoints = player->Health;
			blessingHealthRegenTickers[player->PlayerId] = ITEM_BLESSING_HEALTH_REGEN_RATE_TPS;
			break;
		}
		case BLESSING_ITEM_AMMO_REGEN:
		{
			if (!player->IsLocal)
				break;
			if (blessingAmmoRegenTickers[player->PlayerId] > 0)
			{
				--blessingAmmoRegenTickers[player->PlayerId];
			}
			else
			{

				VECTOR dt;
				vector_subtract(dt, blessingLastPosition[player->PlayerId], player->PlayerPosition);
				if (vector_sqrmag(dt) > (2 * 2))
				{
					struct GadgetEntry *gadgetEntry = &player->GadgetBox->Gadgets[player->WeaponHeldId];
					int maxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, player->WeaponHeldId);
					int ammo = gadgetEntry->Ammo;
					int ammoInc = (int)ceilf(maxAmmo * 0.1); // playerBlessingGetAmmoRegenAmount(player->WeaponHeldId);
					if (ammoInc > 0 && ammo < maxAmmo)
					{
						gadgetEntry->Ammo = (ammo + ammoInc) > maxAmmo ? maxAmmo : (ammo + ammoInc);
					}
				}

				vector_copy(blessingLastPosition[player->PlayerId], player->PlayerPosition);
				blessingAmmoRegenTickers[player->PlayerId] = ITEM_BLESSING_AMMO_REGEN_RATE_TPS;
			}
			break;
		}
		}
	}
}

//--------------------------------------------------------------------------
void blessingsDrawHud(void)
{
	float x = 15;
	float y = SCREEN_HEIGHT - 100;
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		struct PlayerBlessingState *blessingsState = &BlessingStates[i];

		// draw blessings
		int j;
		for (j = blessingsState->Slots - 1; j >= 0; --j)
		{
			int blessingItem = blessingsState->Blessings[j];
			int itemTexId = blessingTexIds[blessingItem];
			int itemTexWH = blessingTexDims[blessingItem];
			u32 itemColor = 0x80C0C0C0;

			if (itemTexId > 0)
			{
				float off = 15 + (j * 20);
				transformToSplitscreenPixelCoordinates(i, &x, &y);

				// draw on ps2 and dzo
				gfxSetupGifPaging(0);
				gfxHelperDrawSprite(x, y, off + 2, 2, 32, 32, itemTexWH, itemTexWH, itemTexId, 0x40000000, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
				gfxHelperDrawSprite(x, y, off + 0, 0, 32, 32, itemTexWH, itemTexWH, itemTexId, itemColor, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
				gfxDoGifPaging();
			}
		}
	}
}

//--------------------------------------------------------------------------
void blessingsFrameTick(void)
{
	blessingsDrawHud();
}

//--------------------------------------------------------------------------
void blessingsTick(void)
{

	int i;
	Player **players = playerGetAll();
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = players[i];
		if (!player || !playerIsConnected(player))
			continue;

		// check for blessing slot
		if (BlessingStates[i].Slots)
		{
			processPlayerBlessings(player);
		}
	}
}

void blessingsInit(void)
{

	// hook blessings
	HOOK_JAL(0x0060C660, &playerBlessingOnDoubleJump);
	HOOK_JAL(0x0060CD44, &playerStateUpdate);

	// enable headbutt
	*(u32 *)0x005F98D0 = 0x24020001;
	*(u32 *)0x005f9920 = 0x0C000000 | ((u32)&headbuttDamage >> 2);

	// hook when player takes damage
	HOOK_JAL(0x00605820, &playerDecHitpointHooked);
	POKE_U32(0x005E1870, 0);
}
