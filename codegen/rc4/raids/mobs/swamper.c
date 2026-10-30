#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/sha1.h>
#include <libdl/radar.h>
#include <libdl/color.h>

#include "game.h"
#include "mobs/mob.h"
#include "pathfind.h"
#include "utils.h"
#include "maputils.h"
#include "spawner.h"
#include "shared.h"

void swamperResetActionCooldownTicks(Moby *moby, enum SwamperAttackActions action);
int swamperGetActionReady(Moby *moby, enum SwamperAttackActions action);
void swamperPreUpdate(Moby *moby);
void swamperPostUpdate(Moby *moby);
void swamperPostDraw(Moby *moby);
void swamperMove(Moby* moby);
void swamperOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void swamperOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void swamperOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int swamperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void swamperOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *swamperGetNextTarget(Moby *moby);
int swamperGetPreferredAction(Moby *moby, int *delayTicks);
void swamperRenderPath(Moby* moby);
void swamperDoAction(Moby *moby);
void swamperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void swamperForceLocalAction(Moby *moby, int action);
short swamperGetArmor(Moby *moby);
int swamperIsAttacking(Moby *moby);
int swamperCanNonOwnerTransitionToAction(Moby *moby, int action);
int swamperShouldForceStateUpdateOnAction(Moby *moby, int action);
int swamperIsSpawning(struct MobPVar *pvars);
int swamperCanAttack(struct MobPVar *pvars);
int swamperIsFlinching(Moby *moby);
int swamperIsDying(Moby *moby);
int swamperIsIdling(struct MobPVar* pvars);
int swamperCreate(struct MobCreateArgs* args);
int swamperIsRoaming(struct MobPVar* pvars);

struct MobVTable SwamperVTable = {
  .PreUpdate = &swamperPreUpdate,
  .PostUpdate = &swamperPostUpdate,
  .PostDraw = &swamperPostDraw,
  .Move = &swamperMove,
  .OnSpawn = &swamperOnSpawn,
  .OnDestroy = &swamperOnDestroy,
  .OnDamage = &swamperOnDamage,
  .OnLocalDamage = &swamperOnLocalDamage,
  .OnStateUpdate = &swamperOnStateUpdate,
  .GetNextTarget = &swamperGetNextTarget,
  .GetPreferredAction = &swamperGetPreferredAction,
  .ForceLocalAction = &swamperForceLocalAction,
  .DoAction = &swamperDoAction,
  .DoDamage = &swamperDoDamage,
  .GetArmor = &swamperGetArmor,
  .IsAttacking = &swamperIsAttacking,
  .CanNonOwnerTransitionToAction = &swamperCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &swamperShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------

void swamperResetActionCooldownTicks(Moby *moby, enum SwamperAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwamperMobVars_t *swamperVars = (SwamperMobVars_t *)pvars->AdditionalMobVarsPtr;
	swamperVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	swamperVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int swamperGetActionReady(Moby *moby, enum SwamperAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwamperMobVars_t *swamperVars = (SwamperMobVars_t *)pvars->AdditionalMobVarsPtr;
	return swamperVars->ActionQueuedForTicks[action] > 0;
}

//--------------------------------------------------------------------------
void swamperPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwamperMobVars_t *swamperVars = (SwamperMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);

	if (!mobIsFrozen(moby))
		mobTickActionCooldowns(moby, SWAMPER_ATTACK_ACTION_COUNT, swamperVars->ActionCooldownTicks, swamperVars->ActionQueuedForTicks);
}

//--------------------------------------------------------------------------
void swamperPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float baseSpeed = 0.9;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == SWAMPER_ANIM_JUMP)
	{
		animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / SWAMPER_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
		}
	}
	else if (swamperIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / SWAMPER_FLINCH_ANIM_AIR_DURATION, 2));
	}
	else if (swamperIsAttacking(moby))
	{
		animSpeed = baseSpeed * mobGetFloat(moby, SWAMPER_PARAM_BITE_ATTACK_SPEED_MULTIPLIER);
	}
	else if (swamperIsDying(moby))
	{
		animSpeed = baseSpeed;
	}
	else if (moby->AnimSeqId == SWAMPER_ANIM_WALK)
	{
		animSpeed *= mobGetCurrentMoveSpeed(moby);
	}

	if (pvars->MobVars.Action == SWAMPER_ACTION_DIE)
	{
		animSpeed = baseSpeed;
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == SWAMPER_ACTION_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void swamperPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 0.5, color, SWAMPER_SUBSKELETON_JOINT_JAW);
}

