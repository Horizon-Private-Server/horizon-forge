#ifndef SURVIVAL_SOULCOLLECTOR_H
#define SURVIVAL_SOULCOLLECTOR_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define SOULCOLLECTOR_OCLASS                        (0x4010)

enum SoulCollectorEventType {
	SOULCOLLECTOR_EVENT_SPAWN,
  SOULCOLLECTOR_EVENT_SET_STATE,
};

enum SoulCollectorState {
	SOULCOLLECTOR_STATE_DEACTIVATED,
	SOULCOLLECTOR_STATE_ACTIVATED,
	SOULCOLLECTOR_STATE_TARGET_REACHED,
  SOULCOLLECTOR_STATE_RESET
};

struct SoulCollectorRuntimeState
{
  int TicksSinceLastDamage;
};

struct SoulCollectorPVar
{
  char DefaultState;
  char Log;
  char TargetMobyOnState;
  char TargetMobyOffState;
  int Value;
  int Target;
  Moby* TargetMoby;
  float DecayRate;
  float TimeBeforeDecay;
  float DecayCounter;
  int Cuboid[4];
  int PrintCuboid;
  u32 SyncTicker;
  u32 TicksSinceLastSoul;
};

void soulcollectorActivateAll(void);
void soulcollectorOnSoul(VECTOR position, int collectedByPlayerId);
void soulcollectorBroadcastState(Moby* moby, enum SoulCollectorState state, int value);
struct Guber* soulcollectorGetGuber(Moby* moby);
int soulcollectorHandleEvent(Moby* moby, GuberEvent* event);
void soulcollectorFrameUpdate(void);
void soulcollectorStart(void);
void soulcollectorInit(void);

#endif // SURVIVAL_SOULCOLLECTOR_H
