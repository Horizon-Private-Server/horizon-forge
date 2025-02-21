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

void tremorPreUpdate(Moby* moby);
void tremorPostUpdate(Moby* moby);
void tremorPostDraw(Moby* moby);
void tremorMove(Moby* moby);
void tremorOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void tremorOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void tremorOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int tremorOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void tremorOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
int tremorGetPreferredAction(Moby* moby, int * delayTicks);
void tremorDoAction(Moby* moby);
void tremorDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void tremorForceLocalAction(Moby* moby, int action);
short tremorGetArmor(Moby* moby);
int tremorIsAttacking(Moby* moby);
int tremorCanNonOwnerTransitionToAction(Moby* moby, int action);
int tremorShouldForceStateUpdateOnAction(Moby* moby, int action);

int tremorIsSpawning(struct MobPVar* pvars);
int tremorIsRoaming(struct MobPVar* pvars);
int tremorIsIdling(struct MobPVar* pvars);
int tremorCanAttack(struct MobPVar* pvars);
int tremorIsFlinching(Moby* moby);
int tremorIsDying(Moby* moby);

struct MobVTable TremorVTable = {
  .PreUpdate = &tremorPreUpdate,
  .PostUpdate = &tremorPostUpdate,
  .PostDraw = &tremorPostDraw,
  .Move = &tremorMove,
  .OnSpawn = &tremorOnSpawn,
  .OnDestroy = &tremorOnDestroy,
  .OnDamage = &tremorOnDamage,
  .OnLocalDamage = &tremorOnLocalDamage,
  .OnStateUpdate = &tremorOnStateUpdate,
  .GetNextTarget = &mobGetNextTarget,
  .GetPreferredAction = &tremorGetPreferredAction,
  .ForceLocalAction = &tremorForceLocalAction,
  .DoAction = &tremorDoAction,
  .DoDamage = &tremorDoDamage,
  .GetArmor = &tremorGetArmor,
  .IsAttacking = &tremorIsAttacking,
  .CanNonOwnerTransitionToAction = &tremorCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &tremorShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
int tremorCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar), &guberEvent, NULL);
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
void tremorPreUpdate(Moby* moby)
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
void tremorPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = pvars->MobVars.Config.Scale;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(TREMOR_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(TREMOR_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = TREMOR_PRIMARY_COLOR;
  }

  // adjust animSpeed by speed and by animation
  float baseSpeed = 0.5;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;
  if (moby->AnimSeqId == TREMOR_ANIM_JUMP) {
    animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / 21, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = baseSpeed;
    }
  } else if (tremorIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (tremorIsDying(moby)) {
    animSpeed = 1.0;
  } else if (moby->AnimSeqId == TREMOR_ANIM_RUN || moby->AnimSeqId == TREMOR_ANIM_WALK) {
    animSpeed *= mobGetCurrentMoveSpeed(moby);
  }

	if ((moby->DrawDist == 0 && !tremorIsAttacking(moby) && !tremorIsSpawning(pvars) && !tremorIsDying(moby) && !tremorIsFlinching(moby))) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void tremorPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = TREMOR_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, TREMOR_SUBSKELETON_JOINT_RIGHT_SHOULDER); // todo
}

//--------------------------------------------------------------------------
void tremorMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void tremorOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.256339 * scale;

  // colors by mob type
	moby->GlowRGBA = TREMOR_GLOW_COLOR;
	moby->PrimaryColor = TREMOR_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 0.75 + (scale * 0.25);
  
