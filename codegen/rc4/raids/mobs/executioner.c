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

void executionerResetActionCooldownTicks(Moby *moby, enum ExecutionerAttackActions action);
int executionerGetActionReady(Moby *moby, enum ExecutionerAttackActions action);
void executionerPreUpdate(Moby *moby);
void executionerPostUpdate(Moby *moby);
void executionerPostDraw(Moby *moby);
void executionerAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount);
void executionerMove(Moby* moby);
void executionerOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void executionerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void executionerOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int executionerOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void executionerOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *executionerGetNextTarget(Moby *moby);
int executionerGetPreferredAction(Moby *moby, int *delayTicks);
Moby *executionerFireShot(Moby *moby, Moby *target);
void executionerDoAction(Moby *moby);
void executionerDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void executionerForceLocalAction(Moby *moby, int action);
short executionerGetArmor(Moby *moby);
int executionerIsAttacking(Moby *moby);
int executionerCanNonOwnerTransitionToAction(Moby *moby, int action);
int executionerShouldForceStateUpdateOnAction(Moby *moby, int action);
int executionerIsSpawning(struct MobPVar *pvars);
int executionerCanAttack(struct MobPVar *pvars);
int executionerIsFlinching(Moby *moby);
int executionerCreate(struct MobCreateArgs* args);
void executionerRenderPath(Moby* moby);
int executionerIsRoaming(struct MobPVar* pvars);
int executionerIsIdling(struct MobPVar* pvars);

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
  .GetNextTarget = &executionerGetNextTarget,
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

void executionerResetActionCooldownTicks(Moby *moby, enum ExecutionerAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;
	executionerVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	executionerVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int executionerGetActionReady(Moby *moby, enum ExecutionerAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;
	return executionerVars->ActionQueuedForTicks[action] > 0;
}

//--------------------------------------------------------------------------
void executionerPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);

	if (!mobIsFrozen(moby))
		mobTickActionCooldowns(moby, EXECUTIONER_ATTACK_ACTION_COUNT, executionerVars->ActionCooldownTicks, executionerVars->ActionQueuedForTicks);
}

//--------------------------------------------------------------------------
void executionerPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float baseSpeed = 0.6;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == EXECUTIONER_ANIM_JUMP)
	{
		animSpeed = 1 * (1 - powf(moby->AnimSeqT / EXECUTIONER_JUMP_ANIM_DURATION_FRAMES, 1));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
		}
	}
	else if (executionerIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / EXECUTIONER_FLINCH_ANIM_DURATION_FRAMES, 2));
	}
	else if (moby->AnimSeqId == EXECUTIONER_ANIM_SWING)
	{
		animSpeed = baseSpeed * mobGetFloat(moby, EXECUTIONER_PARAM_MELEE_ATTACK_SPEED_MULTIPLIER);
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == EXECUTIONER_ACTION_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void executionerPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 2.6, color, 1);
}

//--------------------------------------------------------------------------
void executionerAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void executionerMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void executionerOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(ExecutionerMobVars_t));
	float scale = mobGetScaleMultiplier(moby);

	// set scale
	moby->Scale = 0.35 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 1.5 + (scale * 0.5);
	pvars->MobVars.BlipType = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;

	// initialize action cooldowns
	int i;
	for (i = 0; i < EXECUTIONER_ATTACK_ACTION_COUNT; ++i)
		executionerResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void executionerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
  
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
int executionerOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
}

