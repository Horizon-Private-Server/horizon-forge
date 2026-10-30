#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include <libdl/color.h>

#include "game.h"
#include "mobs/mob.h"
#include "utils.h"
#include "maputils.h"
#include "pathfind.h"
#include "spawner.h"
#include "shared.h"

Moby *tremorGetQuakeMoby(Moby *moby);
void tremorResetActionCooldownTicks(Moby *moby, enum TremorAttackActions action);
int tremorGetActionReady(Moby *moby, enum TremorAttackActions action);
void tremorPreUpdate(Moby *moby);
void tremorPostUpdate(Moby *moby);
void tremorPostDraw(Moby *moby);
void tremorAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount);
void tremorMove(Moby* moby);
void tremorOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void tremorOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void tremorOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int tremorOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void tremorOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *tremorGetNextTarget(Moby *moby);
int tremorGetPreferredAction(Moby *moby, int *delayTicks);
void tremorDoAction(Moby *moby);
void tremorQuakeMobyUpdate(Moby *moby);
void tremorSpawnQuake(Moby *moby, float speed, int jointIdx);
void tremorDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void tremorForceLocalAction(Moby *moby, int action);
short tremorGetArmor(Moby *moby);
int tremorIsAttacking(Moby *moby);
int tremorCanNonOwnerTransitionToAction(Moby *moby, int action);
int tremorShouldForceStateUpdateOnAction(Moby *moby, int action);
int tremorIsSpawning(struct MobPVar *pvars);
int tremorCanAttack(struct MobPVar *pvars);
int tremorIsFlinching(Moby *moby);
int tremorIsIdling(struct MobPVar* pvars);
int tremorIsRoaming(struct MobPVar* pvars);
void tremorRenderPath(Moby* moby);
int tremorCreate(struct MobCreateArgs* args);

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
  .GetNextTarget = &tremorGetNextTarget,
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

Moby *tremorGetQuakeMoby(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	TremorMobVars_t *tremorVars = (TremorMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *quakeMoby = tremorVars->QuakeMoby;

	if (!quakeMoby)
		return NULL;

	if (mobyIsDestroyed(quakeMoby))
		return NULL;

	if (quakeMoby->State == 1)
		return NULL;

	if (quakeMoby->OClass != 0x210B)
		return NULL;

	struct TremorQuakePVars *quakePvars = (struct TremorQuakePVars *)quakeMoby->PVar;
	if (!quakePvars)
		return NULL;

	if (quakePvars->CreatorMob != moby)
		return NULL;

	return quakeMoby;
}

//--------------------------------------------------------------------------
void tremorResetActionCooldownTicks(Moby *moby, enum TremorAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	TremorMobVars_t *tremorVars = (TremorMobVars_t *)pvars->AdditionalMobVarsPtr;
	tremorVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	tremorVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int tremorGetActionReady(Moby *moby, enum TremorAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	TremorMobVars_t *tremorVars = (TremorMobVars_t *)pvars->AdditionalMobVarsPtr;
	return tremorVars->ActionQueuedForTicks[action] > 0;
}

//--------------------------------------------------------------------------
void tremorPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	TremorMobVars_t *tremorVars = (TremorMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);

	if (!mobIsFrozen(moby))
		mobTickActionCooldowns(moby, TREMOR_ATTACK_ACTION_COUNT, tremorVars->ActionCooldownTicks, tremorVars->ActionQueuedForTicks);
}

//--------------------------------------------------------------------------
void tremorPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float baseSpeed = 0.5;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == TREMOR_ANIM_JUMP)
	{
		animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / TREMOR_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
		}
	}
	else if (tremorIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / TREMOR_FLINCH_ANIM_AIR_DURATION, 2));
	}
	else if (moby->AnimSeqId == TREMOR_ANIM_SWING)
	{
		animSpeed *= mobGetFloat(moby, TREMOR_PARAM_MELEE_ATTACK_SPEED_MULTIPLIER);
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == TREMOR_ACTION_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void tremorPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 1.25, color, 1);
}

//--------------------------------------------------------------------------
void tremorAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void tremorMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void tremorOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);
	int i;

	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(TremorMobVars_t));

	// set scale
	moby->Scale = 0.256339 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 0.5 + (scale * 0.5);
	pvars->MobVars.BlipType = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;

	// initialize action cooldowns
	for (i = 0; i < TREMOR_ATTACK_ACTION_COUNT; ++i)
		tremorResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void tremorOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
  
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
int tremorOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
}

