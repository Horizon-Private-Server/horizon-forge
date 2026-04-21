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

void tremorPreUpdate(Moby *moby);
void tremorPostUpdate(Moby *moby);
void tremorPostDraw(Moby *moby);
void tremorMove(Moby *moby);
int tremorGetExtraDataSize(int spawnParamsIdx);
void tremorOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void tremorOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void tremorOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void tremorOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int tremorOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void tremorOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *tremorGetNextTarget(Moby *moby);
int tremorGetPreferredAction(Moby *moby, int *delayTicks);
void tremorDoAction(Moby *moby);
void tremorDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void tremorForceLocalAction(Moby *moby, int action);
short tremorGetArmor(Moby *moby);
int tremorIsAttacking(Moby *moby);
int tremorCanNonOwnerTransitionToAction(Moby *moby, int action);
int tremorShouldForceStateUpdateOnAction(Moby *moby, int action);

int tremorIsSpawning(struct MobPVar *pvars);
int tremorCanAttack(struct MobPVar *pvars);
int tremorIsFlinching(Moby *moby);

struct MobVTable TremorVTable = {
		.PreUpdate = &tremorPreUpdate,
		.PostUpdate = &tremorPostUpdate,
		.PostDraw = &tremorPostDraw,
		.Move = &tremorMove,
		.GetExtraDataSize = &tremorGetExtraDataSize,
		.OnSpawning = &tremorOnSpawning,
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
void tremorPreUpdate(Moby *moby)
{
	mobDefaultPreUpdate(moby);
}

//--------------------------------------------------------------------------
void tremorPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float animSpeed = 0.5 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == TREMOR_ANIM_JUMP)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / TREMOR_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = 0.5;
		}
	}
	else if (tremorIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / TREMOR_FLINCH_ANIM_AIR_DURATION, 2));
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
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
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
void tremorMove(Moby *moby)
{
	mobMove(moby);
}

//--------------------------------------------------------------------------
int tremorGetExtraDataSize(int spawnParamsIdx)
{
	return 0;
}

//--------------------------------------------------------------------------
void tremorOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
}

//--------------------------------------------------------------------------
void tremorOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// set scale
	moby->Scale = 0.256339 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 0.5 + (scale * 0.5);
	pvars->MobVars.BlipType = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
}

//--------------------------------------------------------------------------
void tremorOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;
}

//--------------------------------------------------------------------------
void tremorOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
	float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != TREMOR_ACTION_FLINCH && pvars->MobVars.Action != TREMOR_ACTION_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & MOB_DAMAGE_FLAG_SHOCK;
	int isShortFreeze = e->DamageFlags & MOB_DAMAGE_FLAG_SHORT_FREEZE;

	// destroy
	if (newHp <= 0)
	{
		tremorForceLocalAction(moby, TREMOR_ACTION_DIE);
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}

	float damageRatio = damage / pvars->MobVars.Config.Health;
	float powerFactor = TREMOR_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
	float probability = clamp((damageRatio * TREMOR_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);
	mobHandleFlinch(moby, e, canFlinch, isShock, probability, powerFactor, TREMOR_ACTION_FLINCH, TREMOR_ACTION_BIG_FLINCH);

	// short freeze
	if (isShortFreeze && pvars->MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS)
	{
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
	return mobGetNextTarget(moby, TREMOR_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int tremorGetPreferredAction(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// no preferred action
	if (tremorIsAttacking(moby))
		return -1;

	if (tremorIsSpawning(pvars))
		return -1;

	if (tremorIsFlinching(moby))
		return -1;

	if (pvars->MobVars.Action == TREMOR_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return TREMOR_ACTION_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > TREMOR_MAX_WALKABLE_SLOPE)
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
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		if (dist <= attackRadius)
		{
			if (tremorCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return TREMOR_ACTION_ATTACK;
			}
			return TREMOR_ACTION_WALK;
		}
		else
		{
			return TREMOR_ACTION_WALK;
		}
	}

	return TREMOR_ACTION_IDLE;
}

//--------------------------------------------------------------------------
void tremorDoAction(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? TREMOR_TURN_RADIANS_PER_SEC : TREMOR_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? TREMOR_MOVE_ACCELERATION : TREMOR_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == TREMOR_ACTION_FLINCH || pvars->MobVars.LastAction == TREMOR_ACTION_BIG_FLINCH);

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

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
		decTimerU8(&pvars->MobVars.Knockback.Ticks);
		int animFlinchId = pvars->MobVars.Action == TREMOR_ACTION_BIG_FLINCH ? TREMOR_ANIM_FLINCH_FALL_GET_UP : TREMOR_ANIM_FLINCH;

		mobTransAnim(moby, animFlinchId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, TREMOR_KNOCKBACK_MULTIPLIER);
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
	case TREMOR_ACTION_IDLE:
	{
		mobTransAnim(moby, TREMOR_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case TREMOR_ACTION_JUMP:
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
			mobTransAnim(moby, TREMOR_ANIM_JUMP, 10);

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
				jumpSpeed = TREMOR_DEFAULT_JUMP_SPEED;
			}

			vector_write(pvars->MobVars.MoveVars.Velocity, 0);
			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
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
	case TREMOR_ACTION_WALK:
	{
		if (!isInAirFromFlinching)
		{
			if (target)
			{
				if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
					pvars->MobVars.Dirty = 1; // new path, sync with other clients
				mobMoveTowards(moby, t, pvars->MobVars.Config.Speed, turnSpeed, acceleration, mobGetCurrentWalkAngle(moby));
			}
			else
			{
				mobStand(moby);
			}
		}

		//
		if (moby->AnimSeqId == TREMOR_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded)
		{
			// wait for jump to land
		}
		else if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			tremorForceLocalAction(moby, TREMOR_ACTION_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, TREMOR_ANIM_RUN, 0);
		}
		else if (moby->AnimSeqId != TREMOR_ANIM_RUN || pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, TREMOR_ANIM_IDLE, 0);
		}
		break;
	}
	case TREMOR_ACTION_DIE:
	{
		mobTransAnimLerp(moby, TREMOR_ANIM_FLINCH_BACK_FLIP_FALL, 5, 0);

		if (moby->AnimSeqId == TREMOR_ANIM_FLINCH_BACK_FLIP_FALL && moby->AnimSeqT > TREMOR_FLINCH_ANIM_BACK_FLIP_FALL_DESTROY_FRAME)
		{
			pvars->MobVars.Destroy = 1;
		}

		mobStand(moby);
		break;
	}
	case TREMOR_ACTION_ATTACK:
	{
		int attack1AnimId = TREMOR_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float speedMult = 0; // (moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 4) ? (difficulty * 2) : 1;
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= TREMOR_ATTACK_HIT_FRAME_START && moby->AnimSeqT < TREMOR_ATTACK_HIT_FRAME_END;
		u32 damageFlags = MOB_DAMAGE_FLAG_BASE;

		if (!isInAirFromFlinching)
		{
			if (target)
			{
				mobMoveTowards(moby, target->Position, speedMult * pvars->MobVars.Config.Speed, turnSpeed, acceleration, 0);
			}
			else
			{
				// stand
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
			tremorDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
		}
		break;
	}
	}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void tremorDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, TREMOR_SUBSKELETON_JOINT_RIGHT_HAND_CLAW, 1, 0);
}

//--------------------------------------------------------------------------
void tremorForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
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
short tremorGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Config.Bangles;
}

//--------------------------------------------------------------------------
int tremorIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == TREMOR_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
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
	// only send state updates at regular intervals, unless dying or flinching
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
