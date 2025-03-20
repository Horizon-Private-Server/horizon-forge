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
#include "game.h"

int dropCount = 0;
int dropThisFrame = 0;

GuberEvent* dropCreateEvent(Moby* moby, u32 eventType);

char DropTexIds[] = {
	[DROP_AMMO] 90,
	[DROP_HEALTH] 94,
	[DROP_NUKE] 19,
	[DROP_FREEZE] 43,
	[DROP_DOUBLE_POINTS] 3,
  [DROP_DOUBLE_XP] 41,
};

//--------------------------------------------------------------------------
int dropAmIOwner(Moby* moby)
{
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;
	return gameGetMyClientId() == pvars->Owner;
}

//--------------------------------------------------------------------------
void dropPlayPickupSound(Moby* moby)
{
  mobyPlaySoundByClass(1, 0, moby, MOBY_ID_PICKUP_PAD);
}	

//--------------------------------------------------------------------------
void dropDestroy(Moby* moby)
{
	// create event
	dropCreateEvent(moby, DROP_EVENT_DESTROY);
}

//--------------------------------------------------------------------------
void dropPickup(Moby* moby, int pickedUpByPlayerId)
{
	// create event
	GuberEvent * guberEvent = dropCreateEvent(moby, DROP_EVENT_PICKUP);
	if (guberEvent) {
		guberEventWrite(guberEvent, &pickedUpByPlayerId, sizeof(int));
	}
}

//--------------------------------------------------------------------------
void dropPostDraw(Moby* moby)
{
	struct QuadDef quad;
	MATRIX m2;
	VECTOR t;
	VECTOR pTL = {0.5,0,0.5,1};
	VECTOR pTR = {-0.5,0,0.5,1};
	VECTOR pBL = {0.5,0,-0.5,1};
	VECTOR pBR = {-0.5,0,-0.5,1};
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;
	if (!pvars)
		return;

	// determine color
	u32 color = 0x70FFFFFF;

	// fade as we approach destruction
	int timeUntilDestruction = (pvars->DestroyAtTime - gameGetTime()) / TIME_SECOND;
	if (timeUntilDestruction < 1)
		timeUntilDestruction = 1;

	if (timeUntilDestruction < 10) {
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
  quad.VertexUVs[0] = (struct UV){0,0};
  quad.VertexUVs[1] = (struct UV){1,0};
  quad.VertexUVs[2] = (struct UV){0,1};
  quad.VertexUVs[3] = (struct UV){1,1};
	quad.Clamp = 0x0000000100000001;
	quad.Tex0 = gfxGetFrameTex(DropTexIds[pvars->Type]);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera* camera = cameraGetGameCamera(0);
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
	gfxDrawQuad((void*)0x00222590, &quad, m2, 1);
}

//--------------------------------------------------------------------------
void dropUpdate(Moby* moby)
{
	const float rotSpeeds[] = { 0.05, 0.02, -0.03, -0.1 };
	const int opacities[] = { 64, 32, 44, 51 };

	VECTOR t;
  VECTOR down = {0,0,-2 * MATH_DT,0};
  VECTOR offset = {0,0,1,0};
	int i;
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;
	Player** players = playerGetAll();
	if (!pvars)
		return;

	int isOwner = dropAmIOwner(moby);

	// register draw event
	gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&dropPostDraw, moby);

  // fall to ground
  if (!pvars->HitGround) {
    vector_add(t, moby->Position, down);
    vector_subtract(t, t, offset);
    if (CollLine_Fix(moby->Position, t, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
      pvars->HitGround = 1;
    } else {
      vector_add(moby->Position, t, offset);
    }
  }

	// handle particles
	u32 color = colorLerp(0, TEAM_COLORS[pvars->Team], 1.0 / 4);
	color |= 0x40000000;
	for (i = 0; i < 4; ++i) {
		struct PartInstance * particle = pvars->Particles[i];
		if (!particle) {
			pvars->Particles[i] = particle = spawnParticle(moby->Position, color, opacities[i], i);
		}

		// update
		if (particle) {
			particle->Rot = (int)((gameGetTime() + (i * 100)) / (TIME_SECOND * rotSpeeds[i])) & 0xFF;
      vector_copy(particle->Position, moby->Position);
		}
	}

	if (!isOwner)
		return;

	// handle pickup
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player * player = players[i];
		if (player && !playerIsDead(player)) {
			vector_subtract(t, player->PlayerPosition, moby->Position);
			if (vector_sqrmag(t) < (DROP_PICKUP_RADIUS * DROP_PICKUP_RADIUS)) {
				dropPickup(moby, i);
				break;
			}
		}
	}

	// handle auto destruct
	if (pvars->DestroyAtTime && gameGetTime() > pvars->DestroyAtTime) {
		dropDestroy(moby);
	}
}