//--------------------------------------------------------------------------
void tremorOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *tremorGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int tremorGetPreferredAction(Moby *moby, int *delayTicks)
{
	VECTOR t;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int canRanged = mobGetBehavior(moby) != TREMOR_BEHAVIOR_MELEE;
	int preferRanged = mobGetBehavior(moby) == TREMOR_BEHAVIOR_RANGED;

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
	if (mobHitWallShouldJump(moby, TREMOR_MAX_WALKABLE_SLOPE))
	{
		return TREMOR_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return TREMOR_ACTION_JUMP;
	}

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby *target = tremorGetNextTarget(moby);
	if (target)
	{
		// can't attack yet so move towards target
		if (!tremorCanAttack(pvars))
			return preferRanged ? TREMOR_ACTION_LOOK_AT_TARGET : TREMOR_ACTION_WALK;

		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;
		float rangedAttackRadius = mobGetFloat(moby, TREMOR_PARAM_GROUND_QUAKE_MAX_DISTANCE);
		int isQuakeState = pvars->MobVars.Action == TREMOR_ACTION_ATTACK_GROUND_QUAKE;

		if (dist <= attackRadius && tremorGetActionReady(moby, TREMOR_ATTACK_ACTION_MELEE))
		{
			if (delayTicks)
				*delayTicks = pvars->MobVars.Config.ReactionTickCount;
			return TREMOR_ACTION_ATTACK;
		}
		else if (canRanged && pvars->MobVars.MoveVars.Grounded && dist <= rangedAttackRadius && mobCanSeeMoby(moby, target))
		{
			VECTOR dt;
			vector_subtract(dt, target->Position, moby->Position);
			float theta = acosf(vector_innerproduct(dt, moby->M0_03));
			if (!isQuakeState && fabsf(theta) < TREMOR_QUAKE_FIRE_AT_MAX_TURN_ANGLE && tremorGetActionReady(moby, TREMOR_ATTACK_ACTION_GROUND_QUAKE))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return TREMOR_ACTION_ATTACK_GROUND_QUAKE;
			}

			return preferRanged ? TREMOR_ACTION_LOOK_AT_TARGET : TREMOR_ACTION_WALK;
		}
		else
		{
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
void tremorDoAction(Moby *moby)
{
  struct PathGraph* path = pathGetMobyPathGraph(moby, &((struct MobPVar*)moby->PVar)->MobVars.MoveVars);
  float speed = ((struct MobPVar*)moby->PVar)->MobVars.Config.Speed;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? TREMOR_TURN_RADIANS_PER_SEC : TREMOR_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? TREMOR_MOVE_ACCELERATION : TREMOR_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == TREMOR_ACTION_FLINCH || pvars->MobVars.LastAction == TREMOR_ACTION_BIG_FLINCH);
	u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

	turnSpeed *= pvars->MobVars.Config.TurnSpeed;

	switch (pvars->MobVars.Action)
	{
case TREMOR_ACTION_ATTACK:
	{
		int attack1AnimId = TREMOR_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, TREMOR_PARAM_MELEE_DAMAGE_MULTIPLIER);
		float lungeMult = mobGetFloat(moby, TREMOR_PARAM_MELEE_LUNGE_MULTIPLIER);

		float speed = mobGetLungeSpeed(moby, attack1AnimId, 0, TREMOR_ATTACK_ANIM_LUNGE_DURATION, lungeMult);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= TREMOR_ATTACK_HIT_FRAME_START && moby->AnimSeqT < TREMOR_ATTACK_HIT_FRAME_END;

		if (!isInAirFromFlinching)
		{
			if (target && speed > 0)
			{
				mobTurnTowardsPredictiveWithSpeed(moby, target, TREMOR_TURN_LUNGE_RADIANS_PER_SEC, speed);
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
			tremorDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
		}
		break;
	}
	case TREMOR_ACTION_ATTACK_GROUND_QUAKE:
	{
		int nextAnimId = moby->AnimSeqId;

		float quakeSpeed = MATH_DT * mobGetFloat(moby, TREMOR_PARAM_GROUND_QUAKE_QUAKE_SPEED);

		switch (moby->AnimSeqId)
		{
		case TREMOR_ANIM_JUMP_AND_STAB_DOWN:
		{
			// turn towards player
			if (!pvars->MobVars.AnimationLooped && moby->AnimSeqT < TREMOR_ATTACK_QUAKE_SPAWN_FRAME_START)
			{
				// randomize quake speed by 80-100%
				float randomizedSpeed = randRange(0.8, 1.0) * quakeSpeed;
				mobTurnTowardsPredictiveWithSpeed(moby, target, TREMOR_QUAKE_TURN_RADIANS_PER_SEC, randomizedSpeed);
			}

			// spawn quake when animation stabs ground
			if (!tremorGetQuakeMoby(moby) && moby->AnimSeqT >= TREMOR_ATTACK_QUAKE_SPAWN_FRAME_START)
			{
				tremorSpawnQuake(moby, quakeSpeed, TREMOR_SUBSKELETON_JOINT_RIGHT_HAND_CLAW);
			}

			// transition to stab ground repeat anim when start animation sequence completes
			if (pvars->MobVars.AnimationLooped)
			{
				nextAnimId = TREMOR_ANIM_STAB_DOWN_POSITION_REPEAT;
			}
			break;
		}
		case TREMOR_ANIM_STAB_DOWN_POSITION_REPEAT:
		{
			// stay in this action until attack ends
			int inForTicks = pvars->MobVars.CurrentActionForTicks;
			if (inForTicks > TREMOR_ANIM_ATTACK_QUAKE_TICKS)
				nextAnimId = TREMOR_ANIM_IDLE;
			else if (!tremorGetQuakeMoby(moby))
				nextAnimId = TREMOR_ANIM_IDLE;

			break;
		}
		}

		// begin animation sequence
		if (!pvars->MobVars.CurrentActionForTicks)
			nextAnimId = TREMOR_ANIM_JUMP_AND_STAB_DOWN;

		// not moving in this action
		if (!isInAirFromFlinching)
			mobStand(moby);

		mobTransAnim(moby, nextAnimId, 0);
		break;
	}
	case TREMOR_ACTION_SPAWN:
		{
      mobTransAnim(moby, TREMOR_ANIM_IDLE, 0);
      mobStand(moby);
			break;
		}
		case TREMOR_ACTION_FLINCH:
		case TREMOR_ACTION_BIG_FLINCH:
		{
      decTimerU8(&pvars->MobVars.Knockback.Ticks);
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
		}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void tremorQuakeMobyUpdate(Moby *moby)
{
	struct TremorQuakePVars *pvars = (struct TremorQuakePVars *)moby->PVar;
	if (!pvars)
		return;

	// detect when parent dies
	Moby *parentMoby = pvars->CreatorMob;
	if (!parentMoby || mobyIsDestroyed(parentMoby))
	{
		mobyDestroy(moby);
		return;
	}

	if (moby->State == 0)
	{
		// seed initial action
		if ((moby->Triggers & 1) == 0)
		{
			moby->Triggers |= 1;
			pvars->LifeTicks = TREMOR_QUAKE_DURATION_TICKS;
		}

		// backup starting position
		VECTOR lastPosition;
		vector_copy(lastPosition, moby->Position);

		// move
		((void (*)(Moby *))0x00436aa0)(moby);
		((void (*)(Moby *))0x00437298)(moby);

		// damage
		struct MobPVar *mobPvars = (struct MobPVar *)parentMoby->PVar;
		float damage = mobPvars->MobVars.Config.Damage * mobGetFloat(parentMoby, TREMOR_PARAM_GROUND_QUAKE_DAMAGE_MULTIPLIER);
		u32 damageFlags = mobGetDamageFlags(parentMoby, MOB_DAMAGE_FLAG_BASE);
		mobDoSweepDamage(parentMoby, parentMoby, lastPosition, moby->Position, TREMOR_QUAKE_HIT_RADIUS * 0.5, TREMOR_QUAKE_HIT_RADIUS, damage, damageFlags, 0, 0, 1);

		// kill when life hits 0
		pvars->LifeTicks--;
		if (pvars->LifeTicks <= 0)
			mobySetState(moby, 1, -1);
	}
	else if (moby->State == 1)
	{
		mobyDestroy(moby);
	}
}

//--------------------------------------------------------------------------
void tremorSpawnQuake(Moby *moby, float speed, int jointIdx)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	TremorMobVars_t *tremorVars = (TremorMobVars_t *)pvars->AdditionalMobVarsPtr;

	// get position to spawn quake from joint
	MATRIX jointMtx;
	VECTOR spawnAt, spawnVelocity;
	mobyGetJointMatrix(moby, jointIdx, jointMtx);
	vector_copy(spawnAt, &jointMtx[12]);

	// angle quake direction to mob forward planar direction
	vector_fromyaw(spawnVelocity, moby->Rotation[2]);
	vector_scale(spawnVelocity, spawnVelocity, speed);

	// find ground
	VECTOR groundCheckFrom = {0, 0, 2, 0};
	VECTOR groundCheckTo = {0, 0, 0, 0};
	vector_add(groundCheckFrom, groundCheckFrom, spawnAt);
	vector_add(groundCheckTo, groundCheckTo, spawnAt);
	if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL))
		vector_add(spawnAt, CollLine_Fix_GetHitPosition(), (VECTOR){0, 0, 0.1, 0});

	// spawn quake moby
	// use custom update function to drive mob damage
	Moby *spawnedQuakeMoby = tremorVars->QuakeMoby = ((Moby * (*)(Moby *, u128 pos, u128 vel))0x004368e0)(moby, vector_read(spawnAt), vector_read(spawnVelocity));
	if (spawnedQuakeMoby)
		spawnedQuakeMoby->PUpdate = &tremorQuakeMobyUpdate;
}

