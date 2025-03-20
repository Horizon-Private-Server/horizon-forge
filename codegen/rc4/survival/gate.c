/***************************************************
 * FILENAME :		gate.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the gates.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include "messageid.h"
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "gate.h"
#include "game.h"

extern struct SurvivalMapConfig MapConfig;
extern VECTOR GateLocations[];

Moby* GateMobies[GATE_MAX_COUNT] = {};
void * GateCollisionData = NULL;

//--------------------------------------------------------------------------
void gateSetCollision(int collActive)
{
  int i;

  for (i = 0; i < GATE_MAX_COUNT; ++i) {
    if (!GateMobies[i])
      break;

    // if any gate is already in the correct state
    // then we can stop because the rest will be
    if (!collActive && !GateMobies[i]->CollData)
      break;
    if (collActive && GateMobies[i]->CollData)
      break;

    if (!collActive) {
      GateMobies[i]->CollData = 0;
    } else {
      GateMobies[i]->CollData = GateCollisionData;
    }
  }
}

//--------------------------------------------------------------------------
void gateDrawQuad(Moby* moby, float direction)
{
  if (!moby || !moby->PVar)
    return;

  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;
  struct QuadDef quad;
	MATRIX m2;
	VECTOR pTL = {0,-0.5,0.5,1};
	VECTOR pTR = {0,0.5,0.5,1};
	VECTOR pBL = {0,-0.5,-0.5,1};
	VECTOR pBR = {0,0.5,-0.5,1};
	VECTOR scale = {1,pvars->Height,pvars->Height,0};

  float uScale = direction * 1.5;
  float vScale = direction * 1.0;
  float uOff = direction * (gameGetTime() / 4000.0);
  float vOff = 0;

	float u0 = fastmodf(uOff*uScale, 1);
	float u1 = uScale + u0;
	float v0 = vOff*vScale;
	float v1 = (1+vOff)*vScale;

  u32 color = colorLerp(0, 0x40404040, pvars->Opacity);

	// init
  gfxResetQuad(&quad);

	// color of each corner?
	vector_copy(quad.VertexPositions[0], pTL);
	vector_copy(quad.VertexPositions[1], pTR);
	vector_copy(quad.VertexPositions[2], pBL);
	vector_copy(quad.VertexPositions[3], pBR);
	quad.VertexColors[0] = quad.VertexColors[1] = quad.VertexColors[2] = quad.VertexColors[3] = color;
	quad.VertexUVs[0] = (struct UV){u0,v0};
	quad.VertexUVs[1] = (struct UV){u1,v0};
	quad.VertexUVs[2] = (struct UV){u0,v1};
	quad.VertexUVs[3] = (struct UV){u1,v1};
	quad.Clamp = 0;
	quad.Tex0 = gfxGetEffectTex(13, 1);
	quad.Tex1 = 0;
	quad.Alpha = 0x8000000058;

	// copy matrix from moby
  matrix_unit(m2);
  matrix_scale(m2, m2, scale);
  matrix_multiply(m2, m2, moby->M0_03);
	memcpy(&m2[12], moby->Position, sizeof(VECTOR));

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 0);
}

//--------------------------------------------------------------------------
void gateDraw(Moby* moby)
{
  gateDrawQuad(moby, 1);
  gateDrawQuad(moby, -1);
}

//--------------------------------------------------------------------------
void gatePlayOpenSound(Moby* moby)
{
  mobyPlaySoundByClass(0, 0, moby, MOBY_ID_NODE_BASE);
}

//--------------------------------------------------------------------------
void gatePayToken(Moby* moby, int playerIdx)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, GATE_EVENT_PAY_TOKEN);
  if (guberEvent) {
    guberEventWrite(guberEvent, &playerIdx, 4);
  }
}

//--------------------------------------------------------------------------
int gateCanInteract(Moby* gate, VECTOR point)
{
  VECTOR delta, gateTangent, gateClosestToPoint;
  if (!gate || !gate->PVar)
    return 0;

  struct GatePVar* pvars = (struct GatePVar*)gate->PVar;

  // get delta from center of gate to point
  vector_subtract(delta, point, gate->Position);

  // outside vertical range
  if (fabsf(delta[2]) > ((pvars->Height/2) + 0.5))
    return 0;

  // get closest point on gate to point
  vector_subtract(gateTangent, pvars->To, pvars->From);
  float gateLength = pvars->Length;
  vector_scale(gateTangent, gateTangent, 1 / gateLength);

  vector_subtract(delta, point, gate->Position);
  float dot = vector_innerproduct_unscaled(delta, gateTangent);
  if (dot < (-gateLength/2 - GATE_INTERACT_CAP_RADIUS))
    return 0;

  if (dot > (gateLength/2 + GATE_INTERACT_CAP_RADIUS))
    return 0;

  vector_scale(gateClosestToPoint, gateTangent, dot);
  vector_add(gateClosestToPoint, gateClosestToPoint, gate->Position);
  vector_subtract(delta, point, gateClosestToPoint);
  delta[2] = 0;
  return vector_sqrmag(delta) < (GATE_INTERACT_RADIUS*GATE_INTERACT_RADIUS);
}

//--------------------------------------------------------------------------
void gateUpdate(Moby* moby)
{
  VECTOR delta;
  char buf[32];
  int i;
  if (!moby || !moby->PVar)
    return;
    
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;

	// draw gate
  if (pvars->Opacity > 0)
    gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&gateDraw, moby);

  // handle state
  if (moby->State == GATE_STATE_ACTIVATED) {
    pvars->Opacity = clamp(pvars->Opacity + MATH_DT, 0, 1);
  } else if (moby->State == GATE_STATE_DEACTIVATED) {
    pvars->Opacity = clamp(pvars->Opacity - MATH_DT, 0, 1);
  }
  
  // collision only if visible
  if (pvars->Opacity > 0)
    moby->CollActive = 0;
  else
    moby->CollActive = -1;

  // update moby to fit from position to target
  if (pvars->Dirty)
  {
    moby->Scale = pvars->Height * 0.02083333395;

    vector_subtract(delta, pvars->To, pvars->From);
    vector_scale(delta, delta, 0.5);
    vector_add(moby->Position, pvars->From, delta);

    vector_scale(delta, delta, 2 / pvars->Height);

    matrix_unit(moby->M0_03);
    vector_copy(moby->M1_03, delta);
    vector_outerproduct(moby->M0_03, moby->M1_03, moby->M2_03);

    // allow game to update BSphere
    moby->ModeBits &= ~MOBY_MODE_BIT_NO_POST_UPDATE;
    pvars->Dirty = 0;
  }
  else
  {
    // prevent game from updating BSphere
    moby->ModeBits |= MOBY_MODE_BIT_NO_POST_UPDATE;
  }

#if DEBUG1
  if (padGetButton(0, PAD_UP) > 0) {
    mobySetState(moby, GATE_STATE_ACTIVATED, -1);
  } else if (padGetButton(0, PAD_DOWN) > 0) {
    mobySetState(moby, GATE_STATE_DEACTIVATED, -1);
    gatePlayOpenSound(moby);
  }
#endif

  // handle interact
  if (pvars->Cost > 0 && moby->State == GATE_STATE_ACTIVATED) {
    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
      Player* lp = playerGetFromSlot(i);
      if (lp) {
        
        // draw help popup
        snprintf(buf, 32, "\x11 %d Tokens to Open", pvars->Cost);
        if (gateCanInteract(moby, lp->PlayerPosition) && tryPlayerInteract(moby, lp, buf, NULL, 0, 1, PLAYER_GATE_COOLDOWN_TICKS, 10000, PAD_CIRCLE)) {
          gatePayToken(moby, lp->PlayerId);
          break;
        }
      }
    }
  }

  // force BSphere radius to something larger so that entire collision registers
  moby->BSphere[3] = 10000 * pvars->Length;
}

//--------------------------------------------------------------------------
int gateHandleEvent_Spawned(Moby* moby, GuberEvent* event)
{
	int i;
  VECTOR fromToDelta;
  
  DPRINTF("gate spawned: %08X\n", (u32)moby);
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;
  if (!pvars)
    return 0;
  
	// read event
	guberEventRead(event, pvars->From, 12);
	guberEventRead(event, pvars->To, 12);
	guberEventRead(event, &pvars->Height, 4);
	guberEventRead(event, &pvars->Cost, 4);
	guberEventRead(event, &pvars->Id, 1);

  vector_subtract(fromToDelta, pvars->To, pvars->From);
  pvars->Length = vector_length(fromToDelta);

	// set update
	moby->PUpdate = &gateUpdate;

  // set dirty
  pvars->Dirty = 1;

  // don't render
  moby->ModeBits |= 0x101;

  // update global collision data ptr
  if (!GateCollisionData)
    GateCollisionData = moby->CollData;

  // add to list
  for (i = 0; i < GATE_MAX_COUNT; ++i) {
    if (GateMobies[i])
      continue;
    
    GateMobies[i] = moby;
    break;
  }

  // set default state
	mobySetState(moby, GATE_STATE_ACTIVATED, -1);
  return 0;
}

//--------------------------------------------------------------------------
int gateHandleEvent_PayToken(Moby* moby, GuberEvent* event)
{
  int pIdx;
  Player** players = playerGetAll();
  
  DPRINTF("gate token paid: %08X\n", (u32)moby);
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  guberEventRead(event, &pIdx, 4);

  // increment stat
  if (pIdx >= 0 && MapConfig.State) {
    MapConfig.State->PlayerStates[pIdx].State.TokensUsedOnGates += 1;
  }
  
  // reduce cost by 1
  // open gate if cost reduced to 0
  if (pvars->Cost > 0 && moby->State == GATE_STATE_ACTIVATED) {
    pvars->Cost -= 1;
    if (pvars->Cost == 0) {
      mobySetState(moby, GATE_STATE_DEACTIVATED, -1);
      gatePlayOpenSound(moby);
    }

    // charge player
    if (MapConfig.State) {
      MapConfig.State->PlayerStates[pIdx].State.CurrentTokens -= 1;
    }
    
    playPaidSound(players[pIdx]);
  }

  return 0;
}

//--------------------------------------------------------------------------
int gateHandleEvent_SetCost(Moby* moby, GuberEvent* event)
{
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  guberEventRead(event, &pvars->Cost, 4);

  DPRINTF("gate cost set: %08X => %d\n", (u32)moby, pvars->Cost);

  // reactivate
  if (moby->State != GATE_STATE_ACTIVATED) {
    mobySetState(moby, GATE_STATE_ACTIVATED, -1);
  }

  return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby* gateGetGuber(Moby* moby)
{
	if (moby->OClass == GATE_OCLASS && moby->PVar)
		return moby->GuberMoby;
	
	return 0;
}

//--------------------------------------------------------------------------
void gateResetRandomGate(void)
{
  int i = 0;
  int r = 1 + rand(GATE_MAX_COUNT);
  int loops = 0;
  Moby* gateToReset = NULL;

  while (r > 0) {
    gateToReset = GateMobies[i];

    // skip mobies that are NULL, don't have pvars, or gates that aren't deactivated yet
    // we only want to reset an open gate
    if (!gateToReset || !gateToReset->PVar || gateToReset->State != GATE_STATE_DEACTIVATED) {
      i = (i+1) % GATE_MAX_COUNT;

      // we've looped through the entire list without finding
      // a gate that we can reset
      // so stop
      if (loops == 0 && i == 0) {
        return;
      }

      continue;
    }

    // increment gate index
    // and decrement random number
    i = (i+1) % GATE_MAX_COUNT;
    --r;

    ++loops;
  }
  
  // create event set cost event
  if (gateToReset) {
    struct GatePVar* pvars = (struct GatePVar*)gateToReset->PVar;
    GuberEvent * guberEvent = guberCreateEvent(gateToReset, GATE_EVENT_SET_COST);
    if (guberEvent) {
      int cost = (int)GateLocations[(pvars->Id*2)+1][3];
      guberEventWrite(guberEvent, &cost, 4);
    }
  }
}

//--------------------------------------------------------------------------
int gateHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == GATE_OCLASS && moby->PVar) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
			case GATE_EVENT_SPAWN: return gateHandleEvent_Spawned(moby, event);
      case GATE_EVENT_ACTIVATE: { mobySetState(moby, GATE_STATE_ACTIVATED, -1); return 0; }
      case GATE_EVENT_DEACTIVATE: { mobySetState(moby, GATE_STATE_DEACTIVATED, -1); gatePlayOpenSound(moby); return 0; }
			case GATE_EVENT_PAY_TOKEN: return gateHandleEvent_PayToken(moby, event);
			case GATE_EVENT_SET_COST: return gateHandleEvent_SetCost(moby, event);
			default:
			{
				DPRINTF("unhandle gate event %d\n", upgradeEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int gateCreate(VECTOR start, VECTOR end, float height, int cost, u8 id)
{
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(GATE_OCLASS, sizeof(struct GatePVar), &guberEvent, NULL);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, start, 12);
		guberEventWrite(guberEvent, end, 12);
    guberEventWrite(guberEvent, &height, 4);
    guberEventWrite(guberEvent, &cost, 4);
    guberEventWrite(guberEvent, &id, 1);
	}
	else
	{
		DPRINTF("failed to guberevent gate\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void gateSpawn(VECTOR gateData[], int count)
{
  static int spawned = 0;
  int i;
  
  if (spawned)
    return;

  // create gates
  if (gameAmIHost()) {
    for (i = 0; i < count; i += 2) {
      gateCreate(gateData[i], gateData[i+1], gateData[i][3], (int)gateData[i+1][3], i/2);
    }
  }

  spawned = 1;
}

//--------------------------------------------------------------------------
void gateInit(void)
{
  Moby* temp = mobySpawn(GATE_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  u32 mobyFunctionsPtr = (u32)mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    *(u32*)(mobyFunctionsPtr + 0x04) = (u32)&gateGetGuber;
    *(u32*)(mobyFunctionsPtr + 0x14) = (u32)&gateHandleEvent;
    DPRINTF("GATE oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
}