//--------------------------------------------------------------------------
void swamperMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void swamperOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(SwamperMobVars_t));

	float scale = mobGetScaleMultiplier(moby);

	// set scale
	moby->Scale = 0.11 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 0.5 + (scale * 0.25);
	pvars->MobVars.BlipType = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;

	// initialize action cooldowns
	int i;
	for (i = 0; i < SWAMPER_ATTACK_ACTION_COUNT; ++i)
		swamperResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void swamperOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
  
  // spawn corn
  mobBlowCorn(moby, moby->Bangles);
}

//--------------------------------------------------------------------------
void swamperOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != SWAMPER_ACTION_FLINCH 
            && pvars->MobVars.Action != SWAMPER_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    swamperForceLocalAction(moby, SWAMPER_ACTION_DIE);
	}

	// knockback
  // swampers always have knockback
  if (e->Knockback.Power < 3) e->Knockback.Power = 3;
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		memcpy(&pvars->MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
	}

  // flinch
	if (mobAmIOwner(moby))
	{
		float damageRatio = damage / pvars->MobVars.Config.Health;
    float powerFactor = SWAMPER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * SWAMPER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, SWAMPER_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, SWAMPER_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, SWAMPER_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, SWAMPER_ACTION_FLINCH);
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
int swamperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
}