#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void tremorOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = TREMOR_PRIMARY_COLOR;
  
  // spawn corn
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void tremorOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != TREMOR_ACTION_FLINCH 
            && pvars->MobVars.Action != TREMOR_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    tremorForceLocalAction(moby, TREMOR_ACTION_DIE);
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
    float powerFactor = TREMOR_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * TREMOR_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, TREMOR_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, TREMOR_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, TREMOR_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, TREMOR_ACTION_FLINCH);
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
int tremorOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void tremorOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
int tremorGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	VECTOR t;

	// no preferred action
	if (tremorIsAttacking(moby))
		return -1;

	if (tremorIsSpawning(pvars))
		return -1;

  if (tremorIsFlinching(moby))
    return -1;

  if (pvars->MobVars.Action == TREMOR_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == TREMOR_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return TREMOR_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (mobHitWallShouldJump(moby, TREMOR_MAX_WALKABLE_SLOPE)) {
    return TREMOR_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return TREMOR_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = mobGetNextTarget(moby);
	if (target) {
		vector_copy(t, target->Position);
		vector_subtract(t, t, moby->Position);
		float distSqr = vector_sqrmag(t);
		float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;

		if (distSqr <= attackRadiusSqr) {
			if (tremorCanAttack(pvars)) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return TREMOR_ACTION_ATTACK;
      }
			return TREMOR_ACTION_WALK;
		} else {
			return TREMOR_ACTION_WALK;
		}
	}

  // if roaming, then we want to periodically stop or reroute
  if (tremorIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return TREMOR_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (tremorIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return TREMOR_ACTION_IDLE;
  }
	
	return TREMOR_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void tremorRenderPath(Moby* moby)
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
void tremorDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.Config.TurnSpeed * (pvars->MobVars.MoveVars.Grounded ? TREMOR_TURN_RADIANS_PER_SEC : TREMOR_TURN_AIR_RADIANS_PER_SEC);
  float acceleration = pvars->MobVars.MoveVars.Grounded ? TREMOR_MOVE_ACCELERATION : TREMOR_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == TREMOR_ACTION_FLINCH || pvars->MobVars.LastAction == TREMOR_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&tremorRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
		case TREMOR_ACTION_SPAWN:
		{
      mobTransAnim(moby, TREMOR_ANIM_IDLE, 0);
      mobStand(moby);
			break;
		}
		case TREMOR_ACTION_FLINCH:
		case TREMOR_ACTION_BIG_FLINCH:
		{
      int animFlinchId = pvars->MobVars.Action == TREMOR_ACTION_BIG_FLINCH ? TREMOR_ANIM_FLINCH_FALL_GET_UP : TREMOR_ANIM_FLINCH;

      mobTransAnim(moby, animFlinchId, 0);
      
			if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == TREMOR_ACTION_BIG_FLINCH) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, TREMOR_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case TREMOR_ACTION_IDLE:
		{
      if (pvars->MobVars.AnimationLooped || (moby->AnimSeqId != TREMOR_ANIM_IDLE && moby->AnimSeqId != TREMOR_ANIM_IDLE_LOOK_AROUND)) {
			  mobTransAnim(moby, (rand(1000) != 1) ? TREMOR_ANIM_IDLE : TREMOR_ANIM_IDLE_LOOK_AROUND, 0);
      } else {
        mobTransAnim(moby, moby->AnimSeqId, 0);
      }
      mobStand(moby);
			break;
		}
		case TREMOR_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    mobTransAnim(moby, TREMOR_ANIM_JUMP, 5);

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
		case TREMOR_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case TREMOR_ACTION_ROAM:
    {
      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, 0);
      }

			// 
      if (moby->AnimSeqId == TREMOR_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        tremorForceLocalAction(moby, TREMOR_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, TREMOR_ANIM_WALK, 0);
      } else {
				mobTransAnim(moby, TREMOR_ANIM_IDLE, 0);
      }
      break;
    }
    case TREMOR_ACTION_WALK:
		{
      float dir = mobGetCurrentWalkAngle(moby);

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
      }

			// 
      if (moby->AnimSeqId == TREMOR_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        tremorForceLocalAction(moby, TREMOR_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, TREMOR_ANIM_RUN, 0);
      } else {
				mobTransAnim(moby, TREMOR_ANIM_IDLE, 0);
      }
			break;
		}
    case TREMOR_ACTION_DIE:
    {
			mobTransAnim(moby, TREMOR_ANIM_FLINCH_BACK_FLIP_FALL, 0);

      if (moby->AnimSeqId == TREMOR_ANIM_FLINCH_BACK_FLIP_FALL && moby->AnimSeqT > 15) {
        pvars->MobVars.Destroy = 1;
      }

      mobStand(moby);
      break;
    }
		case TREMOR_ACTION_ATTACK:
		{
      int attack1AnimId = TREMOR_ANIM_SWING;
      int defaultAnimId = mobHasVelocity(pvars) ? TREMOR_ANIM_RUN : TREMOR_ANIM_IDLE;
      int nextAnimId = mobGetAnimIf(moby, defaultAnimId, attack1AnimId, !pvars->MobVars.CurrentActionForTicks, 1);
			mobTransAnim(moby, nextAnimId, 0);

			float speedMult = 0;
			int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 4 && moby->AnimSeqT < 8;
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
				tremorDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
			}
			break;
		}
  }

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void tremorDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, TREMOR_SUBSKELETON_JOINT_RIGHT_HAND_CLAW, 1, 0);
}

//--------------------------------------------------------------------------
void tremorForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
		case TREMOR_ACTION_SPAWN:
		{
			// enable collision
			moby->CollActive = 0;
			break;
		}
    case TREMOR_ACTION_DIE:
    {
      // can't undie
      return;
    }
    case TREMOR_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
		case TREMOR_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case TREMOR_ACTION_ROAM:
    {
      // if we're in a spawner
      // then let it determine where we roam
      if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
        spawnerOnChildGetRandomRoamTarget(moby->PParent, moby, pvars->MobVars.MoveVars.TargetPosition);
      }
      break;
    }
		case TREMOR_ACTION_WALK:
		{
			
			break;
		}
		case TREMOR_ACTION_DIE:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
		case TREMOR_ACTION_ATTACK:
		{
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
			break;
		}
		case TREMOR_ACTION_FLINCH:
		case TREMOR_ACTION_BIG_FLINCH:
		{
			pvars->MobVars.FlinchCooldownTicks = TREMOR_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = TREMOR_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short tremorGetArmor(Moby* moby)
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
int tremorIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == TREMOR_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int tremorCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == TREMOR_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int tremorShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == TREMOR_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == TREMOR_ACTION_ROAM || action == TREMOR_ACTION_ROAM) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int tremorIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == TREMOR_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int tremorIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == TREMOR_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int tremorIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == TREMOR_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int tremorCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0 && pvars->MobVars.Action != TREMOR_ACTION_ATTACK;
}

//--------------------------------------------------------------------------
int tremorIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == TREMOR_ANIM_FLINCH || moby->AnimSeqId == TREMOR_ANIM_FLINCH_FALL_GET_UP) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int tremorIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == TREMOR_ACTION_DIE;
}
