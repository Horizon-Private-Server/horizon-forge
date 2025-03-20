#ifndef SURVIVAL_GATE_H
#define SURVIVAL_GATE_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define GATE_OCLASS                           (0x1F6)
#define GATE_MAX_COUNT                        (16)
#define GATE_INTERACT_RADIUS                  (4)
#define GATE_INTERACT_CAP_RADIUS              (-1)
#define PLAYER_GATE_COOLDOWN_TICKS            (15)

struct GatePVar
{
  VECTOR From;
  VECTOR To;
  float Height;
  float Length;
  float Opacity;
  int Cost;
  int Dirty;
  u8 Id;
};

enum GateEventType {
	GATE_EVENT_SPAWN,
  GATE_EVENT_ACTIVATE,
  GATE_EVENT_DEACTIVATE,
  GATE_EVENT_PAY_TOKEN,
  GATE_EVENT_SET_COST,
};

enum GateState {
	GATE_STATE_DEACTIVATED,
	GATE_STATE_ACTIVATED,
};

void gateResetRandomGate(void);

#endif // SURVIVAL_GATE_H
