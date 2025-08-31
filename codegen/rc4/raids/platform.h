#ifndef RAIDS_PLATFORM_H
#define RAIDS_PLATFORM_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define PLATFORM_FLIPPER_MOBY_OCLASS    (8432)
#define PLATFORM_PIVOT_MOBY_OCLASS      (9355)

#define PLATFORM_FLIP_SPEED             (0.5)
#define PLATFORM_FALL_SPEED             (15.0)
#define PLATFORM_FALL_DELAY             (1.0)
#define PLATFORM_SHAKE_FREQUENCY        (100.0)
#define PLATFORM_SHAKE_AMT              (0.1)
#define PLATFORM_HOVER_FREQUENCY        (1.0)
#define PLATFORM_HOVER_AMT              (0.05)
#define PLATFORM_HOVER_PIVOT_FREQUENCY  (5.0)
#define PLATFORM_HOVER_PIVOT_AMT        (0.01)
#define PLATFORM_PIVOT_FORCE_BY_DIST    (0.25)
#define PLATFORM_PIVOT_FORCE_MAX        (0.1)
#define PLATFORM_BUOYANCY_SINK_SPEED    (1)
#define PLATFORM_BUOYANCY_RISE_SPEED    (3)

enum PlatformFlipperState {
  PLATFORM_FLIPPER_STATE_UP = 0,
  PLATFORM_FLIPPER_STATE_DOWN = 1,
  PLATFORM_FLIPPER_STATE_FALL = 2,
};

enum PlatformFlipperType {
	PLATFORM_FLIPPER_STATIC = 0,
	PLATFORM_FLIPPER_HOVER = 1,
	PLATFORM_FLIPPER_FLIP = 2,
	PLATFORM_FLIPPER_FALL = 3,
};

enum PlatformPivotState {
  PLATFORM_PIVOT_STATE_ON = 0,
  PLATFORM_PIVOT_STATE_OFF = 1,
};

enum PlatformEventType {
	PLATFORM_EVENT_SPAWN,
  PLATFORM_EVENT_SET_STATE,
};

struct PlatformSharedPVar
{
  char DefaultState;
  char Log;
  char Type;
  float LastRotation[3];
  float LastPosition[3];
};

struct PlatformFlipperPVar
{
  // general
  char DefaultState;
  char Log;
  char Type;
  float LastRotation[3];
  float LastPosition[3];

  // config
  float TimeDelay;
  float FlipAfterSeconds;
  float UnflipAfterSeconds;
  float FallDistance;
  float FallResetSeconds;

  // runtime state
  // 0-1, 0=up, 1=down
  float FlipperRotation;
  float FallAmount;
  int TimeStarted;
  int TimeStateLastChanged;
};

struct PlatformPivotPVar
{
  // general
  char DefaultState;
  char Log;
  float LastRotation[3];
  float LastPosition[3];

  // config
  float PivotMultiplier;
  float SinkDistance;
  float SinkSpeed;

  // runtime state
  float LastPivotAmount[2];
  float LastBuoyancyAmount;
};

int platformMobyIsPlatform(Moby* moby);
struct Guber* platformGetGuber(Moby* moby);
int platformHandleEvent(Moby* moby, GuberEvent* event);
void platformInit(void);

#endif // RAIDS_PLATFORM_H
