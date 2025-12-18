#ifndef COMMON_GATE_H
#define COMMON_GATE_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define GATE_OCLASS                           (0x4004)
#define GATE_MAX_COUNT                        (32)
#define GATE_INTERACT_RADIUS                  (4)
#define GATE_INTERACT_CAP_RADIUS              (-1)

enum GateEventType {
	GATE_EVENT_SPAWN,
  GATE_EVENT_SET_STATE,
  GATE_EVENT_PAY_TOKEN,
  GATE_EVENT_SET_COST,
};

enum GateState {
	GATE_STATE_DEACTIVATED,
	GATE_STATE_ACTIVATED,
};

struct GatePVar
{
  char DefaultState;
  int InitialCost;
  float Length;
  float Height;
  float Opacity;
  int CurrentCost;
};

void gateBroadcastNewState(Moby* moby, enum GateState state);
void gateSetCollision(int enabled);
void gateResetRandomGate(void);
struct Guber* gateGetGuber(Moby* moby);
int gateHandleEvent(Moby* moby, GuberEvent* event);
void gateStart(void);
void gateInit(void);

#endif // COMMON_GATE_H
