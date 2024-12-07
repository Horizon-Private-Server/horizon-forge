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
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "spawner.h"
#include "mob.h"
#include "game.h"

#define DLOG(moby, format, ...) if (((struct SpawnerPVar*)moby->PVar)->Log) { DPRINTF(format, ##__VA_ARGS__); }

int spawnerInitialized = 0;

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
  pos[0] = randRange(-1, 1);
  pos[1] = randRange(-1, 1);
  pos[2] = 0;
  vector_apply(pos, pos, cuboid->M0);
  pos[2] += 3;

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
    .Parent = moby,
    .Userdata = mobParamsIdx,
    .DifficultyMult = mobParams->DifficultyMultiplier,
    .Config = &mobSpawnParams->Config,
    .SpawnFromUID = fromUid,
  };

  if (spawnerGetRandomSpawnPoint(moby, mobParamsIdx, args.Position, &args.Yaw)) {
    if (MapConfig.TryCreateMobFunc(&args)) {
      pvars->State.NumSpawned[mobParamsIdx]++;
      pvars->State.NumTotalSpawned++;
      return 1;
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
int spawnerSpawnRandom(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  int spawnerMobIdx = selectRandomIndex(SPAWNER_MAX_MOB_TYPES, moby, &spawnerIsValidMobSpawnIdx);
  if (spawnerMobIdx < 0) return 0;
  
  struct SpawnerMobParams* mobParams = &pvars->SpawnableMobParam[spawnerMobIdx];
  if (mobParams->MobParamIdx < 0 || mobParams->Probability <= 0)
    return 0;

  if (mobParams->MaxCanSpawnOrUnlimited > 0 && pvars->State.NumSpawned[spawnerMobIdx] >= mobParams->MaxCanSpawnOrUnlimited)
    return 0;

  if (MapConfig.State && mobParams->MaxCanAliveAtOnce > 0 && mobParams->MaxCanAliveAtOnce <= MapConfig.State->MobStats.NumAlive[mobParams->MobParamIdx])
    return 0;

  if (MapConfig.State && (mobParams->StarsMask & (1 << MapConfig.State->DifficultyStars)) == 0)
    return 0;

  if (pvars->State.Cooldown[spawnerMobIdx] > 0)
    return 0;

  if (randRange(0, 1) > mobParams->Probability)
    return 0;

  // spawn
  if (spawnerSpawn(moby, spawnerMobIdx, -1)) {
    pvars->State.Cooldown[spawnerMobIdx] = mobParams->CooldownTicks;
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int spawnerIsCompleted(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  return pvars->State.NumTotalKilled >= pvars->NumMobsToSpawn;
}

//--------------------------------------------------------------------------
int spawnerCanSpawn(Moby* moby)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  if (MapConfig.State) {
    int totalAlive = MapConfig.State->MobStats.TotalAlive + MapConfig.State->MobStats.TotalSpawning;
    if (totalAlive >= MAX_MOBS_ALIVE_REAL) return 0;
  }

  int total = pvars->State.NumTotalSpawned + pvars->State.NumTotalKilled;
  return moby->State == SPAWNER_STATE_ACTIVATED && !spawnerIsCompleted(moby) && total < pvars->NumMobsToSpawn;
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

  // check if completed
  if (moby->State != SPAWNER_STATE_COMPLETED && spawnerIsCompleted(moby)) {
    DLOG(moby, "spawner %08X completed\n", (u32)moby);
    spawnerBroadcastNewState(moby, SPAWNER_STATE_COMPLETED);
    return;
  }

  if (moby->State != SPAWNER_STATE_ACTIVATED) return;

  // spawn
  if (spawnerCanSpawn(moby)) {
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
  if (pvars->PathGraphIdx >= 0) {
    childPVars->MobVars.MoveVars.PathGraphIdx = pvars->PathGraphIdx;
  }

  // destroy if spawner has completed
  if (moby->State == SPAWNER_STATE_COMPLETED) {
    if (!childPVars->MobVars.Destroyed) {
      childPVars->MobVars.Destroy = 2;
    }
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
    if (!childPVars->MobVars.Destroyed) {
      childPVars->MobVars.Destroy = 2;
    }
    return;
  }

  // handle respawn
  if (childPVars->MobVars.Respawn || notInside) {
    // pass to mob
    // let mob override respawn logic
    if (!childPVars->VTable->OnRespawn || childPVars->VTable->OnRespawn(childMoby)) {
      if (spawnerSpawn(moby, userdata, guberGetUID(childMoby))) {
        childPVars->MobVars.Destroyed = 2;
        pvars->State.NumTotalSpawned--;
        pvars->State.NumSpawned[userdata]--;
      }
    }

    childPVars->MobVars.Respawn = 0;
  }

}

//--------------------------------------------------------------------------
void spawnerOnChildMobSpawned(Moby* moby, Moby* childMoby, u32 userdata)
{
  DLOG(moby, "MOB%d: spawned %08X\n", userdata, (u32)childMoby);
}

//--------------------------------------------------------------------------
void spawnerOnChildMobKilled(Moby* moby, Moby* childMoby, u32 userdata, int killedByPlayerId, int weaponId)
{
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // log kill
  if (killedByPlayerId >= 0) {
    pvars->State.NumTotalKilled++;
    pvars->State.NumKilled[userdata]++;
  }

  pvars->State.NumTotalSpawned--;
  pvars->State.NumSpawned[userdata]--;

  DLOG(moby, "MOB%d: spawned:%d killed:%d\n", userdata, pvars->State.NumSpawned[userdata], pvars->State.NumKilled[userdata]);
  //DLOG(moby, "SPAWNER %d/%d\n", pvars->State.NumTotalKilled, pvars->NumMobsToSpawn);
}

//--------------------------------------------------------------------------
int spawnerOnChildConsiderTarget(Moby* moby, Moby* childMoby, u32 userdata, Moby* target)
{
  int i;
  struct SpawnerPVar* pvars = (struct SpawnerPVar*)moby->PVar;

  // check if target has left aggro cuboids
  // if no aggro cuboids are defined, then return 1
  int inside = 1;
  for (i = 0; i < SPAWNER_MAX_AGGRO_CUBOIDS; ++i) {
    int cuboidIdx = pvars->AggroCuboidIds[i];
    if (cuboidIdx < 0) continue;

    SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
    if (spawnPointIsPointInside(cuboid, target->Position, NULL)) {
      inside = 1;
      break;
    }

    inside = 0;
  }

  return inside;
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
    DLOG(moby, "NumMobsToSpawn=%d\n", pvars->NumMobsToSpawn);
    DLOG(moby, "PathGraphIdx=%d\n", pvars->PathGraphIdx);
    DLOG(moby, "TriggerCuboids\n");
    for (i = 0; i < SPAWNER_MAX_TRIGGER_CUBOIDS; ++i) {
      DLOG(moby, " [%d].CuboidIdx=%d\n", i, pvars->TriggerCuboids[i].CuboidIdx);
      DLOG(moby, " [%d].InteractType=%d\n", i, pvars->TriggerCuboids[i].InteractType);
    }
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
void spawnerStart(void)
{
  spawnerInitialized = 1;
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

  DPRINTF("spawner pvar size %d\n", sizeof(struct SpawnerPVar));
}
