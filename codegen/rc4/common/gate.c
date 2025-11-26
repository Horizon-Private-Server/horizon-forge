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

int gateInitialized = 0;
Moby* gateMobies[GATE_MAX_COUNT] = {};
void * gateCollisionData = NULL;

void gateOnGuberCreated(Moby* moby);

//--------------------------------------------------------------------------
void gateSetCollision(int enabled)
{
  int i;

  for (i = 0; i < GATE_MAX_COUNT; ++i) {
    if (!gateMobies[i])
      break;

    // if any gate is already in the correct state
    // then we can stop because the rest will be
    if (!enabled && !gateMobies[i]->CollData)
      break;
    if (enabled && gateMobies[i]->CollData)
      break;

    if (!enabled) {
      gateMobies[i]->CollData = 0;
    } else {
      gateMobies[i]->CollData = gateCollisionData;
    }
  }
}

//--------------------------------------------------------------------------
void gateRegisterGateMoby(Moby* moby)
{
  int i;

  for (i = 0; i < GATE_MAX_COUNT; ++i) {
    if (!gateMobies[i] || mobyIsDestroyed(gateMobies[i]) || gateMobies[i]->OClass != GATE_OCLASS) {
      gateMobies[i] = moby;
      return;
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

  u32 color = colorLerp(0, moby->PrimaryColor, pvars->Opacity);

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
void gateOnStateChanged(Moby* moby)
{
  if (moby->State == GATE_STATE_DEACTIVATED) {
    gatePlayOpenSound(moby);
  }
}

//--------------------------------------------------------------------------
void gateBroadcastNewState(Moby* moby, enum GateState state)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, GATE_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
  }
}

//--------------------------------------------------------------------------
void gateUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;

  // initialize by sending first state
  if (!pvars->Init) {
    if (gameAmIHost() && gateInitialized) {
      gateBroadcastNewState(moby, pvars->DefaultState);
    }
    
    return;
  }

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    gateOnStateChanged(moby);
    moby->Triggers |= 1;
  }

	// draw gate
  if (pvars->Opacity > 0) {
    gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&gateDraw, moby);
  }

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

  // update moby rotation/scale
  moby->Scale = pvars->Height * 0.02083333395;
  moby->ModeBits &= ~MOBY_MODE_BIT_LOCK_ROTATION;
  mobyUpdateTransform(moby);
  moby->ModeBits |= MOBY_MODE_BIT_LOCK_ROTATION;

  // scale length
  vector_scale(moby->M1_03, moby->M1_03, pvars->Length / pvars->Height);

  // force BSphere radius to something larger so that entire collision registers
  moby->BSphere[3] = 1024 * pvars->Length * pvars->Length;
  moby->LSphere[3] = 1024 * pvars->Length * pvars->Length;
}

//--------------------------------------------------------------------------
int gateHandleEvent_Spawned(Moby* moby, GuberEvent* event)
{
  DPRINTF("gate spawned: %08X\n", (u32)moby);
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;
  if (!pvars)
    return 0;
  
	// read event
	guberEventRead(event, moby->Position, 12);
	guberEventRead(event, moby->Rotation, 12);
	guberEventRead(event, &pvars->Height, 4);
	guberEventRead(event, &pvars->Length, 4);
  mobyUpdateTransform(moby);

  gateOnGuberCreated(moby);
  return 0;
}

//--------------------------------------------------------------------------
int gateHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  if (!moby || !moby->PVar)
    return 0;

	// read event
	guberEventRead(event, &state, 4);
  mobySetState(moby, state, -1);
  
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;
  pvars->Init = 1;
  return 0;
}

//--------------------------------------------------------------------------
void gateOnGuberCreated(Moby* moby)
{
  struct GatePVar* pvars = (struct GatePVar*)moby->PVar;

	moby->PUpdate = &gateUpdate;
  moby->ModeBits = MOBY_MODE_BIT_LOCK_ROTATION | MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
  pvars->Init = 0;
  pvars->Opacity = 0;

  mobySetState(moby, pvars->DefaultState, -1);

  // update global collision data ptr
  if (!gateCollisionData)
    gateCollisionData = moby->CollData;

  // add to list
  gateRegisterGateMoby(moby);
}

//--------------------------------------------------------------------------
struct Guber* gateGetGuber(Moby* moby)
{
	if (moby->OClass == GATE_OCLASS && moby->PVar)
		return (Guber*)moby->GuberMoby;
	
	return 0;
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
      case GATE_EVENT_SET_STATE: return gateHandleEvent_SetState(moby, event);
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
int gateCreate(VECTOR position, VECTOR rotation, float length, float height)
{
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(GATE_OCLASS, sizeof(struct GatePVar), &guberEvent, NULL);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, rotation, 12);
    guberEventWrite(guberEvent, &height, 4);
    guberEventWrite(guberEvent, &length, 4);
	}
	else
	{
		DPRINTF("failed to guberevent gate\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void gateStart(void)
{
  gateInitialized = 1;
}

//--------------------------------------------------------------------------
void gateInit(void)
{
  Moby* temp = mobySpawn(GATE_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("GATE oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);

  // create gubers for gates
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, GATE_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DPRINTF("found gate %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        gateOnGuberCreated(moby);
      }
    }

		++moby;
	}
}
