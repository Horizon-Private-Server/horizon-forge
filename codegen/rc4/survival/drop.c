#include <string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/color.h>
#include <libdl/collision.h>
#include <libdl/moby.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include "drop.h"
#include "mob.h"
#include "maputils.h"
#include "utils.h"
#include "game.h"

const char *DROP_CANNOT_PICKUP_MESSAGE = "Property of %s";

int dropCount = 0;
int dropThisFrame = 0;
char dropLocalStrBuf[GAME_MAX_LOCALS][64];

GuberEvent *dropCreateEvent(Moby *moby, u32 eventType);

//--------------------------------------------------------------------------
int dropGetItem(int itemIdx, SurvivalItemDef_t *item)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	memcpy(item, &MapConfig.ItemDefs[itemIdx], sizeof(SurvivalItemDef_t));
	return 1;
}

//--------------------------------------------------------------------------
int dropGetItems(int forPlayerId, int *itemIdxs, int count)
{
	if (!itemIdxs)
		return 0;

	// build list items with mysterybox chance
	int outIdx = 0;
	int i;
	SurvivalItemDef_t itemDef;
	Player *player = playerGetFromIndex(forPlayerId);
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		if (outIdx >= count)
			break;

		dropGetItem(i, &itemDef);
		float itemWeight = itemGetDropChanceWeight(i, &itemDef, forPlayerId);
		int canAcquire = itemCanAcquire(player, i);
		if (itemWeight > 0 && canAcquire)
		{
			itemIdxs[outIdx] = i;
			outIdx++;
		}
	}

	return outIdx;
}

//--------------------------------------------------------------------------
int dropGetRandomItem(Moby *mobMoby, int forPlayerId, int gadgetId)
{
	int i;
	SurvivalItemDef_t itemDef;
	int itemIdxs[MAX_ITEM_COUNT];
	int count = dropGetItems(forPlayerId, &itemIdxs, MAX_ITEM_COUNT);
	if (count <= 0)
		return -1;

	// get sum
	float sumWeight = 0;
	for (i = 0; i < count; ++i)
	{
		int itemIdx = itemIdxs[i];
		dropGetItem(itemIdxs[i], &itemDef);
		float itemWeight = itemGetDropChanceWeight(itemIdx, &itemDef, forPlayerId);

		sumWeight += itemWeight;
	}

	// generate random value within range of weights
	// grab the first item where r > last
	float r = randRange(0, sumWeight);
	for (i = 0; i < (count - 1); ++i)
	{
		int itemIdx = itemIdxs[i];
		dropGetItem(itemIdx, &itemDef);
		float itemWeight = itemGetDropChanceWeight(itemIdx, &itemDef, forPlayerId);
		if (r < itemWeight)
		{
			DPRINTF("drop hit %d (%s) %f<%f\n", itemIdxs[i], itemDef.Name, r, itemWeight);
			break;
		}

		r -= itemWeight;
	}

	return itemIdxs[i];
}

//--------------------------------------------------------------------------
int dropAmIOwner(Moby *moby)
{
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;
	Player *player = playerGetFromIndex(pvars->OwnerPlayerId);
	return playerIsValid(player) && player->IsLocal;
}

//--------------------------------------------------------------------------
void dropPlayPickupSound(Moby *moby)
{
	mobyPlaySoundByClass(1, 0, moby, MOBY_ID_PICKUP_PAD);
}

//--------------------------------------------------------------------------
void dropDestroy(Moby *moby)
{
	// create event
	dropCreateEvent(moby, DROP_EVENT_DESTROY);
}

//--------------------------------------------------------------------------
void dropPickup(Moby *moby, int pickedUpByPlayerId)
{
	// create event
	GuberEvent *guberEvent = dropCreateEvent(moby, DROP_EVENT_PICKUP);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, &pickedUpByPlayerId, sizeof(int));
	}
}

