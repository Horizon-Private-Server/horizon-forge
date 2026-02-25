#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/color.h>
#include <libdl/collision.h>
#include <libdl/moby.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include <libdl/string.h>
#include "upgrade.h"
#include "drop.h"
#include "mob.h"
#include "utils.h"
#include "game.h"
#include "utils.h"
#include "maputils.h"

GuberEvent *upgradeCreateEvent(Moby *moby, u32 eventType);

char UpgradeBakedSpawnInUse[BAKED_SPAWNPOINT_COUNT] = {};

//--------------------------------------------------------------------------
int upgradeGetItems(int *itemIdxs, int count)
{
	if (!itemIdxs)
		return 0;

	// build list items with mysterybox chance
	int outIdx = 0;
	int i;
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		if (outIdx >= count)
			break;

		if (MapConfig.ItemDefs[i].AppearOnWall)
		{
			itemIdxs[outIdx] = i;
			outIdx++;
		}
	}

	return outIdx;
}

//--------------------------------------------------------------------------
int upgradeGetItem(int itemIdx, SurvivalItemDef_t *item)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	memcpy(item, &MapConfig.ItemDefs[itemIdx], sizeof(SurvivalItemDef_t));
	return 1;
}

//--------------------------------------------------------------------------
void upgradePlayPickupSound(Moby *moby)
{
	mobyPlaySoundByClass(1, 0, moby, MOBY_ID_PICKUP_PAD);
}

//--------------------------------------------------------------------------
void upgradeDestroy(Moby *moby)
{
	// create event
	upgradeCreateEvent(moby, UPGRADE_EVENT_DESTROY);
}

//--------------------------------------------------------------------------
void upgradePickup(Moby *moby, int pickedUpByPlayerId)
{
	// create event
	GuberEvent *guberEvent = upgradeCreateEvent(moby, UPGRADE_EVENT_PICKUP);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, &pickedUpByPlayerId, sizeof(int));
	}
}

//--------------------------------------------------------------------------
int upgradeSpawnNew(int currBakedSpawnIdx, int itemIdx)
{
	char freeBakedSpawnPointsIdxs[BAKED_SPAWNPOINT_COUNT];
	int freeBakedSpawnPointsIdxCount = 0;
	int i, j;
	VECTOR spPos;
	if (!MapConfig.State)
		return 0;

	if (!MapConfig.Functions.CreateUpgradePickupFunc)
		return 0;

	// iterate list of baked spawn points
	for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i)
	{
		if (bakedConfig.BakedSpawnPoints[i].Type != BAKED_SPAWNPOINT_UPGRADE)
			continue;

		// allow respawning at same location
		if (UpgradeBakedSpawnInUse[i] == 0 || currBakedSpawnIdx == i)
			freeBakedSpawnPointsIdxs[freeBakedSpawnPointsIdxCount++] = i;
	}

	// if no free points then fail
	if (!freeBakedSpawnPointsIdxCount)
		return 0;

	// pick random
	int random = randRangeInt(0, 100) % freeBakedSpawnPointsIdxCount;
	i = freeBakedSpawnPointsIdxs[random];

	// if picked current position, try to move to another
	memcpy(spPos, bakedConfig.BakedSpawnPoints[i].Position, 12);
	if (currBakedSpawnIdx == i)
		i = freeBakedSpawnPointsIdxs[(random + 1) % freeBakedSpawnPointsIdxCount];

	// spawn
	MapConfig.Functions.CreateUpgradePickupFunc(i, itemIdx);
	return 1;
}

