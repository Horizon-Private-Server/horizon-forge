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
#include "laserbeam.h"

void leviathanPreUpdate(Moby* moby);
void leviathanPostUpdate(Moby* moby);
void leviathanPostDraw(Moby* moby);
void leviathanMove(Moby* moby);
void leviathanOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void leviathanOnDestroy(Moby* moby, int killedByPlayerId, int weaponId);
void leviathanOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int leviathanOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void leviathanOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
enum LeviathanAction leviathanGetPreferredAttack(Moby* moby);
int leviathanGetPreferredAction(Moby* moby, int * delayTicks);
void leviathanDoAction(Moby* moby);
void leviathanDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void leviathanForceLocalAction(Moby* moby, int action);
short leviathanGetArmor(Moby* moby);
int leviathanIsAttacking(Moby* moby);
int leviathanCanNonOwnerTransitionToAction(Moby* moby, int action);
int leviathanShouldForceStateUpdateOnAction(Moby* moby, int action);

int leviathanIsSpawning(struct MobPVar* pvars);
int leviathanIsRoaming(struct MobPVar* pvars);
int leviathanIsIdling(struct MobPVar* pvars);
int leviathanCanAttack(struct MobPVar* pvars);
int leviathanIsFlinching(Moby* moby);
int leviathanIsDying(Moby* moby);
int leviathanShouldStrafe(Moby* moby);
int leviathanShouldChase(Moby* moby);
int leviathanGetLaserForTicks(Moby* moby);

struct MobVTable LeviathanVTable = {
  .PreUpdate = &leviathanPreUpdate,
  .PostUpdate = &leviathanPostUpdate,
  .PostDraw = &leviathanPostDraw,
  .Move = &leviathanMove,
  .OnSpawn = &leviathanOnSpawn,
  .OnDestroy = &leviathanOnDestroy,
  .OnDamage = &leviathanOnDamage,
  .OnLocalDamage = &leviathanOnLocalDamage,
  .OnStateUpdate = &leviathanOnStateUpdate,
  .GetNextTarget = &mobGetNextTarget,
  .GetPreferredAction = &leviathanGetPreferredAction,
  .ForceLocalAction = &leviathanForceLocalAction,
  .DoAction = &leviathanDoAction,
  .DoDamage = &leviathanDoDamage,
  .GetArmor = &leviathanGetArmor,
  .IsAttacking = &leviathanIsAttacking,
  .CanNonOwnerTransitionToAction = &leviathanCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &leviathanShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
int leviathanCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(LeviathanMobVars_t), &guberEvent, NULL);
	if (guberEvent)
	{
    if (MapConfig.PopulateSpawnArgsFunc) {
      MapConfig.PopulateSpawnArgsFunc(&spawnArgs, args->Config, args->SpawnParamsIdx, args->SpawnFromUID == -1, args->DifficultyMult);
    }

		u8 random = (u8)rand(100);
    int parentUid = -1;
    if (args->Parent)
      parentUid = guberGetUID(args->Parent);


    // spawn slightly above point
    vector_add(position, position, args->Position);
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
void leviathanPreUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;

  // decrement path target pos ticker
  decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);
  decTimerU32(&leviathanVars->AttackLaserCooldownTicks);

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void leviathanPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = pvars->MobVars.Config.Scale;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;

  // 
  if (leviathanVars->AttackLaserCooldownTicks == 0) {
    leviathanVars->AttackLaserCooldownTicks = randRangeInt(LEVIATHAN_LASER_COOLDOWN_TICKS_MIN, LEVIATHAN_LASER_COOLDOWN_TICKS_MAX);
  }

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(LEVIATHAN_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(LEVIATHAN_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = LEVIATHAN_PRIMARY_COLOR;
  }

  // adjust animSpeed by speed and by animation
  float baseSpeed = 0.5;
	float animSpeed = baseSpeed;
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;

  if (moby->AnimSeqId == LEVIATHAN_ANIM_JUMP) {
    animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / 35, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = baseSpeed;
    }
  } else if (leviathanIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (leviathanIsDying(moby)) {
    animSpeed = 1;
  } else if (moby->AnimSeqId == LEVIATHAN_ANIM_WALK || moby->AnimSeqId == LEVIATHAN_ANIM_WALK_LEFT || moby->AnimSeqId == LEVIATHAN_ANIM_WALK_RIGHT) {
    animSpeed *= mobGetCurrentMoveSpeed(moby);
  }

  // scale up attack and walk animations by speed
  switch (moby->AnimSeqId) {
    case LEVIATHAN_ANIM_SWING:
    case LEVIATHAN_ANIM_STAB_DOWN:
    case LEVIATHAN_ANIM_WALK:
    case LEVIATHAN_ANIM_WALK_LEFT:
    case LEVIATHAN_ANIM_WALK_RIGHT:
      {
        animSpeed *= (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
        break;
      }
  }

	if ((moby->DrawDist == 0 && !leviathanIsAttacking(moby))) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void leviathanPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = LEVIATHAN_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, LEVIATHAN_SUBSKELETON_JOINT_BODY);
}