//--------------------------------------------------------------------------
void swamperOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *swamperGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int swamperGetPreferredAction(Moby *moby, int *delayTicks)
{
	VECTOR t;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// no preferred action
	if (swamperIsAttacking(moby))
		return -1;

	if (swamperIsSpawning(pvars))
		return -1;

	if (swamperIsFlinching(moby))
		return -1;

  if (pvars->MobVars.Action == SWAMPER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == SWAMPER_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return SWAMPER_ACTION_WALK;
  }

	// jump if we've hit a slope and are grounded
	if (mobHitWallShouldJump(moby, SWAMPER_MAX_WALKABLE_SLOPE))
	{
		return SWAMPER_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return SWAMPER_ACTION_JUMP;
	}

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby *target = swamperGetNextTarget(moby);
	if (target)
	{
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		if (dist <= attackRadius && swamperGetActionReady(moby, SWAMPER_ATTACK_ACTION_BITE))
		{
			if (swamperCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return SWAMPER_ACTION_ATTACK;
			}
			return SWAMPER_ACTION_WALK;
		}
		else
		{
			return SWAMPER_ACTION_WALK;
		}
	}

	  // if roaming, then we want to periodically stop or reroute
  if (swamperIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return SWAMPER_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (swamperIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return SWAMPER_ACTION_IDLE;
  }
	
	return SWAMPER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void swamperRenderPath(Moby* moby)
{
  int x,y;
  int i;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* pathGraph = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
  u8* path = (u8*)pvars->MobVars.MoveVars.CurrentPath;
  int pathLen = pvars->MobVars.MoveVars.PathEdgeCount;
  int pathIdx = pvars->MobVars.MoveVars.PathEdgeCurrent;

  if (pathLen > 0) {
    u8* edge = pathGraph->Edges[path[0]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[0]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }
  for (i = 0; i < pathLen; ++i) {
    u8* edge = pathGraph->Edges[path[i]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[1]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }

  VECTOR t;
  if (pathGetTargetPos(pathGraph, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
    pvars->MobVars.Dirty = 1;
  if (gfxWorldSpaceToScreenSpace(t, &x, &y)) {
    gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "+", -1, 4);
  }
}
#endif

//--------------------------------------------------------------------------
void swamperDoAction(Moby *moby)
{
  struct PathGraph* path = pathGetMobyPathGraph(moby, &((struct MobPVar*)moby->PVar)->MobVars.MoveVars);
  float speed = ((struct MobPVar*)moby->PVar)->MobVars.Config.Speed;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? SWAMPER_TURN_RADIANS_PER_SEC : SWAMPER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? SWAMPER_MOVE_ACCELERATION : SWAMPER_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == SWAMPER_ACTION_FLINCH || pvars->MobVars.LastAction == SWAMPER_ACTION_BIG_FLINCH);

	turnSpeed *= pvars->MobVars.Config.TurnSpeed;

#if DEBUGPATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&swamperRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
case SWAMPER_ACTION_ATTACK:
	{
		int attack1AnimId = SWAMPER_ANIM_BITE;
		mobTransAnim(moby, attack1AnimId, 0);

		float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, SWAMPER_PARAM_BITE_DAMAGE_MULTIPLIER);
		float lungeMult = mobGetFloat(moby, SWAMPER_PARAM_BITE_LUNGE_MULTIPLIER);

		float speed = mobGetLungeSpeed(moby, attack1AnimId, SWAMPER_BITE_LUNGE_FRAME_START, SWAMPER_BITE_LUNGE_FRAME_END, lungeMult);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= SWAMPER_BITE_ATTACK_HIT_FRAME_START && moby->AnimSeqT < SWAMPER_BITE_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (!isInAirFromFlinching)
		{
			if (target && speed > 0)
			{
				mobTurnTowardsPredictiveWithSpeed(moby, target, SWAMPER_TURN_LUNGE_RADIANS_PER_SEC, speed);
				mobMoveTowards(moby, target->Position, speed, 0, acceleration, 0);
			}
			else
			{
				// stand
				mobStand(moby);
			}
		}

		if (swingAttackReady && damageFlags)
		{
			swamperDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
		}
		break;
	}
	case SWAMPER_ACTION_SPAWN:
		{
      mobTransAnim(moby, SWAMPER_ANIM_ROAR, 0);
      //mobStand(moby);
			break;
		}
		case SWAMPER_ACTION_FLINCH:
		case SWAMPER_ACTION_BIG_FLINCH:
		{
      decTimerU8(&pvars->MobVars.Knockback.Ticks);
      int animFlinchId = pvars->MobVars.Action == SWAMPER_ACTION_BIG_FLINCH ? SWAMPER_ANIM_FALL_BACKWARDS : SWAMPER_ANIM_JUMP_BACKWARDS;

      mobTransAnim(moby, animFlinchId, 0);

      if (pvars->MobVars.Knockback.Ticks > 0) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, SWAMPER_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
      } else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case SWAMPER_ACTION_IDLE:
		{
			mobTransAnim(moby, SWAMPER_ANIM_IDLE, 0);
      mobStand(moby);
			break;
		}
		case SWAMPER_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    mobTransAnim(moby, SWAMPER_ANIM_JUMP, 5);

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

          //DPRINTF("jump %f\n", jumpSpeed);
          vector_write(pvars->MobVars.MoveVars.Velocity, 0);
          pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
          pvars->MobVars.MoveVars.Grounded = 0;
          pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
          pvars->MobVars.MoveVars.JumpedThisAction = 1;
          mobResetMoveStep(moby);
        }
				break;
			}
		case SWAMPER_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case SWAMPER_ACTION_ROAM:
    case SWAMPER_ACTION_WALK:
		{
      int walkAnim = SWAMPER_ANIM_WALK;
      float dir = mobGetCurrentWalkAngle(moby);
      if (target) {
        walkAnim = SWAMPER_ANIM_RUN;
      } else {
        speed *= 0.5;
      }

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
      }

      
			// 
      if (moby->AnimSeqId == SWAMPER_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        swamperForceLocalAction(moby, SWAMPER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, walkAnim, 0);
      } else {
				mobTransAnim(moby, SWAMPER_ANIM_IDLE, 0);
      }
			break;
		}
    case SWAMPER_ACTION_DIE:
    {
      // on destroy, give some upwards velocity for the backflip
      if (moby->AnimSeqId != SWAMPER_ANIM_FALL_BACKWARDS) {
        vector_fromyaw(t, pvars->MobVars.Knockback.Angle / 1000.0);
        t[2] = 1.0;
        vector_scale(t, t, 4 * 2 * MATH_DT);
        vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
      }

      mobTransAnimLerp(moby, SWAMPER_ANIM_FALL_BACKWARDS, 5, 0, &pvars->MobVars.AnimationReset, &pvars->MobVars.AnimationLooped);
      if (moby->AnimSeqId == SWAMPER_ANIM_FALL_BACKWARDS && moby->AnimSeqT > 25) {
        pvars->MobVars.Destroy = 1;
      }

      //mobStand(moby);
      break;
    }
		}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void swamperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, SWAMPER_SUBSKELETON_JOINT_JAW, 1, 0);
}

