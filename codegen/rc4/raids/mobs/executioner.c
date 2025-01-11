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

void executionerPreUpdate(Moby* moby);
void executionerPostUpdate(Moby* moby);
void executionerPostDraw(Moby* moby);
void executionerMove(Moby* moby);
void executionerOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void executionerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void executionerOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int executionerOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void executionerOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
enum ExecutionerAction executionerGetPreferredAttack(Moby* moby);
int executionerGetPreferredAction(Moby* moby, int * delayTicks);
void executionerDoAction(Moby* moby);
void executionerDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void executionerForceLocalAction(Moby* moby, int action);
short executionerGetArmor(Moby* moby);
int executionerIsAttacking(Moby* moby);
int executionerCanNonOwnerTransitionToAction(Moby* moby, int action);
int executionerShouldForceStateUpdateOnAction(Moby* moby, int action);

int executionerIsSpawning(struct MobPVar* pvars);
int executionerIsRoaming(struct MobPVar* pvars);
int executionerIsIdling(struct MobPVar* pvars);
int executionerCanAttack(struct MobPVar* pvars);
int executionerIsFlinching(Moby* moby);
int executionerIsDying(Moby* moby);

struct MobVTable ExecutionerVTable = {
  .PreUpdate = &executionerPreUpdate,
  .PostUpdate = &executionerPostUpdate,
  .PostDraw = &executionerPostDraw,
  .Move = &executionerMove,
  .OnSpawn = &executionerOnSpawn,
  .OnDestroy = &executionerOnDestroy,
  .OnDamage = &executionerOnDamage,
  .OnLocalDamage = &executionerOnLocalDamage,
  .OnStateUpdate = &executionerOnStateUpdate,
  .GetNextTarget = &mobGetNextTarget,
  .GetPreferredAction = &executionerGetPreferredAction,
  .ForceLocalAction = &executionerForceLocalAction,
  .DoAction = &executionerDoAction,
  .DoDamage = &executionerDoDamage,
  .GetArmor = &executionerGetArmor,
  .IsAttacking = &executionerIsAttacking,
  .CanNonOwnerTransitionToAction = &executionerCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &executionerShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
ExecutionerMobVars_t* executionerGetExtraVars(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return (ExecutionerMobVars_t*)pvars->AdditionalMobVarsPtr;
}

//--------------------------------------------------------------------------
int executionerCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(ExecutionerMobVars_t), &guberEvent, NULL);
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
void executionerPreUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // decrement path target pos ticker
  decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void executionerPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = pvars->MobVars.Config.Scale;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(EXECUTIONER_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(EXECUTIONER_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = EXECUTIONER_PRIMARY_COLOR;
  }

  // adjust animSpeed by speed and by animation
  float baseSpeed = 0.7;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;
  if (moby->AnimSeqId == EXECUTIONER_ANIM_JUMP) {
    animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / 35, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = baseSpeed;
    }
  } else if (executionerIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (executionerIsDying(moby)) {
    animSpeed = baseSpeed;
  } else if (moby->AnimSeqId == EXECUTIONER_ANIM_WALK) {
    animSpeed *= mobGetCurrentMoveSpeed(moby);
  }

	if ((moby->DrawDist == 0 && !executionerIsAttacking(moby) && !executionerIsSpawning(pvars) && !executionerIsDying(moby) && !executionerIsFlinching(moby))) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void executionerPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = EXECUTIONER_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, EXECUTIONER_SUBSKELETON_JOINT_CHEST);
}

//--------------------------------------------------------------------------
void executionerMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void executionerOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.35 * scale;

  // colors by mob type
	moby->GlowRGBA = EXECUTIONER_GLOW_COLOR;
	moby->PrimaryColor = EXECUTIONER_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 1.5 + (scale * 0.5);
  
#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void executionerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = EXECUTIONER_PRIMARY_COLOR;
  
  // spawn corn
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void executionerOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != EXECUTIONER_ACTION_FLINCH 
            && pvars->MobVars.Action != EXECUTIONER_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    executionerForceLocalAction(moby, EXECUTIONER_ACTION_DIE);
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
    float powerFactor = EXECUTIONER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * EXECUTIONER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, EXECUTIONER_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, EXECUTIONER_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, EXECUTIONER_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, EXECUTIONER_ACTION_FLINCH);
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
int executionerOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void executionerOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
int executionerIsTargetOutOfRange(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  Moby* target = pvars->MobVars.MoveVars.Target;
  if (!target) return 0;

  // check if target is within range
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  dt[2] = 0;
  float distSqr = vector_sqrmag(dt);
  float rangedAttackRadiusSqr = pvars->MobVars.Config.RangedMaxDistanceToTarget*pvars->MobVars.Config.RangedMaxDistanceToTarget;
  return distSqr > rangedAttackRadiusSqr;
}

