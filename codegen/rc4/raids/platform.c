/***************************************************
 * FILENAME :		platform.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the platform (flip & swivel).
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
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "common.h"
#include "maputils.h"
#include "shared.h"
#include "spawner.h"
#include "gate.h"
#include "mover.h"
#include "platform.h"
#include "mob.h"
#include "game.h"

#if DEBUG
#define DLOG(moby, format, ...) if (((struct PlatformSharedPVar*)moby->PVar)->Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#else
#define DLOG(moby, format, ...) 
#endif

void moverApplyMobyTransformationToAttachedPlayers(Moby* moby, MATRIX mWorldBeforeTransformation);

//--------------------------------------------------------------------------
int platformMobyIsPlatform(Moby* moby)
{
  return moby && (moby->OClass == PLATFORM_FLIPPER_MOBY_OCLASS || moby->OClass == PLATFORM_PIVOT_MOBY_OCLASS);
}

//--------------------------------------------------------------------------
void platformBroadcastNewState(Moby* moby, int state)
{
	// create event
  // or if not synced just update state
	GuberEvent * guberEvent = guberCreateEvent(moby, PLATFORM_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 1);
  } else if (moby->State != state) {
    mobySetState(moby, state, -1);
  }
}

//--------------------------------------------------------------------------
void platformUndoTransformation(Moby* moby)
{
  struct PlatformSharedPVar* pvars = (struct PlatformSharedPVar*)moby->PVar;

  // undo
  moby->Position[0] -= pvars->LastPosition[0];
  moby->Position[1] -= pvars->LastPosition[1];
  moby->Position[2] -= pvars->LastPosition[2];
  moby->Rotation[0] -= pvars->LastRotation[0];
  moby->Rotation[1] -= pvars->LastRotation[1];
  moby->Rotation[2] -= pvars->LastRotation[2];

  // reset
  memset(pvars->LastPosition, 0, sizeof(pvars->LastPosition));
  memset(pvars->LastRotation, 0, sizeof(pvars->LastRotation));
}

//--------------------------------------------------------------------------
void platformApplyTransformation(Moby* moby, MATRIX mBefore)
{
  struct PlatformSharedPVar* pvars = (struct PlatformSharedPVar*)moby->PVar;

  // apply
  moby->Position[0] += pvars->LastPosition[0];
  moby->Position[1] += pvars->LastPosition[1];
  moby->Position[2] += pvars->LastPosition[2];
  moby->Rotation[0] += pvars->LastRotation[0];
  moby->Rotation[1] += pvars->LastRotation[1];
  moby->Rotation[2] += pvars->LastRotation[2];
  
  // apply to any players standing on moby
  if (mBefore && (moby->ModeBits & MOBY_MODE_BIT_NO_POST_UPDATE) == 0) {
    mobyUpdateTransform(moby);
    moverApplyMobyTransformationToAttachedPlayers(moby, mBefore);
  }
}

//--------------------------------------------------------------------------
void platformDoShake(Moby* moby)
{
  struct PlatformSharedPVar* pvars = (struct PlatformSharedPVar*)moby->PVar;

  float t = (gameGetTime() / 1000.0);
  float p0 = sinf(t * PLATFORM_SHAKE_FREQUENCY) * PLATFORM_SHAKE_AMT;
  float p1 = cosf(t * PLATFORM_SHAKE_FREQUENCY) * PLATFORM_SHAKE_AMT;

  // save deltas
  pvars->LastPosition[0] += p0;
  pvars->LastPosition[1] += p1;
}

//--------------------------------------------------------------------------
void platformDoHover(Moby* moby)
{
  struct PlatformSharedPVar* pvars = (struct PlatformSharedPVar*)moby->PVar;

  float t = (gameGetTime() / 1000.0);
  float r0 = sinf(t * PLATFORM_HOVER_PIVOT_FREQUENCY) * PLATFORM_HOVER_PIVOT_AMT;
  float r1 = cosf(t * PLATFORM_HOVER_PIVOT_FREQUENCY) * PLATFORM_HOVER_PIVOT_AMT;
  float p2 = powf(sinf(t * PLATFORM_HOVER_FREQUENCY), 2) * PLATFORM_HOVER_AMT;

  // save deltas
  pvars->LastRotation[0] += r0;
  pvars->LastRotation[1] += r1;
  pvars->LastPosition[2] += p2;
}

//--------------------------------------------------------------------------
void platformFlipperDoFlip(Moby* moby)
{
  struct PlatformFlipperPVar* pvars = (struct PlatformFlipperPVar*)moby->PVar;

  // flipping disabled
  moby->GlowRGBA = 0x00006666; // orange
  float flipAfterSec = moby->State == PLATFORM_FLIPPER_STATE_UP ? pvars->FlipAfterSeconds : pvars->UnflipAfterSeconds;
  float expectedRotation = moby->State == PLATFORM_FLIPPER_STATE_UP ? 0 : 1;
  if (flipAfterSec <= 0) return;
  if (pvars->FlipperRotation != expectedRotation) return;

  // 
  float t = ((gameGetTime() - pvars->TimeStarted) / 1000.0) - pvars->TimeDelay;
  float secondsSinceLastFlip = fastmodf(t, pvars->FlipAfterSeconds + pvars->UnflipAfterSeconds);

  // flash red when about to flip colors
  if (secondsSinceLastFlip > (flipAfterSec - 1) && (int)(secondsSinceLastFlip * 10) % 2) {
    moby->GlowRGBA = 0x000000FF; // red
  }

  // flip / unflip
  if (secondsSinceLastFlip > flipAfterSec && gameAmIHost()) {
    platformBroadcastNewState(moby, !moby->State);
    DLOG(moby, "flip %d\n", !moby->State);
  }
}

//--------------------------------------------------------------------------
void platformFlipperDoFall(Moby* moby)
{
  struct PlatformFlipperPVar* pvars = (struct PlatformFlipperPVar*)moby->PVar;

  // no fall reset
  moby->GlowRGBA = 0x00990000; // blue
  moby->PrimaryColor = 0x4D3333; // blue tint

  if (moby->State == PLATFORM_FLIPPER_STATE_FALL) {

    // check for reset
    if (pvars->FallResetSeconds <= 0) return;

    float secondsSinceLastFall = (gameGetTime() - pvars->TimeStateLastChanged) / 1000.0;
    if (secondsSinceLastFall > pvars->FallResetSeconds) {
      platformBroadcastNewState(moby, PLATFORM_FLIPPER_STATE_UP);
      DLOG(moby, "reset fall\n");
    } else if (secondsSinceLastFall < PLATFORM_FALL_DELAY) {
      platformDoShake(moby);
    }

  } else {

    // check for fall
    int i;
    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
      Player* player = playerGetFromSlot(i);
      if (playerIsValid(player) && player->Ground.pMoby == moby && player->Ground.onGood) {
        platformBroadcastNewState(moby, PLATFORM_FLIPPER_STATE_FALL);
        DLOG(moby, "fall from player %d\n", player->PlayerId);
      }
    }
  }
}

//--------------------------------------------------------------------------
void platformDoPivot(Moby* moby)
{
  struct PlatformPivotPVar* pvars = (struct PlatformPivotPVar*)moby->PVar;

  // do pivot
  VECTOR targetRotation;
  vector_write(targetRotation, 0);

  // calculate force from players on platform
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player)) continue;

    if (player->Ground.pMoby != moby) continue;
    if (!player->Ground.onGood) continue;

    VECTOR dt;
    vector_subtract(dt, player->PlayerPosition, moby->Position);
    vector_projectonhorizontal(dt, dt);
    float dist = vector_length(dt);
    float angle = clampAngle(atan2f(dt[1] / dist, dt[0] / dist) + (MATH_PI/2));
    float magnitude = powf(dist * PLATFORM_PIVOT_FORCE_BY_DIST, 2);

    VECTOR force;
    vector_fromyaw(force, angle);
    vector_scale(force, force, magnitude);

    DLOG(moby, "PID %d, dist:%f angle:%f => %f (%f %f)\n", i, dist, angle * MATH_RAD2DEG, magnitude, force[0], force[1]);
    vector_add(targetRotation, targetRotation, force);
  }

  // clamp magnitude
  float maxMag = PLATFORM_PIVOT_FORCE_MAX * pvars->PivotMultiplier;
  float targetRotMag = vector_length(targetRotation);
  if (targetRotMag > maxMag)
    vector_scale(targetRotation, targetRotation, maxMag / targetRotMag);

  // recompute moby rotation with last pivot amount
  VECTOR mobyRotation;
  vector_copy(mobyRotation, moby->Rotation);
  mobyRotation[0] += pvars->LastPivotAmount[0];
  mobyRotation[1] += pvars->LastPivotAmount[1];

  // 
  VECTOR finalRotationAmount;
  vector_lerp(finalRotationAmount, mobyRotation, targetRotation, 1 - powf(MATH_E, -5 * MATH_DT));
  vector_subtract(finalRotationAmount, finalRotationAmount, moby->Rotation);
  //DLOG(moby, "rot:(%f %f) => (%f %f)\n", mobyRotation[0], mobyRotation[1], targetRotation[0], targetRotation[1]);
  //DLOG(moby, "final:(%f %f) => (%f %f)\n", moby->Rotation[0], moby->Rotation[1], moby->Rotation[0] + finalRotationAmount[0], moby->Rotation[1] + finalRotationAmount[1]);
  
  // save deltas
  pvars->LastRotation[0] += finalRotationAmount[0];
  pvars->LastRotation[1] += finalRotationAmount[1];
  pvars->LastPivotAmount[0] = finalRotationAmount[0];
  pvars->LastPivotAmount[1] = finalRotationAmount[1];
}

//--------------------------------------------------------------------------
void platformDoBuoyancy(Moby* moby)
{
  struct PlatformPivotPVar* pvars = (struct PlatformPivotPVar*)moby->PVar;
  float targetAmount = 0;

  // calculate force from players on platform
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player)) continue;

    if (player->Ground.pMoby != moby) continue;
    if (!player->Ground.onGood) continue;

    targetAmount = 1;
    break;
  }

  float sharpness = targetAmount > 0 ? (PLATFORM_BUOYANCY_SINK_SPEED * pvars->SinkSpeed) : PLATFORM_BUOYANCY_RISE_SPEED;
  float amount = lerpf(pvars->LastBuoyancyAmount, clamp(targetAmount, 0, 1) * pvars->SinkDistance, 1 - powf(MATH_E, -sharpness * MATH_DT));

  pvars->LastPosition[2] -= amount;
  pvars->LastBuoyancyAmount = amount;
}

//--------------------------------------------------------------------------
void platformFlipperOnStateChanged(Moby* moby)
{
  struct PlatformFlipperPVar* pvars = (struct PlatformFlipperPVar*)moby->PVar;
  pvars->TimeStateLastChanged = gameGetTime();

  // reset fall
  if (moby->State != PLATFORM_FLIPPER_STATE_FALL) {
    moby->Position[2] += pvars->FallAmount * signf(pvars->FallDistance);
    pvars->FallAmount = 0;
  }
}

//--------------------------------------------------------------------------
void platformFlipperUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct PlatformFlipperPVar* pvars = (struct PlatformFlipperPVar*)moby->PVar;
  float absFallDist = fabsf(pvars->FallDistance);
  float fallDir = signf(pvars->FallDistance);
  int snapPlayer = 1;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    platformFlipperOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  // backup current world matrix
  MATRIX mRot0;
  memcpy(mRot0, moby->M0_03, sizeof(VECTOR) * 3);
  vector_copy(&mRot0[12], moby->Position);

  // remove last transform
  platformUndoTransformation(moby);

  // handle flipper rotation / falling
  float secondsSinceLastStateChange = (gameGetTime() - pvars->TimeStateLastChanged) / 1000.0;
  if (moby->State == PLATFORM_FLIPPER_STATE_UP && pvars->FlipperRotation > 0) {
    pvars->FlipperRotation -= MATH_PI * MATH_DT * PLATFORM_FLIP_SPEED;
    if (pvars->FlipperRotation < 0) pvars->FlipperRotation = 0;
    moby->Rotation[0] = pvars->FlipperRotation * MATH_PI;
  } else if (moby->State == PLATFORM_FLIPPER_STATE_DOWN && pvars->FlipperRotation < 1) {
    pvars->FlipperRotation += MATH_PI * MATH_DT * PLATFORM_FLIP_SPEED;
    if (pvars->FlipperRotation > 1) pvars->FlipperRotation = 1;
    moby->Rotation[0] = pvars->FlipperRotation * MATH_PI;
  } else if (moby->State == PLATFORM_FLIPPER_STATE_FALL && moby->Position[2] > 0 && pvars->FallAmount < absFallDist && secondsSinceLastStateChange > PLATFORM_FALL_DELAY) {
    float fallAmount = MATH_DT * PLATFORM_FALL_SPEED * powf(secondsSinceLastStateChange, 2);
    if ((pvars->FallAmount + fallAmount) > absFallDist)
      fallAmount = absFallDist - pvars->FallAmount;
    pvars->FallAmount += fallAmount;
    moby->Position[2] -= fallAmount * fallDir;
  }

  switch (pvars->Type)
  {
    case PLATFORM_FLIPPER_STATIC:
      break;
    case PLATFORM_FLIPPER_HOVER:
      moby->GlowRGBA = 0x0033FF33; // always green
      platformDoHover(moby);
      break;
    case PLATFORM_FLIPPER_FLIP:
      platformDoHover(moby);
      platformFlipperDoFlip(moby);
      break;
    case PLATFORM_FLIPPER_FALL:
      platformDoHover(moby);
      platformFlipperDoFall(moby);
      snapPlayer = fallDir < 0; // snap if going up
      break;
  }
  
  platformApplyTransformation(moby, snapPlayer ? mRot0 : NULL);
}

//--------------------------------------------------------------------------
void platformPivotUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  if (moby->State != PLATFORM_PIVOT_STATE_ON) return;

  // backup current world matrix
  MATRIX mRot0;
  memcpy(mRot0, moby->M0_03, sizeof(VECTOR) * 3);
  vector_copy(&mRot0[12], moby->Position);

  // remove last transform
  platformUndoTransformation(moby);

  struct PlatformPivotPVar* pvars = (struct PlatformPivotPVar*)moby->PVar;
  platformDoHover(moby);
  platformDoPivot(moby);
  platformDoBuoyancy(moby);
  platformApplyTransformation(moby, mRot0);
}

//--------------------------------------------------------------------------
int platformHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  char state;
  if (!moby || !moby->PVar)
    return 0;

  struct PlatformFlipperPVar* pvars = (struct PlatformFlipperPVar*)moby->PVar;

	// read event
	guberEventRead(event, &state, 1);
  if (moby->State != state) {
    DLOG(moby, "platform %08X recv state %d => %d\n", (u32)moby, moby->State, state);
    mobySetState(moby, state, -1);
  }
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* platformGetGuber(Moby* moby)
{
	if (platformMobyIsPlatform(moby) && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int platformHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && platformMobyIsPlatform(moby) && moby->PVar) {
		u32 eventId = event->NetEvent.EventID;

		switch (eventId)
		{
      case PLATFORM_EVENT_SET_STATE: { return platformHandleEvent_SetState(moby, event); }
			default:
			{
				DLOG(moby, "unhandle platform event %d\n", eventId);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void platformInitType(int oclass, void* updateFunc)
{
  int isFlipper = oclass == PLATFORM_FLIPPER_MOBY_OCLASS;
  Moby* temp = mobySpawn(oclass, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    
    mobyFunctionsPtr->GetGuberObject = &mapGetGuber;
    mobyFunctionsPtr->MobyEventHandler = &mapHandleEvent;
    DPRINTF("PLATFORM oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for flippers
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, oclass)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct PlatformFlipperPVar* flipperPVars = (struct PlatformFlipperPVar*)moby->PVar;
      struct PlatformPivotPVar* pivotPVars = (struct PlatformPivotPVar*)moby->PVar;
      struct PlatformSharedPVar* pvars = (struct PlatformSharedPVar*)moby->PVar;

      // only fall platforms should be synced
      if (isFlipper && flipperPVars->Type == PLATFORM_FLIPPER_FALL) {
        struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
        DLOG(moby, "found platform %08X %08X\n", (u32)moby, (u32)guber);
      } else {
        DLOG(moby, "found platform %08X\n", (u32)moby);
      }

      moby->PUpdate = updateFunc;
      moby->ModeBits &= ~MOBY_MODE_BIT_NO_UPDATE;

      // update pvars
      memset(pvars->LastPosition, 0, sizeof(pvars->LastPosition));
      memset(pvars->LastRotation, 0, sizeof(pvars->LastRotation));
      if (isFlipper) {
        flipperPVars->TimeStateLastChanged = flipperPVars->TimeStarted = gameGetTime();
      } else {
        memset(pivotPVars->LastPivotAmount, 0, sizeof(pivotPVars->LastPivotAmount));
        pivotPVars->LastBuoyancyAmount = 0;
      }
    }

		++moby;
	}
}

//--------------------------------------------------------------------------
void platformInit(void)
{
  platformInitType(PLATFORM_FLIPPER_MOBY_OCLASS, &platformFlipperUpdate);
  platformInitType(PLATFORM_PIVOT_MOBY_OCLASS, &platformPivotUpdate);

  DPRINTF("platform flipper pvar size %d\n", sizeof(struct PlatformFlipperPVar));
  DPRINTF("platform pivot pvar size %d\n", sizeof(struct PlatformPivotPVar));
}
