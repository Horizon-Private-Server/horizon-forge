#ifndef COMMON_HOLDER_H
#define COMMON_HOLDER_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define HOLDER_OCLASS                        (0x4012)

enum HolderEventType {
	HOLDER_EVENT_SPAWN,
  HOLDER_EVENT_SET_STATE,
  HOLDER_EVENT_PICKUP,
  HOLDER_EVENT_DROP,
};

enum HolderState {
	HOLDER_STATE_DEACTIVATED,
	HOLDER_STATE_ACTIVATED,
	HOLDER_STATE_RESET,
};

struct HolderRuntimeState
{
  VECTOR ResetPosition;
  VECTOR ResetRotation;
  int TargetMobyUid;
  Player* HeldByPlayer;
};

struct HolderConfig
{
  char DefaultState;
  char Log;
  char WrenchOnly;
  char ResetOnDeath;
  Moby* TargetMoby;
  Moby* OnPickupControllerMoby;
  Moby* OnDropControllerMoby;
};

struct HolderPVar
{
	struct TargetVars * TargetVarsPtr;
	char _pad0[0x0C];
	struct ReactVars * ReactVarsPtr;
	char _pad1[0x08];
  struct MoveVars_V2 * MoveVarsPtr;
	char _pad2[0x14];
 	struct FlashVars * FlashVarsPtr;
	char _pad3[0x18];

  struct HolderConfig Config;
  struct HolderRuntimeState State;
	struct TargetVars TargetVars;
	struct FlashVars FlashVars;
};

void holderBroadcastState(Moby* moby, enum HolderState state);
struct Guber* holderGetGuber(Moby* moby);
int holderHandleEvent(Moby* moby, GuberEvent* event);
void holderStart(void);
void holderInit(void);

#endif // COMMON_HOLDER_H