//--------------------------------------------------------------------------
enum ExecutionerAction executionerGetPreferredAttack(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  Moby* target = pvars->MobVars.MoveVars.Target;
  int behavior = pvars->MobVars.Behavior;
  if (!target)
    return -1;
  
  if (pvars->MobVars.TimeTargetOutOfSightTicks > TPS)
    return -1;

  // check if target is within range
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float distSqr = vector_sqrmag(dt);
  float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;
  if (distSqr > attackRadiusSqr) {

    // check if in range
    if (!executionerIsTargetOutOfRange(moby)) {
      dt[2] = 0;
      float theta = acosf(vector_innerproduct(dt, moby->M0_03));
      if (fabsf(theta) < (30 * MATH_DEG2RAD))
        return EXECUTIONER_ACTION_FIRE;
    }

    return -1;
  }

  // default to swing
	return EXECUTIONER_ACTION_ATTACK;
}

//--------------------------------------------------------------------------
int executionerGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	VECTOR t;

	// no preferred action
	if (executionerIsAttacking(moby))
		return -1;

	if (executionerIsSpawning(pvars))
		return -1;

  if (executionerIsFlinching(moby))
    return -1;

  if (pvars->MobVars.Action == EXECUTIONER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == EXECUTIONER_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return EXECUTIONER_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > EXECUTIONER_MAX_WALKABLE_SLOPE) {
    return EXECUTIONER_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return EXECUTIONER_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = mobGetNextTarget(moby);
	if (target) {
    if (executionerCanAttack(pvars)) {
      int preferredAttack = executionerGetPreferredAttack(moby);
      if (preferredAttack >= 0) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
        return preferredAttack;
      }
		}
    
    if (!executionerIsTargetOutOfRange(moby) && pvars->MobVars.TimeTargetOutOfSightTicks < TPS)
      return EXECUTIONER_ACTION_LOOK_AT_TARGET;

    return EXECUTIONER_ACTION_WALK;
	}

  // if roaming, then we want to periodically stop or reroute
  if (executionerIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return EXECUTIONER_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (executionerIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return EXECUTIONER_ACTION_IDLE;
  }
	
	return EXECUTIONER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void executionerRenderPath(Moby* moby)
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
Moby* executionerFireShot(Moby* moby, Moby* target)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int jointId = EXECUTIONER_SUBSKELETON_JOINT_STAFF_END;

  VECTOR from, to={0,0,1,0}, dir, vel, offset;
  MATRIX m;
  mobyGetJointMatrix(moby, jointId, m);
  vector_copy(from, &m[12]);
  vector_copy(vel, &m[0]);
  
  // move shot from forward
  //vector_scale(offset, &m[0], 0.15);
  //vector_add(from, from, offset);

  if (target) {

    // determine if we should shoot directly towards target
    vector_subtract(dir, target->Position, moby->Position);
    vector_normalize(dir, dir);
    VECTOR planarForward;
    vector_projectonplane(planarForward, dir, moby->M2_03);
    float angle = acosf(vector_innerproduct(planarForward, moby->M0_03));
    if (angle < (1*MATH_DEG2RAD)) {
      mobGetTargetCenter(target, to);
      vector_subtract(vel, to, from);
      vector_normalize(vel, vel);
    } else {
      vector_subtract(dir, dir, planarForward);
      vector_projectonplane(vel, vel, moby->M2_03);
      vector_normalize(vel, vel);
      vector_add(vel, vel, dir);
    }
  }

  vector_scale(vel, vel, 0.5);

  // fire shot
  Moby* shotMoby = ((Moby* (*)(float, float, VECTOR, VECTOR, Moby*, int, int, int, int))0x0045d598)(4, pvars->MobVars.Config.Damage, from, vel, moby, 1, 0x222124, -1, 0);
  if (shotMoby) {
    ((void (*)(Moby*, int))0x0045d758)(shotMoby, 2); // shot type
    ((void (*)(Moby*, int))0x0045d788)(shotMoby, TEAM_RED); // shot color
    ((void (*)(Moby*, int))0x0045d7A8)(shotMoby, 1); // hit flag
    ((void (*)(Moby*, int))0x0045d798)(shotMoby, 2*TPS + (int)(pvars->MobVars.Config.VisionRange)); // shot life (ticks)
    shotMoby->Bolts = -1; // indicate to gamemode shot can damage player
    shotMoby->PParent = moby;
  }

  // spawn flare
  //((void (*)(float, float, float, Moby*, int, int, int))0x0042c178)(0.75, 0.75, 1.0, moby, 0, 0, jointId);
  
  // play sound
  //mobyPlaySoundByClass(1, 0, moby, MOBY_ID_LANDSTALKER);
}