//--------------------------------------------------------------------------
void leviathanMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void leviathanOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.256339 * scale;

  // colors by mob type
	moby->GlowRGBA = LEVIATHAN_GLOW_COLOR;
	moby->PrimaryColor = LEVIATHAN_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 0.5 + (scale * 0.25);

#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void leviathanOnDestroy(Moby* moby, int killedByPlayerId, int weaponId)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;
  if (leviathanVars->LaserbeamMoby) {
    laserbeamDestroy(leviathanVars->LaserbeamMoby);
    leviathanVars->LaserbeamMoby = NULL;
  }

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = LEVIATHAN_PRIMARY_COLOR;
  
  // spawn corn
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void leviathanOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  
  // take more damage in exhausted state
  if ((pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER || pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER_LOCKON) && moby->AnimSeqId == LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED) {
    e->DamageQuarters *= 2;
    damage *= 2;
  }

  float newHp = pvars->MobVars.Health - damage;
	int canFlinch = pvars->MobVars.Action != LEVIATHAN_ACTION_FLINCH 
            && pvars->MobVars.Action != LEVIATHAN_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    leviathanForceLocalAction(moby, LEVIATHAN_ACTION_DIE);
	}

	// knockback
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		memcpy(&pvars->MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
	}

  // flinch
	if (mobAmIOwner(moby))
	{
		float damageRatio = damage / pvars->MobVars.Config.Health;
    float powerFactor = LEVIATHAN_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * LEVIATHAN_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, LEVIATHAN_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, LEVIATHAN_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, LEVIATHAN_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, LEVIATHAN_ACTION_FLINCH);
        }
      }
    }

    // auto aggro
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

  // short freeze
  if (isShortFreeze && pvars->MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS) {
    pvars->MobVars.SlowTicks = MOB_SHORT_FREEZE_DURATION_TICKS;
    mobResetMoveStep(moby);
  }
}

