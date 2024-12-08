#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include <libdl/color.h>

#include "game.h"
#include "mob.h"
#include "pathfind.h"
#include "spawner.h"
#include "maputils.h"
#include "shared.h"

void stalkerturretPreUpdate(Moby* moby);
void stalkerturretPostUpdate(Moby* moby);
void stalkerturretPostDraw(Moby* moby);
void stalkerturretMove(Moby* moby);
void stalkerturretOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void stalkerturretOnDestroy(Moby* moby, int killedByPlayerId, int weaponId);
void stalkerturretOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int stalkerturretOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void stalkerturretOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
Moby* stalkerturretGetNextTarget(Moby* moby);
int stalkerturretGetPreferredAction(Moby* moby, int * delayTicks);
void stalkerturretDoAction(Moby* moby);
void stalkerturretDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void stalkerturretForceLocalAction(Moby* moby, int action);
short stalkerturretGetArmor(Moby* moby);
int stalkerturretIsAttacking(Moby* moby);
int stalkerturretCanNonOwnerTransitionToAction(Moby* moby, int action);
int stalkerturretShouldForceStateUpdateOnAction(Moby* moby, int action);

int stalkerturretIsSpawning(struct MobPVar* pvars);
int stalkerturretIsRoaming(struct MobPVar* pvars);
int stalkerturretIsResettingRotation(struct MobPVar* pvars);
int stalkerturretIsIdling(struct MobPVar* pvars);
int stalkerturretCanAttack(struct MobPVar* pvars);
int stalkerturretCanShoot(struct MobPVar* pvars);
int stalkerturretIsDying(Moby* moby);

struct MobVTable StalkerturretVTable = {
  .PreUpdate = &stalkerturretPreUpdate,
  .PostUpdate = &stalkerturretPostUpdate,
  //.PostDraw = &stalkerturretPostDraw,
  .Move = &stalkerturretMove,
  .OnSpawn = &stalkerturretOnSpawn,
  .OnDestroy = &stalkerturretOnDestroy,
  .OnDamage = &stalkerturretOnDamage,
  .OnLocalDamage = &stalkerturretOnLocalDamage,
  .OnStateUpdate = &stalkerturretOnStateUpdate,
  .GetNextTarget = &stalkerturretGetNextTarget,
  .GetPreferredAction = &stalkerturretGetPreferredAction,
  .ForceLocalAction = &stalkerturretForceLocalAction,
  .DoAction = &stalkerturretDoAction,
  .DoDamage = &stalkerturretDoDamage,
  .GetArmor = &stalkerturretGetArmor,
  .IsAttacking = &stalkerturretIsAttacking,
  .CanNonOwnerTransitionToAction = &stalkerturretCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &stalkerturretShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
int stalkerturretCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(StalkerturretMobVars_t), &guberEvent, NULL);
	if (guberEvent)
	{
    if (MapConfig.PopulateSpawnArgsFunc) {
      MapConfig.PopulateSpawnArgsFunc(&spawnArgs, args->Config, args->SpawnParamsIdx, args->SpawnFromUID == -1, args->DifficultyMult);
    }

		u8 random = (u8)rand(100);
    int parentUid = -1;
    if (args->Parent)
      parentUid = guberGetUID(args->Parent);

    vector_copy(position, args->Position);
    char yaw = args->Yaw * 32;
    
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, &yaw, 1);
		guberEventWrite(guberEvent, &args->SpawnFromUID, 4);
		guberEventWrite(guberEvent, &parentUid, 4);
		guberEventWrite(guberEvent, &args->Userdata, 4);
		guberEventWrite(guberEvent, &random, 1);
		guberEventWrite(guberEvent, &args->Behavior, 1);
		guberEventWrite(guberEvent, &spawnArgs, sizeof(struct MobSpawnEventArgs));
	}
	else
	{
		DPRINTF("failed to guberevent mob\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void stalkerturretPreUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;

  int i;
  for (i = 0; i < STALKERTURRET_TARGET_CACHE_COUNT; ++i) {
    struct StalkerturretTargetCache* cache = &turretVars->TargetCache[i];
    if (cache->Moby && cache->TicksSinceLastCheck < 15) {
      cache->TicksSinceLastCheck++;
    }
  }
  
  turretVars->TargetCacheThisFrame = 0;

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void stalkerturretPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
  pvars->MobVars.MoveVars.IsStuck = 0;
  pvars->MobVars.MoveVars.StuckCounter = 0;
  pvars->MobVars.MoveVars.Grounded = 1;

  if (turretVars->GatlingActive)
    decTimerU8(&turretVars->GatlingDelay1);
  else
    decTimerU8(&turretVars->GatlingDelay2);

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    u32 color = colorLerp(STALKERTURRET_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
    if (turretVars->BaseMoby) turretVars->BaseMoby->PrimaryColor = color;
    if (turretVars->TurretMoby) turretVars->TurretMoby->PrimaryColor = color;
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    u32 color = colorLerp(STALKERTURRET_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
    if (turretVars->BaseMoby) turretVars->BaseMoby->PrimaryColor = color;
    if (turretVars->TurretMoby) turretVars->TurretMoby->PrimaryColor = color;
  } else {
    if (turretVars->BaseMoby) turretVars->BaseMoby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;
    if (turretVars->TurretMoby) turretVars->TurretMoby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;
  }
}

//--------------------------------------------------------------------------
void stalkerturretPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = STALKERTURRET_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, 1);
}

