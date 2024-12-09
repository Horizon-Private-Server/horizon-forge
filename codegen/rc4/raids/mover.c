/***************************************************
 * FILENAME :		mover.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the movers.
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
#include <libdl/spline.h>
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
#include "maputils.h"
#include "shared.h"
#include "mover.h"
#include "mob.h"
#include "game.h"

#define DLOG(moby, format, ...) if (((struct MoverPVar*)moby->PVar)->Log) { DPRINTF(format, ##__VA_ARGS__); }

int moverInitialized = 0;

//--------------------------------------------------------------------------
float moverGetT(Moby* moby)
{
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;
  return (gameGetTime() - pvars->State.TimeStarted) / 1000.0;
}

//--------------------------------------------------------------------------
void moverInitTVec(VECTOR out, VECTOR offset, float t, int freq)
{
  if (freq > 0) {
    out[0] = cosf((t + offset[0]));
    out[1] = cosf((t + offset[1]));
    out[2] = cosf((t + offset[2]));
  } else {
    out[0] = t + offset[0];
    out[1] = t + offset[1];
    out[2] = t + offset[2];
  }
}

//--------------------------------------------------------------------------
void moverInitSpline(Moby* moby)
{
  int i;
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;

  if (pvars->SplineIdx < 0) return;
  Spline3D_t* spline = splineGetSpline(pvars->SplineIdx);
  if (!spline || spline->Count <= 0) return;

  // reset direction
  pvars->State.CurrentSplineDir = 1;

  // snap to  
  if (pvars->SplineInitSnapTo) {
    for (i = 0; i < MOVER_MAX_TARGETS; ++i) {
      if (pvars->MobyTargets[i]) {
        vector_copy(pvars->MobyTargets[i]->Position, spline->Points[0]);
      }
      if (pvars->CuboidTargets[i] >= 0) {
        vector_copy(&spawnPointGet(pvars->CuboidTargets[i])->M0[12], spline->Points[0]);
      }
    }
  }
}

//--------------------------------------------------------------------------
void moverMoveSpline(Moby* moby, VECTOR outPosDelta, VECTOR outRotDelta)
{
  VECTOR dt;
  VECTOR up = {0,0,1,0};
  int i;
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;
  float t = moverGetT(moby);

  Spline3D_t* spline = splineGetSpline(pvars->SplineIdx);
  if (!spline || spline->Count <= 0) return;

  // determine direction
  if (!pvars->State.CurrentSplineDir) {
    pvars->State.CurrentSplineDir = 1;
  }

  // calculate length of spline
  if (pvars->State.CurrentSplineLen == 0) {
    for (i = 0; i < (spline->Count-1); ++i) {
      vector_subtract(dt, spline->Points[i], spline->Points[i+1]);
      pvars->State.CurrentSplineLen += vector_length(dt);
    }
  }

  // determine how many iterations through the spline we need to travel
  int dir = pvars->State.CurrentSplineDir * (pvars->SplineSpeed >= 0 ? 1 : -1);
  float dist = t * fabsf(pvars->SplineSpeed);
  int iter = floorf(dist / pvars->State.CurrentSplineLen);

  // handle special looping rules
  if (pvars->SplineLoop == MOVER_MOTION_NONE && iter > 0) {
    if (dir > 0) { // stop at cap node (or start node, 0,0,0 rel)
      vector_subtract(outPosDelta, spline->Points[spline->Count - 1], spline->Points[0]);
      vector_subtract(dt, spline->Points[spline->Count - 1], spline->Points[spline->Count - 2]);
      vector_fromforwardup(outRotDelta, dt, up);
    }

    if (gameAmIHost()) {
      moverBroadcastNewState(moby, MOVER_STATE_COMPLETED);
    }
    return;
  } else if (pvars->SplineLoop == MOVER_MOTION_PING_PONG) {
    dir *= (iter%2) ? -1 : 1; // flip flop direction for each loop
  } 

  // remove iterations from total distance
  // so we just have the dist to travel for this iteration
  dist = dist - (iter * pvars->State.CurrentSplineLen);
  for (i = 0; i < (spline->Count-1); ++i) {
    int idx = (dir < 0) ? (spline->Count - i - 1) : i;
    vector_subtract(dt, spline->Points[idx+dir], spline->Points[idx]);
    float seglen = vector_length(dt);
    if (seglen > dist) {
      vector_fromforwardup(outRotDelta, dt, up);
      vector_scale(dt, dt, dist / seglen);
      vector_add(dt, dt, spline->Points[idx]);
      vector_subtract(outPosDelta, dt, spline->Points[0]);
      break;
    }

    dist -= seglen;
  }
}

//--------------------------------------------------------------------------
void moverMove(Moby* moby, VECTOR outPosDelta, VECTOR outRotDelta)
{
  VECTOR velocity, acceleration, delta, tvec;
  VECTOR splinePosDelta={0,0,0,0}, splineRotDelta={0,0,0,0};
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;

  // spline
  if (pvars->SplineIdx >= 0) {
    moverMoveSpline(moby, splinePosDelta, splineRotDelta);
  }

  // don't rotate if not align
  if (!pvars->SplineAlign) {
    vector_write(splineRotDelta, 0);
  }

  float t = moverGetT(moby);

  // we want to calculate position as a function of time t
  // rather than calculate velocity as a function of time t, and then position as += velocity*dt
  // which could desync between clients
  // as long as t is the same (or close to) on all clients then this will be synced


  // p += v * speed * dt (BAD)
  // p = integrate(v) * speed * dt (GOOD)
  // p = pvars->Velocity * t + (0.5 * pvars->Acceleration * t^2)
  moverInitTVec(tvec, pvars->FrequencyStart, t * pvars->Speed, pvars->Frequency);
  vector_multiply(velocity, pvars->Velocity, tvec);
  vector_multiply(tvec, tvec, tvec);
  vector_scale(tvec, tvec, 0.5);
  vector_multiply(acceleration, pvars->Acceleration, tvec);
  vector_add(delta, velocity, acceleration);
  vector_add(delta, delta, splinePosDelta);
  vector_copy(outPosDelta, delta);

  // r
  moverInitTVec(tvec, pvars->AngularFrequencyStart, t * pvars->AngularSpeed, pvars->AngularFrequency);
  vector_multiply(velocity, pvars->AngularVelocity, tvec);
  vector_multiply(tvec, tvec, tvec);
  vector_scale(tvec, tvec, 0.5);
  vector_multiply(acceleration, pvars->AngularAcceleration, tvec);
  vector_add(delta, velocity, acceleration);
  vector_add(delta, delta, splineRotDelta);
  vector_clampeuler(delta, delta);
  vector_copy(outRotDelta, delta);
}

//--------------------------------------------------------------------------
void moverApplyMoby(Moby* moby, Moby* target, VECTOR posDelta, VECTOR rotDelta)
{
  int i;
  Player** players = playerGetAll();
  VECTOR position, rotation;
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;
  if (!target) return;

  // remove last applied position delta, and apply our newly calculated one
  // this lets the target moby move independently (ie we aren't forcing its position)
  vector_subtract(position, target->Position, pvars->State.LastAppliedPositionDelta);
  vector_add(position, position, posDelta);

  vector_subtract(rotation, target->Rotation, pvars->State.LastAppliedRotationDelta);
  vector_add(rotation, rotation, rotDelta);
  vector_clampeuler(rotation, rotation);

  // since MP doesn't have proper support for ground moby tracking
  // ie Ratchet won't stick to a moving platform like in Singleplayer
  // we can hack a similar effect by finding any player standing on the target
  // and applying the same delta to them
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* p = players[i];
    if (!p || !p->SkinMoby || !p->PlayerMoby || p->Ground.pMoby != target) continue;

    // transform player position to target local space
    // apply new transformation
    // transform back to world space
    MATRIX w2l, l2w;
    VECTOR pos;
    matrix_unit(w2l);
    memcpy(w2l, target->M0_03, 3 * sizeof(VECTOR));
    matrix_transpose(w2l, w2l);
    vector_subtract(pos, p->PlayerPosition, target->Position);
    vector_apply(pos, pos, w2l);
    
    matrix_unit(l2w);
    matrix_rotate_x(l2w, l2w, rotation[0]);
    matrix_rotate_y(l2w, l2w, rotation[1]);
    matrix_rotate_z(l2w, l2w, rotation[2]);
    vector_apply(pos, pos, l2w);
    vector_add(pos, pos, position);

    vector_copy(p->PlayerPosition, pos);
    vector_copy(p->PlayerMoby->Position, pos);
    vector_copy(p->Ground.point, pos);
    p->Ground.stickLanding = 5;
  }

  vector_copy(target->Position, position);
  vector_copy(target->Rotation, rotation);
  if ((target->ModeBits & MOBY_MODE_BIT_NO_POST_UPDATE) == 0) {
    mobyUpdateTransform(target);
  }
}

//--------------------------------------------------------------------------
void moverApplyCuboid(Moby* moby, SpawnPoint* target, VECTOR posDelta, VECTOR rotDelta)
{
  VECTOR position, rotation;
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;
  if (!target) return;

  // remove last applied position delta, and apply our newly calculated one
  // this lets the target moby move independently (ie we aren't forcing its position)
  vector_subtract(position, &target->M0[12], pvars->State.LastAppliedPositionDelta);
  vector_add(position, position, posDelta);

  vector_subtract(rotation, &target->M1[12], pvars->State.LastAppliedRotationDelta);
  vector_add(rotation, rotation, rotDelta);
  vector_clampeuler(rotation, rotation);

  vector_copy(&target->M0[12], position);
  vector_copy(&target->M1[12], rotation);
}

//--------------------------------------------------------------------------
void moverOnStateChanged(Moby* moby)
{
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;
  
  if (moby->State == MOVER_STATE_ACTIVATED) {
    //pvars->State.TimeStarted = time;
    moverInitSpline(moby);
  } else {
    memset(&pvars->State, 0, sizeof(pvars->State));
    pvars->State.TimeStarted = -1;
  }
}

//--------------------------------------------------------------------------
void moverBroadcastNewState(Moby* moby, enum MoverState state)
{
  int time = gameGetTime();

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, MOVER_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
    guberEventWrite(guberEvent, &time, 4);
  }
}

//--------------------------------------------------------------------------
void moverUpdate(Moby* moby)
{
  int i;
  VECTOR posDelta, rotDelta;
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;

  // initialize by sending first state
  if (!pvars->Init) {
    if (gameAmIHost() && moverInitialized) {
      moverBroadcastNewState(moby, pvars->DefaultState);
    }
    
    return;
  }

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    moverOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  if (moby->State != MOVER_STATE_ACTIVATED) {
    return;
  }

  // don't run past runtime
  float t = moverGetT(moby);
  if (pvars->RuntimeSeconds > 0 && t > pvars->RuntimeSeconds) {
    if (gameAmIHost()) {
      moverBroadcastNewState(moby, MOVER_STATE_COMPLETED);
    }
    return;
  }

  // run move logic
  moverMove(moby, posDelta, rotDelta);

  // update target mobys
  for (i = 0; i < MOVER_MAX_TARGETS; ++i) {
    Moby* targetMoby = pvars->MobyTargets[i];
    if (!targetMoby || mobyIsDestroyed(targetMoby)) {
      pvars->MobyTargets[i] = NULL;
      continue;
    }

    moverApplyMoby(moby, targetMoby, posDelta, rotDelta);
  }

  // update target cuboids
  for (i = 0; i < MOVER_MAX_TARGETS; ++i) {
    int cuboidIdx = pvars->CuboidTargets[i];
    if (cuboidIdx < 0) continue;

    moverApplyCuboid(moby, spawnPointGet(cuboidIdx), posDelta, rotDelta);
  }

  vector_copy(pvars->State.LastAppliedPositionDelta, posDelta);
  vector_copy(pvars->State.LastAppliedRotationDelta, rotDelta);
}

//--------------------------------------------------------------------------
void moverOnGuberCreated(Moby* moby)
{
  int i;
  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;

  moby->PUpdate = &moverUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;

  // set default state
  memset(&pvars->State, 0, sizeof(pvars->State));
  pvars->Init = 0;
  pvars->State.TimeStarted = -1;
  
  // update pvars target references
  Moby* mobyStart = mobyListGetStart();
  for (i = 0; i < MOVER_MAX_TARGETS; ++i) {
    int mobyIdx = (int)pvars->MobyTargets[i];
    if (mobyIdx < 0) {
      pvars->MobyTargets[i] = NULL;
      continue;
    }

    Moby* targetMoby = mobyStart + mobyIdx;
    if (mobyIsDestroyed(targetMoby)) {
      pvars->MobyTargets[i] = NULL;
      continue;
    }

    DLOG(moby, "mover %08X found target %d %08X\n", (u32)moby, i, (u32)targetMoby);
    pvars->MobyTargets[i] = targetMoby;
  }
}

//--------------------------------------------------------------------------
int moverHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state, time;
  if (!moby || !moby->PVar)
    return 0;

	// read event
	guberEventRead(event, &state, 4);
	guberEventRead(event, &time, 4);
  mobySetState(moby, state, -1);

  struct MoverPVar* pvars = (struct MoverPVar*)moby->PVar;
  pvars->Init = 1;
  if (moby->State == MOVER_STATE_ACTIVATED) {
    pvars->State.TimeStarted = time;
  }

  return 0;
}

//--------------------------------------------------------------------------
struct Guber* moverGetGuber(Moby* moby)
{
	if (moby->OClass == MOVER_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int moverHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == MOVER_OCLASS && moby->PVar) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
      case MOVER_EVENT_SET_STATE: { return moverHandleEvent_SetState(moby, event); }
			default:
			{
				DLOG(moby, "unhandle mover event %d\n", upgradeEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void moverStart(void)
{
  moverInitialized = 1;
}

//--------------------------------------------------------------------------
void moverInit(void)
{
  Moby* temp = mobySpawn(MOVER_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("MOVER oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);

  // create gubers for movers
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, MOVER_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found mover %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        moverOnGuberCreated(moby);
      }
    }

		++moby;
	}

  DPRINTF("mover pvar size %d\n", sizeof(struct MoverPVar));
}