//--------------------------------------------------------------------------
int leviathanOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void leviathanOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
enum LeviathanAction leviathanGetPreferredAttack(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;
  Moby* target = pvars->MobVars.MoveVars.Target;
  int behavior = pvars->MobVars.Behavior;
  if (!target)
    return -1;

  // check if target is within range
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float distSqr = vector_sqrmag(dt);
  float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;
  float rangedAttackRadiusSqr = pvars->MobVars.Config.RangedMaxDistanceToTarget*pvars->MobVars.Config.RangedMaxDistanceToTarget;
  if (distSqr > attackRadiusSqr) {

    // check if moby is looking at (close to) target
    int isLaserAction = pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER || pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER_LOCKON;
    if (distSqr <= rangedAttackRadiusSqr && leviathanVars->AttackLaserCooldownTicks == 0 && (MapConfig.State ? MapConfig.State->DifficultyStars : 0) > 0) {
      float theta = acosf(vector_innerproduct(dt, moby->M0_03));
      if (!isLaserAction && fabsf(theta) < (30 * MATH_DEG2RAD))
        return rand(20 / (MapConfig.State->DifficultyStars+1)) ? LEVIATHAN_ACTION_ATTACK_LASER : LEVIATHAN_ACTION_ATTACK_LASER_LOCKON;
    }

    return -1;
  }

  // if target is right in front try stab
  if (pvars->MobVars.MoveVars.Target) {
    VECTOR inFrontPos;

    vector_scale(dt, moby->M0_03, pvars->MobVars.Config.Scale * 2);
    vector_add(inFrontPos, moby->Position, dt);
    vector_subtract(dt, inFrontPos, pvars->MobVars.MoveVars.Target->Position);
    if (vector_sqrmag(dt) < 1) {
      return LEVIATHAN_ACTION_ATTACK_STAB;
    }
  }

  // default to swing
	return LEVIATHAN_ACTION_ATTACK_SWING;
}

//--------------------------------------------------------------------------
int leviathanGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int behavior = pvars->MobVars.Behavior;
	VECTOR t;

	// no preferred action
	if (leviathanIsAttacking(moby))
		return -1;

	if (leviathanIsSpawning(pvars))
		return -1;

  if (leviathanIsFlinching(moby))
    return -1;

  if (pvars->MobVars.Action == LEVIATHAN_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == LEVIATHAN_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return LEVIATHAN_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > LEVIATHAN_MAX_WALKABLE_SLOPE) {
    return LEVIATHAN_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return LEVIATHAN_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = mobGetNextTarget(moby);
	if (target) {
    if (leviathanCanAttack(pvars)) {
      int preferredAttack = leviathanGetPreferredAttack(moby);
      if (preferredAttack >= 0) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
        return preferredAttack;
      }
    }

    if (leviathanShouldChase(moby))
      return LEVIATHAN_ACTION_CHASE;

    if (leviathanShouldStrafe(moby))
      return LEVIATHAN_ACTION_STRAFE;

    return LEVIATHAN_ACTION_WALK;
	}

  // if roaming, then we want to periodically stop or reroute
  if (leviathanIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return LEVIATHAN_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (leviathanIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return LEVIATHAN_ACTION_IDLE;
  }
	
	return LEVIATHAN_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void leviathanRenderPath(Moby* moby)
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
int leviathanDoActionMove(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  int behavior = pvars->MobVars.Behavior;
  int strafe = pvars->MobVars.Action == LEVIATHAN_ACTION_STRAFE;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_TURN_RADIANS_PER_SEC : LEVIATHAN_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_MOVE_ACCELERATION : LEVIATHAN_MOVE_AIR_ACCELERATION;

  // 
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float sqrDistToTarget = vector_sqrmag(dt);
  float dir = ((pvars->MobVars.ActionId + pvars->MobVars.DynamicRandom) % 3) - 1;

  // chase goes directly towards target, quickly
  if (pvars->MobVars.Action == LEVIATHAN_ACTION_CHASE) {
    speed *= LEVIATHAN_CHASE_SPEED_MULT;
    turnSpeed *= LEVIATHAN_CHASE_SPEED_MULT;
    strafe = 0;
    dir = 0;
  }

  pvars->MobVars.MoveVars.ForceUseTargetPosition = strafe;
  float strafeDir = ((pvars->MobVars.DynamicRandom + (pvars->MobVars.ActionId/2)) % 2) ? 1 : -1;
  if (strafe) {
    VECTOR strafeVec, strafeFwd;
    vector_scale(strafeVec, moby->M1_03, 5 * strafeDir);
    if (dir != 0 && sqrDistToTarget < (LEVIATHAN_CHASE_TARGET_RADIUS*LEVIATHAN_CHASE_TARGET_RADIUS)) {
      // move towards target if normal/aggro
      // move away if evasive
      vector_scale(strafeFwd, moby->M0_03, behavior == LEVIATHAN_BEHAVIOR_EVASIVE ? -5 : 5);
      vector_add(strafeVec, strafeVec, strafeFwd);
    }

    vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    vector_add(pvars->MobVars.MoveVars.TargetPosition, pvars->MobVars.MoveVars.TargetPosition, strafeVec);
  }
  
  if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
    pvars->MobVars.Dirty = 1; // new path, sync with other clients

  if (strafe) {
    VECTOR dt;
    vector_subtract(dt, t, moby->Position);
    float yaw = atan2f(dt[1], dt[0]);
    if (fabsf(mobTurnTowards(moby, target->Position, turnSpeed)) < (5*MATH_DEG2RAD)) {
      mobGetVelocityToTargetWithDirection(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, yaw, speed * LEVIATHAN_MOVE_STRAFE_MULT, acceleration);
    }
    return strafeDir > 0 ? LEVIATHAN_ANIM_WALK_LEFT : LEVIATHAN_ANIM_WALK_RIGHT;
  } else {
    mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
    return LEVIATHAN_ANIM_WALK;
  }
}

//--------------------------------------------------------------------------
void leviathanDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
  Moby* laserbeamMoby = leviathanVars->LaserbeamMoby;
  VECTOR up = {0,0,1,0};
	VECTOR t;
  int behavior = pvars->MobVars.Behavior;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_TURN_RADIANS_PER_SEC : LEVIATHAN_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_MOVE_ACCELERATION : LEVIATHAN_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == LEVIATHAN_ACTION_FLINCH || pvars->MobVars.LastAction == LEVIATHAN_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&leviathanRenderPath, moby);