//--------------------------------------------------------------------------
void stalkerturretMove(Moby* moby)
{
  
}

//--------------------------------------------------------------------------
void stalkerturretOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  VECTOR turretOffset = {0,0,0.75,0}; 
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
  struct MobSpawnParams* params = &MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx];

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.1 * scale;

  // colors by mob type
	moby->GlowRGBA = STALKERTURRET_GLOW_COLOR;
	moby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;
  moby->ModeBits2 |= (0x80 + (8 * params->TeamPalette)) << 8;

  // targeting
	pvars->TargetVars.targetHeight = 0.75 + (scale * 0.25);
  
  turretVars->GatlingDelay1 = STALKERTURRET_SHOT_ALTERNATE_DELAY;
  turretVars->GatlingDelay2 = STALKERTURRET_SHOT_ALTERNATE_DELAY;
  
#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  Moby* temp = mobySpawn(0x2038, 0);

  // create turret moby
  void* mobyClass = mobyGetClassPtr(0x2038);
  turretVars->TurretMoby = moby;
  moby->PClass = mobyClass;
  moby->CollData = *(int*)((u32)mobyClass + 0x10);
  moby->MClass = *(u8*)(0x0024a110 + 0x2038);
  moby->AnimSeq = *(void**)((u32)mobyClass + 0x48);
  moby->AnimSpeed = 1;
  moby->JointCache = temp->JointCache;
  moby->JointCnt = *(char*)((u32)mobyClass + 0x08);
  moby->ModeBits &= 0xFFF0;
  DPRINTF("stalker turret %08X\n", (u32)moby);
  mobyDestroy(temp);

  // configure turret base moby
  Moby* baseMoby = turretVars->BaseMoby = mobySpawn(8304, 0);
  if (baseMoby) {
    vector_copy(baseMoby->Position, position);
    baseMoby->Rotation[2] = moby->Rotation[2];
    baseMoby->Scale = 0.1 * scale;
    baseMoby->PUpdate = NULL;
    baseMoby->DrawDist = moby->DrawDist;
    baseMoby->UpdateDist = moby->UpdateDist;
    baseMoby->ModeBits2 |= (0x80 + (8 * (params->TeamPalette ? TEAM_RED : TEAM_BLUE))) << 8;
    DPRINTF("base %08X\n", (u32)baseMoby);
  }
  
  // move turret above base
  vector_add(moby->Position, position, turretOffset);

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void stalkerturretOnDestroy(Moby* moby, int killedByPlayerId, int weaponId)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
  if (turretVars && turretVars->TurretMoby && !mobyIsDestroyed(turretVars->TurretMoby)) {
    //mobyDestroy(turretVars->TurretMoby);
    turretVars->TurretMoby = NULL;
  }
  if (turretVars && turretVars->BaseMoby && !mobyIsDestroyed(turretVars->BaseMoby)) {
    mobyDestroy(turretVars->BaseMoby);
    turretVars->BaseMoby = NULL;
  }

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;
  
	// limit corn spawning to prevent freezing/framelag
	if (MapConfig.State && MapConfig.State->MobStats.TotalAlive < 30 && killedByPlayerId >= 0) {
		//mobSpawnCorn(moby, STALKERTURRET_BANGLE_LARM | STALKERTURRET_BANGLE_RARM | STALKERTURRET_BANGLE_LLEG | STALKERTURRET_BANGLE_RLEG | STALKERTURRET_BANGLE_RFOOT | STALKERTURRET_BANGLE_HIPS);
	}
}