//--------------------------------------------------------------------------
GuberEvent* dropCreateEvent(Moby* moby, u32 eventType)
{
	GuberEvent * event = NULL;

	// create guber object
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
int dropHandleEvent_Spawn(Moby* moby, GuberEvent* event)
{
	VECTOR p;
	struct DropSpawnEventArgs args;
	VECTOR offset = {0,0,1.5,0};

	// read event
	guberEventRead(event, p, 12);
	guberEventRead(event, &args, sizeof(struct DropSpawnEventArgs));

	// set position
	vector_add(moby->Position, p, offset);

	// set update
	moby->PUpdate = &dropUpdate;

	// 
	moby->ModeBits &= ~2;
	//moby->GlowRGBA = MobSecondaryColors[(int)args.MobType];
	//moby->PrimaryColor = MobPrimaryColors[(int)args.MobType];
	moby->CollData = NULL;
	moby->DrawDist = 0;
	//moby->PClass = NULL;

	// update pvars
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;
	pvars->Type = args.Type;
	pvars->Team = args.Team;
	pvars->DestroyAtTime = args.DestroyAtTime;
	pvars->Owner = args.Owner;
	pvars->Destroyed = 0;
	memset(pvars->Particles, 0, sizeof(pvars->Particles));

	// set team
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		((GuberMoby*)guber)->TeamNum = args.Team;
	
	// 
	++dropCount;

	// 
	mobySetState(moby, 0, -1);
	DPRINTF("drop spawned at %08X type:%d team:%d destroyAt:%d\n", (u32)moby, pvars->Type, pvars->Team, pvars->DestroyAtTime);
	return 0;
}

//--------------------------------------------------------------------------
int dropHandleEvent_Destroy(Moby* moby, GuberEvent* event)
{
	int i;
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;
	if (!pvars || pvars->Destroyed)
		return 0;

	// destroy particles
	for (i = 0; i < 4; ++i) {
		if (pvars->Particles[i]) {
			destroyParticle(pvars->Particles[i]);
			pvars->Particles[i] = 0;
		}
	}

	//pvars->Destroyed = 1;
	guberMobyDestroy(moby);
	--dropCount;
	return 0;
}

//--------------------------------------------------------------------------
int dropHandleEvent_Pickup(Moby* moby, GuberEvent* event)
{
	struct DropPickupEventArgs args;
	int i,j;
	Player** players = playerGetAll();
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;

	if (!pvars || pvars->Destroyed)
		return 0;

	// read event
	guberEventRead(event, &args, sizeof(struct DropPickupEventArgs));

	// handle effect
	switch (pvars->Type)
	{
		case DROP_AMMO:
		{
			DPRINTF("giving ammo to all players\n");
			for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
				Player * p = players[i];
				if (p && p->SkinMoby) {
					for (j = 0; j <= 8; ++j) {
						int gadgetId = weaponSlotToId(j);
						if (p->GadgetBox->Gadgets[gadgetId].Level >= 0)
							p->GadgetBox->Gadgets[gadgetId].Ammo = playerGetWeaponMaxAmmo(p->GadgetBox, gadgetId);
					}

					if (p->IsLocal)
						uiShowPopup(p->LocalPlayerIndex, "You got ammo!");
				}
			}
			break;
		}
		case DROP_HEALTH:
		{
			DPRINTF("giving health to all players\n");
			for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
				Player * p = players[i];
				if (p && p->SkinMoby) {
					if (!playerIsDead(p) && p->Health > 0) {
						playerSetHealth(p, p->MaxHealth);
					}
					else if (MapConfig.ModeRevivePlayerFunc && MapConfig.State && MapConfig.State->PlayerStates[i].ReviveCooldownTicks) {
			      MapConfig.ModeRevivePlayerFunc(p, args.PickedUpByPlayerId);
					}

					if (p->IsLocal)
						uiShowPopup(p->LocalPlayerIndex, "You got health!");
				}
			}
			break;
		}
		case DROP_DOUBLE_POINTS:
		{
			DPRINTF("giving double bolts to all players\n");
			uiShowPopup(0, "Double bolts!");
			uiShowPopup(1, "Double bolts!");
			if (MapConfig.ModeSetDoublePointsFunc) MapConfig.ModeSetDoublePointsFunc(1);
			break;
		}
		case DROP_DOUBLE_XP:
		{
			DPRINTF("giving double xp to all players\n");
			uiShowPopup(0, "Double XP!");
			uiShowPopup(1, "Double XP!");
			if (MapConfig.ModeSetDoubleXPFunc) MapConfig.ModeSetDoubleXPFunc(1);
			break;
		}
		case DROP_FREEZE:
		{
			DPRINTF("freezing all mobs\n");
			uiShowPopup(0, "Freeze activated!");
			uiShowPopup(1, "Freeze activated!");
			if (MapConfig.ModeSetFreezeMobsFunc) MapConfig.ModeSetFreezeMobsFunc(1);
			break;
		}
		case DROP_NUKE:
		{
			DPRINTF("killing all mobs\n");
			uiShowPopup(0, "Nuke activated!");
			uiShowPopup(1, "Nuke activated!");
			if (MapConfig.ModeMobNukeFunc) MapConfig.ModeMobNukeFunc(args.PickedUpByPlayerId);
			break;
		}
	}

	// destroy particles
	for (i = 0; i < 4; ++i) {
		if (pvars->Particles[i]) {
			destroyParticle(pvars->Particles[i]);
			pvars->Particles[i] = 0;
		}
	}

	// play pickup sound
	dropPlayPickupSound(moby);

	//pvars->Destroyed = 1;
	guberMobyDestroy(moby);
	--dropCount;
	return 0;
}