#endif

  // static int aaa = 0;
  // if (padGetButtonDown(0, PAD_RIGHT) && aaa > 0) {
  //   --aaa;
  //   DPRINTF("%d\n", aaa);
  //   mobTransAnim(moby, aaa, 0);
  // } else if (padGetButtonDown(0, PAD_DOWN) && aaa < (0x24 - 1)) {
  //   ++aaa;
  //   DPRINTF("%d\n", aaa);
  //   mobTransAnim(moby, aaa, 0);
  // }

  // mobStand(moby);
  // return;

  // default deactivate laserbeam
  // will get set to activated below
  if (laserbeamMoby) {
    laserbeamMoby->State = LASERBEAM_STATE_DEACTIVATED;
  }

	switch (pvars->MobVars.Action)
	{
		case LEVIATHAN_ACTION_SPAWN:
		{
      mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
      mobStand(moby);
			break;
		}
		case LEVIATHAN_ACTION_FLINCH:
		case LEVIATHAN_ACTION_BIG_FLINCH:
		{
      int animFlinchId = pvars->MobVars.Action == LEVIATHAN_ACTION_BIG_FLINCH ? LEVIATHAN_ANIM_BIG_FLINCH : LEVIATHAN_ANIM_BIG_FLINCH;

      mobTransAnim(moby, animFlinchId, 0);
      
			if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == LEVIATHAN_ACTION_BIG_FLINCH) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, LEVIATHAN_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case LEVIATHAN_ACTION_IDLE:
		{
      if (pvars->MobVars.AnimationLooped || (moby->AnimSeqId != LEVIATHAN_ANIM_IDLE)) {
			  mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
      } else {
        mobTransAnim(moby, moby->AnimSeqId, 0);
      }
      mobStand(moby);
      //mobResetSoundTrigger(moby);
			break;
		}
		case LEVIATHAN_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    mobTransAnim(moby, LEVIATHAN_ANIM_JUMP, 5);
          mobResetSoundTrigger(moby);

          // check if we're near last jump pos
          // if so increment StuckJumpCount
          if (pvars->MobVars.MoveVars.IsStuck) {
            if (pvars->MobVars.MoveVars.StuckJumpCount < 255)
              pvars->MobVars.MoveVars.StuckJumpCount++;
          }

          // use delta height between target as base of jump speed
          // with min speed
          float jumpSpeed = pvars->MobVars.MoveVars.QueueJumpSpeed;
          if (jumpSpeed <= 0) {
            jumpSpeed = 8; //clamp(0 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->MobVars.MoveVars.WallSlope) * 1, 3, 15);
          }

          vector_write(pvars->MobVars.MoveVars.Velocity, 0);
          pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
          pvars->MobVars.MoveVars.Grounded = 0;
          pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
          pvars->MobVars.MoveVars.JumpedThisAction = 1;
          mobResetMoveStep(moby);
        }
				break;
			}
		case LEVIATHAN_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case LEVIATHAN_ACTION_ROAM:
    {
      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, 0);
      }

			// 
      if (moby->AnimSeqId == LEVIATHAN_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        leviathanForceLocalAction(moby, LEVIATHAN_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, LEVIATHAN_ANIM_WALK, 0);
      } else if (moby->AnimSeqId != LEVIATHAN_ANIM_WALK || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
      }
      break;
    }
    case LEVIATHAN_ACTION_CHASE:
    case LEVIATHAN_ACTION_WALK:
    case LEVIATHAN_ACTION_STRAFE:
		{
      int walkAnimId = LEVIATHAN_ANIM_WALK;
      if (!isInAirFromFlinching && target) {
        walkAnimId = leviathanDoActionMove(moby);
      }

			// 
      if (moby->AnimSeqId == LEVIATHAN_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        leviathanForceLocalAction(moby, LEVIATHAN_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, walkAnimId, 0);
      } else if (moby->AnimSeqId != walkAnimId || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
      }
			break;
		}
    case LEVIATHAN_ACTION_DIE:
    {
      mobTransAnim(moby, LEVIATHAN_ANIM_DIE, 0);
      mobStand(moby);

      if (moby->AnimSeqId == LEVIATHAN_ANIM_DIE && pvars->MobVars.AnimationLooped) {
        pvars->MobVars.Destroy = 1;
      }
      break;
    }
		case LEVIATHAN_ACTION_ATTACK_SWING:
		case LEVIATHAN_ACTION_ATTACK_STAB:
		{
      int attackAnimId = LEVIATHAN_ANIM_SWING;
      float attackAnimHitStart = 15;
      float attackAnimHitEnd = 18;
      float damage = pvars->MobVars.Config.Damage;

      if (pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_STAB) {
        attackAnimId = LEVIATHAN_ANIM_STAB_DOWN;
        attackAnimHitStart = 4.5;
        attackAnimHitEnd = 6;
        damage *= 1.5; // more damage
      }

			mobTransAnim(moby, attackAnimId, 0);
			int swingAttackReady = moby->AnimSeqId == attackAnimId && moby->AnimSeqT >= attackAnimHitStart && moby->AnimSeqT < attackAnimHitEnd;
			u32 damageFlags = 0x00081801;

      if (!isInAirFromFlinching) {
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed);
        }

        mobStand(moby);
      }

			if (swingAttackReady && damageFlags) {
				leviathanDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
			}
			break;
		}
    case LEVIATHAN_ACTION_ATTACK_LASER:
    case LEVIATHAN_ACTION_ATTACK_LASER_LOCKON:
    {
      int nextAnimId = moby->AnimSeqId;
      int isLockOn = pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER_LOCKON;
      
      switch (moby->AnimSeqId)
      {
        case LEVIATHAN_ANIM_LASER_FIRE_BEGIN:
        {
          if (pvars->MobVars.AnimationLooped) {
            
            // set initial direction to tail forward
            MATRIX mtxTailHead;
            mobyGetJointMatrix(moby, LEVIATHAN_SUBSKELETON_JOINT_TAIL_HEAD, mtxTailHead);
            //vector_copy(leviathanVars->LaserbeamDirection, &mtxTailHead[8]);
            vector_subtract(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamTarget1, &mtxTailHead[12]);
            leviathanVars->LaserAtTicks = pvars->MobVars.CurrentActionForTicks;

            nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_ACTIVE;
          }
          break;
        }
        case LEVIATHAN_ANIM_LASER_FIRE_ACTIVE:
        case LEVIATHAN_ANIM_LASER_FIRE_ACTIVE_WALK:
        {
          float damage = pvars->MobVars.Config.Damage;
          int laserForTicks = pvars->MobVars.CurrentActionForTicks-leviathanVars->LaserAtTicks;

          // save player from instant hit on laser start (annoying)
          if (laserForTicks < 10) damage = 0;

          // get or create laserbeam
          if (!laserbeamMoby) {
            laserbeamMoby = leviathanVars->LaserbeamMoby = laserbeamCreate(moby);
          }

          // get fire from position
          MATRIX mtxTailHead;
          mobyGetJointMatrix(moby, LEVIATHAN_SUBSKELETON_JOINT_TAIL_HEAD, mtxTailHead);

          // move direction towards target
          VECTOR targetPos, idealDir, dt;

          if (isLockOn && target) {
            mobGetTargetCenter(target, targetPos);
          } else {
            float t = (-cosf(laserForTicks / (float)LEVIATHAN_LASER_FIRE_CYCLE_TICKS) + 1) / 2;
            vector_lerp(targetPos, leviathanVars->LaserbeamTarget1, leviathanVars->LaserbeamTarget2, t);
          }

          //mobGetTargetCenter(target, targetPos);
          vector_subtract(idealDir, targetPos, &mtxTailHead[12]);
          vector_subtract(dt, idealDir, leviathanVars->LaserbeamDirection);
          
          vector_normalize(dt, dt);
          vector_scale(dt, dt, MATH_DT * 5);
          vector_add(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamDirection, dt);
          
          //vector_lerp(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamDirection, idealDir, MATH_DT * 0.2);
          vector_normalize(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamDirection);

          // stop if target is behind moby
          if (fabsf(acosf(vector_innerproduct(idealDir, moby->M0_03))) > LEVIATHAN_LASER_MAX_ANGLE) {
            nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED;
          }

          // update laserbeam
          if (laserbeamMoby) {
            laserbeamMoby->State = LASERBEAM_STATE_ACTIVATED;
            laserbeamSet(laserbeamMoby, &mtxTailHead[12], leviathanVars->LaserbeamDirection, 100, 0.3, damage, 0x1, 0x80208040, 0x3020FF20, 0x00ff00, 0x00ff00, 0x45, 0x0E);
          }

          // check for laser hit target
          if (mobAmIOwner(moby) && laserbeamMoby) {
	          struct LaserbeamPVar* pvars = (struct LaserbeamPVar*)laserbeamMoby->PVar;
            if (pvars->Hit && pvars->HitMoby == target && pvars->Damage > 0) {
              leviathanForceLocalAction(moby, LEVIATHAN_ACTION_WALK);
            }
          }

          // stop after n seconds
          if (laserForTicks > leviathanGetLaserForTicks(moby)) {
            nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED;
          }
          break;
        }
        case LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED:
        {
          break;
        }
      }

      // first animation is begin
      if (!pvars->MobVars.CurrentActionForTicks) {
        nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_BEGIN;
      }

      if (!isInAirFromFlinching) {
        mobStand(moby);
      }

			mobTransAnim(moby, nextAnimId, 0);
      break;
    }
  }

  //DPRINTF("(%d) %d:%f\n", pvars->MobVars.Action, moby->AnimSeqId, moby->AnimSeqT);

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void leviathanDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, LEVIATHAN_SUBSKELETON_JOINT_TAIL_HEAD, 1, 0);
}