//--------------------------------------------------------------------------
void stalkerturretOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    stalkerturretForceLocalAction(moby, STALKERTURRET_ACTION_DIE);
	}

  // auto aggro
	if (mobAmIOwner(moby)) {
    if (!pvars->MobVars.MoveVars.Target) {
      Guber * guber = guberGetObjectByUID(e->SourceUID);
      Moby* moby = guber ? guber->VTable->GetMoby(guber) : NULL;
      Player* target = mobyGetPlayer(moby);
      if (target) {
        Moby* targetMoby = playerGetTargetMoby(target);
        if (targetMoby) {
          pvars->MobVars.MoveVars.Target = targetMoby;
          pvars->MobVars.Dirty = 1;
        }
      }
    }
	}
}

//--------------------------------------------------------------------------
int stalkerturretOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void stalkerturretOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
float stalkerturretGetRoamYaw(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;

  if (!turretVars->BaseMoby) return 0;

  float thetaRange = (90 * MATH_DEG2RAD) * 0.5;
  float baseTheta = turretVars->BaseMoby->Rotation[2];
  float minTheta = baseTheta - thetaRange;
  float maxTheta = baseTheta + thetaRange;
  float t = (pvars->MobVars.CurrentActionForTicks / (float)TPS);
  float yaw = lerpfAngle(minTheta, maxTheta, (sinf(t) + 1) * 0.5);

  return yaw;
}

//--------------------------------------------------------------------------
float stalkerturretSpinTurretGatling(Moby* turretMoby, int jointIdx, float rot)
{
  MATRIX* mtxs = (MATRIX*)turretMoby->JointCache;
  MATRIX m;
  matrix_copy(m, mtxs[jointIdx]);
  vector_write(&m[12], 0);
  matrix_rotate_x(m, m, rot);
  memcpy(mtxs[jointIdx], m, sizeof(VECTOR) * 3);
}

//--------------------------------------------------------------------------
Moby* stalkerturretFireShot(Moby* moby, Moby* target, int jointId)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;

  VECTOR from, to={0,0,1,0}, dir, vel, offset;
  MATRIX m;
  mobyGetJointMatrix(turretVars->TurretMoby, jointId, m);
  vector_copy(from, &m[12]);
  vector_copy(vel, &m[0]);
  if (target) {

    // determine if we should shoot directly towards target
    vector_subtract(dir, target->Position, moby->Position);
    vector_normalize(dir, dir);
    VECTOR planarForward;
    vector_projectonplane(planarForward, dir, moby->M2_03);
    if (acosf(vector_innerproduct(planarForward, moby->M0_03)) < STALKERTURRET_SHOT_LOCK_ON_WITHIN_RAD) {
      vector_add(to, to, target->Position);
      vector_subtract(vel, to, from);
      vector_normalize(vel, vel);
    } else {
      vector_subtract(dir, dir, planarForward);
      vector_projectonplane(vel, vel, moby->M2_03);
      vector_add(vel, vel, dir);
    }
  }

  // move shot from forward
  vector_scale(offset, &m[0], 0.5);
  vector_add(from, from, offset);
  //vector_scale(vel, vel, 1.0); // speed

  // fire shot
  Moby* shotMoby = ((Moby* (*)(float, float, VECTOR, VECTOR, Moby*, int, int, int, int))0x0045d598)(4.0, pvars->MobVars.Config.Damage, from, vel, turretVars->TurretMoby, 1, 0x222124, 0, 0);
  if (shotMoby) {
    ((void (*)(Moby*, int))0x0045d758)(shotMoby, 0);
    ((void (*)(Moby*, int))0x0045d788)(shotMoby, 1);
    ((void (*)(Moby*, int))0x0045d7A8)(shotMoby, 0x001);
    ((void (*)(Moby*, int))0x0045d798)(shotMoby, 0x3C);
    shotMoby->PParent = moby;
  }

  // spawn flare
  ((void (*)(float, float, float, Moby*, int, int, int))0x0042c178)(0.75, 0.75, 1.0, turretVars->TurretMoby, 0, 0, 4);
  
  // play sound
  mobyPlaySound(1, 0, turretVars->TurretMoby);
}