//--------------------------------------------------------------------------
void upgradePostDraw(Moby *moby)
{
	struct QuadDef quad;
	MATRIX m2;
	VECTOR pTL = {0.5, 0, 0.5, 1};
	VECTOR pTR = {-0.5, 0, 0.5, 1};
	VECTOR pBL = {0.5, 0, -0.5, 1};
	VECTOR pBR = {-0.5, 0, -0.5, 1};
	struct UpgradePVar *pvars = (struct UpgradePVar *)moby->PVar;
	if (!pvars)
		return;

	SurvivalItemDef_t itemDef;
	if (!upgradeGetItem(pvars->ItemIdx, &itemDef))
		return;

	// determine color
	u32 color = itemDef.TexColor & 0xffffff;
	float opacity = lerpf(0, 1, clamp(pvars->Uses / 5.0, 0, 1));
	color |= (u8)(0x70 * opacity) << 24;
	pvars->Opacity = opacity;

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
	quad.Tex0 = gfxGetFrameTex(itemDef.TexId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	// copy from moby
	memcpy(m2, moby->M0_03, sizeof(VECTOR) * 3);
	memcpy(&m2[12], moby->Position, sizeof(VECTOR));

	// draw
	gfxDrawQuad((void *)0x00222590, &quad, m2, 1);
}

//--------------------------------------------------------------------------
void upgradeUpdate(Moby *moby)
{
	const float rotSpeeds[] = {0.05, 0.02, -0.03, -0.1};
	const int opacities[] = {64, 32, 44, 51};

	int i;
	struct UpgradePVar *pvars = (struct UpgradePVar *)moby->PVar;
	char costBuf[32];
	char descBuf[64];
	char interactBuf[64];
	if (!pvars)
		return;

	SurvivalItemDef_t itemDef;
	upgradeGetItem(pvars->ItemIdx, &itemDef);

	// register draw event
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&upgradePostDraw, moby);

	// draw on radar
	addRadarBlip(moby, 31, 4, TEAM_AQUA);

	// handle interaction
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player) || !MapConfig.State)
			continue;

		struct SurvivalPlayer *playerData = &MapConfig.State->PlayerStates[player->PlayerId];
		if (vector_sqrdistance(moby->Position, player->PlayerPosition) > (UPGRADE_PICKUP_RADIUS * UPGRADE_PICKUP_RADIUS))
			continue;

		// draw description even if player can't acquire
		itemGetDescription(descBuf, sizeof(descBuf), pvars->ItemIdx, player->PlayerId);
		pushSnack(player->LocalPlayerIndex, descBuf, 0);

		if (!itemCanAcquire(player, pvars->ItemIdx))
		{
			// draw maxed out popup
			safe_strcpy(interactBuf, "Maxed Out", 10);
			tryPlayerInteract(moby, player, interactBuf, NULL, 0, 0, 0, 100, 0, 0);
			continue;
		}

		u32 cost = itemGetCost(i, pvars->ItemIdx);
		int tokens = itemDef.StoreCostType >= SURVIVAL_ITEM_STORE_COST_TOKENS_LINEAR;
		uiPrintCommaNumber(costBuf, sizeof(costBuf), cost, 0);
		snprintf(interactBuf, sizeof(interactBuf), "%s (%d)\x01\x01\x11   \x0E%s\x08 %s", itemDef.Name, pvars->Uses, costBuf, tokens ? "Tokens" : "Bolts");
		if (tryPlayerInteract(moby, player, interactBuf, NULL, tokens ? 0 : cost, tokens ? cost : 0, PLAYER_UPGRADE_COOLDOWN_TICKS, 100, PAD_CIRCLE, 1))
		{
			if (itemChargePlayerBank(i, pvars->ItemIdx))
			{
				MapConfig.Functions.PickupUpgradeFunc(moby, player->PlayerId);
				playPaidSound(player);
			}
		}
	}

	return;

	// handle particles
	u32 color = 0x80C0C0C0;
	for (i = 0; i < 4; ++i)
	{
		struct PartInstance *particle = pvars->Particles[i];
		if (!particle)
		{
			pvars->Particles[i] = particle = spawnParticle(moby->Position, color, opacities[i], i);
		}

		// update
		if (particle)
		{
			particle->Rot = (int)((gameGetTime() + (i * 100)) / (TIME_SECOND * rotSpeeds[i])) & 0xFF;
		}
	}
}

