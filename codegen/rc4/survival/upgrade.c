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
#include "upgrade.h"
#include "drop.h"
#include "mob.h"
#include "utils.h"
#include "game.h"
#include "maputils.h"

GuberEvent* upgradeCreateEvent(Moby* moby, u32 eventType);

char UpgradeTexIds[] = {
	[UPGRADE_HEALTH] 94,
	[UPGRADE_SPEED] 52,
	[UPGRADE_DAMAGE] 9,
	[UPGRADE_MEDIC] 13,
	[UPGRADE_VENDOR] 46,
	[UPGRADE_PICKUPS] 2,
  [UPGRADE_CRIT] 7,
};

//--------------------------------------------------------------------------
void upgradePlayPickupSound(Moby* moby)
{
  mobyPlaySoundByClass(1, 0, moby, MOBY_ID_PICKUP_PAD);
}	

//--------------------------------------------------------------------------
void upgradeDestroy(Moby* moby)
{
	// create event
	upgradeCreateEvent(moby, UPGRADE_EVENT_DESTROY);
}

//--------------------------------------------------------------------------
void upgradePickup(Moby* moby, int pickedUpByPlayerId)
{
	// create event
	GuberEvent * guberEvent = upgradeCreateEvent(moby, UPGRADE_EVENT_PICKUP);
	if (guberEvent) {
		guberEventWrite(guberEvent, &pickedUpByPlayerId, sizeof(int));
	}
}

//--------------------------------------------------------------------------
void upgradeSpawnNew(enum UpgradeType type)
{
  char freeBakedSpawnPointsIdxs[BAKED_SPAWNPOINT_COUNT];
  int freeBakedSpawnPointsIdxCount = 0;
  int i,j;
  VECTOR delta, spPos;
  int isFree = 0;

  if (!MapConfig.State || !MapConfig.BakedConfig) return;

  // iterate list of baked spawn points
  for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {

    isFree = 1;
    
    // find upgrade spawn points
    if (MapConfig.BakedConfig->BakedSpawnPoints[i].Type == BAKED_SPAWNPOINT_UPGRADE) {

      // check if already in use
      for (j = 0; j < UPGRADE_COUNT; ++j) {
        if (MapConfig.State->UpgradeMobies[j]) {
          memcpy(spPos, MapConfig.BakedConfig->BakedSpawnPoints[i].Position, 12);
          vector_subtract(delta, MapConfig.State->UpgradeMobies[j]->Position, spPos);
          if (vector_sqrmag(delta) < 0.1) {
            isFree = 0;
            break;
          }
        }
      }

      // 
      if (isFree) {
        freeBakedSpawnPointsIdxs[freeBakedSpawnPointsIdxCount++] = i;
      }
    }
  }

  // if no free points then fail
  if (!freeBakedSpawnPointsIdxCount)
    return;

  // pick random
  i = freeBakedSpawnPointsIdxs[randRangeInt(0, 100) % freeBakedSpawnPointsIdxCount];

  // spawn
  upgradeCreate(MapConfig.BakedConfig->BakedSpawnPoints[i].Position, MapConfig.BakedConfig->BakedSpawnPoints[i].Rotation, type);
}

//--------------------------------------------------------------------------
void upgradePostDraw(Moby* moby)
{
	struct QuadDef quad;
	MATRIX m2;
	VECTOR pTL = {0.5,0,0.5,1};
	VECTOR pTR = {-0.5,0,0.5,1};
	VECTOR pBL = {0.5,0,-0.5,1};
	VECTOR pBR = {-0.5,0,-0.5,1};
	struct UpgradePVar* pvars = (struct UpgradePVar*)moby->PVar;
	if (!pvars)
		return;

	// determine color
	u32 color = 0x00FFFFFF;
  float opacity = lerpf(0, 1, clamp(pvars->Uses / 5.0, 0, 1));
  color |= (u8)(0x70 * opacity) << 24;

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
	quad.Tex0 = gfxGetFrameTex(UpgradeTexIds[pvars->Type]);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	// copy from moby
	memcpy(m2, moby->M0_03, sizeof(VECTOR)*3);
	memcpy(&m2[12], moby->Position, sizeof(VECTOR));

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 1);
}

//--------------------------------------------------------------------------
void upgradeUpdate(Moby* moby)
{
	const float rotSpeeds[] = { 0.05, 0.02, -0.03, -0.1 };
	const int opacities[] = { 64, 32, 44, 51 };

	int i;
	struct UpgradePVar* pvars = (struct UpgradePVar*)moby->PVar;
	if (!pvars)
		return;

	// register draw event
	gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&upgradePostDraw, moby);

  // draw on radar
  int blipIdx = radarGetBlipIndex(moby);
  if (blipIdx >= 0) {
    RadarBlip * blip = radarGetBlips() + blipIdx;
    blip->X = moby->Position[0];
    blip->Y = moby->Position[1];
    blip->Life = 0x1F;
    blip->Type = 4;
    blip->Team = TEAM_AQUA;
  }

	return;

	// handle particles
	u32 color = 0x80C0C0C0;
	for (i = 0; i < 4; ++i) {
		struct PartInstance * particle = pvars->Particles[i];
		if (!particle) {
			pvars->Particles[i] = particle = spawnParticle(moby->Position, color, opacities[i], i);
		}

		// update
		if (particle) {
			particle->Rot = (int)((gameGetTime() + (i * 100)) / (TIME_SECOND * rotSpeeds[i])) & 0xFF;
		}
	}
}