//--------------------------------------------------------------------------
int stalkerturretCanSeeMoby(Moby* moby, Moby* canSeeMoby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;

  int i = 0;
  int freeSlot = -1;
  for (i = 0; i < STALKERTURRET_TARGET_CACHE_COUNT; ++i) {
    struct StalkerturretTargetCache* cache = &turretVars->TargetCache[i];
    if (!cache->Moby) {
      freeSlot = i;
      continue;
    }

    if (cache->TicksSinceLastCheck >= 15) {
      freeSlot = i;
      continue;
    }

    if (canSeeMoby != cache->Moby) continue;
    return cache->CanSee;
  }

  if (turretVars->TargetCacheThisFrame)
    return 0;

  turretVars->TargetCacheThisFrame = 1;
  int canSee = mobCanSeeMoby(moby, canSeeMoby);
  if (freeSlot >= 0) {
    struct StalkerturretTargetCache* cache = &turretVars->TargetCache[freeSlot];
    cache->Moby = canSeeMoby;
    cache->CanSee = canSee;
    cache->TicksSinceLastCheck = 0;
  }

  return canSee;
}

//--------------------------------------------------------------------------
Moby* stalkerturretGetNextTarget(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
	Player ** players = playerGetAll();
	int i;
	VECTOR delta;
  VECTOR forward;
	Moby * currentTarget = pvars->MobVars.MoveVars.Target;
  Moby* turretMoby = turretVars->TurretMoby;
  Moby* baseMoby = turretVars->BaseMoby;
	Player * closestPlayer = NULL;
	float closestPlayerDist = 100000;

  if (!turretMoby || !baseMoby) return NULL;

  vector_fromyaw(forward, turretMoby->Rotation[2]);
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player * p = players[i];
		if (p && p->SkinMoby && !playerIsDead(p) && p->Health > 0 && p->SkinMoby->Opacity >= 0x80) {
			vector_subtract(delta, p->PlayerPosition, moby->Position);
			float dist = vector_length(delta);
      Moby* pTargetMoby = playerGetTargetMoby(p);
      int isCurrentTarget = pTargetMoby == currentTarget;
      
      // determine angle from mob forward to player
      float theta = acosf(vector_innerproduct(forward, delta));
			if (dist < 300) {

        // skip if not in sight or aggro zone, unless already targeted
        if (!isCurrentTarget) {
          if (dist > pvars->MobVars.Config.AutoAggroMaxRange && (dist > pvars->MobVars.Config.VisionRange || fabsf(theta) > pvars->MobVars.Config.PeripheryRangeTheta)) continue;
          if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS && !spawnerOnChildConsiderTarget(moby->PParent, moby, pvars->MobVars.Userdata, pTargetMoby)) continue;
          if (!stalkerturretCanSeeMoby(turretMoby, pTargetMoby)) continue;
        }

				// favor existing target
				if (isCurrentTarget)
					dist *= (1.0 / STALKERTURRET_TARGET_KEEP_CURRENT_FACTOR);
				
				// pick closest target
				if (dist < closestPlayerDist) {
					closestPlayer = p;
					closestPlayerDist = dist;
				}
			}
		}
	}

	if (closestPlayer)
		return playerGetTargetMoby(closestPlayer);

	return NULL;
}

