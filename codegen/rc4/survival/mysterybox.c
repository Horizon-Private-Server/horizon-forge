/***************************************************
 * FILENAME :		mysterybox.c
 *
 * DESCRIPTION :
 * 		Handles logic for the mystery box.
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
#include "mysterybox.h"
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "utils.h"
#include "maputils.h"

#if DEBUGMBOX
int MysteryBoxItemCounts[MAX_ITEM_COUNT];
int MysteryBoxTotalRolls = 0;
#endif

VECTOR MysteryBoxPositions[MYSTERY_BOX_MAX_LOCATIONS];
VECTOR MysteryBoxRotations[MYSTERY_BOX_MAX_LOCATIONS];
int MysteryBoxLocationCount = 0;

SoundDef RespawnSoundDef = {
		.MinRange = 0.0,
		.MaxRange = 2000.0,
		.MinVolume = 600,
		.MaxVolume = 600,
		.MinPitch = 0,
		.MaxPitch = 0,
		.Loop = 0,
		.Flags = 0x10,
		.Index = 54,
		.BankIndex = 3};

char MysteryBoxRespawnImmediately = 0;

//--------------------------------------------------------------------------
void mboxPlayOpenSound(Moby *moby)
{
	mobyPlaySoundByClass(0, 0, moby, MOBY_ID_NODE_BASE);
}

//--------------------------------------------------------------------------
void mboxPlayRespawnSound(Moby *moby)
{
	soundPlay(&RespawnSoundDef, 0, moby, 0, 0x400);
}

//--------------------------------------------------------------------------
int mboxGetCost(Moby *moby, int playerId)
{
	if (!moby || !moby->PVar)
		return 0;

	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;
	return pvars->BoltCostMultiplier * (MYSTERY_BOX_COST + (MYSTERY_BOX_COST_PER_VOX * pvars->NumVoxPerPlayer[playerId]));
}

//--------------------------------------------------------------------------
float mboxRand(void)
{
	return randRange(0, 1);
}

//--------------------------------------------------------------------------
int mboxGetItem(Moby *moby, int itemIdx, SurvivalItemDef_t *item)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	memcpy(item, &MapConfig.ItemDefs[itemIdx], sizeof(SurvivalItemDef_t));
	return 1;
}

//--------------------------------------------------------------------------
int mboxGetItems(Moby *moby, int forPlayerId, int *itemIdxs, int count)
{
	if (!itemIdxs)
		return 0;

	// build list items with mysterybox chance
	int outIdx = 0;
	int i;
	SurvivalItemDef_t itemDef;
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		if (outIdx >= count)
			break;

		mboxGetItem(moby, i, &itemDef);
		float itemWeight = itemGetMysteryboxChanceWeight(i, &itemDef, forPlayerId);
		if (itemWeight > 0)
		{
			itemIdxs[outIdx] = i;
			outIdx++;
		}
	}

	return outIdx;
}

//--------------------------------------------------------------------------
int mboxGetRandomItem(Moby *moby, int forPlayerId)
{
	int i;
	SurvivalItemDef_t itemDef;
	int itemIdxs[MAX_ITEM_COUNT];
	int count = mboxGetItems(moby, forPlayerId, &itemIdxs, MAX_ITEM_COUNT);
	if (count <= 0)
		return -1;

	// get sum
	float sumWeight = 0;
	for (i = 0; i < count; ++i)
	{
		int itemIdx = itemIdxs[i];
		mboxGetItem(moby, itemIdxs[i], &itemDef);
		float itemWeight = itemGetMysteryboxChanceWeight(itemIdx, &itemDef, forPlayerId);

		sumWeight += itemWeight;
	}

	// generate random value within range of weights
	// grab the first item where r > last
	float r = mboxRand() * sumWeight;
	for (i = 0; i < (count - 1); ++i)
	{
		int itemIdx = itemIdxs[i];
		mboxGetItem(moby, itemIdx, &itemDef);
		float itemWeight = itemGetMysteryboxChanceWeight(itemIdx, &itemDef, forPlayerId);
		if (r < itemWeight)
		{
#if DEBUGMBOX
			DPRINTF("mbox hit %d (%s) %f<%f\n", itemIdxs[i], itemDef.Name, r, itemWeight);
#endif
			break;
		}

		r -= itemWeight;
	}

	return itemIdxs[i];
}

//--------------------------------------------------------------------------
void mboxActivate(Moby *moby, int activatedByPlayerId)
{
	int i;
	int itemIdx = mboxGetRandomItem(moby, activatedByPlayerId);
	if (itemIdx < 0)
		return;

	// create event
	GuberEvent *guberEvent = guberCreateEvent(moby, MYSTERY_BOX_EVENT_ACTIVATE);
	if (guberEvent)
	{
		int random = rand(100);

		guberEventWrite(guberEvent, &activatedByPlayerId, 4);
		guberEventWrite(guberEvent, &itemIdx, 4);
		guberEventWrite(guberEvent, &random, 4);
	}
}

//--------------------------------------------------------------------------
void mboxGetRandomRespawn(int random, VECTOR outPos, VECTOR outRot)
{
	int i;

	if (!MysteryBoxLocationCount)
		return;

	// get random index into locations
	int r = random % MysteryBoxLocationCount;
	vector_copy(outPos, MysteryBoxPositions[r]);
	vector_copy(outRot, MysteryBoxRotations[r]);
}

//--------------------------------------------------------------------------
void mboxSetRandomRespawn(Moby *moby, int random)
{
	if (!moby || !moby->PVar)
		return;

	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;

	// find next spawn point
	mboxGetRandomRespawn(random, pvars->SpawnpointPosition, pvars->SpawnpointRotation);

	if (MapConfig.State)
		pvars->RoundHidden = MapConfig.State->RoundNumber;

	mobySetState(moby, MYSTERY_BOX_STATE_HIDDEN, -1);
}

//--------------------------------------------------------------------------
void mboxGivePlayer(Moby *moby, int playerId, int itemIdx, int random)
{
	// create event
	GuberEvent *guberEvent = guberCreateEvent(moby, MYSTERY_BOX_EVENT_GIVE_PLAYER);
	if (guberEvent)
	{

		guberEventWrite(guberEvent, &playerId, 4);
		guberEventWrite(guberEvent, &itemIdx, 4);
		guberEventWrite(guberEvent, &random, 4);

		// DPRINTF("mbox give player %d item %d (%d)\n", playerId, item, random);
	}
}

//--------------------------------------------------------------------------
void mboxOpenDoor(Moby *moby, float openFactor)
{
	MATRIX m1;
	MATRIX *jointCache;
	if (!moby)
		return;

	jointCache = (MATRIX *)moby->JointCache;
	if (!jointCache)
		return;

	// door 1
	matrix_unit(m1);
	matrix_rotate_x(m1, m1, openFactor * 90 * MATH_DEG2RAD);
	matrix_multiply(jointCache[1], m1, jointCache[1]);

	// door 2
	matrix_unit(m1);
	matrix_rotate_x(m1, m1, openFactor * 90 * MATH_DEG2RAD);
	matrix_multiply(jointCache[3], m1, jointCache[3]);
}

//--------------------------------------------------------------------------
void mboxDraw(Moby *moby)
{
	if (!moby || !moby->PVar || moby->State == MYSTERY_BOX_STATE_IDLE)
		return;

	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;
	struct QuadDef quad;
	MATRIX m2, mRot;
	VECTOR t;
	VECTOR offset = {1, 0, 1.25, 0};
	VECTOR pTL = {0.25, 0, 0.25, 1};
	VECTOR pTR = {-0.25, 0, 0.25, 1};
	VECTOR pBL = {0.25, 0, -0.25, 1};
	VECTOR pBR = {-0.25, 0, -0.25, 1};

	int itemIdx = pvars->CycleItemIdx = pvars->ItemIdx;
	if (moby->State == MYSTERY_BOX_STATE_CYCLING_ITEMS)
	{
		pvars->CycleItemIdx = itemIdx = mboxGetRandomItem(moby, pvars->ActivatedByPlayerId);
	}

	SurvivalItemDef_t itemDef;
	if (!mboxGetItem(moby, itemIdx, &itemDef))
		return;

	u32 color = itemDef.TexColor;
	int texId = itemDef.TexId;

	// save
	pvars->ItemTexId = texId;
	pvars->ItemTexColor = color;

	// determine how far out to draw the sprite
	float openFactor = clamp((gameGetTime() - pvars->ActivatedTime) / (1.0 * TIME_SECOND), 0, 1);
	if (moby->State == MYSTERY_BOX_STATE_CLOSING)
	{
		openFactor = 1 - clamp((gameGetTime() - pvars->StateChangedAtTime) / (0.1 * TIME_SECOND), 0, 1);
	}

	// set draw args
	matrix_unit(m2);

	// init
	gfxResetQuad(&quad);

	// color of each corner?
	vector_copy(quad.VertexPositions[0], pTL);
	vector_copy(quad.VertexPositions[1], pTR);
	vector_copy(quad.VertexPositions[2], pBL);
	vector_copy(quad.VertexPositions[3], pBR);
	quad.VertexColors[0] = quad.VertexColors[1] = quad.VertexColors[2] = quad.VertexColors[3] = color;
	quad.VertexUVs[0] = (struct UV){0, 0};
	quad.VertexUVs[1] = (struct UV){1, 0};
	quad.VertexUVs[2] = (struct UV){0, 1};
	quad.VertexUVs[3] = (struct UV){1, 1};
	quad.Clamp = 0x0000000100000001;
	quad.Tex0 = gfxGetFrameTex(texId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera *camera = cameraGetGameCamera(0);
	if (!camera)
		return;

	// get forward vector
	vector_subtract(t, camera->pos, moby->Position);
	t[2] = 0;
	vector_normalize(&m2[4], t);

	// up vector
	m2[8 + 2] = 1;

	// right vector
	vector_outerproduct(&m2[0], &m2[4], &m2[8]);

	// position
	matrix_unit(mRot);
	memcpy(mRot, moby->M0_03, sizeof(VECTOR) * 3);
	vector_apply(offset, offset, mRot);
	vector_scale(offset, offset, openFactor);
	vector_add(&m2[12], moby->Position, offset);

	// draw
	gfxDrawQuad((void *)0x00222590, &quad, m2, 0);
}

//--------------------------------------------------------------------------
void mboxUpdate(Moby *moby)
{
	Player **players = playerGetAll();
	int i;
	char buf[48];
	if (!moby || !moby->PVar)
		return;

	SurvivalItemDef_t itemDef;
	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;
	Player *activatedByPlayer = players[pvars->ActivatedByPlayerId];
	int timeSinceActivated = gameGetTime() - pvars->ActivatedTime;
	int timeSinceStateChanged = gameGetTime() - pvars->StateChangedAtTime;
	int ticksSinceLastStateChange = pvars->TicksSinceLastStateChanged++;

	if (pvars->ActivatedByPlayerId < 0)
		activatedByPlayer = NULL;

	// post draw
	if (moby->State != MYSTERY_BOX_STATE_HIDDEN)
	{
		gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&mboxDraw, moby);

		int blipIdx = radarGetBlipIndex(moby);
		if (blipIdx >= 0)
		{
			RadarBlip *blip = radarGetBlips() + blipIdx;
			blip->X = moby->Position[0];
			blip->Y = moby->Position[1];
			blip->Life = 0x1F;
			blip->Type = 4;
			blip->Team = TEAM_YELLOW;
		}
	}

	// handle state
	switch (moby->State)
	{
	case MYSTERY_BOX_STATE_CLOSING:
	{
		float t = 1 - clamp(powf(timeSinceStateChanged / (0.1 * TIME_SECOND), 2), 0, 1);
		mboxOpenDoor(moby, t);

		// transition to next state after
		if (t <= 0)
		{
			mobySetState(moby, MYSTERY_BOX_STATE_IDLE, -1);
			pvars->StateChangedAtTime = gameGetTime();
			pvars->TicksSinceLastStateChanged = 0;
		}
		break;
	}
	case MYSTERY_BOX_STATE_BEFORE_CLOSING:
	{
		mboxOpenDoor(moby, 1);

		// handle items that are forced onto player
		if (mboxGetItem(moby, pvars->ItemIdx, &itemDef))
		{
			if (itemDef.MysteryboxForceAcquire && activatedByPlayer && activatedByPlayer->IsLocal)
			{
				mboxGivePlayer(moby, pvars->ActivatedByPlayerId, pvars->ItemIdx, pvars->Random);
			}
		}

		// transition to next state after
		mobySetState(moby, MYSTERY_BOX_STATE_CLOSING, -1);
		pvars->StateChangedAtTime = gameGetTime();
		pvars->TicksSinceLastStateChanged = 0;
		break;
	}
	case MYSTERY_BOX_STATE_DISPLAYING_ITEM:
	{
		mboxOpenDoor(moby, 1);

#if ITEM_IMMEDIATE_VOX_MYSTERYBOX
		// play vox dialog on appear
		if (!ticksSinceLastStateChange && pvars->ItemIdx == ITEM_IMMEDIATE_VOX_MYSTERYBOX)
		{
			playDialog(DIALOG_ID_VOX_JACKPOT, 1);
		}
#endif

		// let player who activated interact with item
		// if the item is interactable
		if (pvars->ActivatedByPlayerId >= 0 && mboxGetItem(moby, pvars->ItemIdx, &itemDef))
		{
			int showInteract = itemDef.MysteryboxForceAcquire == 0;
			int random = pvars->Random;
			snprintf(buf, sizeof(buf), "\x11 %s", itemDef.Name);
			if (showInteract && tryPlayerInteract(moby, players[pvars->ActivatedByPlayerId], buf, NULL, 0, 0, PLAYER_MYSTERY_BOX_COOLDOWN_TICKS, 9, PAD_CIRCLE, 0))
			{
				mboxGivePlayer(moby, pvars->ActivatedByPlayerId, pvars->ItemIdx, pvars->Random);
			}
		}

		// transition to next state after
#if DEBUGMBOX1
		mobySetState(moby, MYSTERY_BOX_STATE_BEFORE_CLOSING, -1);
		pvars->StateChangedAtTime = gameGetTime();
		pvars->TicksSinceLastStateChanged = 0;
#else
		if (timeSinceStateChanged > (TIME_SECOND * 5))
		{
			mobySetState(moby, MYSTERY_BOX_STATE_BEFORE_CLOSING, -1);
			pvars->StateChangedAtTime = gameGetTime();
			pvars->TicksSinceLastStateChanged = 0;
		}
#endif
		break;
	}
	case MYSTERY_BOX_STATE_CYCLING_ITEMS:
	{
		mboxOpenDoor(moby, 1);

		// transition to next state after
#if DEBUGMBOX
		mobySetState(moby, MYSTERY_BOX_STATE_DISPLAYING_ITEM, -1);
		pvars->StateChangedAtTime = gameGetTime();
		pvars->TicksSinceLastStateChanged = 0;
#else
		if (timeSinceStateChanged > MYSTERY_BOX_CYCLE_ITEMS_DURATION)
		{
			mobySetState(moby, MYSTERY_BOX_STATE_DISPLAYING_ITEM, -1);
			pvars->StateChangedAtTime = gameGetTime();
			pvars->TicksSinceLastStateChanged = 0;
		}
#endif
		break;
	}
	case MYSTERY_BOX_STATE_OPENING:
	{
		float t = 1 - clamp(powf(1 - (timeSinceActivated / (0.1 * TIME_SECOND)), 3), 0, 1);
		mboxOpenDoor(moby, t);

		// transition to next state after
		if (t >= 1)
		{
			mobySetState(moby, MYSTERY_BOX_STATE_CYCLING_ITEMS, -1);
			pvars->StateChangedAtTime = gameGetTime();
			pvars->TicksSinceLastStateChanged = 0;
		}
		break;
	}
	case MYSTERY_BOX_STATE_IDLE:
	{
		moby->ModeBits &= ~MOBY_MODE_BIT_DISABLED;
		moby->CollActive = 0;

		// find local players to activate
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			int cost = mboxGetCost(moby, i);
			snprintf(buf, sizeof(buf), "\x11 Open [\x0E%'d\x08]", cost);

			if (tryPlayerInteract(moby, players[i], buf, NULL, cost, 0, PLAYER_MYSTERY_BOX_COOLDOWN_TICKS, 9, PAD_CIRCLE, 0))
			{
				mboxActivate(moby, i);
				break;
			}
		}
		break;
	}
	case MYSTERY_BOX_STATE_HIDDEN:
	{
		moby->ModeBits |= MOBY_MODE_BIT_DISABLED;
		moby->CollActive = -1;

		if (MapConfig.State && (MysteryBoxRespawnImmediately || MapConfig.State->RoundNumber != pvars->RoundHidden))
		{
			vector_copy(moby->Position, pvars->SpawnpointPosition);
			vector_copy(moby->Rotation, pvars->SpawnpointRotation);
			mobySetState(moby, MYSTERY_BOX_STATE_IDLE, -1);
			mboxPlayRespawnSound(moby);
		}
		break;
	}
	}
}

//--------------------------------------------------------------------------
int mboxHandleEvent_Spawned(Moby *moby, GuberEvent *event)
{
	DPRINTF("mbox spawned: %08X\n", (u32)moby);
	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;
	if (!pvars)
		return 0;

	// read event
	guberEventRead(event, moby->Position, 12);
	guberEventRead(event, moby->Rotation, 12);

	// set update
	moby->PUpdate = &mboxUpdate;

	// indicate to survival mode that we can damage players
	moby->Bolts = -1;

	// init pvars
	pvars->BoltCostMultiplier = 1;

	// update mode reference
	if (MapConfig.State)
		MapConfig.State->MysteryBoxMoby = moby;

	// set default state
	mobySetState(moby, MYSTERY_BOX_STATE_IDLE, -1);
	return 0;
}

//--------------------------------------------------------------------------
int mboxHandleEvent_Activate(Moby *moby, GuberEvent *event)
{
	int activatedByPlayerId, itemIdx, random;
	SurvivalItemDef_t itemDef;

	// DPRINTF("mbox activate: %08X\n", (u32)moby);
	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;
	if (!pvars)
		return 0;

	guberEventRead(event, &activatedByPlayerId, 4);
	guberEventRead(event, &itemIdx, 4);
	guberEventRead(event, &random, 4);

#if DEBUGMBOX
	MysteryBoxTotalRolls += 1;
	MysteryBoxItemCounts[itemIdx] += 1;
	int itemIdxs[MAX_ITEM_COUNT];
	int count = mboxGetItems(moby, activatedByPlayerId, &itemIdxs, MAX_ITEM_COUNT);
	printf("Total Rolls: %d\n", MysteryBoxTotalRolls);
	int i;
	for (i = 0; i < count; ++i)
	{
		int it = itemIdxs[i];
		mboxGetItem(moby, it, &itemDef);
		float p = MysteryBoxItemCounts[it] / (float)MysteryBoxTotalRolls;
		printf("%s: %d (%f of %f)\n", itemDef.Name, MysteryBoxItemCounts[it], p, itemDef.MysteryboxChanceWeight);
	}

	printf("\n\n");
#endif

	// increment stat
	if (activatedByPlayerId >= 0 && MapConfig.State)
	{
		MapConfig.State->PlayerStates[activatedByPlayerId].State.TimesRolledMysteryBox += 1;
	}

	//
	if (moby->State == MYSTERY_BOX_STATE_IDLE)
	{
		pvars->ItemIdx = itemIdx;
		pvars->Random = random;
		pvars->ActivatedByPlayerId = activatedByPlayerId;
		pvars->ActivatedTime = gameGetTime();
		pvars->StateChangedAtTime = gameGetTime();

		// charge player
		if (MapConfig.State)
		{
			int cost = mboxGetCost(moby, activatedByPlayerId);
			MapConfig.State->PlayerStates[activatedByPlayerId].State.Bolts -= cost;
		}

		mobySetState(moby, MYSTERY_BOX_STATE_OPENING, -1);
		mboxPlayOpenSound(moby);
	}

	return 0;
}

//--------------------------------------------------------------------------
int mboxHandleEvent_GivePlayer(Moby *moby, GuberEvent *event)
{
	int playerId, itemIdx, random, i;
	Player **players = playerGetAll();

	// DPRINTF("mbox give player: %08X\n", (u32)moby);
	struct MysteryBoxPVar *pvars = (struct MysteryBoxPVar *)moby->PVar;
	if (!pvars)
		return 0;

	// read
	guberEventRead(event, &playerId, 4);
	guberEventRead(event, &itemIdx, 4);
	guberEventRead(event, &random, 4);

	// set set to closing
	if (moby->State == MYSTERY_BOX_STATE_DISPLAYING_ITEM)
	{
		mobySetState(moby, MYSTERY_BOX_STATE_CLOSING, -1);
		pvars->StateChangedAtTime = gameGetTime();
	}

	if (playerId >= 0)
	{
		Player *player = players[playerId];
		if (playerIsValid(player))
		{
			// make sure player can have item
			if (player->IsLocal && itemCanAcquire(player, itemIdx))
				itemBeginAcquire(playerId, itemIdx);

#if ITEM_IMMEDIATE_VOX_MYSTERYBOX
			if (itemIdx == ITEM_IMMEDIATE_VOX_MYSTERYBOX)
			{
				if (playerId >= 0)
					pvars->NumVoxPerPlayer[playerId]++;
				DPRINTF("mbox voxed %d %d times\n", playerId, pvars->NumVoxPerPlayer[playerId]);

				spawnExplosion(moby->Position, 5, 0x802060C0);
				damageRadius(moby, moby->Position, 0x00081801, 5, 5);
#if !DEBUGMBOX
				mboxSetRandomRespawn(moby, pvars->Random);
#endif
			}
#endif
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby *mboxGetGuber(Moby *moby)
{
	if (moby->OClass == MYSTERY_BOX_OCLASS && moby->PVar)
		return moby->GuberMoby;

	return 0;
}

//--------------------------------------------------------------------------
int mboxHandleEvent(Moby *moby, GuberEvent *event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == MYSTERY_BOX_OCLASS && moby->PVar)
	{
		u32 mboxEvent = event->NetEvent.EventID;

		switch (mboxEvent)
		{
		case MYSTERY_BOX_EVENT_SPAWN:
			return mboxHandleEvent_Spawned(moby, event);
		case MYSTERY_BOX_EVENT_ACTIVATE:
			return mboxHandleEvent_Activate(moby, event);
		case MYSTERY_BOX_EVENT_GIVE_PLAYER:
			return mboxHandleEvent_GivePlayer(moby, event);
		default:
		{
			DPRINTF("unhandle mbox event %d\n", mboxEvent);
			break;
		}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int mboxCreate(VECTOR position, VECTOR rotation)
{
	// create guber object
	GuberEvent *guberEvent = 0;
	guberMobyCreateSpawned(MYSTERY_BOX_OCLASS, sizeof(struct MysteryBoxPVar), &guberEvent, NULL);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, rotation, 12);
	}
	else
	{
		DPRINTF("failed to guberevent mbox\n");
	}

	return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void mboxSpawn(void)
{
	static int spawned = 0;

	if (spawned)
		return;

	// spawn
	if (gameAmIHost())
	{

		VECTOR p = {184.1991, 439.6302, 85.858, 0};
		VECTOR r = {0, 0, 0, 0};
		mboxGetRandomRespawn(rand(10), p, r);

		mboxCreate(p, r);
	}

	spawned = 1;
}

//--------------------------------------------------------------------------
void mboxInit(void)
{
	Moby *temp = mobySpawn(MYSTERY_BOX_OCLASS, 0);
	if (!temp)
		return;

	// set vtable callbacks
	u32 mobyFunctionsPtr = (u32)mobyGetFunctions(temp);
	if (mobyFunctionsPtr)
	{
		mapInstallMobyFunctions(mobyFunctionsPtr);
		DPRINTF("MBOX oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, mobyFunctionsPtr, *(u32 *)(mobyFunctionsPtr + 0x04), *(u32 *)(mobyFunctionsPtr + 0x14));
	}
	mobyDestroy(temp);

	// collect mbox locations by finding and destroying all placed mobies
	Moby *moby = mobyListGetStart();
	MysteryBoxLocationCount = 0;
	while ((moby = mobyFindNextByOClass(moby, MYSTERY_BOX_OCLASS)))
	{
		if (!mobyIsDestroyed(moby))
		{
			if (MysteryBoxLocationCount < MYSTERY_BOX_MAX_LOCATIONS)
			{
				vector_copy(MysteryBoxPositions[MysteryBoxLocationCount], moby->Position);
				vector_copy(MysteryBoxRotations[MysteryBoxLocationCount], moby->Rotation);
				MysteryBoxLocationCount++;
			}

			mobyDestroy(moby); // destroy
		}

		++moby;
	}

#if DEBUGMBOX
	memset(MysteryBoxItemCounts, 0, sizeof(MysteryBoxItemCounts));
#endif
}