//--------------------------------------------------------------------------
void leviathanForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  LeviathanMobVars_t* leviathanVars = (LeviathanMobVars_t*)pvars->AdditionalMobVarsPtr;
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
		case LEVIATHAN_ACTION_SPAWN:
		{
			// enable collision
			moby->CollActive = 0;
			break;
		}
    case LEVIATHAN_ACTION_DIE:
    {
      // can't undie
      return;
    }
    case LEVIATHAN_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
		case LEVIATHAN_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case LEVIATHAN_ACTION_ROAM:
    {
      // if we're in a spawner
      // then let it determine where we roam
      if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
        spawnerOnChildGetRandomRoamTarget(moby->PParent, moby, pvars->MobVars.MoveVars.TargetPosition);
      }
      break;
    }
		case LEVIATHAN_ACTION_WALK:
		{
			
			break;
		}
		case LEVIATHAN_ACTION_DIE:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case LEVIATHAN_ACTION_ATTACK_LASER:
    case LEVIATHAN_ACTION_ATTACK_LASER_LOCKON:
    case LEVIATHAN_ACTION_ATTACK_PROJECTILE:
		case LEVIATHAN_ACTION_ATTACK_SWING:
		case LEVIATHAN_ACTION_ATTACK_STAB:
		{
      VECTOR offset, dir, ndir, targetPos;
      if (pvars->MobVars.MoveVars.Target) {
        mobGetTargetCenter(pvars->MobVars.MoveVars.Target, targetPos);
      } else {
        vector_scale(dir, moby->M0_03, 25);
        vector_add(targetPos, moby->Position, dir);
      }
      vector_subtract(dir, targetPos, moby->Position);
      float dist = vector_length(dir);
      vector_scale(ndir, dir, 1 / dist);
      vector_outerproduct(offset, moby->M2_03, ndir);
      vector_scale(offset, offset, sinf(30 * MATH_DEG2RAD) * dist);
      vector_add(leviathanVars->LaserbeamTarget1, targetPos, offset);
      vector_subtract(leviathanVars->LaserbeamTarget2, targetPos, offset);
      leviathanVars->AttackLaserCooldownTicks = randRangeInt(LEVIATHAN_LASER_COOLDOWN_TICKS_MIN, LEVIATHAN_LASER_COOLDOWN_TICKS_MAX);
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
			break;
		}
		case LEVIATHAN_ACTION_FLINCH:
		case LEVIATHAN_ACTION_BIG_FLINCH:
		{
			pvars->MobVars.FlinchCooldownTicks = LEVIATHAN_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = LEVIATHAN_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short leviathanGetArmor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return pvars->MobVars.Config.Bangles;
}