//--------------------------------------------------------------------------
int stalkerturretGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
  Moby* turretMoby = turretVars->TurretMoby;
  Moby* baseMoby = turretVars->BaseMoby;
	VECTOR t;

	// no preferred action
	if (stalkerturretIsSpawning(pvars))
		return -1;

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = stalkerturretGetNextTarget(moby);
	if (target) {
    if (stalkerturretCanAttack(pvars)) {
      if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
      return STALKERTURRET_ACTION_ATTACK;
    }
    return STALKERTURRET_ACTION_LOOK_AT_TARGET;
	}

  // wait for resetting rotation to finish
  if (stalkerturretIsResettingRotation(pvars) && baseMoby && turretMoby) {
    float yaw = stalkerturretGetRoamYaw(moby);
    if (fabsf(baseMoby->Rotation[2] - turretMoby->Rotation[2]) < 0.01 && fabsf(yaw - turretMoby->Rotation[2]) < 0.01) {
      return STALKERTURRET_ACTION_ROAM;
    }
  }

  // reset rotation before roam
  if (!stalkerturretIsRoaming(pvars)) {
    return STALKERTURRET_ACTION_RESET_ROTATION;
  }

	return STALKERTURRET_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void stalkerturretRenderPath(Moby* moby)
{
  int x,y;
  int i;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* pathGraph = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
  u8* path = (u8*)pvars->MobVars.MoveVars.CurrentPath;
  int pathLen = pvars->MobVars.MoveVars.PathEdgeCount;
  int pathIdx = pvars->MobVars.MoveVars.PathEdgeCurrent;


  for (i = 0; i < pathLen; ++i) {
    u8* edge = pathGraph->Edges[path[i]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[1]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }

  VECTOR t;
  if (pathGetTargetPos(pathGraph, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
    pvars->MobVars.Dirty = 1; // new path, sync with other clients
  if (gfxWorldSpaceToScreenSpace(t, &x, &y)) {
    gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "+", -1, 4);
  }
}
#endif

//--------------------------------------------------------------------------
void stalkerturretDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
  Moby* turretMoby = turretVars->TurretMoby;
  Moby* baseMoby = turretVars->BaseMoby;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float freezeFactor = (!stalkerturretIsDying(moby) && pvars->MobVars.FreezeEffectActiveTicks > 0) ? MOB_POSTFX_FREEZE_FACTOR : 1;
  float turnSpeed = speed * freezeFactor * STALKERTURRET_TURN_RADIANS_PER_SEC;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&stalkerturretRenderPath, moby);
#endif

  mobStand(moby);
	switch (pvars->MobVars.Action)
	{
		case STALKERTURRET_ACTION_SPAWN:
		{
      //mobTransAnim(moby, STALKERTURRET_ANIM_0, 0);
			break;
		}
		case STALKERTURRET_ACTION_IDLE:
		{
			break;
		}
		case STALKERTURRET_ACTION_LOOK_AT_TARGET:
    {
      if (target && turretMoby)
        mobTurnTowards(turretMoby, target->Position, turnSpeed);
      break;
    }
    case STALKERTURRET_ACTION_RESET_ROTATION:
    {
      if (turretMoby && baseMoby) {
        float delta = baseMoby->Rotation[2] - turretMoby->Rotation[2];
        if (fabsf(delta) < 0.01) {

        } else {
          float dir = delta < 0 ? -1 : 1;
          turretMoby->Rotation[2] = clampAngle(turretMoby->Rotation[2] + (delta * MATH_DT));
        }
      }
      break;
    }
    case STALKERTURRET_ACTION_ROAM:
    {
      if (turretMoby)
        turretMoby->Rotation[2] = stalkerturretGetRoamYaw(moby);
      break;
    }
    case STALKERTURRET_ACTION_DIE:
    {
      break;
    }
		case STALKERTURRET_ACTION_ATTACK:
		{
      if (turretMoby) {

        // face target
        if (target) {
          float dist = vector_distance(target->Position, moby->Position);
          float predictFactor = MapConfig.State->DifficultyStars * STALKERTURRET_TURN_PREDICT_FACTOR_PER_STAR * lerpf(1, 7, clamp(dist / 30, 0, 1));
          mobTurnTowardsPredictive(turretMoby, target, turnSpeed, predictFactor);
        }

        // shoot
        stalkerturretSpinTurretGatling(turretMoby, 3, turretVars->GatlingRotation);
        stalkerturretSpinTurretGatling(turretMoby, 8, -turretVars->GatlingRotation);

        if (stalkerturretCanShoot(pvars) && turretVars->GatlingDelay1 == 0) {
          stalkerturretFireShot(moby, target, 3);
          turretVars->GatlingDelay1 = STALKERTURRET_SHOT_ALTERNATE_DELAY - (2 * MapConfig.State->DifficultyStars);
          turretVars->GatlingActive = !turretVars->GatlingActive;
        }
        if (stalkerturretCanShoot(pvars) && turretVars->GatlingDelay2 == 0) {
          stalkerturretFireShot(moby, target, 8);
          turretVars->GatlingDelay2 = STALKERTURRET_SHOT_ALTERNATE_DELAY - (2 * MapConfig.State->DifficultyStars);
          turretVars->GatlingActive = !turretVars->GatlingActive;
        }
      }
			break;
		}
  }

  // update gatling speed
  float targetGatlingSpeed = 0;
  if (stalkerturretIsAttacking(moby)) targetGatlingSpeed = STALKERTURRET_MAX_GATLING_SPEED;
  turretVars->GatlingSpeed = lerpf(turretVars->GatlingSpeed, targetGatlingSpeed, 1 - powf(MATH_E, -STALKERTURRET_GATLING_ACCELERATION * MATH_DT));
  turretVars->GatlingRotation = clampAngle(turretVars->GatlingRotation + (turretVars->GatlingSpeed * MATH_DT));

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void stalkerturretDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  //mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, STALKERTURRET_SUBSKELETON_JOINT_LEFT_HAND, 1, 0);
}