//--------------------------------------------------------------------------
void tremorDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, TREMOR_SUBSKELETON_JOINT_RIGHT_HAND_CLAW, 1, 0);
}

//--------------------------------------------------------------------------
void tremorForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	TremorMobVars_t *tremorVars = (TremorMobVars_t *)pvars->AdditionalMobVarsPtr;
	int stateCooldownTicks = TREMOR_ACTION_COOLDOWN_TICKS;

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
	case TREMOR_ACTION_ATTACK_GROUND_QUAKE:
	{
		// reset ptr to quake moby
		tremorVars->QuakeMoby = NULL;
		break;
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
		stateCooldownTicks = 0;
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
	case TREMOR_ACTION_ATTACK_GROUND_QUAKE:
	{
		tremorResetActionCooldownTicks(moby, TREMOR_ATTACK_ACTION_MELEE);
		tremorResetActionCooldownTicks(moby, TREMOR_ATTACK_ACTION_GROUND_QUAKE);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		stateCooldownTicks = 0;
		break;
	}
	case TREMOR_ACTION_FLINCH:
	case TREMOR_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = TREMOR_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
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
		}

	//
	if (action != pvars->MobVars.Action)
		pvars->MobVars.CurrentActionForTicks = 0;

	pvars->MobVars.Action = action;
	pvars->MobVars.NextAction = -1;
	pvars->MobVars.ActionCooldownTicks = stateCooldownTicks;
}

