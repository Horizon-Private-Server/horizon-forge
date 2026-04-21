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

void executionerPreUpdate(Moby *moby);
void executionerPostUpdate(Moby *moby);
void executionerPostDraw(Moby *moby);
void executionerMove(Moby *moby);
int executionerGetExtraDataSize(int spawnParamsIdx);
void executionerOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void executionerOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void executionerOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void executionerOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int executionerOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void executionerOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *executionerGetNextTarget(Moby *moby);
int executionerGetPreferredAction(Moby *moby, int *delayTicks);
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

struct MobVTable ExecutionerVTable = {
		.PreUpdate = &executionerPreUpdate,
		.PostUpdate = &executionerPostUpdate,
		.PostDraw = &executionerPostDraw,
		.Move = &executionerMove,
		.GetExtraDataSize = &executionerGetExtraDataSize,
		.OnSpawning = &executionerOnSpawning,
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
void executionerPreUpdate(Moby *moby)
{
	mobDefaultPreUpdate(moby);
}

//--------------------------------------------------------------------------
void executionerPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float animSpeed = 0.6 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == EXECUTIONER_ANIM_JUMP)
	{
		animSpeed = 1 * (1 - powf(moby->AnimSeqT / EXECUTIONER_JUMP_ANIM_DURATION_FRAMES, 1));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = 0.6;
		}
	}
	else if (executionerIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / EXECUTIONER_FLINCH_ANIM_DURATION_FRAMES, 2));
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
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
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
void executionerMove(Moby *moby)
{
	mobMove(moby);
}

//--------------------------------------------------------------------------
int executionerGetExtraDataSize(int spawnParamsIdx)
{
	return sizeof(ExecutionerMobVars_t);
}

//--------------------------------------------------------------------------
void executionerOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
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
	moby->GlowRGBA = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 1.5 + (scale * 0.5);
	pvars->MobVars.BlipType = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
}

//--------------------------------------------------------------------------
void executionerOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	VECTOR expOffset;
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// spawn explosion
	u32 expColor = 0x801E70D6;
	vector_copy(expOffset, moby->Position);
	expOffset[2] += pvars->TargetVars.targetHeight;
	u128 expPos = vector_read(expOffset);
	mobySpawnExplosion(expPos, 0, 0, 0, 0, 16, 0, 16, 0, 1, 0, 0, 0, 0,
										 0, 0, expColor, expColor, expColor, expColor, expColor, expColor, expColor, expColor,
										 0, 0, 0, 0, 0, 1, 0, 0, 0);
}

