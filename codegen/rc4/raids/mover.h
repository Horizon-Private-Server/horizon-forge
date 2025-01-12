#ifndef RAIDS_MOVER_H
#define RAIDS_MOVER_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define MOVER_OCLASS                        (0x4002)
#define MOVER_MAX_TARGETS                   (4)

enum MoverEventType {
	MOVER_EVENT_SPAWN,
  MOVER_EVENT_SET_STATE,
};

enum MoverState {
	MOVER_STATE_DEACTIVATED,
	MOVER_STATE_ACTIVATED,
	MOVER_STATE_PAUSED,
	MOVER_STATE_COMPLETED = 127,
};

enum MoverSplineLoopType {
	MOVER_MOTION_NONE,
	MOVER_MOTION_LOOP,
	MOVER_MOTION_PING_PONG,
};

enum MoverAttachedType {
	MOVER_ATTACHED_NONE,
	MOVER_ATTACHED_SPLINE,
	MOVER_ATTACHED_MOBY,
};

struct MoverRuntimeState
{
  VECTOR LastAppliedPositionDelta;
  VECTOR LastAppliedRotationDelta;
  VECTOR MobyLastPosition;
  VECTOR MobyLastRotation;
  int TimeStarted;
  float TimePausedT;
  int CurrentSplineDir;
  float CurrentSplineLen;
};

struct MoverPVar
{
  // linear motion
  VECTOR Velocity;
  VECTOR Acceleration;
  VECTOR AngularVelocity;
  VECTOR AngularAcceleration;

  // cyclic motion
  VECTOR FrequencyStart;
  VECTOR AngularFrequencyStart;
  
  // general
  int Init;
  char DefaultState;
  char Log;

  // linear motion
  float Speed;
  float AngularSpeed;

  // cyclic motion
  int Frequency;
  int AngularFrequency;

  // spline
  int AttachedToSplineIdx;
  Moby* AttachedToMoby;
  char AttachedType;
  char AttachedInitSnapTo;
  char AttachedAlign;
  float SplineSpeed;
  enum MoverSplineLoopType SplineLoop;

  // 
  float RuntimeSeconds;

  // targets
  Moby* MobyTargets[MOVER_MAX_TARGETS];
  int CuboidTargets[MOVER_MAX_TARGETS];

  struct MoverRuntimeState State;
};

void moverBroadcastNewState(Moby* moby, enum MoverState state);
struct Guber* moverGetGuber(Moby* moby);
int moverHandleEvent(Moby* moby, GuberEvent* event);
void moverStart(void);
void moverInit(void);

#endif // RAIDS_MOVER_H
