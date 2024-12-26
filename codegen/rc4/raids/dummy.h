#ifndef RAIDS_DUMMY_H
#define RAIDS_DUMMY_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "mob.h"
#include "game.h"

#define DUMMY_OCLASS                        (0x400D)

enum DummyEventType {
	DUMMY_EVENT_SPAWN,
  DUMMY_EVENT_SET_STATE,
  DUMMY_EVENT_SET_HEALTH,
};

enum DummyState {
	DUMMY_STATE_DEACTIVATED,
	DUMMY_STATE_ACTIVATED,
	DUMMY_STATE_PAUSED,
	DUMMY_STATE_RESET,
	DUMMY_STATE_DEAD = 127,
};

enum DummyOnDeathType {
  DUMMY_ON_DEATH_NONE = 0,
  DUMMY_ON_DEATH_DESTROY,
  DUMMY_ON_DEATH_EXPLODE,
  DUMMY_ON_DEATH_EXPLODE_BLOW_CORN,
};

enum DummyAggroType {
  DUMMY_MOB_AGGRO_IGNORE = 0,
  DUMMY_MOB_AGGRO_IN_RANGE,
  DUMMY_MOB_AGGRO_ALWAYS,
};

struct DummyDifficultyConfig
{
  float Health;
  float ExplosionRadius;
  float ExplosionDamage;
};

struct DummyConfig
{
  char DefaultState;
  char Log;
  char IsOnEnemyTeam;
  char OnDeathType;
  Moby* TargetMoby;
  float TargetHeight;
  float TargetRadius;
  char Healthbar;
  char MobTargetType;
  float MobTargetDistance;
  float HealthbarScale;
  float HealthbarOffset;
  Moby* OnHitControllerMoby;
  Moby* OnKilledControllerMoby;
  struct DummyDifficultyConfig DifficultyConfigs[RAIDS_DIFFICULTY_COUNT];
};

struct DummyPVar
{
	struct TargetVars * TargetVarsPtr;
	char _pad0[0x0C];
	struct ReactVars * ReactVarsPtr;
	char _pad1[0x08];
  struct MoveVars_V2 * MoveVarsPtr;
	char _pad2[0x14];
 	struct FlashVars * FlashVarsPtr;
	char _pad3[0x18];

  struct DummyConfig Config;
	struct TargetVars TargetVars;
	struct FlashVars FlashVars;
};

void dummyBroadcastState(Moby* moby, enum DummyState state, float health);
struct Guber* dummyGetGuber(Moby* moby);
int dummyHandleEvent(Moby* moby, GuberEvent* event);
void dummyStart(void);
void dummyInit(void);

#endif // RAIDS_DUMMY_H