//--------------------------------------------------------------------------
void executionerOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
	float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != EXECUTIONER_ACTION_FLINCH && pvars->MobVars.Action != EXECUTIONER_ACTION_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & MOB_DAMAGE_FLAG_SHOCK;
	int isShortFreeze = e->DamageFlags & MOB_DAMAGE_FLAG_SHORT_FREEZE;

	// destroy
	if (newHp <= 0)
	{
		executionerForceLocalAction(moby, EXECUTIONER_ACTION_DIE);
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}

	float damageRatio = damage / pvars->MobVars.Config.Health;
	float powerFactor = EXECUTIONER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
	float probability = clamp((damageRatio * EXECUTIONER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);
	mobHandleFlinch(moby, e, canFlinch, isShock, probability, powerFactor, EXECUTIONER_ACTION_FLINCH, EXECUTIONER_ACTION_BIG_FLINCH);

	// short freeze
	if (isShortFreeze && pvars->MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS)
	{
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
	return mobGetNextTarget(moby, EXECUTIONER_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int executionerGetPreferredAction(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	VECTOR t;

	// no preferred action
	if (executionerIsAttacking(moby))
		return -1;

	if (executionerIsSpawning(pvars))
		return -1;

	if (executionerIsFlinching(moby))
		return -1;

	if (pvars->MobVars.Action == EXECUTIONER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return EXECUTIONER_ACTION_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > EXECUTIONER_MAX_WALKABLE_SLOPE)
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
		float rangedAttackRadius = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].RangedAttackDistance;
		int preferredRanged = mobGetBehavior(moby) == EXECUTIONER_BEHAVIOR_RANGED;
		int canRanged = mobGetBehavior(moby) == EXECUTIONER_BEHAVIOR_NORMAL || preferredRanged;

		if (1)
		{
			if (dist <= attackRadius)
			{
				// near target, swing
				if (executionerCanAttack(pvars) && dist > (EXECUTIONER_TOO_CLOSE_TO_TARGET_RADIUS))
				{
					if (delayTicks)
						*delayTicks = pvars->MobVars.Config.ReactionTickCount;
					return EXECUTIONER_ACTION_ATTACK;
				}
				return EXECUTIONER_ACTION_WALK;
			}
			else if (!preferredRanged && dist <= (attackRadius * 2) && rand(101))
			{
				// chase most of the time, sometimes defer to ranged attack
				return EXECUTIONER_ACTION_WALK;
			}
			else if (canRanged && dist <= rangedAttackRadius && mobCanSeeMoby(moby, target))
			{
				// away from target
				// check if facing
				// and fire
				t[2] = 0;
				float theta = acosf(vector_innerproduct(t, moby->M0_03));
				if (fabsf(theta) < (30 * MATH_DEG2RAD))
					return EXECUTIONER_ACTION_FIRE;
			}
		}

		return EXECUTIONER_ACTION_WALK;
	}

	return EXECUTIONER_ACTION_IDLE;
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

	vector_scale(vel, vel, 0.5);

	// fire shot
	float rangedAttack = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].RangedAttackDistance;
	Moby *shotMoby = ((Moby * (*)(float, float, VECTOR, VECTOR, Moby *, int, int, int, int))0x0045d598)(4, pvars->MobVars.Config.Damage, from, vel, moby, 1, 0x222124, -1, 0);
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
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER_TURN_RADIANS_PER_SEC : EXECUTIONER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER_MOVE_ACCELERATION : EXECUTIONER_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == EXECUTIONER_ACTION_FLINCH || pvars->MobVars.LastAction == EXECUTIONER_ACTION_BIG_FLINCH);

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

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
		decTimerU8(&pvars->MobVars.Knockback.Ticks);
		int animFlinchId = pvars->MobVars.Action == EXECUTIONER_ACTION_BIG_FLINCH ? EXECUTIONER_ANIM_BIG_FLINCH : EXECUTIONER_ANIM_FLINCH;

		mobTransAnim(moby, animFlinchId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, EXECUTIONER_KNOCKBACK_MULTIPLIER);
			vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
		}
		else if (pvars->MobVars.MoveVars.Grounded)
		{
			mobStand(moby);
		}
		else if (pvars->MobVars.CurrentActionForTicks > (1 * TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter)
		{
			mobStand(moby);
		}
		break;
	}
	case EXECUTIONER_ACTION_IDLE:
	{
		mobTransAnim(moby, EXECUTIONER_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case EXECUTIONER_ACTION_JUMP:
	{
		// move
		if (!isInAirFromFlinching)
		{
			if (target)
			{
				if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
					pvars->MobVars.Dirty = 1; // new path, sync with other clients
				mobJumpTowards(moby, t);
			}
			else
			{
				mobStand(moby);
			}
		}

		// handle jumping
		if (pvars->MobVars.MoveVars.Grounded)
		{
			mobTransAnim(moby, EXECUTIONER_ANIM_JUMP, 5);

			// check if we're near last jump pos
			// if so increment StuckJumpCount
			if (pvars->MobVars.MoveVars.IsStuck)
			{
				if (pvars->MobVars.MoveVars.StuckJumpCount < 255)
					pvars->MobVars.MoveVars.StuckJumpCount++;
			}

			// use delta height between target as base of jump speed
			// with min speed
			float jumpSpeed = pvars->MobVars.MoveVars.QueueJumpSpeed;
			if (jumpSpeed <= 0 && target)
			{
				jumpSpeed = EXECUTIONER_DEFAULT_JUMP_SPEED;
			}

			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case EXECUTIONER_ACTION_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
		{
			mobTurnTowards(moby, target->Position, turnSpeed);
		}
		break;
	}
	case EXECUTIONER_ACTION_WALK:
	{
		int walkBackwards = 0;

		if (!isInAirFromFlinching)
		{
			if (target)
			{

				float dir = mobGetCurrentWalkAngle(moby);

				// determine next position
				vector_copy(t, target->Position);
				vector_subtract(t, t, moby->Position);
				float dist = vector_length(t);

				// walk backwards if too close
				if (dist < (EXECUTIONER_TOO_CLOSE_TO_TARGET_RADIUS + pvars->MobVars.Config.CollRadius))
				{
					walkBackwards = 1;

					if (dist < 0.1)
					{
						vector_fromyaw(t, moby->Rotation[2]);
					}
					else
					{
						vector_normalize(t, t);
					}

					vector_scale(t, t, 5);
					vector_subtract(t, moby->Position, t);
					mobGetVelocityToTargetSimple(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, pvars->MobVars.Config.Speed, acceleration);
					DPRINTF("%f\n", vector_length(pvars->MobVars.MoveVars.Velocity));
				}
				else if (dist > (pvars->MobVars.Config.AttackRadius - pvars->MobVars.Config.HitRadius))
				{

					if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
						pvars->MobVars.Dirty = 1; // new path, sync with other clients
					mobMoveTowards(moby, t, pvars->MobVars.Config.Speed, turnSpeed, acceleration, dir);
				}
				else
				{
					mobStand(moby);
				}
			}
			else
			{
				mobStand(moby);
			}
		}

		//
		if (moby->AnimSeqId == EXECUTIONER_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded)
		{
			// wait for jump to land
		}
		else if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			executionerForceLocalAction(moby, EXECUTIONER_ACTION_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, walkBackwards ? EXECUTIONER_ANIM_WALK_BACKWARD : EXECUTIONER_ANIM_RUN, 0);
		}
		else if (moby->AnimSeqId != EXECUTIONER_ANIM_WALK_BACKWARD || moby->AnimSeqId != EXECUTIONER_ANIM_RUN || pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, EXECUTIONER_ANIM_IDLE, 0);
		}
		break;
	}
	case EXECUTIONER_ACTION_DIE:
	{
		mobTransAnimLerp(moby, EXECUTIONER_ANIM_BIG_FLINCH, 5, 0);

		if (moby->AnimSeqId == EXECUTIONER_ANIM_BIG_FLINCH && moby->AnimSeqT > EXECUTIONER_BIG_FLINCH_COMPLETE_FRAME)
		{
			pvars->MobVars.Destroy = 1;
		}

		mobStand(moby);
		break;
	}
	case EXECUTIONER_ACTION_ATTACK:
	{
		int attack1AnimId = EXECUTIONER_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float speedMult = 0; // (moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1;
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= EXECUTIONER_ATTACK_HIT_FRAME_START && moby->AnimSeqT < EXECUTIONER_ATTACK_HIT_FRAME_END;
		u32 damageFlags = MOB_DAMAGE_FLAG_BASE;

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

		// attribute damage
		switch (pvars->MobVars.Config.MobAttribute)
		{
		case MOB_ATTRIBUTE_FREEZE: damageFlags |= MOB_DAMAGE_FLAG_FREEZE; break;
		case MOB_ATTRIBUTE_ACID:   damageFlags |= MOB_DAMAGE_FLAG_ACID;   break;
		}

		if (swingAttackReady && damageFlags)
		{
			executionerDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
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
	}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void executionerDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, 6, 1, 0);
}

//--------------------------------------------------------------------------
void executionerForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ExecutionerMobVars_t *executionerVars = (ExecutionerMobVars_t *)pvars->AdditionalMobVarsPtr;
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
		break;
	}
	case EXECUTIONER_ACTION_ATTACK:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
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
short executionerGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;
	int bangles = pvars->MobVars.Config.Bangles;

	if (t < MOB_ARMOR_THRESHOLD_LOW)
		return 0x0000;
	else if (t < MOB_ARMOR_THRESHOLD_HIGH)
		return bangles & 0x1f; // remove torso bangle

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
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// only send state updates at regular intervals, unless dying or flinching
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
