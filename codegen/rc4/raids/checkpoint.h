#ifndef RAIDS_CHECKPOINT_H
#define RAIDS_CHECKPOINT_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define CHECKPOINT_MANAGER_OCLASS                 (0x4007)
#define CHECKPOINT_OCLASS                         (0x4008)
#define CHECKPOINT_MAX_CHECKPOINTS                (32)

enum CheckpointState
{
  CHECKPOINT_NOT_ACTIVE = 0,
  CHECKPOINT_ACTIVE = 1
};

enum CheckpointEventType {
	CHECKPOINT_EVENT_SPAWN,
  CHECKPOINT_EVENT_SET_STATE,
};

struct CheckpointManagerPVar
{
  char Log;
  char LastCheckpoint;
  Moby* DefaultCheckpointMoby;
  Moby* CheckpointMobys[CHECKPOINT_MAX_CHECKPOINTS];
};

struct CheckpointPVar
{
  char Log;
  Moby* OnActivateControllerMoby;
  Moby* OnDeactivateControllerMoby;
};

int checkpointSetActive(Moby* checkpointMoby);
struct Guber* checkpointGetGuber(Moby* moby);
int checkpointHandleEvent(Moby* moby, GuberEvent* event);
void checkpointStart(void);
void checkpointInit(void);

#endif // RAIDS_CHECKPOINT_H