//--------------------------------------------------------------------------
void dropPostDraw(Moby *moby)
{
	struct QuadDef quad;
	MATRIX m2;
	VECTOR t;
	VECTOR pTL = {0.5, 0, 0.5, 1};
	VECTOR pTR = {-0.5, 0, 0.5, 1};
	VECTOR pBL = {0.5, 0, -0.5, 1};
	VECTOR pBR = {-0.5, 0, -0.5, 1};
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;
	if (!pvars || mobyIsDestroyed(moby))
		return;

	// determine color
	u32 color = pvars->TexColor;

	// fade as we approach destruction
	int timeUntilDestruction = (pvars->DestroyAtTime - gameGetTime()) / TIME_SECOND;
	if (timeUntilDestruction < 1)
		timeUntilDestruction = 1;

	if (timeUntilDestruction < 10)
	{
		float speed = timeUntilDestruction < 3 ? 20.0 : 3.0;
		float pulse = (1 + sinf((gameGetTime() / 1000.0) * speed)) * 0.5;
		int opacity = 32 + (pulse * 96);
		color = (opacity << 24) | (color & 0xFFFFFF);
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
	quad.Tex0 = gfxGetFrameTex(pvars->TexId);
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
	memcpy(&m2[12], moby->Position, sizeof(VECTOR));

	// draw
	gfxDrawQuad((void *)0x00222590, &quad, m2, 1);
}

//--------------------------------------------------------------------------
void dropUpdate(Moby *moby)
{
	const float rotSpeeds[] = {0.05, 0.02, -0.03, -0.1};
	const int opacities[] = {64, 32, 44, 51};

	VECTOR t;
	VECTOR down = {0, 0, -2 * MATH_DT, 0};
	VECTOR offset = {0, 0, 1, 0};
	int i;
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;
	GameSettings *gs = gameGetSettings();
	if (!pvars)
		return;

	Player *ownerPlayer = playerGetFromIndex(pvars->OwnerPlayerId);
	int isOwner = dropAmIOwner(moby);

	// register draw event
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&dropPostDraw, moby);

	// add radar blip (star)
	if (playerIsValid(ownerPlayer))
		addRadarBlip(moby, 31, 17, ownerPlayer->Team);

	// fall to ground
	if (!pvars->HitGround)
	{
		vector_add(t, moby->Position, down);
		vector_subtract(t, t, offset);
		if (CollLine_Fix(moby->Position, t, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
		{
			pvars->HitGround = 1;
		}
		else
		{
			vector_add(moby->Position, t, offset);
		}
	}

	// handle particles
	u32 color = colorLerp(0, TEAM_COLORS[pvars->Team], 1.0 / 4);
	color |= 0x40000000;
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
			vector_copy(particle->Position, moby->Position);
		}
	}

	// handle pickup
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (player && !playerIsDead(player) && player->IsLocal)
		{
			vector_subtract(t, player->PlayerPosition, moby->Position);
			if (vector_sqrmag(t) < (DROP_PICKUP_RADIUS * DROP_PICKUP_RADIUS))
			{
				if (isOwner)
				{
					dropPickup(moby, i);
				}
				else
				{
					snprintf(dropLocalStrBuf[player->LocalPlayerIndex], sizeof(dropLocalStrBuf[player->LocalPlayerIndex]), DROP_CANNOT_PICKUP_MESSAGE, gs->PlayerNames[pvars->OwnerPlayerId]);
					uiShowPopup(player->LocalPlayerIndex, dropLocalStrBuf[player->LocalPlayerIndex]);
				}
				break;
			}
		}
	}

	// handle auto destruct
	if (pvars->DestroyAtTime && gameGetTime() > pvars->DestroyAtTime)
	{
		dropDestroy(moby);
	}
}

