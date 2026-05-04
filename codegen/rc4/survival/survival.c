/***************************************************
 * FILENAME :		map.c
 *
 * DESCRIPTION :
 * 		Custom map logic for Survival.
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
#include <libdl/area.h>
#include <libdl/math3d.h>
#include <libdl/random.h>
#include <libdl/collision.h>
#include <libdl/stdio.h>
#include <libdl/spawnpoint.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/patch.h>
#include <libdl/hud.h>
#include <libdl/ui.h>
#include <libdl/radar.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "messageid.h"
#include "game.h"
#include "mobs/mob.h"
#include "mobys/vendor.h"
#include "config.h"
#include "interop.h"
#include "pathfind.h"
#include "maputils.h"
#include "mobys/store.h"
#include "items/item.h"
#include "mobys/upgrade.h"
#include "mobys/drop.h"
#include "gambits.h"
#include "mobys/pool.h"
#include "mobys/ammosupply.h"
#include "mobys/ammodrop.h"

#if SOULCOLLECTOR
#include "soulcollector.h"
#endif

#ifndef MAP_BASE_COMPLEXITY
#define MAP_BASE_COMPLEXITY (5000)
#endif

struct Guber *mapGetGuber(Moby *moby);
void mapHandleEvent(Moby *moby, GuberEvent *event);

void mobInit(void);
void mobTick(void);
void pathTick(void);

void stackableOnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId);
void frameTick(void);

extern struct StoreDef DefaultStores[];
extern const int DefaultStoresCount;

char LocalPlayerStrBuffer[GAME_MAX_LOCALS][64];

typedef struct HudBoss_CommonData
{ // 0x38
	/* 0x00 */ int iShown;
	/* 0x04 */ int iState;
	/* 0x08 */ float fHP;
	/* 0x0c */ float fDisplayHP;
	/* 0x10 */ unsigned int iColor;
	/* 0x14 */ unsigned int iColor2;
	/* 0x18 */ int iIcon;
	/* 0x1c */ float fHideX;
	/* 0x20 */ float fShown;
	/* 0x24 */ char bShown;
	/* 0x25 */ char bFancy;
	/* 0x28 */ int iDelay;
	/* 0x2c */ char bUpdate;
	/* 0x30 */ float fPulse;
	/* 0x34 */ int iFillMode;
} HudBoss_CommonData_t;

// set by map
struct SurvivalMapConfig MapConfig __attribute__((section(".config"))) = {
		.Magic = MAP_CONFIG_MAGIC,
		.State = NULL,
		.Functions.OnFrameTickFunc = &frameTick,
};

//--------------------------------------------------------------------------
int mapSpawnMob(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags)
{
	if (!gameAmIHost())
		return 0;

	// increment # mobs to spawn
	if (MapConfig.State && spawnFromUID < 0)
		MapConfig.State->RoundMaxMobCount += 1;

	struct MobConfig *config = &MapConfig.DefaultSpawnParams[spawnParamsIdx].Config;

	// spawn
	if (MapConfig.Functions.ModeCreateMobFunc)
		return MapConfig.Functions.ModeCreateMobFunc(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);
	else
		return MapConfig.Functions.OnMobCreateFunc(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);
}

//--------------------------------------------------------------------------
void mapReturnPlayersToMap(void)
{
	int i;
	float deathHeight = maxf(1, gameGetDeathHeight()) + 1; // if death height is 0, use 1

	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!player || !player->SkinMoby)
			continue;

		// if we're under the map, teleport back up
		if (player->PlayerPosition[2] < deathHeight)
		{
			// move back to player start
			playerTeleportToSpawn(player, 0.5);
		}
	}
}

//--------------------------------------------------------------------------
void mobForceIntoMapBounds(Moby *moby)
{
	if (!moby)
		return;

	int i;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// restrict mobs to moby grid range
	// mobys outside these bounds will crash the game
	// run first before mob allowed cuboid
	{
		VECTOR min = {1, 1, 1, 0};
		VECTOR max = {1023, 1023, 1023, 0};
		int respawn = 0;
		for (i = 0; i < 3; ++i)
		{
			if (moby->Position[i] < min[i])
			{
				moby->Position[i] = min[i];
				respawn = 1;
			}
			else if (moby->Position[i] > max[i])
			{
				moby->Position[i] = max[i];
				respawn = 1;
			}
		}

		// if hit bounds of moby grid, respawn and exit
		if (respawn)
		{
			pvars->MobVars.Respawn = respawn;
			return;
		}
	}

	// restrict to allowed area cuboid
	if (mobAllowedCuboidIdx < 0)
		return;

	SpawnPoint *cuboid = spawnPointGet(mobAllowedCuboidIdx);
	VECTOR rel;
	if (spawnPointIsPointInside(cuboid, moby->Position, rel))
		return;

	// clamp position & respawn
	rel[0] = clamp(rel[0], -1, 1);
	rel[1] = clamp(rel[1], -1, 1);
	rel[2] = clamp(rel[2], -1, 1);
	vector_apply(moby->Position, rel, cuboid->M0);
	pvars->MobVars.Respawn = 1;
}