//--------------------------------------------------------------------------
int dropHandleEvent(Moby* moby, GuberEvent* event)
{
	struct DropPVar* pvars = (struct DropPVar*)moby->PVar;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == DROP_MOBY_OCLASS && pvars) {
		u32 dropEvent = event->NetEvent.EventID;
		int isFromHost = gameIsHost(event->NetEvent.OriginClientIdx);
		if (!isFromHost)
		{
			DPRINTF("ignoring drop event %d from %d (not owner, %d)\n", dropEvent, event->NetEvent.OriginClientIdx, pvars->Owner);
			return 0;
		}

		switch (dropEvent)
		{
			case DROP_EVENT_SPAWN: return dropHandleEvent_Spawn(moby, event);
			case DROP_EVENT_DESTROY: return dropHandleEvent_Destroy(moby, event);
			case DROP_EVENT_PICKUP: return dropHandleEvent_Pickup(moby, event);
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
int dropCreate(VECTOR position, enum DropType dropType, int destroyAtTime, int team)
{
	if (!MapConfig.State || MapConfig.State->DropCooldownTicks > 0)
		return 0;

	struct DropSpawnEventArgs args;

	// set cooldown
	MapConfig.State->DropCooldownTicks = randRangeInt(DROP_COOLDOWN_TICKS_MIN, DROP_COOLDOWN_TICKS_MAX);

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(DROP_MOBY_OCLASS, sizeof(struct DropPVar), &guberEvent, NULL);
	if (guberEvent)
	{
		// owner is always host
		args.Owner = gameGetHostId();
		args.Type = dropType;
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
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void dropInit(void)
{
  MapConfig.CreateMobDropFunc = &dropCreate;
  MapConfig.OnMobDropEventFunc = &dropHandleEvent;
}

//--------------------------------------------------------------------------
void dropTick(void)
{
	dropThisFrame = 0;

	if (MapConfig.State && MapConfig.State->DropCooldownTicks > 0)
		--MapConfig.State->DropCooldownTicks;
}