//--------------------------------------------------------------------------
GuberEvent *dropCreateEvent(Moby *moby, u32 eventType)
{
	GuberEvent *event = NULL;

	// create guber object
	Guber *guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
int dropHandleEvent_Spawn(Moby *moby, GuberEvent *event)
{
	VECTOR p;
	struct DropSpawnEventArgs args;
	VECTOR offset = {0, 0, 1.5, 0};

	// read event
	guberEventRead(event, p, 12);
	guberEventRead(event, &args, sizeof(struct DropSpawnEventArgs));

	// set position
	vector_add(moby->Position, p, offset);

	// set update
	moby->PUpdate = &dropUpdate;

	//
	moby->ModeBits &= ~2;
	// moby->GlowRGBA = MobSecondaryColors[(int)args.MobType];
	// moby->PrimaryColor = MobPrimaryColors[(int)args.MobType];
	moby->CollData = NULL;
	moby->DrawDist = 0;
	// moby->PClass = NULL;

	SurvivalItemDef_t itemDef;
	int hasItem = dropGetItem(args.ItemIdx, &itemDef);

	// update pvars
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;
	pvars->ItemIdx = args.ItemIdx;
	pvars->TexId = itemDef.TexId;
	pvars->TexColor = itemDef.TexColor;
	pvars->Team = args.Team;
	pvars->DestroyAtTime = args.DestroyAtTime;
	pvars->OwnerPlayerId = args.OwnerPlayerId;
	pvars->Destroyed = 0;
	memset(pvars->Particles, 0, sizeof(pvars->Particles));

	// set team
	Guber *guber = guberGetObjectByMoby(moby);
	if (guber)
		((GuberMoby *)guber)->TeamNum = args.Team;

	//
	++dropCount;

	//
	mobySetState(moby, 0, -1);
	DPRINTF("drop spawned at %08X item:%d team:%d destroyAt:%d\n", (u32)moby, pvars->ItemIdx, pvars->Team, pvars->DestroyAtTime);
	return 0;
}

//--------------------------------------------------------------------------
int dropHandleEvent_Destroy(Moby *moby, GuberEvent *event)
{
	int i;
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;
	if (!pvars || pvars->Destroyed)
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

	// pvars->Destroyed = 1;
	guberMobyDestroy(moby);
	--dropCount;
	return 0;
}

//--------------------------------------------------------------------------
int dropHandleEvent_Pickup(Moby *moby, GuberEvent *event)
{
	struct DropPickupEventArgs args;
	int i, j;
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;

	if (!pvars || pvars->Destroyed)
		return 0;

	// read event
	guberEventRead(event, &args, sizeof(struct DropPickupEventArgs));

	// acquire item
	if (dropAmIOwner(moby))
		itemBeginAcquire(args.PickedUpByPlayerId, pvars->ItemIdx);

	// destroy particles
	for (i = 0; i < 4; ++i)
	{
		if (pvars->Particles[i])
		{
			destroyParticle(pvars->Particles[i]);
			pvars->Particles[i] = 0;
		}
	}

	// play pickup sound
	dropPlayPickupSound(moby);

	// pvars->Destroyed = 1;
	guberMobyDestroy(moby);
	--dropCount;
	return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby *dropGetGuber(Moby *moby)
{
	if (moby->OClass == DROP_MOBY_OCLASS && moby->PVar)
		return moby->GuberMoby;

	return 0;
}

//--------------------------------------------------------------------------
int dropHandleEvent(Moby *moby, GuberEvent *event)
{
	struct DropPVar *pvars = (struct DropPVar *)moby->PVar;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == DROP_MOBY_OCLASS && pvars)
	{
		u32 dropEvent = event->NetEvent.EventID;

		switch (dropEvent)
		{
		case DROP_EVENT_SPAWN:
			return dropHandleEvent_Spawn(moby, event);
		case DROP_EVENT_DESTROY:
			return dropHandleEvent_Destroy(moby, event);
		case DROP_EVENT_PICKUP:
			return dropHandleEvent_Pickup(moby, event);
		default:
		{
			DPRINTF("unhandle drop event %d\n", dropEvent);
			break;
		}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void dropOnCreated(VECTOR position, int itemIdx, int destroyAtTime, int team)
{
	if (!MapConfig.State)
		return;

	int min = DROP_COOLDOWN_TICKS_MIN;
	int max = DROP_COOLDOWN_TICKS_MAX;

#ifdef MOB_DROP_COOLDOWN_MIN
	min = MOB_DROP_COOLDOWN_MIN;
#endif

#ifdef MOB_DROP_COOLDOWN_MAX
	max = MOB_DROP_COOLDOWN_MAX;
#endif

	// set cooldown
	MapConfig.State->DropCooldownTicks = randRangeInt(min, max);
}

//--------------------------------------------------------------------------
int dropCreate(VECTOR position, int itemIdx, int destroyAtTime, int team)
{
	struct DropSpawnEventArgs args;

	// create guber object
	GuberEvent *guberEvent = 0;
	guberMobyCreateSpawned(DROP_MOBY_OCLASS, sizeof(struct DropPVar), &guberEvent, NULL);
	if (guberEvent)
	{
		args.OwnerPlayerId = playerGetFromSlot(0)->PlayerId;
		args.ItemIdx = itemIdx;
		args.DestroyAtTime = destroyAtTime;
		args.Team = team;

		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, &args, sizeof(struct DropSpawnEventArgs));
		dropThisFrame = 1;
	}
	else
	{
		DPRINTF("failed to guberevent drop\n");
	}

	dropOnCreated(position, itemIdx, destroyAtTime, team);
	return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void dropInit(void)
{
	Moby *temp = mobySpawn(DROP_MOBY_OCLASS, 0);
	if (!temp)
		return;

	// set vtable callbacks
	u32 mobyFunctionsPtr = (u32)mobyGetFunctions(temp);
	if (mobyFunctionsPtr)
	{
		mapInstallMobyFunctions(mobyFunctionsPtr);
		DPRINTF("DROP oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, mobyFunctionsPtr, *(u32 *)(mobyFunctionsPtr + 0x04), *(u32 *)(mobyFunctionsPtr + 0x14));
	}
	mobyDestroy(temp);

	MapConfig.Functions.CreateMobDropFunc = &dropCreate;
}

//--------------------------------------------------------------------------
void dropTick(void)
{
	dropThisFrame = 0;

	if (MapConfig.State && MapConfig.State->DropCooldownTicks > 0)
		--MapConfig.State->DropCooldownTicks;
}
