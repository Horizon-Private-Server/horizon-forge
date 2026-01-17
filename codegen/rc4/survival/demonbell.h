#ifndef SURVIVAL_DEMONBELL_H
#define SURVIVAL_DEMONBELL_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>

#define DEMONBELL_MOBY_OCLASS				            (0x2479)
#define DEMONBELL_DAMAGE_SCALE                  (1 / 100.0)
#define DEMONBELL_HIT_COOLDOWN_TICKS            (TPS * 3)

#define DEMONBELL_DEFAULT_COLOR                 (0x80787878)
#define DEMONBELL_PULSE_COLOR_1                 (0x80787878)
#define DEMONBELL_PULSE_COLOR_2                 (0x8000C8E8)
#define DEMONBELL_PULSE_SPEED                   (10.0)

enum DemonBellEventType {
	DEMONBELL_EVENT_ACTIVATE = 1,
  DEMONBELL_EVENT_DEACTIVATE
};

struct DemonBellPVar {
  int Id;
  float HitAmount;
  u32 RecoverCooldownTicks;
  int RoundActivated;
  int ForcedOn;
};

struct GuberMoby* demonbellGetGuber(Moby* moby);
int demonbellHandleEvent(Moby* moby, GuberEvent* event);
void demonbellTick(void);
void demonbellInit(void);

#endif // SURVIVAL_DEMONBELL_H