//--------------------------------------------------------------------------
void executionerOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *executionerGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int executionerGetPreferredAction(Moby *moby, int *delayTicks)
{
	VECTOR t;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

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
	if (mobHitWallShouldJump(moby, EXECUTIONER_MAX_WALKABLE_SLOPE))
	{
		return EXECUTIONER_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return EXECUTIONER_ACTION_JUMP;
	}

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby *target = executionerGetNextTarget(moby);
	if (target)
	{
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;
		float rangedAttackRadius = mobGetFloat(moby, EXECUTIONER_PARAM_FIRE_MAX_DISTANCE);
		int preferRanged = mobGetBehavior(moby) == EXECUTIONER_BEHAVIOR_RANGED;
		int canRanged = mobGetBehavior(moby) == EXECUTIONER_BEHAVIOR_NORMAL || preferRanged;

		// can't attack
		if (!executionerCanAttack(pvars))
			return preferRanged ? EXECUTIONER_ACTION_LOOK_AT_TARGET : EXECUTIONER_ACTION_WALK;

		if (1)
		{
			if (dist <= attackRadius && executionerGetActionReady(moby, EXECUTIONER_ATTACK_ACTION_MELEE))
			{
				// near target, swing
				if (dist > (EXECUTIONER_TOO_CLOSE_TO_TARGET_RADIUS))
				{
					if (delayTicks)
						*delayTicks = pvars->MobVars.Config.ReactionTickCount;
					return EXECUTIONER_ACTION_ATTACK;
				}
				return EXECUTIONER_ACTION_WALK;
			}
			else if (canRanged && dist <= rangedAttackRadius && mobCanSeeMoby(moby, target))
			{
				// away from target
				// check if facing
				// and fire
				mobGetTargetCenter(target, t);
				vector_subtract(t, t, moby->Position);
				vector_projectonhorizontal(t, t);
				float theta = acosf(vector_innerproduct(t, moby->M0_03));
				if (fabsf(theta) < (30 * MATH_DEG2RAD) && executionerGetActionReady(moby, EXECUTIONER_ATTACK_ACTION_FIRE))
					return EXECUTIONER_ACTION_FIRE;

				return preferRanged ? EXECUTIONER_ACTION_LOOK_AT_TARGET : EXECUTIONER_ACTION_WALK;
			}
		}

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
Moby *executionerFireShot(Moby *moby, Moby *target)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int jointId = EXECUTIONER_SUBSKELETON_JOINT_STAFF_END;

	VECTOR from, to = {0, 0, 1, 0}, dir, vel;
	MATRIX m;
	mobyGetJointMatrix(moby, jointId, m);
	vector_copy(from, &m[12]);
	vector_copy(vel, &m[0]);

	float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, EXECUTIONER_PARAM_FIRE_DAMAGE_MULTIPLIER);
	float projectileSpeed = MATH_DT * mobGetFloat(moby, EXECUTIONER_PARAM_FIRE_PROJECTILE_SPEED);

	// move shot from forward
	// VECTOR offset;
	// vector_scale(offset, &m[0], 0.15);
	// vector_add(from, from, offset);

	if (target)
	{

		// determine if we should shoot directly towards target
		vector_subtract(dir, target->Position, moby->Position);
		vector_normalize(dir, dir);
		VECTOR planarForward;
		vector_projectonplane(planarForward, dir, moby->M2_03);
		float angle = acosf(vector_innerproduct(planarForward, moby->M0_03));
		if (angle < (1 * MATH_DEG2RAD))
		{
			mobGetTargetCenter(target, to);
			vector_subtract(vel, to, from);
			vector_normalize(vel, vel);
		}
		else
		{
			vector_subtract(dir, dir, planarForward);
			vector_projectonplane(vel, vel, moby->M2_03);
			vector_normalize(vel, vel);
			vector_add(vel, vel, dir);
		}
	}

	vector_scale(vel, vel, projectileSpeed);

	// fire shot
	float rangedAttack = mobGetFloat(moby, EXECUTIONER_PARAM_FIRE_MAX_DISTANCE);
	Moby *shotMoby = ((Moby * (*)(float, float, VECTOR, VECTOR, Moby *, int, int, int, int))0x0045d598)(4, damage, from, vel, moby, 1, 0x222124, -1, 0);
	if (shotMoby)
	{
		((void (*)(Moby *, int))0x0045d758)(shotMoby, 2);														// shot type
		((void (*)(Moby *, int))0x0045d788)(shotMoby, TEAM_RED);										// shot color
		((void (*)(Moby *, int))0x0045d7A8)(shotMoby, 1);														// hit flag
		((void (*)(Moby *, int))0x0045d798)(shotMoby, 2 * TPS + (int)rangedAttack); // shot life (ticks)
		shotMoby->Bolts = -1;																												// indicate to gamemode shot can damage player
		shotMoby->PParent = moby;
	}

	// spawn flare
	//((void (*)(float, float, float, Moby*, int, int, int))0x0042c178)(0.75, 0.75, 1.0, moby, 0, 0, jointId);

	// play sound
	// mobyPlaySoundByClass(1, 0, moby, MOBY_ID_LANDSTALKER);

	return shotMoby;
}