//--------------------------------------------------------------------------
void swamperForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int stateCooldownTicks = SWAMPER_ACTION_COOLDOWN_TICKS;

	// from
	switch (pvars->MobVars.Action)
	{
	case SWAMPER_ACTION_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case SWAMPER_ACTION_DIE:
	{
		// can't undie
		return;
	}
	case SWAMPER_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
	case SWAMPER_ACTION_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		stateCooldownTicks = 0;
		break;
	}
	case SWAMPER_ACTION_WALK:
	{

		break;
	}
	case SWAMPER_ACTION_DIE:
	{
		pvars->MobVars.Destroy = 1;
		break;
	}
	case SWAMPER_ACTION_ATTACK:
	{
		swamperResetActionCooldownTicks(moby, SWAMPER_ATTACK_ACTION_BITE);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		mobResetMoveStep(moby); // force move step reset for accurate lunge
		stateCooldownTicks = 0;
		break;
	}
	case SWAMPER_ACTION_FLINCH:
	case SWAMPER_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = SWAMPER_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
		break;
	}
	case SWAMPER_ACTION_ROAM:
    {
      // if we're in a spawner
      // then let it determine where we roam
      if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
        spawnerOnChildGetRandomRoamTarget(moby->PParent, moby, pvars->MobVars.MoveVars.TargetPosition);
      }
      break;
    }
		}

	//
	if (action != pvars->MobVars.Action)
		pvars->MobVars.CurrentActionForTicks = 0;

	pvars->MobVars.Action = action;
	pvars->MobVars.NextAction = -1;
	pvars->MobVars.ActionCooldownTicks = stateCooldownTicks;
}

//--------------------------------------------------------------------------
short swamperGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;
	int bangles = pvars->MobVars.Config.Bangles;

	if (t < MOB_ARMOR_THRESHOLD_LOW)
		return 0; // remove armor bangle
	else if (t < MOB_ARMOR_THRESHOLD_HIGH)
		return bangles;

	return bangles;
}

//--------------------------------------------------------------------------
int swamperIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == SWAMPER_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swamperCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// always let non-owners simulate an action unless its the death action
	if (action == SWAMPER_ACTION_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int swamperShouldForceStateUpdateOnAction(Moby *moby, int action)
{
  if (((struct MobPVar*)moby->PVar)->MobVars.Action == SWAMPER_ACTION_ROAM || action == SWAMPER_ACTION_ROAM) return 1;

	// only send action updates at regular intervals, unless dying
	if (action == SWAMPER_ACTION_DIE)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int swamperIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == SWAMPER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swamperCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int swamperIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == SWAMPER_ANIM_FALL_BACKWARDS || moby->AnimSeqId == SWAMPER_ANIM_JUMP_BACKWARDS) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swamperIsDying(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == SWAMPER_ACTION_DIE;
}

//--------------------------------------------------------------------------
int swamperIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWAMPER_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int swamperCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(SwamperMobVars_t), &guberEvent, NULL);
	if (guberEvent)
	{
    if (MapConfig.Functions.ModePopulateSpawnArgsFunc) {
      MapConfig.Functions.ModePopulateSpawnArgsFunc(&spawnArgs, args);
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
int swamperIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWAMPER_ACTION_ROAM;
}
