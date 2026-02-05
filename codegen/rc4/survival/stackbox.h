#ifndef SURVIVAL_STACK_BOX_H
#define SURVIVAL_STACK_BOX_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define STACK_BOX_OCLASS                         (0x2083)
#define STACK_BOX_MAX_DIST                       (4)
#define STACK_BOX_REROLL_COST                    (50000)
#define PLAYER_STACK_BOX_COOLDOWN_TICKS          (5)

enum StackBoxState {
	STACK_BOX_STATE_DISABLED,
	STACK_BOX_STATE_ACTIVE,
};

enum StackBoxEventType {
  STACK_BOX_EVENT_SPAWN,
  STACK_BOX_EVENT_ACTIVATE,
  STACK_BOX_EVENT_GIVE_PLAYER,
  STACK_BOX_EVENT_PLAYER_BUY,
  STACK_BOX_EVENT_PLAYER_REROLL,
};

struct StackBoxPVar
{
  int Item;
  int ItemTexId;
  int ActivatedTime;
  Moby* BaseMoby;
};

void sboxFrameTick(void);
void sboxInit(void);

#endif // SURVIVAL_STACK_BOX_H