//--------------------------------------------------------------------------
int leviathanIsExhausted(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int exhaustedLoopCount = LEVIATHAN_LASER_EXHAUSTED_ANIM_LOOP;
  return moby->AnimSeqId == LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED && pvars->MobVars.AnimationLooped < exhaustedLoopCount;
}

//--------------------------------------------------------------------------
int leviathanIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  switch (pvars->MobVars.Action) {
    case LEVIATHAN_ACTION_ATTACK_LASER:
    case LEVIATHAN_ACTION_ATTACK_LASER_LOCKON:
      // stop after fire exhausted
      return moby->AnimSeqId != LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED || leviathanIsExhausted(moby);
    case LEVIATHAN_ACTION_ATTACK_SWING:
    case LEVIATHAN_ACTION_ATTACK_STAB:
      return !pvars->MobVars.AnimationLooped;
    default:
      return 0;
  }
}

//--------------------------------------------------------------------------
int leviathanCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == LEVIATHAN_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int leviathanShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming/laser/chase states
  if (action == LEVIATHAN_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == LEVIATHAN_ACTION_ROAM || action == LEVIATHAN_ACTION_ROAM) return 1;
  if (pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER || action == LEVIATHAN_ACTION_ATTACK_LASER) return 1;
  if (pvars->MobVars.Action == LEVIATHAN_ACTION_ATTACK_LASER_LOCKON || action == LEVIATHAN_ACTION_ATTACK_LASER_LOCKON) return 1;
  if (pvars->MobVars.Action == LEVIATHAN_ACTION_CHASE || action == LEVIATHAN_ACTION_CHASE) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int leviathanIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == LEVIATHAN_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int leviathanIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == LEVIATHAN_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int leviathanIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == LEVIATHAN_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int leviathanCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int leviathanIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == LEVIATHAN_ANIM_FLINCH || moby->AnimSeqId == LEVIATHAN_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int leviathanIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == LEVIATHAN_ACTION_DIE;
}

