#ifndef RAIDS_SPAWNER_H
#define RAIDS_SPAWNER_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "mob.h"
#include "game.h"

#define SPAWNER_OCLASS                        (0x4001)
#define SPAWNER_MAX_SPAWN_CUBOIDS             (4)
#define SPAWNER_MAX_HABITABLE_CUBOIDS         (4)
#define SPAWNER_MAX_AGGRO_CUBOIDS             (4)
#define SPAWNER_MAX_ROAMABLE_CUBOIDS          (4)
#define SPAWNER_MAX_MOB_TYPES                 (8)

#define SPAWNER_MAX_SPAWN_REQUESTS            (20)
#define SPAWNER_SPAWN_NEAR_DISTANCE           (100)

enum SpawnerEventType {
	SPAWNER_EVENT_SPAWN,
  SPAWNER_EVENT_SET_STATE,
};

enum SpawnerState {
	SPAWNER_STATE_DEACTIVATED,
	SPAWNER_STATE_PAUSED,
	SPAWNER_STATE_ACTIVATED,
	SPAWNER_STATE_COMPLETED = 127,
};

struct SpawnerSpawnRequest
{
  Moby* Spawner;
  struct MobCreateArgs SpawnArgs;
};

struct SpawnerSpawnConfig
{
  int NumMobsToSpawn;
  float SpawnRateMultiplier;
  u8 MaxSpawnedAtOnce; // 0=MAX
};

struct SpawnerMobParams
{
  int MobParamIdx;
  float Probability;
  float DifficultyMultiplier;
  int MaxCanSpawnOrUnlimited;
  int MaxCanAliveAtOnce;
  int CooldownTicks;
  char StarsMask;
  char MobBehavior;
};

struct SpawnerRuntimeState
{
  float ClosestPlayerDistSqr;
  int PlayerIsNear;
  u32 NumTotalSpawned;
  u32 NumTotalAlive;
  u32 NumTotalKilled;
  u32 Cooldown[SPAWNER_MAX_MOB_TYPES];
  u16 NumSpawned[SPAWNER_MAX_MOB_TYPES];
  u16 NumAlive[SPAWNER_MAX_MOB_TYPES];
  u16 NumKilled[SPAWNER_MAX_MOB_TYPES];
};

struct SpawnerPVar
{
  int Init;
  char DefaultState;
  char Log;
  int SpawnCuboidIds[SPAWNER_MAX_SPAWN_CUBOIDS];
  int HabitableCuboidIds[SPAWNER_MAX_HABITABLE_CUBOIDS];
  int AggroCuboidIds[SPAWNER_MAX_AGGRO_CUBOIDS];
  int RoamableCuboidIds[SPAWNER_MAX_ROAMABLE_CUBOIDS];
  int PathGraphIdx;
  float LimitDespawnPercent;
  struct SpawnerSpawnConfig Config[RAIDS_DIFFICULTY_COUNT];
  struct SpawnerMobParams SpawnableMobParam[SPAWNER_MAX_MOB_TYPES];
  struct SpawnerRuntimeState State;
};

void spawnerBroadcastNewState(Moby* moby, enum SpawnerState state);
void spawnerOnChildMobUpdate(Moby* moby, Moby* childMoby, u32 userdata);
void spawnerOnChildMobKilled(Moby* moby, Moby* childMoby, u32 userdata, int killedByPlayerId, int weaponId);
void spawnerOnChildMobDestroyed(Moby* moby, Moby* childMoby, u32 userdata);
void spawnerOnChildMobSpawned(Moby* moby, Moby* childMoby, u32 userdata);
int spawnerOnChildIsTargetInAggroZone(Moby* moby, Moby* childMoby, u32 userdata, Moby* target);
int spawnerOnChildConsiderRoamTarget(Moby* moby, Moby* childMoby, u32 userdata, VECTOR targetPosition);
void spawnerOnChildGetRandomRoamTarget(Moby* moby, Moby* childMoby, VECTOR outPosition);
struct Guber* spawnerGetGuber(Moby* moby);
int spawnerHandleEvent(Moby* moby, GuberEvent* event);
void spawnerStart(void);
void spawnerInit(void);

#endif // RAIDS_SPAWNER_H