//--------------------------------------------------------------------------
int mapPathCanBeSkippedForTarget(Moby *moby)
{
	return 1;
}

//--------------------------------------------------------------------------
void updateBossMeter(void)
{
	static float lastHealth = -1;
	HudBoss_CommonData_t *hudBossData = (HudBoss_CommonData_t *)0x00310400;
	int i;
	Moby *bossMoby = NULL;

	if (MapConfig.State)
		bossMoby = MapConfig.State->BossMoby;

	// show/hide boss meter
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		PlayerHUDFlags *hud = hudGetPlayerFlags(i);
		if (hud)
		{
			hud->Flags.Meter = bossMoby ? 1 : 0;
		}
	}

	if (!bossMoby)
		return;
	struct MobPVar *pvars = (struct MobPVar *)bossMoby->PVar;
	float health = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;

	// update meter and icon
	hudBossData->fHP = health;

	// refresh on health change
	if (lastHealth != health)
	{
		lastHealth = health;
		hudBossData->bUpdate = 1;
	}

	// set boss image to reactor sprite
	u32 id = hudPanelGetElement((void *)0x222b18, 6);
	struct HUDWidgetRectangleObject *bossImgRectObject = (struct HUDWidgetRectangleObject *)hudCanvasGetObject(hudGetCanvas(4), id);
	if (bossImgRectObject)
		((void (*)(struct HUDWidgetRectangleObject *, int))0x005ca3e8)(bossImgRectObject, MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BossTexUid /*0x75AF + 3*/);
}

//--------------------------------------------------------------------------
void setWeaponPickupRespawnTime(void)
{
	int pickupOffsets[GAME_MAX_PLAYERS] = {0, 0, 5, 10, 12, 14, 16, 18, 21, 24};
	GameSettings *gameSettings = gameGetSettings();

	// compute pickup respawn time in ms
	int playerCountAtStart = (gameSettings->PlayerCountAtStart <= 0 ? 1 : gameSettings->PlayerCountAtStart) - 1;
	int respawnTimeOffset = pickupOffsets[playerCountAtStart];

	Moby *moby = mobyListGetStart();
	Moby *mEnd = mobyListGetEnd();

	float mult = 1;
	if (MapConfig.Functions.GetWeaponPickupCooldownMultiplierFunc)
		mult = MapConfig.Functions.GetWeaponPickupCooldownMultiplierFunc();

	while (moby < mEnd)
	{
		if (moby->OClass == MOBY_ID_WEAPON_PICKUP && moby->PVar)
		{

			// hide if wrench
			// wrenches are set as pickup id when we've reached the max number of
			// pickups per gadget
			int gadgetId = *(int *)moby->PVar;
			int slotId = weaponIdToSlot(gadgetId) - 1;
			if (gadgetId == 1)
			{
				moby->Position[2] = 0;
			}
			else if (slotId >= 0 && slotId < 8)
			{
				int weaponBaseRespawnTime = 30;

				// otherwise set cooldown by configuration
				int ms = (weaponBaseRespawnTime - respawnTimeOffset) * mult * TIME_SECOND;
				POKE_U32((u32)moby->PVar + 0x0C, ms);
			}
		}
		++moby;
	}
}

//--------------------------------------------------------------------------
int mapBlockPlayerUseTeleporter(Moby *moby, Player *player)
{
	// pointer to player is in $s1
	asm volatile(
			".set noreorder;\n"
			"move %0, $s1"
			: : "r"(player));

	return 0;
}

//--------------------------------------------------------------------------
void playerDamageAndTeleportToSpawn(Player *player, int toState, int bTransAnim, int bForce, int bFall)
{
	// check if toState is a death state
	// and if so tp to spawn and subtract health
	// otherwise pass state transition to handler
	if (player->Health <= 0 || !playerStateIsDead(toState))
	{
		playerGetVTable(player)->UpdateState(player, toState, bTransAnim, bForce, bFall);
		return;
	}

	playerTeleportToSpawn(player, 0.5);
}