//--------------------------------------------------------------------------
int leviathanShouldStrafe(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.MoveVars.Target;
  int behavior = pvars->MobVars.Behavior;

  if (!target) return 0;

  // get distance to target
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float sqrDistToTarget = vector_sqrmag(dt);
  float maxDistSqr = pvars->MobVars.Config.RangedMaxDistanceToTarget*pvars->MobVars.Config.RangedMaxDistanceToTarget;
  if (sqrDistToTarget > maxDistSqr)
    return 0;

  int strafe = 0;
  if (behavior == LEVIATHAN_BEHAVIOR_NORMAL) {
    if (pvars->MobVars.TimeTargetOutOfSightTicks < LEVIATHAN_EVADE_MAX_OUT_OF_SIGHT_TICKS && sqrDistToTarget < (LEVIATHAN_CHASE_TARGET_RADIUS*LEVIATHAN_CHASE_TARGET_RADIUS)) {
      strafe = 1;
    }
  } else if (behavior == LEVIATHAN_BEHAVIOR_EVASIVE) {
    strafe = pvars->MobVars.TimeTargetOutOfSightTicks < LEVIATHAN_EVADE_MAX_OUT_OF_SIGHT_TICKS;
  }

  // if mob is ready to attack
  // and we're not already strafing (if we are timeout at 15 seconds)
  // or if target is looking away, rush at them
  if (behavior != LEVIATHAN_BEHAVIOR_EVASIVE && pvars->MobVars.AttackCooldownTicks <= 10 && (pvars->MobVars.Action != LEVIATHAN_ACTION_STRAFE || pvars->MobVars.CurrentActionForTicks > (15*TPS) || vector_innerproduct_unscaled(moby->M0_03, target->M0_03) >= 0)) {
    strafe = 0;
  }

  // if stuck, return to walk state
  if (pvars->MobVars.MoveVars.IsStuck) {
    strafe = 0;
  }

  return strafe;
}