//--------------------------------------------------------------------------
void executionerDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
  ExecutionerMobVars_t* executionerVars = executionerGetExtraVars(moby);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER_TURN_RADIANS_PER_SEC : EXECUTIONER_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER_MOVE_ACCELERATION : EXECUTIONER_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == EXECUTIONER_ACTION_FLINCH || pvars->MobVars.LastAction == EXECUTIONER_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&executionerRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
		case EXECUTIONER_ACTION_SPAWN:
		{
      mobTransAnim(moby, EXECUTIONER_ANIM_SPAWN, 0);
      mobStand(moby);
			break;
		}
		case EXECUTIONER_ACTION_FLINCH:
		case EXECUTIONER_ACTION_BIG_FLINCH:
		{
      int animFlinchId = pvars->MobVars.Action == EXECUTIONER_ACTION_BIG_FLINCH ? EXECUTIONER_ANIM_BIG_FLINCH : EXECUTIONER_ANIM_BIG_FLINCH;

      mobTransAnim(moby, animFlinchId, 0);
      
			if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == EXECUTIONER_ACTION_BIG_FLINCH) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, EXECUTIONER_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case EXECUTIONER_ACTION_IDLE:
		{
      if (pvars->MobVars.AnimationLooped || (moby->AnimSeqId != EXECUTIONER_ANIM_IDLE)) {
			  mobTransAnim(moby, EXECUTIONER_ANIM_IDLE, 0);
      } else {
        mobTransAnim(moby, moby->AnimSeqId, 0);
      }
      mobStand(moby);
      //mobResetSoundTrigger(moby);
			break;
		}
		case EXECUTIONER_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    mobTransAnim(moby, EXECUTIONER_ANIM_JUMP, 5);
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
		case EXECUTIONER_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      mobTransAnimLerp(moby, EXECUTIONER_ANIM_IDLE, 20, 0, &pvars->MobVars.AnimationReset, &pvars->MobVars.AnimationLooped);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case EXECUTIONER_ACTION_ROAM:
    {
      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, 0);
      }

			// 
      if (moby->AnimSeqId == EXECUTIONER_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        executionerForceLocalAction(moby, EXECUTIONER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, EXECUTIONER_ANIM_WALK, 0);
      } else if (moby->AnimSeqId != EXECUTIONER_ANIM_WALK || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, EXECUTIONER_ANIM_IDLE, 0);
      }
      break;
    }
    case EXECUTIONER_ACTION_WALK:
		{
      float dir = 0;
      if (target) {
        dir = (pvars->MobVars.DynamicRandom % 3) - 1;
      }

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
      }

			// 
      if (moby->AnimSeqId == EXECUTIONER_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        executionerForceLocalAction(moby, EXECUTIONER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, EXECUTIONER_ANIM_RUN, 0);
      } else if (moby->AnimSeqId != EXECUTIONER_ANIM_RUN || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, EXECUTIONER_ANIM_IDLE, 0);
      }
			break;
		}
    case EXECUTIONER_ACTION_DIE:
    {
      mobStand(moby);
      mobTransAnim(moby, EXECUTIONER_ANIM_FLINCH_FALL_DOWN, 0);
      
      if (moby->AnimSeqId == EXECUTIONER_ANIM_FLINCH_FALL_DOWN && moby->AnimSeqT > 7) {
        pvars->MobVars.Destroy = 1;
      }
      break;
    }
		case EXECUTIONER_ACTION_ATTACK:
		{
      int attack1AnimId = EXECUTIONER_ANIM_SWING;
			mobTransAnim(moby, attack1AnimId, 0);

			float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1, 1, 5);
			int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 11 && moby->AnimSeqT < 12;
			u32 damageFlags = 0x00081801;

      if (!isInAirFromFlinching) {
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed);
          mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, target->Position, speedMult * pvars->MobVars.Config.Speed, acceleration);
        } else {
          // stand
          mobStand(moby);
        }
      }

			if (swingAttackReady && damageFlags) {
				executionerDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage*0.5, damageFlags, 0);
			}
			break;
		}
    case EXECUTIONER_ACTION_FIRE:
    {
      int animId = EXECUTIONER_ANIM_FIRE;

      if (!isInAirFromFlinching) {
        mobStand(moby);
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed*0.5);

          if (moby->AnimSeqId == animId && moby->AnimSeqT >= 31.5 && moby->AnimSeqT < 33 && executionerVars->AnimationLoopLastFire != pvars->MobVars.AnimationLooped) {
            executionerFireShot(moby, target);
            executionerVars->AnimationLoopLastFire = pvars->MobVars.AnimationLooped;
          }
        }
      }

      mobTransAnim(moby, animId, 0);
      break;
    }
  }

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void executionerDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  MATRIX mFrom, mTo;
  VECTOR from, to;
  //mobyGetJointMatrix(moby, EXECUTIONER_SUBSKELETON_JOINT_RIGHT_HAND, mFrom);
  mobyGetJointMatrix(moby, EXECUTIONER_SUBSKELETON_JOINT_STAFF_END, mTo);
  //vector_copy(from, &mFrom[12]);
  vector_copy(to, &mTo[12]);

  mobDoSweepDamage(moby, moby, moby->Position, to, 0.25, radius, amount, damageFlags, friendlyFire, 0, 0);
}