//--------------------------------------------------------------------------
GuberEvent* upgradeCreateEvent(Moby* moby, u32 eventType)
{
	GuberEvent * event = NULL;

	// create guber object
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent_Spawn(Moby* moby, GuberEvent* event)
{
	VECTOR p,r;
	struct UpgradeSpawnEventArgs args;

	// read event
	guberEventRead(event, p, 12);
	guberEventRead(event, r, 12);
	guberEventRead(event, &args, sizeof(struct UpgradeSpawnEventArgs));

	// set position
	vector_copy(moby->Position, p);
	vector_copy(moby->Rotation, r);

	// set update
	moby->PUpdate = &upgradeUpdate;

	// 
	//moby->ModeBits |= 0x30;
	//moby->GlowRGBA = MobSecondaryColors[(int)args.MobType];
	//moby->PrimaryColor = MobPrimaryColors[(int)args.MobType];
	moby->CollData = NULL;
	moby->DrawDist = 0;
  moby->ModeBits = 0;
  moby->AnimSeq = NULL;
  moby->AnimSeqId = moby->LSeq = 0;
	//moby->PClass = NULL;

	// update pvars
	struct UpgradePVar* pvars = (struct UpgradePVar*)moby->PVar;
	pvars->Type = args.Type;
  pvars->Uses = UPGRADE_MAX_USES;
  pvars->TexId = UpgradeTexIds[args.Type];
	memset(pvars->Particles, 0, sizeof(pvars->Particles));

	// set team
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		((GuberMoby*)guber)->TeamNum = 10;
	
	// set reference in state
  if (MapConfig.State)
	  MapConfig.State->UpgradeMobies[args.Type] = moby;

	// 
	mobySetState(moby, 0, -1);
	DPRINTF("upgrade spawned at %08X type:%d\n", (u32)moby, pvars->Type);
	return 0;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent_Destroy(Moby* moby, GuberEvent* event)
{
	int i;
	struct UpgradePVar* pvars = (struct UpgradePVar*)moby->PVar;
	if (!pvars)
		return 0;

	// destroy particles
	for (i = 0; i < 4; ++i) {
		if (pvars->Particles[i]) {
			destroyParticle(pvars->Particles[i]);
			pvars->Particles[i] = 0;
		}
	}

  // remove reference
  if (MapConfig.State && MapConfig.State->UpgradeMobies[pvars->Type] == moby)
    MapConfig.State->UpgradeMobies[pvars->Type] = NULL;

	guberMobyDestroy(moby);
	return 0;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent_Pickup(Moby* moby, GuberEvent* event)
{
	struct UpgradePickupEventArgs args;
	Player** players = playerGetAll();
	struct UpgradePVar* pvars = (struct UpgradePVar*)moby->PVar;

	if (!pvars)
		return 0;

	// read event
	guberEventRead(event, &args, sizeof(struct UpgradePickupEventArgs));

	Player* targetPlayer = players[args.PickedUpByPlayerId];
	if (!targetPlayer)
		return 0;

	// give
  if (MapConfig.State) {
	  MapConfig.State->PlayerStates[args.PickedUpByPlayerId].State.Upgrades[pvars->Type] += 1;
  }

	// handle effect
	if (targetPlayer->IsLocal) {
		switch (pvars->Type)
		{
			case UPGRADE_HEALTH:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Health Upgraded!");
				break;
			}
			case UPGRADE_SPEED:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Speed Upgraded!");
				break;
			}
			case UPGRADE_DAMAGE:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Damage Upgraded!");
				break;
			}
			case UPGRADE_MEDIC:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Revive Discount!");
				break;
			}
			case UPGRADE_VENDOR:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Vendor Discount!");
				break;
			}
			case UPGRADE_PICKUPS:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Powerup Duration Increased!");
				break;
			}
			case UPGRADE_CRIT:
			{
				uiShowPopup(targetPlayer->LocalPlayerIndex, "Critical Hit Upgraded!");
				break;
			}
		}
	}
	
	// play pickup sound
	upgradePlayPickupSound(moby);

  // reduce uses
  // respawn at next spot if used
#if !DEBUG
  pvars->Uses--;
#endif
  if (!pvars->Uses && gameAmIHost()) {
    upgradeSpawnNew(pvars->Type);
    upgradeDestroy(moby);
  }
  
	return 0;
}

//--------------------------------------------------------------------------
int upgradeHandleEvent(Moby* moby, GuberEvent* event)
{
	struct UpgradePVar* pvars = (struct UpgradePVar*)moby->PVar;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == UPGRADE_MOBY_OCLASS && pvars) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
			case UPGRADE_EVENT_SPAWN: return upgradeHandleEvent_Spawn(moby, event);
			case UPGRADE_EVENT_DESTROY: return upgradeHandleEvent_Destroy(moby, event);
			case UPGRADE_EVENT_PICKUP: return upgradeHandleEvent_Pickup(moby, event);
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
int upgradeCreate(VECTOR position, VECTOR rotation, enum UpgradeType upgradeType)
{
	struct UpgradeSpawnEventArgs args;

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(UPGRADE_MOBY_OCLASS, sizeof(struct UpgradePVar), &guberEvent, NULL);
	if (guberEvent)
	{
		args.Type = upgradeType;
		
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, rotation, 12);
		guberEventWrite(guberEvent, &args, sizeof(struct UpgradeSpawnEventArgs));
	}
	else
	{
		DPRINTF("failed to guberevent upgrade\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void upgradeInit(void)
{
  MapConfig.CreateUpgradePickupFunc = &upgradeCreate;
  MapConfig.OnUpgradePickupEventFunc = &upgradeHandleEvent;
  MapConfig.PickupUpgradeFunc = &upgradePickup;
}

//--------------------------------------------------------------------------
void upgradeTick(void)
{

}