//--------------------------------------------------------------------------
int leviathanShouldChase(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.MoveVars.Target;
  int behavior = pvars->MobVars.Behavior;
  int chase = pvars->MobVars.Action == LEVIATHAN_ACTION_CHASE;

  if (!target) return 0;
  if (behavior == LEVIATHAN_BEHAVIOR_EVASIVE) return 0;
  if (!chase && rand(101)) return 0;

  // get distance to target
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float sqrDistToTarget = vector_sqrmag(dt);

  // if mob is ready to attack
  // and we're not already strafing (if we are timeout at 15 seconds)
  // or if target is looking away, rush at them
  if (pvars->MobVars.AttackCooldownTicks <= 10 && (pvars->MobVars.Action != LEVIATHAN_ACTION_CHASE || pvars->MobVars.CurrentActionForTicks > (15*TPS))) {
    chase = 0;
  }

  if (pvars->MobVars.TimeTargetOutOfSightTicks < LEVIATHAN_CHASE_MAX_OUT_OF_SIGHT_TICKS && sqrDistToTarget > (LEVIATHAN_CHASE_TARGET_RADIUS*LEVIATHAN_CHASE_TARGET_RADIUS)) {
    chase = 1;
  }

  // if stuck, return to walk state
  if (pvars->MobVars.MoveVars.IsStuck) {
    chase = 0;
  }

  return chase;
}

//--------------------------------------------------------------------------
int leviathanGetLaserForTicks(Moby* moby)
{
  return LEVIATHAN_LASER_FIRE_FOR_TICKS;
}