//--------------------------------------------------------------------------
void playerDrownAndTeleportToSpawn(Player *player)
{
	playerTeleportToSpawn(player, 0.5);
}

//--------------------------------------------------------------------------
void playerOnPushedIntoWall(Player *player)
{
	if (!player || !player->SkinMoby || !player->PlayerMoby)
		return;

	// this is also called when a player drowns (sometimes)
	if (player->PlayerState == PLAYER_STATE_QUICKSAND_SINK)
	{
		playerDrownAndTeleportToSpawn(player);
		return;
	}

	// push mobs away
	mobReactToExplosionAt(player->PlayerMoby, player->PlayerPosition, 1, 8, 6);

	// move player out of clipped wall
	// using lastGoodPos doesn't always return us to before the clip
	if (player->IsLocal)
	{
		playerSetPosRot(player, player->Ground.lastGoodPos, player->PlayerRotation);
	}
}

//--------------------------------------------------------------------------
float mapGetWeaponXpProgressFromGadgetBox(GadgetBox *gbox, int gadgetId)
{
	int slot = weaponIdToSlot(gadgetId);
	if (slot <= 0)
		return 0;

	// get percent as weapon level
	// store percent in experience
	int level = gbox->Gadgets[gadgetId].Level;
	float perc = clamp(level / (float)VENDOR_MAX_WEAPON_LEVEL, 0, 1);
	gbox->Gadgets[gadgetId].Experience = (int)(perc * 100000);
	return perc;
}

//--------------------------------------------------------------------------
void frameTick(void)
{
	itemDraw();
	storeFrameTick();

#if STACKABLES
	// sboxFrameTick();
#endif

#if SOULCOLLECTOR
	soulcollectorFrameUpdate();
#endif

	// char buf[32];
	// snprintf(buf, sizeof(buf), "%d", mobyGetNumSpawnableMobys());
	// gfxHelperDrawText(5, SCREEN_HEIGHT - 5, 0, 0, 1, 0x80FFFFFF, buf, -1, TEXT_ALIGN_BOTTOMLEFT, COMMON_DZO_DRAW_NORMAL);
}

#if DEBUG

//--------------------------------------------------------------------------
void survivalDebugManualSpawn(void)
{
	static int manSpawnMobId = 0;
	if (MapConfig.DefaultSpawnParamsCount <= 0)
		return; // no mobs
	if (!localPlayerHasInput())
		return;
	if (!gameAmIHost())
		return;

	Player *localPlayer = playerGetFromSlot(0);
	if (!playerIsValid(localPlayer))
		return;

	// check for destroy all mobs
	if (MapConfig.Functions.ModeMobNukeFunc && padGetButtonDown(0, PAD_UP) > 0)
	{
		MapConfig.Functions.ModeMobNukeFunc(-1);
	}

	// nav mobs
	int dir = 0;
	if (padGetButtonDown(0, PAD_LEFT) > 0)
	{
		dir = -1;
	}
	else if (padGetButtonDown(0, PAD_RIGHT) > 0)
	{
		dir = 1;
	}

	if (dir)
	{
		manSpawnMobId = (manSpawnMobId + dir + MapConfig.DefaultSpawnParamsCount) % MapConfig.DefaultSpawnParamsCount;
		DPRINTF("selected mob: %s (%d)\n", MapConfig.DefaultSpawnParams[manSpawnMobId].Name, manSpawnMobId);
	}

	// check for spawn pad button
	if (padGetButtonDown(0, PAD_DOWN) <= 0)
		return;

	// build spawn position
	VECTOR t;
	VECTOR offset = {1, 1, 1, 0};
	vector_scale(t, offset, MapConfig.DefaultSpawnParams[manSpawnMobId].Config.CollRadius * 2);
	vector_add(t, t, localPlayer->PlayerPosition);

	int r = mapSpawnMob(manSpawnMobId, t, 0, -1, 0);
	DPRINTF("manual spawn mob %s (idx %d) returned %d\n", MapConfig.DefaultSpawnParams[manSpawnMobId].Name, manSpawnMobId, r);
}

//--------------------------------------------------------------------------
void survivalDebugInfiniteHealth(void)
{
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		player->Health = player->MaxHealth = maxf(player->MaxHealth, 10000);
	}
}

//--------------------------------------------------------------------------
void survivalDebugInfiniteAmmo(void)
{
	GameOptions *go = gameGetOptions();
	go->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 1;
}

