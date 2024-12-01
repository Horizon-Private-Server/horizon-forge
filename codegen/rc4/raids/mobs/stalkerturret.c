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

struct MobVTable StalkerturretVTable = {
  .PreUpdate = &stalkerturretPreUpdate,
  .PostUpdate = &stalkerturretPostUpdate,
  .PostDraw = &stalkerturretPostDraw,
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

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void stalkerturretPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  pvars->MobVars.MoveVars.IsStuck = 0;
  pvars->MobVars.MoveVars.StuckCounter = 0;
  pvars->MobVars.MoveVars.Grounded = 1;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(STALKERTURRET_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(STALKERTURRET_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;
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

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.1 * scale;

  // colors by mob type
	moby->GlowRGBA = STALKERTURRET_GLOW_COLOR;
	moby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 0.75 + (scale * 0.25);
  pvars->MobVars.BlipType = 4;
  pvars->MobVars.BlipTeam = TEAM_RED;
  
#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // configure turret moby
  if (!turretVars->TurretMoby) {
    Moby* turretMoby = turretVars->TurretMoby = mobySpawn(8248, 1424);
    if (turretMoby) {
      vector_add(turretMoby->Position, position, turretOffset);
      turretMoby->Rotation[2] = yaw;
      turretMoby->Scale = 0.1 * scale;
      turretMoby->PUpdate = NULL;
      turretMoby->DrawDist = 128;
      turretMoby->UpdateDist = 128;
      DPRINTF("%08X\n", (u32)turretMoby);
    }
  }

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
    mobyDestroy(turretVars->TurretMoby);
    turretVars->TurretMoby = NULL;
  }

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = STALKERTURRET_PRIMARY_COLOR;
  
	// limit corn spawning to prevent freezing/framelag
	if (MapConfig.State && MapConfig.State->MobStats.TotalAlive < 30 && killedByPlayerId >= 0) {
		mobSpawnCorn(moby, STALKERTURRET_BANGLE_LARM | STALKERTURRET_BANGLE_RARM | STALKERTURRET_BANGLE_LLEG | STALKERTURRET_BANGLE_RLEG | STALKERTURRET_BANGLE_RFOOT | STALKERTURRET_BANGLE_HIPS);
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
      Player* target = (Player*)guberGetObjectByUID(e->SourceUID);
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

  float thetaRange = (90 * MATH_DEG2RAD) * 0.5;
  float minTheta = moby->Rotation[2] - thetaRange;
  float maxTheta = moby->Rotation[2] + thetaRange;
  float t = (pvars->MobVars.CurrentActionForTicks / (float)TPS);
  float yaw = lerpfAngle(minTheta, maxTheta, (sinf(t) + 1) * 0.5);

  return yaw;
}

//--------------------------------------------------------------------------
void stalkerturretTurretTransAnim(Moby* moby, int animId, float startOff)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;

  if (!turretVars || !turretVars->TurretMoby) return;
 
  Moby* turretMoby = turretVars->TurretMoby;
  if (turretMoby->AnimSeqId == animId) return;

  mobyAnimTransition(turretMoby, animId, 10, startOff);
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
	Player * closestPlayer = NULL;
	float closestPlayerDist = 100000;

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
		vector_copy(t, target->Position);
		vector_subtract(t, t, moby->Position);
		float distSqr = vector_sqrmag(t);
		float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;

		if (distSqr <= attackRadiusSqr) {
			if (stalkerturretCanAttack(pvars)) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return STALKERTURRET_ACTION_ATTACK;
      }
			return STALKERTURRET_ACTION_LOOK_AT_TARGET;
		} else {
			return STALKERTURRET_ACTION_LOOK_AT_TARGET;
		}
	}

  // wait for resetting rotation to finish
  if (stalkerturretIsResettingRotation(pvars) && turretVars && turretVars->TurretMoby) {
    float yaw = stalkerturretGetRoamYaw(moby);
    if (fabsf(moby->Rotation[2] - turretVars->TurretMoby->Rotation[2]) < 0.01 && fabsf(yaw - turretVars->TurretMoby->Rotation[2]) < 0.01) {
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

extern int aaa;

//--------------------------------------------------------------------------
void stalkerturretDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  StalkerturretMobVars_t* turretVars = (StalkerturretMobVars_t*)pvars->AdditionalMobVarsPtr;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
  Moby* turretMoby = turretVars ? turretVars->TurretMoby : NULL;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? STALKERTURRET_TURN_RADIANS_PER_SEC : STALKERTURRET_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? STALKERTURRET_MOVE_ACCELERATION : STALKERTURRET_MOVE_AIR_ACCELERATION;

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
      if (turretMoby) {
        float delta = moby->Rotation[2] - turretMoby->Rotation[2];
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
        if (target)
          mobTurnTowards(turretMoby, target->Position, turnSpeed);

        // shoot
        stalkerturretTurretTransAnim(moby, aaa, 0);
      }
			break;
		}
  }

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
