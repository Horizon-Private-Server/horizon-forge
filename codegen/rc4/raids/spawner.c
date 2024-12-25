/***************************************************
 * FILENAME :		spawner.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the spawners.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/collision.h>
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "spawner.h"
#include "mob.h"
#include "pathfind.h"
#include "game.h"

#define DLOG(moby, format, ...) if (((struct SpawnerPVar*)moby->PVar)->Log) { DPRINTF(format, ##__VA_ARGS__); }

int spawnerInitialized = 0;
int spawnerInitializedTime = 0;
int spawnerNumLastActive = 0;
int spawnerNumActive = 0;
int spawnerTicksSinceLastDelete = 0;
float spawnerMinClosestDistToPlayerSqr = -1;
float spawnerMaxClosestDistToPlayerSqr = 0;
int spawnerSpawnRequestsCount = 0;
struct SpawnerSpawnRequest spawnerSpawnRequests[SPAWNER_MAX_SPAWN_REQUESTS];

//--------------------------------------------------------------------------
struct SpawnerSpawnConfig* spawnerGetConfig(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  return &pvars->Config[MapConfig.State ? MapConfig.State->DifficultyStars : 0];
}

//--------------------------------------------------------------------------
void spawnerRequestSpawn(Moby* moby, struct MobCreateArgs* args)
{
  // should iterate requests and replace the spawner the furthest from any player
  if (spawnerSpawnRequestsCount >= SPAWNER_MAX_SPAWN_REQUESTS)
    return;

  spawnerSpawnRequests[spawnerSpawnRequestsCount].Spawner = moby;
  memcpy(&spawnerSpawnRequests[spawnerSpawnRequestsCount].SpawnArgs, args, sizeof(spawnerSpawnRequests[spawnerSpawnRequestsCount].SpawnArgs));
  spawnerSpawnRequestsCount++;
}

//--------------------------------------------------------------------------
int spawnerIsValidCuboidRoamIdx(void* userdata, int index)
{
  Moby* moby = (Moby*)userdata;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  return pvars->RoamableCuboidIds[index] >= 0;
}

//--------------------------------------------------------------------------
int spawnerIsValidCuboidSpawnIdx(void* userdata, int index)
{
  Moby* moby = (Moby*)userdata;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  return pvars->SpawnCuboidIds[index] >= 0;
}

//--------------------------------------------------------------------------
int spawnerIsValidMobSpawnIdx(void* userdata, int index)
{
  Moby* moby = (Moby*)userdata;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  return pvars->SpawnableMobParam[index].MobParamIdx >= 0 && pvars->SpawnableMobParam[index].Probability > 0;
}

//--------------------------------------------------------------------------
int spawnerDestroyMob(Moby* moby, Moby* mobMoby, u32 userdata, int markAsKilled)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  struct MobPVar* mobPVars = (struct MobPVar*)mobMoby->PVar;

  if (!mobPVars->MobVars.Destroy) {
    mobPVars->MobVars.Destroy = 2;
    if (!markAsKilled) {
      pvars->State.NumSpawned[mobPVars->MobVars.Userdata]--;
      pvars->State.NumTotalSpawned--;
      pvars->State.NumKilled[mobPVars->MobVars.Userdata]--;
      pvars->State.NumTotalKilled--;
      DLOG(moby, "SPAWNER %08X: DESPAWN %08X (spawned:%d alive:%d killed:%d)\n", (u32)moby, (u32)mobMoby, pvars->State.NumTotalSpawned, pvars->State.NumTotalAlive, pvars->State.NumTotalKilled);
    }

    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
void spawnerGetRandomPointInCuboid(SpawnPoint* cuboid, VECTOR outPos)
{
  // determine where to spawn mob
  outPos[0] = randRange(-1, 1);
  outPos[1] = randRange(-1, 1);
  outPos[2] = 1;
  vector_apply(outPos, outPos, cuboid->M0);
}

//--------------------------------------------------------------------------
int spawnerGetRandomSpawnPoint(Moby* moby, int mobParamsIdx, VECTOR outPos, float* outYaw)
{
  VECTOR pos = {0,0,3,0};
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // try and get random spawn cuboid
  // if none exist, return position/yaw of the spawner itself
  int selSpawnIdx = selectRandomIndex(SPAWNER_MAX_SPAWN_CUBOIDS, moby, spawnerIsValidCuboidSpawnIdx);
  if (selSpawnIdx < 0) {
    vector_copy(outPos, moby->Position);
    *outYaw = moby->Rotation[2];
    return 1;
  }

  int cuboidIdx = pvars->SpawnCuboidIds[selSpawnIdx];
  if (cuboidIdx < 0) return 0;

  // get cuboid
  SpawnPoint* cuboid = spawnPointGet(cuboidIdx);

  // determine where to spawn mob
  spawnerGetRandomPointInCuboid(cuboid, pos);

  if (outPos) vector_copy(outPos, pos);
  if (outYaw) *outYaw = cuboid->M1[14] + randRadian();
  return 1;
}

//--------------------------------------------------------------------------
int spawnerSpawn(Moby* moby, int mobParamsIdx, int fromUid)
{
  if (!MapConfig.TryCreateMobFunc) return 0;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  struct SpawnerMobParams* mobParams = &pvars->SpawnableMobParam[mobParamsIdx];
  struct MobSpawnParams* mobSpawnParams = &MapConfig.MobSpawnParams[mobParams->MobParamIdx];

  struct MobCreateArgs args = {
    .SpawnParamsIdx = mobParams->MobParamIdx,
    .Behavior = mobParams->MobBehavior,
    .Parent = moby,
    .Userdata = mobParamsIdx,
    .DifficultyMult = mobParams->DifficultyMultiplier,
    .Config = &mobSpawnParams->Config,
    .SpawnFromUID = fromUid,
  };

  if (spawnerGetRandomSpawnPoint(moby, mobParamsIdx, args.Position, &args.Yaw)) {
    spawnerRequestSpawn(moby, &args);
    return 1;
    // if (MapConfig.TryCreateMobFunc(&args)) {
    //   pvars->State.NumSpawned[mobParamsIdx]++;
    //   pvars->State.NumTotalSpawned++;
    //   return 1;
    // }
  }

  return 0;
}

//--------------------------------------------------------------------------
int spawnerSpawnRandom(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  struct SpawnerSpawnConfig* config = spawnerGetConfig(moby);

  if (config->SpawnRateMultiplier <= 0.000001)
    return 0;

  int spawnerMobIdx = selectRandomIndex(SPAWNER_MAX_MOB_TYPES, moby, &spawnerIsValidMobSpawnIdx);
  if (spawnerMobIdx < 0) return 0;
  
  struct SpawnerMobParams* mobParams = &pvars->SpawnableMobParam[spawnerMobIdx];
  if (mobParams->MobParamIdx < 0 || mobParams->Probability <= 0)
    return 0;

  if (mobParams->MaxCanSpawnOrUnlimited > 0 && pvars->State.NumSpawned[spawnerMobIdx] >= mobParams->MaxCanSpawnOrUnlimited)
    return 0;

  if (MapConfig.State && mobParams->MaxCanAliveAtOnce > 0 && mobParams->MaxCanAliveAtOnce <= pvars->State.NumAlive[spawnerMobIdx])
    return 0;

  if (MapConfig.State && (mobParams->StarsMask & (1 << MapConfig.State->DifficultyStars)) == 0)
    return 0;

  if (pvars->State.Cooldown[spawnerMobIdx] > 0)
    return 0;

  if (randRange(0, 1) > mobParams->Probability)
    return 0;

  // spawn
  if (spawnerSpawn(moby, spawnerMobIdx, -1)) {
    pvars->State.Cooldown[spawnerMobIdx] = mobParams->CooldownTicks / config->SpawnRateMultiplier;
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int spawnerIsCompleted(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  struct SpawnerSpawnConfig* config = spawnerGetConfig(moby);

  if (missionIsComplete()) return 1;
  return pvars->State.NumTotalKilled >= config->NumMobsToSpawn;
}

//--------------------------------------------------------------------------
int spawnerCanSpawn(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  struct SpawnerSpawnConfig* config = spawnerGetConfig(moby);

  // if (MapConfig.State) {
  //   int totalAlive = MapConfig.State->MobStats.TotalAlive; // + MapConfig.State->MobStats.TotalSpawning;
  //   if (totalAlive >= MAX_MOBS_ALIVE_REAL) return 0;
  // }
  
  //DLOG(moby, "%d/%d %d/%d\n", pvars->State.NumTotalSpawned, config->NumMobsToSpawn, pvars->State.NumTotalAlive, config->MaxSpawnedAtOnce);
  return moby->State == SPAWNER_STATE_ACTIVATED
      && !spawnerIsCompleted(moby)
      && pvars->State.NumTotalSpawned < config->NumMobsToSpawn
      && (config->MaxSpawnedAtOnce == 0 || pvars->State.NumTotalAlive < config->MaxSpawnedAtOnce);
}

//--------------------------------------------------------------------------
int spawnerIsPlayerNear(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // player is near
  if (pvars->State.ClosestPlayerDistSqr < (SPAWNER_SPAWN_NEAR_DISTANCE*SPAWNER_SPAWN_NEAR_DISTANCE))
    return 1;

  // player is inside spawn cuboid
  int i,j;
  Player** players = playerGetAll();
  for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
    Player* player = players[j];
    if (!playerIsValid(player)) continue;

    for (i = 0; i < SPAWNER_MAX_SPAWN_CUBOIDS; ++i) {
        
      int cuboidIdx = pvars->SpawnCuboidIds[i];
      if (cuboidIdx < 0) continue;

      // get cuboid
      SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
      if (spawnPointIsPointInside(cuboid, player->PlayerPosition, NULL))
        return 1;
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
void spawnerOnStateChanged(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  switch (moby->State)
  {
    case SPAWNER_STATE_COMPLETED:
    case SPAWNER_STATE_DEACTIVATED:
    {
      // reset runtime stats when deactivating
      memset(&pvars->State, 0, sizeof(pvars->State));
      break;
    }
    case SPAWNER_STATE_ACTIVATED:
    {
      break;
    }
  }

}

//--------------------------------------------------------------------------
void spawnerBroadcastNewState(Moby* moby, enum SpawnerState state)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, SPAWNER_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
  }
}

//--------------------------------------------------------------------------
void spawnerUpdate(Moby* moby)
{
  int i;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // initialize by sending first state
  if (!pvars->Init) {
    if (gameAmIHost() && spawnerInitialized) {
      spawnerBroadcastNewState(moby, pvars->DefaultState ? SPAWNER_STATE_ACTIVATED : SPAWNER_STATE_DEACTIVATED);
    }
    
    return;
  }

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    spawnerOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  for (i = 0; i < SPAWNER_MAX_MOB_TYPES; ++i) {
    decTimerU32(&pvars->State.Cooldown[i]);
  }

  if (!gameAmIHost()) return;

  // add delay after game loads before spawners start spawning
  // to try and mitigate lag/crashing at the start
  if ((gameGetTime() - spawnerInitializedTime) < (1*TIME_SECOND)) return;

  // check if completed
  if (moby->State != SPAWNER_STATE_COMPLETED && spawnerIsCompleted(moby)) {
    DLOG(moby, "spawner %08X completed\n", (u32)moby);
    spawnerBroadcastNewState(moby, SPAWNER_STATE_COMPLETED);
    return;
  }

  if (!missionIsActive()) return;
  if (moby->State != SPAWNER_STATE_ACTIVATED) return;

  // update closest player dist
  float closestDistSqr = 10000000.0;
  Player** players = playerGetAll();
  VECTOR dt;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player)) continue;

    vector_subtract(dt, player->PlayerPosition, moby->Position);
    float distSqr = vector_sqrmag(dt);
    if (distSqr < closestDistSqr) {
      closestDistSqr = distSqr;
    }
  }

  pvars->State.PlayerIsNear = spawnerIsPlayerNear(moby);

  // spawn
  spawnerNumActive++;
  if (spawnerCanSpawn(moby)) {
    
    // 
    pvars->State.ClosestPlayerDistSqr = closestDistSqr;
    if (spawnerMinClosestDistToPlayerSqr < 0 || closestDistSqr < spawnerMinClosestDistToPlayerSqr)
      spawnerMinClosestDistToPlayerSqr = closestDistSqr;
    if (closestDistSqr > spawnerMaxClosestDistToPlayerSqr)
      spawnerMaxClosestDistToPlayerSqr = closestDistSqr;

    if (spawnerSpawnRandom(moby)) {
      
    }
  }
}

//--------------------------------------------------------------------------
void spawnerOnChildMobUpdate(Moby* moby, Moby* childMoby, u32 userdata)
{
	struct MobPVar* childPVars = (struct MobPVar*)childMoby->PVar;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  int i;

  // force path graph
  childPVars->MobVars.MoveVars.PathGraphIdx = pvars->PathGraphIdx;

  // destroy if spawner has completed
  if (moby->State == SPAWNER_STATE_COMPLETED) {
    spawnerDestroyMob(moby, childMoby, userdata, 1);
    return;
  }

  // check if mob has left habitable cuboids
  // if no habitable cuboids are defined, then this will do nothing
  int notInside = 0;
  for (i = 0; i < SPAWNER_MAX_HABITABLE_CUBOIDS; ++i) {
    int cuboidIdx = pvars->HabitableCuboidIds[i];
    if (cuboidIdx < 0) continue;

    SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
    if (spawnPointIsPointInside(cuboid, childMoby->Position, NULL)) {
      notInside = 0;
      break;
    }

    notInside = 1;
  }

  // if spawner is idled and we're not inside a habitable cuboid, despawn
  if (moby->State != SPAWNER_STATE_ACTIVATED && notInside) {
    spawnerDestroyMob(moby, childMoby, userdata, 0);
    return;
  }

  // handle respawn
  // just kill on respawn
  if (childPVars->MobVars.Respawn) {
    // pass to mob
    // let mob override respawn logic
    if (!childPVars->VTable->OnRespawn || childPVars->VTable->OnRespawn(childMoby)) {
      spawnerDestroyMob(moby, childMoby, userdata, 1);
    }

    childPVars->MobVars.Respawn = 0;
  } else if (notInside) {
    // respawn if not in habitable cuboid
    if (!childPVars->VTable->OnRespawn || childPVars->VTable->OnRespawn(childMoby)) {
      if (spawnerSpawn(moby, userdata, guberGetUID(childMoby))) {
        spawnerDestroyMob(moby, childMoby, userdata, 0);
      }
    }
  }

}

//--------------------------------------------------------------------------
void spawnerOnChildMobSpawned(Moby* moby, Moby* childMoby, u32 userdata)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  DLOG(moby, "MOB%d: spawned %08X (%d/%d)\n", userdata, (u32)childMoby, pvars->State.NumSpawned[userdata], pvars->State.NumTotalSpawned + pvars->State.NumTotalKilled);
}

//--------------------------------------------------------------------------
void spawnerOnChildMobKilled(Moby* moby, Moby* childMoby, u32 userdata, int killedByPlayerId, int weaponId)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // log kill
  if (killedByPlayerId >= 0) {
    //pvars->State.NumTotalKilled++;
    //pvars->State.NumKilled[userdata]++;`
  }

  //DLOG(moby, "SPAWNER %08X: ONKILL %08X (spawned:%d alive:%d killed:%d)\n", (u32)moby, (u32)childMoby, pvars->State.NumTotalSpawned, pvars->State.NumTotalAlive, pvars->State.NumTotalKilled);
  //DLOG(moby, "KILL MOB%d: spawned:%d alive:%d killed:%d\n", userdata, pvars->State.NumSpawned[userdata], pvars->State.NumAlive[userdata], pvars->State.NumKilled[userdata]);
  //DLOG(moby, "SPAWNER %d/%d\n", pvars->State.NumTotalKilled, pvars->NumMobsToSpawn);
}

//--------------------------------------------------------------------------
void spawnerOnChildMobDestroyed(Moby* moby, Moby* childMoby, u32 userdata)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  pvars->State.NumTotalAlive--;
  pvars->State.NumAlive[userdata]--;
  pvars->State.NumTotalKilled++;
  pvars->State.NumKilled[userdata]++;

  DLOG(moby, "SPAWNER %08X: ONDESTROY %08X (spawned:%d alive:%d killed:%d)\n", (u32)moby, (u32)childMoby, pvars->State.NumTotalSpawned, pvars->State.NumTotalAlive, pvars->State.NumTotalKilled);
  //DLOG(moby, "DESTROY MOB%d: spawned:%d alive:%d killed:%d\n", userdata, pvars->State.NumSpawned[userdata], pvars->State.NumAlive[userdata], pvars->State.NumKilled[userdata]);
  //DLOG(moby, "SPAWNER %d/%d\n", pvars->State.NumTotalKilled, pvars->NumMobsToSpawn);
}

//--------------------------------------------------------------------------
int spawnerOnChildIsTargetInAggroZone(Moby* moby, Moby* childMoby, u32 userdata, Moby* target)
{
  int i;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  for (i = 0; i < SPAWNER_MAX_AGGRO_CUBOIDS; ++i) {
    int cuboidIdx = pvars->AggroCuboidIds[i];
    if (cuboidIdx < 0) continue;

    SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
    if (spawnPointIsPointInside(cuboid, target->Position, NULL)) {
      return 1;
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
int spawnerOnChildConsiderRoamTarget(Moby* moby, Moby* childMoby, u32 userdata, VECTOR targetPosition)
{
  int i;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // check if target is in a roam cuboids
  // if no roam cuboids are defined, then check habitable cuboids
  int hasRoamCuboid = 0;
  for (i = 0; i < SPAWNER_MAX_ROAMABLE_CUBOIDS; ++i) {
    int cuboidIdx = pvars->RoamableCuboidIds[i];
    if (cuboidIdx < 0) continue;

    SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
    if (spawnPointIsPointInside(cuboid, targetPosition, NULL)) {
      return 1;
    }

    hasRoamCuboid = 1;
  }

  // check if target is in a habitable cuboids
  // if no habitable cuboids are defined, then return 1
  int hasHabitableCuboid = 0;
  if (!hasRoamCuboid) {
    for (i = 0; i < SPAWNER_MAX_HABITABLE_CUBOIDS; ++i) {
      int cuboidIdx = pvars->HabitableCuboidIds[i];
      if (cuboidIdx < 0) continue;

      SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
      if (spawnPointIsPointInside(cuboid, targetPosition, NULL)) {
        return 1;
      }

      hasHabitableCuboid = 1;
    }
  }

  // allow any if no roam/habitable cuboids defined
  return !hasRoamCuboid && !hasHabitableCuboid;
}

//--------------------------------------------------------------------------
void spawnerOnChildGetRandomRoamTarget(Moby* moby, Moby* childMoby, VECTOR outPosition)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  struct MobPVar* childPVars = (struct MobPVar*)childMoby->PVar;
  struct PathGraph* path = pathGetMobyPathGraph(childMoby, &childPVars->MobVars.MoveVars);
  if (mobAmIOwner(childMoby)) {

    // select random roamable cuboid
    int selSpawnIdx = selectRandomIndex(SPAWNER_MAX_ROAMABLE_CUBOIDS, moby, spawnerIsValidCuboidRoamIdx);
    if (path && path->NumNodes > 0 && selSpawnIdx < 0) {

      int r = rand(path->NumNodes);
      int count = 0;

      // try and find a node thats in a habitable cuboid
      while (count < path->NumNodes && !spawnerOnChildConsiderRoamTarget(moby, childMoby, childPVars->MobVars.Userdata, path->Nodes[r])) {
        r = (r + 1) % path->NumNodes;
        ++count;
      }

      pathGetNodePosition(path, r, 0, outPosition);
      return;
    }

    int cuboidIdx = pvars->RoamableCuboidIds[selSpawnIdx];
    if (cuboidIdx < 0) return;

    // get cuboid
    SpawnPoint* cuboid = spawnPointGet(cuboidIdx);

    // determine where to spawn mob
    spawnerGetRandomPointInCuboid(cuboid, outPosition);
    //outPosition[2] += 3;
  }
}

//--------------------------------------------------------------------------
void spawnerOnGuberCreated(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  moby->PUpdate = &spawnerUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;

  memset(&pvars->State, 0, sizeof(pvars->State));
  pvars->Init = 0;

  // 
  struct Guber* guber = guberGetObjectByMoby(moby);
  ((GuberMoby*)guber)->TeamNum = 10;

  // print pvars
  #if PRINT_SPAWNER_PVARS
    int i;
    DLOG(moby, "Configs\n");
    for (i = 0; i < RAIDS_DIFFICULTY_COUNT; ++i) {
      DLOG(moby, " [%d].NumMobsToSpawn=%d\n", i, pvars->Config[i].NumMobsToSpawn);
      DLOG(moby, " [%d].SpawnRateMultiplier=%f\n", i, pvars->Config[i].SpawnRateMultiplier);
    }
    DLOG(moby, "PathGraphIdx=%d\n", pvars->PathGraphIdx);
    DLOG(moby, "SpawnCuboidIds\n");
    for (i = 0; i < SPAWNER_MAX_SPAWN_CUBOIDS; ++i) { DLOG(moby, " [%d]=%d\n", i, pvars->SpawnCuboidIds[i]); }
    DLOG(moby, "SpawnableMobParam\n");
    for (i = 0; i < SPAWNER_MAX_MOB_TYPES; ++i) {
      DLOG(moby, " [%d].MobParamIdx=%d\n", i, pvars->SpawnableMobParam[i].MobParamIdx);
      DLOG(moby, " [%d].Probability=%f\n", i, pvars->SpawnableMobParam[i].Probability);
      DLOG(moby, " [%d].DifficultyMultiplier=%f\n", i, pvars->SpawnableMobParam[i].DifficultyMultiplier);
      DLOG(moby, " [%d].MaxCanSpawnOrUnlimited=%d\n", i, pvars->SpawnableMobParam[i].MaxCanSpawnOrUnlimited);
    }
  #endif
}

//--------------------------------------------------------------------------
int spawnerHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  if (!moby || !moby->PVar)
    return 0;

  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &state, 4);
  pvars->Init = 1;
  mobySetState(moby, state, -1);
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* spawnerGetGuber(Moby* moby)
{
	if (moby->OClass == SPAWNER_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int spawnerHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == SPAWNER_OCLASS && moby->PVar) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
      case SPAWNER_EVENT_SET_STATE: { return spawnerHandleEvent_SetState(moby, event); }
			default:
			{
				DLOG(moby, "unhandle spawner event %d\n", upgradeEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int spawnerIsPointNearPlayer(VECTOR position, float radius)
{
  VECTOR dt;
  float radiusSqr = radius * radius;
  Player** players = playerGetAll();

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* p = players[i];
    if (!playerIsValid(p) || playerIsDead(p)) continue;

    vector_subtract(dt, p->PlayerPosition, position);
    float sqrDist = vector_sqrmag(dt);
    if (sqrDist <= radiusSqr) return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
void spawnerTryDespawnMob(Moby* moby, float maxMobsAllocatedPerSpawner)
{
  Moby* m = mobyListGetStart();
  Moby* mEnd = mobyListGetEnd();

  while (m < mEnd)
  {
    if (mobyIsMob(m) && !mobyIsDestroyed(m)) {
      Moby* parent = m->PParent;
      if (parent && parent != moby && parent->OClass == SPAWNER_OCLASS) {

        struct SpawnerPVar* parentPVars = (struct SpawnerPVar*)parent->PVar;
        struct MobPVar* mobPVars = (struct MobPVar*)m->PVar;
        int destroy = mobPVars->MobVars.ClosestDistToPlayer > (SPAWNER_SPAWN_NEAR_DISTANCE*SPAWNER_SPAWN_NEAR_DISTANCE);
        if (destroy) {
          if (spawnerDestroyMob(parent, m, mobPVars->MobVars.Userdata, 0)) {
            spawnerTicksSinceLastDelete = 0;
            return;
          }
        }

        // struct SpawnerPVar* parentPVars = (struct SpawnerPVar*)parent->PVar;
        // float priority = (parentPVars->State.ClosestPlayerDistSqr - spawnerMinClosestDistToPlayerSqr) / ((spawnerMaxClosestDistToPlayerSqr - spawnerMinClosestDistToPlayerSqr) + 1);
        // float mobsAllocated = maxMobsAllocatedPerSpawner * clamp(1 - priority, 0, 1);
        // int limit = ceilf(clamp(parentPVars->NumMobsToSpawn, 0, MAX_MOBS_ALIVE_REAL) * parentPVars->LimitDespawnPercent);
        // int maxCanHaveAlive = parentPVars->State.PlayerIsNear ? (int)maxf(mobsAllocated, limit) : limit;
        // if (parentPVars->State.NumTotalSpawned > maxCanHaveAlive) {
        //   struct MobPVar* mobPVars = (struct MobPVar*)m->PVar;
        //   if (!mobPVars->MobVars.Destroy) {
        //     mobPVars->MobVars.Destroy = 1;
        //     parentPVars->State.NumSpawned[mobPVars->MobVars.Userdata]--;
        //     parentPVars->State.NumTotalSpawned--;
        //     return;
        //   }
        // }
      }
    }

    ++m;
  }
}

//--------------------------------------------------------------------------
void spawnerStart(void)
{
  spawnerTicksSinceLastDelete++;
  spawnerInitialized = 1;
  spawnerNumLastActive = spawnerNumActive;
  spawnerNumActive = 0;

  int i;
  int countHasPlayerIsNear = 0;
  for (i = 0; i < spawnerSpawnRequestsCount; ++i) {
    struct SpawnerSpawnRequest* request = &spawnerSpawnRequests[i];
    Moby* moby = request->Spawner;
    struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;
    
    if (pvars->State.PlayerIsNear) {
      countHasPlayerIsNear += 1;
    }
  }

  int totalAlive = MapConfig.State ? MapConfig.State->MobStats.TotalAlive : 0;
  int restrictSpawning = totalAlive >= (MAX_MOBS_ALIVE_REAL*0.9);
  for (i = 0; i < spawnerSpawnRequestsCount; ++i) {
    struct SpawnerSpawnRequest* request = &spawnerSpawnRequests[i];
    Moby* moby = request->Spawner;
    struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

    // respawn mob bypasses spawn logic
    int isRespawn = request->SpawnArgs.SpawnFromUID > 0;
    if (isRespawn) {
      //vector_copy(request->SpawnArgs.Position, CollLine_Fix_GetHitPosition());
      if (MapConfig.TryCreateMobFunc(&request->SpawnArgs)) {
        pvars->State.NumSpawned[request->SpawnArgs.Userdata]++;
        pvars->State.NumTotalSpawned++;
      }
      continue;
    }

    float priority = (pvars->State.ClosestPlayerDistSqr - spawnerMinClosestDistToPlayerSqr) / ((spawnerMaxClosestDistToPlayerSqr-spawnerMinClosestDistToPlayerSqr) + 1);
    int spawn = !restrictSpawning || randRange(0, 1) >= priority;
    //DPRINTF("%08X %f-%f (%f) => %f (%d)\n", request->Spawner, spawnerMinClosestDistToPlayerSqr, spawnerMaxClosestDistToPlayerSqr, pvars->State.ClosestPlayerDistSqr, priority, spawn);
    //DLOG("try spawn %08X:%d => %d (%f of [%f - %f] => %f)\n", request->Spawner, request->SpawnArgs.Userdata, spawn, pvars->State.ClosestPlayerDistSqr, spawnerMinClosestDistToPlayerSqr, spawnerMaxClosestDistToPlayerSqr, priority);
    if (spawn) {
    
      // check that the spawn is near a player
      // only if we are near the max # of mobs visible
      // if its been awhile since we've delete a mob, then we can try and spawn a mob that isn't near the player
      // we just want to avoid rapidly spawning/despawning mobs until they converge near the player\
      // given that the despawn mechanism only despawns far-away mobs
      int isSpawnNearPlayer = spawnerIsPointNearPlayer(request->SpawnArgs.Position, SPAWNER_SPAWN_NEAR_DISTANCE*0.8);
      if (spawnerTicksSinceLastDelete < 10 && restrictSpawning && !isSpawnNearPlayer)
        continue;

      // we need to despawn other mobs
      if (totalAlive >= MAX_MOBS_ALIVE_REAL) {

        //if (pvars->State.PlayerIsNear)
        if (isSpawnNearPlayer)
          spawnerTryDespawnMob(moby, MAX_MOBS_ALIVE_REAL / (float)countHasPlayerIsNear);

        continue;
      }

      // don't spawn
      //if (countHasPlayerIsNear && !pvars->State.PlayerIsNear)
      //  continue;

      // check for walkable ground
      VECTOR spawnFrom, spawnTo, up={0,0,0.1,0}, down = {0,0,-30,0};
      vector_add(spawnFrom, request->SpawnArgs.Position, up);
      vector_add(spawnTo, request->SpawnArgs.Position, down);
      if (!CollLine_Fix(spawnFrom, spawnTo, COLLISION_FLAG_IGNORE_DYNAMIC, NULL, NULL))
        continue;

      // verify point is walkable
      if (!mobCollisionIdIsWalkable(CollLine_Fix_GetHitCollisionId()))
        continue;

      vector_copy(request->SpawnArgs.Position, CollLine_Fix_GetHitPosition());
      if (MapConfig.TryCreateMobFunc(&request->SpawnArgs)) {
        pvars->State.NumSpawned[request->SpawnArgs.Userdata]++;
        pvars->State.NumTotalSpawned++;
        pvars->State.NumAlive[request->SpawnArgs.Userdata]++;
        pvars->State.NumTotalAlive++;
      }
    }
  }

  spawnerSpawnRequestsCount = 0;
  spawnerMinClosestDistToPlayerSqr = -1;
  spawnerMaxClosestDistToPlayerSqr = 1;
}

//--------------------------------------------------------------------------
void spawnerInit(void)
{
  Moby* temp = mobySpawn(SPAWNER_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("SPAWNER oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for spawners
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, SPAWNER_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found spawner %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        spawnerOnGuberCreated(moby);
      }
    }

		++moby;
	}

  spawnerInitializedTime = gameGetTime();
  DPRINTF("spawner pvar size %d\n", sizeof(struct SpawnerPVar));
}