//--------------------------------------------------------------------------
void survivalDebugPayday(void)
{
	int i;
	static int init = 0;
	if (!MapConfig.State)
		return;

	// give max alpha mods
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;

		GadgetBox *gbox = player->GadgetBox;
		if (!gbox)
			continue;

		gbox->ModBasic[0] = 64;
		gbox->ModBasic[1] = 64;
		gbox->ModBasic[2] = 64;
		gbox->ModBasic[3] = 64;
		gbox->ModBasic[4] = 64;
		gbox->ModBasic[5] = 64;
		gbox->ModBasic[6] = 64;
		gbox->ModBasic[7] = 64;
	}

	if (init)
		return;

	// set bolts/tokens once
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		MapConfig.State->PlayerStates[i].State.Bolts = 100000000;
		MapConfig.State->PlayerStates[i].State.CurrentTokens = 10000;
	}

	init = 1;
}

//--------------------------------------------------------------------------
void survivalDebugMoonjump(void)
{
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player))
			continue;
		if (padGetButton(i, PAD_CROSS) <= 0)
			continue;

		player->Velocity[2] = 0.125;
	}
}

//--------------------------------------------------------------------------
void survivalDebugStartRound(int roundNumber)
{
	struct SurvivalState *state = MapConfig.State;
	if (!state)
		return;
	if (roundNumber <= 1)
		return; // no round skip

	static int init = 0;
	if (init == 2)
		return;
	if (init == 1)
	{

		// set round to immediately end after it is initialized by the game mode
		if (state->RoundMaxMobCount)
		{
			state->MobStats.TotalSpawnedThisRound = state->RoundMaxMobCount;
			init = 2;
		}

		return;
	}

	// set to round prior to target round number
	// once the game initializes the round we'll force it to end
	// starting the target round (with the interum period)
	state->RoundNumber = roundNumber - 2;
	state->RoundMaxMobCount = 0;

	// set initial bolt/token
	// also bump health upgrades up a bit for testing convenience
	int j;
	for (j = 0; j < GAME_MAX_PLAYERS; ++j)
	{
		state->PlayerStates[j].State.Bolts = powf(1.225, roundNumber) * 10000;
		state->PlayerStates[j].State.CurrentTokens = roundNumber * 5;
#ifdef ITEM_IMMEDIATE_PLAYER_HEALTH_UPGRADE
		state->PlayerStates[j].State.ItemCounts[ITEM_IMMEDIATE_PLAYER_HEALTH_UPGRADE] = roundNumber;
#endif
	}

	// iterate each round
	int i;
	for (i = 0; i < state->RoundNumber; ++i)
	{
		state->MobStats.TotalSpawned += MAX_MOBS_BASE + (int)(MAX_MOBS_ROUND_WEIGHT * (1 + i));
	}

	DPRINTF("Skipped to Round #%d with %d mobs spawned\n", state->RoundNumber + 1, state->MobStats.TotalSpawned);
	init = 1;
}

#endif

//--------------------------------------------------------------------------
struct StoreDef *mapGetStore(Moby *moby, int localPlayerIndex, int storeIdx)
{
	if (storeIdx < 0 || storeIdx >= DefaultStoresCount)
		return NULL;

	return &DefaultStores[storeIdx];
}