//--------------------------------------------------------------------------
void executionerDoAction(Moby *moby)
{
  struct PathGraph* path = pathGetMobyPathGraph(moby, &((struct MobPVar*)moby->PVar)->MobVars.MoveVars);
  float speed = ((struct MobPVar*)moby->PVar)->MobVars.Config.Speed;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.MoveVars.Target;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;
	VECTOR t;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER_TURN_RADIANS_PER_SEC : EXECUTIONER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER_MOVE_ACCELERATION : EXECUTIONER_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == EXECUTIONER_ACTION_FLINCH || pvars->MobVars.LastAction == EXECUTIONER_ACTION_BIG_FLINCH);

	turnSpeed *= pvars->MobVars.Config.TurnSpeed;

	switch (pvars->MobVars.Action)
	{
case EXECUTIONER_ACTION_ATTACK:
	{
		int attack1AnimId = EXECUTIONER_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, EXECUTIONER_PARAM_MELEE_DAMAGE_MULTIPLIER);
		float speedMult = 0;
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= EXECUTIONER_ATTACK_HIT_FRAME_START && moby->AnimSeqT < EXECUTIONER_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (!isInAirFromFlinching)
		{
			if (target)
			{
				mobMoveTowards(moby, target->Position, speedMult * pvars->MobVars.Config.Speed, turnSpeed, acceleration, 0);
			}
			else
			{
				mobStand(moby);
			}
		}

		if (swingAttackReady && damageFlags)
		{
			executionerDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
		}
		break;
	}
	case EXECUTIONER_ACTION_FIRE:
	{
		int animId = EXECUTIONER_ANIM_FIRE;

		if (!isInAirFromFlinching)
		{
			mobStand(moby);
			if (target)
			{
				mobTurnTowards(moby, target->Position, turnSpeed * 0.5);

				if (moby->AnimSeqId == animId && moby->AnimSeqT >= EXECUTIONER_FIRE_HIT_FRAME_START && moby->AnimSeqT < EXECUTIONER_FIRE_HIT_FRAME_END && executionerVars->AnimationLoopLastFire != pvars->MobVars.AnimationLooped)
				{
					executionerFireShot(moby, target);
					executionerVars->AnimationLoopLastFire = pvars->MobVars.AnimationLooped;
				}
			}
		}

		mobTransAnim(moby, animId, 0);
		break;
	}
	case EXECUTIONER_ACTION_SPAWN:
		{
      mobTransAnim(moby, EXECUTIONER_ANIM_SPAWN, 0);
      mobStand(moby);
			break;
		}
		case EXECUTIONER_ACTION_FLINCH:
		case EXECUTIONER_ACTION_BIG_FLINCH:
		{
      decTimerU8(&pvars->MobVars.Knockback.Ticks);
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
          //mobResetSoundTrigger(moby);

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
      } else {
				mobTransAnim(moby, EXECUTIONER_ANIM_IDLE, 0);
      }
      break;
    }
    case EXECUTIONER_ACTION_WALK:
		{
      float dir = mobGetCurrentWalkAngle(moby);

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
      } else {
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
		}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void executionerDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, 6, 1, 0);
}

//--------------------------------------------------------------------------
void executionerForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;
	int stateCooldownTicks = EXECUTIONER_ACTION_COOLDOWN_TICKS;

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
		stateCooldownTicks = 0;
		break;
	}
	case EXECUTIONER_ACTION_WALK:
	{

		break;
	}
	case EXECUTIONER_ACTION_DIE:
	{

		break;
	}
	case EXECUTIONER_ACTION_FIRE:
	{
		executionerVars->AnimationLoopLastFire = -1;
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		executionerResetActionCooldownTicks(moby, EXECUTIONER_ATTACK_ACTION_FIRE);
		stateCooldownTicks = 0;
		break;
	}
	case EXECUTIONER_ACTION_ATTACK:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		executionerResetActionCooldownTicks(moby, EXECUTIONER_ATTACK_ACTION_MELEE);
		stateCooldownTicks = 0;
		break;
	}
	case EXECUTIONER_ACTION_FLINCH:
	case EXECUTIONER_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = EXECUTIONER_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
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
		}

	//
	if (action != pvars->MobVars.Action)
		pvars->MobVars.CurrentActionForTicks = 0;

	pvars->MobVars.Action = action;
	pvars->MobVars.NextAction = -1;
	pvars->MobVars.ActionCooldownTicks = stateCooldownTicks;
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
int executionerIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	switch (pvars->MobVars.Action)
	{
	case EXECUTIONER_ACTION_FIRE:
		return !pvars->MobVars.AnimationLooped;
	case EXECUTIONER_ACTION_ATTACK:
		return !pvars->MobVars.AnimationLooped;
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int executionerCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// always let non-owners simulate an action unless its the death action
	if (action == EXECUTIONER_ACTION_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int executionerShouldForceStateUpdateOnAction(Moby *moby, int action)
{
  if (((struct MobPVar*)moby->PVar)->MobVars.Action == EXECUTIONER_ACTION_ROAM || action == EXECUTIONER_ACTION_ROAM) return 1;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// only send action updates at regular intervals, unless dying or flinching
	if (action == EXECUTIONER_ACTION_DIE || action == EXECUTIONER_ACTION_FLINCH || action == EXECUTIONER_ACTION_BIG_FLINCH)
		return 1;
	if (pvars->MobVars.Action == EXECUTIONER_ACTION_FIRE || action == EXECUTIONER_ACTION_FIRE)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int executionerIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == EXECUTIONER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int executionerCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int executionerIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == EXECUTIONER_ANIM_FLINCH || moby->AnimSeqId == EXECUTIONER_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
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