//--------------------------------------------------------------------------
void executionerForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  ExecutionerMobVars_t* executionerVars = executionerGetExtraVars(moby);
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
		case EXECUTIONER_ACTION_SPAWN:
		{
			// enable collision
			moby->CollActive = 0;
			break;
		}
    case EXECUTIONER_ACTION_DIE:
    {
      // can't undie
      return;
    }
    case EXECUTIONER_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
		case EXECUTIONER_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case EXECUTIONER_ACTION_ROAM:
    {
      // if we're in a spawner
      // then let it determine where we roam
      if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
        spawnerOnChildGetRandomRoamTarget(moby->PParent, moby, pvars->MobVars.MoveVars.TargetPosition);
      }
      break;
    }
		case EXECUTIONER_ACTION_WALK:
		{
			
			break;
		}
		case EXECUTIONER_ACTION_DIE:
		{
      //pvars->MobVars.Destroy = 1;
			break;
		}
    case EXECUTIONER_ACTION_FIRE:
    {
      executionerVars->AnimationLoopLastFire = -1;
      
      float t = (MapConfig.State ? (MapConfig.State->DifficultyStars/(float)RAIDS_DIFFICULTY_5STAR) : 0);
      int cooldown = (int)lerpf(pvars->MobVars.Config.AttackCooldownTickCount, 2*TPS, t);
			pvars->MobVars.AttackCooldownTicks = cooldown;
      break;
    }
		case EXECUTIONER_ACTION_ATTACK:
		{
      pvars->MobVars.AttackCooldownTicks = TPS*2;
			break;
		}
		case EXECUTIONER_ACTION_FLINCH:
		case EXECUTIONER_ACTION_BIG_FLINCH:
		{
			pvars->MobVars.FlinchCooldownTicks = EXECUTIONER_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = EXECUTIONER_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short executionerGetArmor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.Health;
  int bangles = pvars->MobVars.Config.Bangles;

  if (t < 0.1)
    bangles &= !EXECUTIONER_BANGLE_BRAIN;
  if (t < 0.3)
    bangles &= ~EXECUTIONER_BANGLE_LEFT_CHEST_PLATE;
  if (t < 0.4)
    bangles &= ~EXECUTIONER_BANGLE_RIGHT_CHEST_PLATE;
  if (t < 0.6)
    bangles &= ~EXECUTIONER_BANGLE_RIGHT_COLLAR_BONE;
  if (t < 0.7)
    bangles &= ~EXECUTIONER_BANGLE_LEFT_COLLAR_BONE;
  if (t < 0.8)
    bangles &= ~EXECUTIONER_BANGLE_HELMET;

	return bangles;
}

//--------------------------------------------------------------------------
int executionerIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  switch (pvars->MobVars.Action)
  {
    case EXECUTIONER_ACTION_FIRE: return !pvars->MobVars.AnimationLooped;
    case EXECUTIONER_ACTION_ATTACK: return !pvars->MobVars.AnimationLooped;
    default: return 0;
  }
}

//--------------------------------------------------------------------------
int executionerCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == EXECUTIONER_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int executionerShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == EXECUTIONER_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == EXECUTIONER_ACTION_ROAM || action == EXECUTIONER_ACTION_ROAM) return 1;
  if (pvars->MobVars.Action == EXECUTIONER_ACTION_FIRE || action == EXECUTIONER_ACTION_FIRE) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int executionerIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == EXECUTIONER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int executionerIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == EXECUTIONER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int executionerIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == EXECUTIONER_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int executionerCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int executionerIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == EXECUTIONER_ANIM_FLINCH || moby->AnimSeqId == EXECUTIONER_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int executionerIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == EXECUTIONER_ACTION_DIE;
}