//--------------------------------------------------------------------------
void survivalInit(void)
{
	static int initialized = 0;
	if (initialized)
		return;

	// setup required functions
	struct StoreVTable defaultStoreVTable = {
			.GetStoreFunc = &mapGetStore,
	};

	MapConfig.Magic = MAP_CONFIG_MAGIC;

	mapApplyFixes();
	poolInit();
	mboxInit();
	mobInit();
	ammosupplyInit();
	configInit();
	upgradeInit();
	dropInit();
	bboxInit();
	vendorInit();
	demonbellInit();
	itemInit();
	storeInit(&defaultStoreVTable);
#if STACKABLES
	// stackableInit();
#endif
#if GAMBITS
	gambitsInit();
#endif
#if RANDOMIZE_WEAPONS_AT_START
	if (gameAmIHost())
	{
		randomizeWeaponPickups();
	}
#endif
#ifdef AMMO_DROP_PROBABILITY
	ammodropInit();
#endif

	// hook HudAmmo XP bar
	POKE_U32(0x00552CD8, 0x10000013);
	HOOK_JAL(0x00552D28, &mapGetWeaponXpProgressFromGadgetBox);

	// disable jump pad effect
	POKE_U32(0x0042608C, 0);

	// have moby 0x1BC6 use glowColor for particle color
	// for reactor fire effect
	POKE_U32(0x004171D8, 0x8FA80070);
	POKE_U32(0x00417200, 0x8D080060);

	// enable teleporter for everyone
	HOOK_JAL(0x003dfd18, &mapBlockPlayerUseTeleporter);
	POKE_U32(0x003dfd1c, 0x0240202D);
	// POKE_U32(0x003DFD20, 0x10000006);
	POKE_U32(0x003DFD6C, 0x00000000);

	// patch mobs pushing you into walls and killing you
	POKE_U32(0x005e4188, 0);
	POKE_U32(0x005e419c, 0);
	HOOK_JAL(0x005e41bc, &playerOnPushedIntoWall);

	// if a player dies from being stuck or drowning
	// teleport player to spawn and damage
	POKE_U32(0x0060adb4, 0);
	HOOK_JAL(0x0060add4, &playerDamageAndTeleportToSpawn); // slope slide
	POKE_U32(0x0060ade0, 0);
	HOOK_JAL(0x005DA5AC, &playerDamageAndTeleportToSpawn); // acid drown / lava
	POKE_U32(0x006090b8, 0);
	HOOK_JAL(0x006090f0, &playerDrownAndTeleportToSpawn); // water drown

	DPRINTF("path %08X end %08X\n", (u32)&MOB_PATHFINDING_PATHS, (u32)&MOB_PATHFINDING_PATHS + (MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH * MOB_PATHFINDING_NODES_COUNT * MOB_PATHFINDING_NODES_COUNT));

	initialized = 1;
}

//--------------------------------------------------------------------------
int survivalTick(void)
{
	//
	if (MapConfig.ClientsReady || !netGetDmeServerConnection())
	{
		mboxSpawn();
		upgradeSpawn();
	}

	poolTick();
	mobTick();
	pathTick();
	upgradeTick();
	dropTick();
	demonbellTick();
	itemTick();
#if STACKABLES
	// stackableTick();
#endif
#if GAMBITS
	gambitsTick();
#endif
	mapReturnPlayersToMap();
	updateBossMeter();
	setWeaponPickupRespawnTime();

	if (MapConfig.State)
	{
		MapConfig.State->MapBaseComplexity = MAP_BASE_COMPLEXITY;
	}

	//
	if (MapConfig.State)
	{
		addRadarBlip(MapConfig.State->BigAl, 31, 14, TEAM_YELLOW);

		// track round completions
#if GAMBITS
		static int lastRoundCompleted = 0;
		if (MapConfig.State->RoundEndTime && lastRoundCompleted != MapConfig.State->RoundNumber)
		{
			lastRoundCompleted = MapConfig.State->RoundNumber;
			gambitsOnRoundComplete(lastRoundCompleted);
		}
#endif

		// enable prestige if round % 25
#if SHOW_PRESTIGE_EVERY_25
		Moby *prestigeMachineMoby = MapConfig.State->PrestigeMachine;
		if (prestigeMachineMoby)
		{
			int enabled = MapConfig.State->RoundEndTime && ((MapConfig.State->RoundNumber + 0) % 25) == 0;

			if (enabled)
			{
				if (prestigeMachineMoby->CollActive < 0)
				{
					pushSnack(0, "Prestige Machine Activated", 120);
				}
				prestigeMachineMoby->DrawDist = 64;
				prestigeMachineMoby->CollActive = 0;
				prestigeMachineMoby->ModeBits &= ~MOBY_MODE_BIT_DISABLED;
			}
			else
			{
				prestigeMachineMoby->DrawDist = 0;
				prestigeMachineMoby->CollActive = -1;
				prestigeMachineMoby->ModeBits |= MOBY_MODE_BIT_DISABLED;
			}
		}
#endif
	}

#if DEBUG_START_ROUND
	survivalDebugStartRound(DEBUG_START_ROUND);
#endif

#if DEBUG_MANUAL_SPAWNING
	survivalDebugManualSpawn();
#endif

#if DEBUG_INFINITE_HEALTH
	survivalDebugInfiniteHealth();
#endif

#if DEBUG_INFINITE_AMMO
	survivalDebugInfiniteAmmo();
#endif

#if DEBUG_MOONJUMP
	survivalDebugMoonjump();
#endif

#if DEBUG_PAYDAY
	survivalDebugPayday();
#endif

	dlPostUpdate();
	return 0;
}