//--------------------------------------------------------------------------
void stalkerturretForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
		case STALKERTURRET_ACTION_SPAWN:
		{
			// enable collision
			moby->CollActive = 0;
			break;
		}
    case STALKERTURRET_ACTION_DIE:
    {
      // can't undie
      return;
    }
	}

	// to
	switch (action)
	{
		case STALKERTURRET_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case STALKERTURRET_ACTION_ROAM:
    {
      break;
    }
		case STALKERTURRET_ACTION_DIE:
		{
      pvars->MobVars.Destroy = 1;
			break;
		}
		case STALKERTURRET_ACTION_ATTACK:
		{
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
			break;
		}
		default:
		{
			break;
		}
	}

	// 
  if (action != pvars->MobVars.Action)
    pvars->MobVars.CurrentActionForTicks = 0;

	pvars->MobVars.Action = action;
	pvars->MobVars.NextAction = -1;
	pvars->MobVars.ActionCooldownTicks = STALKERTURRET_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short stalkerturretGetArmor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.Health;
  int bangles = pvars->MobVars.Config.Bangles;

  if (t < 0.3)
    return 0x0000;
  else if (t < 0.7)
    return bangles & 0x1f; // remove torso bangle

	return bangles;
}

//--------------------------------------------------------------------------
int stalkerturretIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == STALKERTURRET_ACTION_ATTACK;
}

//--------------------------------------------------------------------------
int stalkerturretCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == STALKERTURRET_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int stalkerturretShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == STALKERTURRET_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == STALKERTURRET_ACTION_ROAM || action == STALKERTURRET_ACTION_ROAM) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int stalkerturretIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == STALKERTURRET_ACTION_SPAWN && pvars->MobVars.CurrentActionForTicks < (TPS * 0.25);
}

//--------------------------------------------------------------------------
int stalkerturretIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == STALKERTURRET_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int stalkerturretIsResettingRotation(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == STALKERTURRET_ACTION_RESET_ROTATION;
}
//--------------------------------------------------------------------------
int stalkerturretIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == STALKERTURRET_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int stalkerturretCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int stalkerturretCanShoot(struct MobPVar* pvars)
{
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
	return turretVars->GatlingSpeed >= STALKERTURRET_SHOOT_AT_GATLING_SPEED;
}

//--------------------------------------------------------------------------
int stalkerturretIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == STALKERTURRET_ACTION_DIE;
}