//--------------------------------------------------------------------------
short tremorGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;
	int bangles = pvars->MobVars.Config.Bangles;

	if (t < MOB_ARMOR_THRESHOLD_LOW)
		return bangles & ~(TREMOR_BANGLE_LEFT_ARM_ARMOR | TREMOR_BANGLE_HEAD_ARMOR | TREMOR_BANGLE_CHEST_ARMOR);
	else if (t < MOB_ARMOR_THRESHOLD_MID)
		return bangles & ~(TREMOR_BANGLE_LEFT_ARM_ARMOR | TREMOR_BANGLE_HEAD_ARMOR);
	else if (t < MOB_ARMOR_THRESHOLD_HIGH)
		return bangles & ~TREMOR_BANGLE_LEFT_ARM_ARMOR;

	return bangles;
}

//--------------------------------------------------------------------------
int tremorIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	switch (pvars->MobVars.Action)
	{
	case TREMOR_ACTION_ATTACK_GROUND_QUAKE:
		// stop after animation exited
		return moby->AnimSeqId == TREMOR_ANIM_JUMP_AND_STAB_DOWN || moby->AnimSeqId == TREMOR_ANIM_STAB_DOWN_POSITION_REPEAT;
	case TREMOR_ACTION_ATTACK:
		return !pvars->MobVars.AnimationLooped;
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int tremorCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// always let non-owners simulate an action unless its the death action
	if (action == TREMOR_ACTION_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int tremorShouldForceStateUpdateOnAction(Moby *moby, int action)
{
  if (((struct MobPVar*)moby->PVar)->MobVars.Action == TREMOR_ACTION_ROAM || action == TREMOR_ACTION_ROAM) return 1;

	// only send action updates at regular intervals, unless dying or flinching
	if (action == TREMOR_ACTION_DIE || action == TREMOR_ACTION_FLINCH || action == TREMOR_ACTION_BIG_FLINCH)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int tremorIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == TREMOR_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int tremorCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int tremorIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == TREMOR_ANIM_FLINCH || moby->AnimSeqId == TREMOR_ANIM_FLINCH_FALL_GET_UP) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int tremorIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == TREMOR_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int tremorIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == TREMOR_ACTION_ROAM;
}

//--------------------------------------------------------------------------
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

//--------------------------------------------------------------------------
int tremorCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(TremorMobVars_t), &guberEvent, NULL);
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