//--------------------------------------------------------------------------
GuberEvent *upgradeCreateEvent(Moby *moby, u32 eventType)
{
	GuberEvent *event = NULL;

	// create guber object
	Guber *guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent_Spawn(Moby *moby, GuberEvent *event)
{
	struct UpgradeSpawnEventArgs args;

	// read event
	guberEventRead(event, &args, sizeof(struct UpgradeSpawnEventArgs));

	// add to table
	UpgradeBakedSpawnInUse[args.BakedSpawnIdx]++;

	// set position
	memcpy(moby->Position, bakedConfig.BakedSpawnPoints[args.BakedSpawnIdx].Position, 12);
	memcpy(moby->Rotation, bakedConfig.BakedSpawnPoints[args.BakedSpawnIdx].Rotation, 12);

	// set update
	moby->PUpdate = &upgradeUpdate;

	//
	// moby->ModeBits |= 0x30;
	// moby->GlowRGBA = MobSecondaryColors[(int)args.MobType];
	// moby->PrimaryColor = MobPrimaryColors[(int)args.MobType];
	moby->CollData = NULL;
	moby->DrawDist = 0;
	moby->ModeBits = 0;
	moby->AnimSeq = NULL;
	moby->AnimSeqId = moby->LSeq = 0;
	// moby->PClass = NULL;

	SurvivalItemDef_t item;
	upgradeGetItem(args.ItemIdx, &item);

	// update pvars
	struct UpgradePVar *pvars = (struct UpgradePVar *)moby->PVar;
	pvars->ItemIdx = args.ItemIdx;
	pvars->Uses = UPGRADE_MAX_USES;
	pvars->MaxUses = UPGRADE_MAX_USES;
	pvars->TexId = item.TexId;
	pvars->TexColor = item.TexColor;
	pvars->BakedSpawnIdx = args.BakedSpawnIdx;
	memset(pvars->Particles, 0, sizeof(pvars->Particles));

	// set team
	Guber *guber = guberGetObjectByMoby(moby);
	if (guber)
		((GuberMoby *)guber)->TeamNum = 10;

	mobySetState(moby, 0, -1);
	DPRINTF("upgrade spawned at %08X item:%d\n", (u32)moby, pvars->ItemIdx);
	return 0;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent_Destroy(Moby *moby, GuberEvent *event)
{
	int i;
	struct UpgradePVar *pvars = (struct UpgradePVar *)moby->PVar;
	if (!pvars)
		return 0;

	// destroy particles
	for (i = 0; i < 4; ++i)
	{
		if (pvars->Particles[i])
		{
			destroyParticle(pvars->Particles[i]);
			pvars->Particles[i] = 0;
		}
	}

	// remove from table
	if (UpgradeBakedSpawnInUse[pvars->BakedSpawnIdx] > 0)
		UpgradeBakedSpawnInUse[pvars->BakedSpawnIdx]--;

	guberMobyDestroy(moby);
	return 0;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent_Pickup(Moby *moby, GuberEvent *event)
{
	struct UpgradePickupEventArgs args;
	struct UpgradePVar *pvars = (struct UpgradePVar *)moby->PVar;

	if (!pvars)
		return 0;

	// read event
	guberEventRead(event, &args, sizeof(struct UpgradePickupEventArgs));

	Player *targetPlayer = playerGetFromIndex(args.PickedUpByPlayerId);
	if (!targetPlayer)
		return 0;

	// give
	if (targetPlayer->IsLocal)
		itemBeginAcquire(args.PickedUpByPlayerId, pvars->ItemIdx);

	// play pickup sound
	upgradePlayPickupSound(moby);

	// reduce uses, if not post round 25 break
	// respawn at next spot if used
	if (!MapConfig.State || MapConfig.State->RoundEndTime != -1)
	{
		pvars->Uses--;
	}

	if (pvars->Uses <= 0 && gameAmIHost())
	{
		VECTOR lastPos;
		vector_copy(lastPos, moby->Position);

		// use the state to check if we've already started the spawning of a new upgrade
		// in case multiple players use the last upgrade and send the Pickup message at the same time
		if (moby->State == 0)
		{
			mobySetState(moby, 1, -1);
			upgradeSpawnNew(pvars->BakedSpawnIdx, pvars->ItemIdx);
			upgradeDestroy(moby);
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby *upgradeGetGuber(Moby *moby)
{
	if (moby->OClass == UPGRADE_MOBY_OCLASS && moby->PVar)
		return moby->GuberMoby;

	return 0;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent(Moby *moby, GuberEvent *event)
{
	struct UpgradePVar *pvars = (struct UpgradePVar *)moby->PVar;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == UPGRADE_MOBY_OCLASS && pvars)
	{
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
		case UPGRADE_EVENT_SPAWN:
			return upgradeHandleEvent_Spawn(moby, event);
		case UPGRADE_EVENT_DESTROY:
			return upgradeHandleEvent_Destroy(moby, event);
		case UPGRADE_EVENT_PICKUP:
			return upgradeHandleEvent_Pickup(moby, event);
		default:
		{
			DPRINTF("unhandle upgrade event %d\n", upgradeEvent);
			break;
		}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int upgradeCreate(int bakedSpawnIdx, int itemIdx)
{
	struct UpgradeSpawnEventArgs args;

	// create guber object
	GuberEvent *guberEvent = 0;
	guberMobyCreateSpawned(UPGRADE_MOBY_OCLASS, sizeof(struct UpgradePVar), &guberEvent, NULL);
	if (guberEvent)
	{
		args.ItemIdx = itemIdx;
		args.BakedSpawnIdx = bakedSpawnIdx;

		guberEventWrite(guberEvent, &args, sizeof(struct UpgradeSpawnEventArgs));
	}
	else
	{
		DPRINTF("failed to guberevent upgrade\n");
	}

	return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void upgradeSpawn(void)
{
	static int spawned = 0;
	if (spawned)
		return;

	spawned = 1;

	// only spawn if host
	if (!gameAmIHost())
		return;

	// need to know where to spawn
	if (!MapConfig.Functions.GetBakedSpawnPointsFunc)
		return;

	VECTOR pos, rot;
	int i, j;
	int r;
	int bakedUpgradeSpawnpointCount = 0;
	char upgradeBakedSpawnpointIdx[MAX_ITEM_COUNT];
	int itemIdxs[MAX_ITEM_COUNT];
	int itemCount = upgradeGetItems(itemIdxs, MAX_ITEM_COUNT);

	// initialize spawnpoint idx to -1
	// and set moby ref to NULL
	for (i = 0; i < MAX_ITEM_COUNT; ++i)
	{
		upgradeBakedSpawnpointIdx[i] = -1;
	}

	// count number of baked spawnpoints used for upgrade
	int bakedSpawnPointCount = 0;
	SurvivalBakedSpawnpoint_t *bakedSpawnPoints = MapConfig.Functions.GetBakedSpawnPointsFunc(&bakedSpawnPointCount);
	for (i = 0; i < bakedSpawnPointCount; ++i)
	{
		if (bakedSpawnPoints[i].Type == BAKED_SPAWNPOINT_UPGRADE)
		{
			upgradeBakedSpawnpointIdx[bakedUpgradeSpawnpointCount] = i;
			++bakedUpgradeSpawnpointCount;
		}
	}

	// shuffle spawn points
	for (i = 0; i < bakedUpgradeSpawnpointCount; ++i)
	{
		for (j = 0; j < bakedUpgradeSpawnpointCount; ++j)
		{
			int r = rand(bakedUpgradeSpawnpointCount);
			int temp = upgradeBakedSpawnpointIdx[j];
			upgradeBakedSpawnpointIdx[j] = upgradeBakedSpawnpointIdx[r];
			upgradeBakedSpawnpointIdx[r] = temp;
		}
	}

	// spawn
	for (i = 0; i < itemCount; ++i)
	{
		int itemIdx = itemIdxs[i];
		int spawnPoint = upgradeBakedSpawnpointIdx[i];
		if (spawnPoint < 0)
			continue;

		// spawn
		if (MapConfig.Functions.CreateUpgradePickupFunc)
			MapConfig.Functions.CreateUpgradePickupFunc(spawnPoint, itemIdx);
	}
}

//--------------------------------------------------------------------------
void upgradeInit(void)
{
	memset(UpgradeBakedSpawnInUse, 0, sizeof(UpgradeBakedSpawnInUse));
	Moby *temp = mobySpawn(UPGRADE_MOBY_OCLASS, 0);
	if (!temp)
		return;

	// set vtable callbacks
	u32 mobyFunctionsPtr = (u32)mobyGetFunctions(temp);
	if (mobyFunctionsPtr)
	{
		mapInstallMobyFunctions(mobyFunctionsPtr);
		DPRINTF("UPGRADE oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, mobyFunctionsPtr, *(u32 *)(mobyFunctionsPtr + 0x04), *(u32 *)(mobyFunctionsPtr + 0x14));
	}
	mobyDestroy(temp);

	MapConfig.Functions.CreateUpgradePickupFunc = &upgradeCreate;
	MapConfig.Functions.PickupUpgradeFunc = &upgradePickup;
}

//--------------------------------------------------------------------------
void upgradeTick(void)
{
}
