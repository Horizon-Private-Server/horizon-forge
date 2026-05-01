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

void swamperPreUpdate(Moby *moby);
void swamperPostUpdate(Moby *moby);
void swamperPostDraw(Moby *moby);
void swamperMove(Moby *moby);
int swamperGetExtraDataSize(int spawnParamsIdx);
void swamperOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void swamperOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void swamperOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void swamperOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int swamperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void swamperOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e);
Moby *swamperGetNextTarget(Moby *moby);
int swamperGetPreferredState(Moby *moby, int *delayTicks);
void swamperDoState(Moby *moby);
void swamperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void swamperForceLocalState(Moby *moby, int state);
short swamperGetArmor(Moby *moby);
int swamperIsAttacking(Moby *moby);
int swamperCanNonOwnerTransitionToState(Moby *moby, int state);
int swamperShouldForceStateUpdateOnState(Moby *moby, int state);

int swamperIsSpawning(struct MobPVar *pvars);
int swamperCanAttack(struct MobPVar *pvars);
int swamperIsFlinching(Moby *moby);
int swamperIsDying(Moby *moby);

struct MobVTable SwamperVTable = {
		.PreUpdate = &swamperPreUpdate,
		.PostUpdate = &swamperPostUpdate,
		.PostDraw = &swamperPostDraw,
		.Move = &swamperMove,
		.GetExtraDataSize = &swamperGetExtraDataSize,
		.OnSpawning = &swamperOnSpawning,
		.OnSpawn = &swamperOnSpawn,
		.OnDestroy = &swamperOnDestroy,
		.OnDamage = &swamperOnDamage,
		.OnLocalDamage = &swamperOnLocalDamage,
		.OnFullStateUpdate = &swamperOnFullStateUpdate,
		.GetNextTarget = &swamperGetNextTarget,
		.GetPreferredState = &swamperGetPreferredState,
		.ForceLocalState = &swamperForceLocalState,
		.DoState = &swamperDoState,
		.DoDamage = &swamperDoDamage,
		.GetArmor = &swamperGetArmor,
		.IsAttacking = &swamperIsAttacking,
		.CanNonOwnerTransitionToState = &swamperCanNonOwnerTransitionToState,
		.ShouldForceStateUpdateOnState = &swamperShouldForceStateUpdateOnState,
};

//--------------------------------------------------------------------------
void swamperResetActionCooldownTicks(Moby *moby, enum SwamperActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwamperMobVars_t *swamperVars = (SwamperMobVars_t *)pvars->AdditionalMobVarsPtr;
	swamperVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	swamperVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int swamperGetActionReady(Moby *moby, enum SwamperActions action)
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
		mobTickActionCooldowns(moby, SWAMPER_ACTION_COUNT, swamperVars->ActionCooldownTicks, swamperVars->ActionQueuedForTicks);
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

	if (pvars->MobVars.State == SWAMPER_STATE_DIE)
	{
		animSpeed = baseSpeed;
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.State == SWAMPER_STATE_WALK))
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
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 0.5, color, SWAMPER_SUBSKELETON_JOINT_JAW);
}

//--------------------------------------------------------------------------
void swamperMove(Moby *moby)
{
	mobMove(moby);
}

//--------------------------------------------------------------------------
int swamperGetExtraDataSize(int spawnParamsIdx)
{
	return sizeof(SwamperMobVars_t);
}

//--------------------------------------------------------------------------
void swamperOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
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
	moby->GlowRGBA = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 0.5 + (scale * 0.25);
	pvars->MobVars.BlipType = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;

	// initialize action cooldowns
	int i;
	for (i = 0; i < SWAMPER_ACTION_COUNT; ++i)
		swamperResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void swamperOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;
}

//--------------------------------------------------------------------------
void swamperOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
	float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.State != SWAMPER_STATE_FLINCH && pvars->MobVars.State != SWAMPER_STATE_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & MOB_DAMAGE_FLAG_SHOCK;
	int isShortFreeze = e->DamageFlags & MOB_DAMAGE_FLAG_SHORT_FREEZE;

	// destroy
	if (newHp <= 0)
	{
		pvars->MobVars.Destroy = 1;
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}

	float damageRatio = damage / pvars->MobVars.Config.MaxHealth;
	float powerFactor = SWAMPER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
	float probability = clamp((damageRatio * SWAMPER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);
	mobHandleFlinch(moby, e, canFlinch, isShock, probability, powerFactor, SWAMPER_STATE_FLINCH, SWAMPER_STATE_BIG_FLINCH);

	// short freeze
	if (isShortFreeze && pvars->MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS)
	{
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
void swamperOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e)
{
	mobOnFullStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *swamperGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby, SWAMPER_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int swamperGetPreferredState(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// no preferred state
	if (swamperIsAttacking(moby))
		return -1;

	if (swamperIsSpawning(pvars))
		return -1;

	if (swamperIsFlinching(moby))
		return -1;

	if (pvars->MobVars.State == SWAMPER_STATE_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return SWAMPER_STATE_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > SWAMPER_MAX_WALKABLE_SLOPE)
	{
		return SWAMPER_STATE_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return SWAMPER_STATE_JUMP;
	}

	// prevent state changing too quickly
	if (pvars->MobVars.StateCooldownTicks)
		return -1;

	// get next target
	Moby *target = swamperGetNextTarget(moby);
	if (target)
	{
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		if (dist <= attackRadius && swamperGetActionReady(moby, SWAMPER_ACTION_BITE))
		{
			if (swamperCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return SWAMPER_STATE_ATTACK;
			}
			return SWAMPER_STATE_WALK;
		}
		else
		{
			return SWAMPER_STATE_WALK;
		}
	}

	return SWAMPER_STATE_IDLE;
}

//--------------------------------------------------------------------------
#if DEBUG_PATH
void swamperRenderPath(Moby *moby)
{
	int x, y;
	int i;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u8 *path = (u8 *)pvars->MobVars.MoveVars.CurrentPath;
	int pathLen = pvars->MobVars.MoveVars.PathEdgeCount;
	int pathIdx = pvars->MobVars.MoveVars.PathEdgeCurrent;

	for (i = 0; i < pathLen; ++i)
	{
		u8 *edge = MOB_PATHFINDING_EDGES[path[i]];
		if (gfxWorldSpaceToScreenSpace(MOB_PATHFINDING_NODES[edge[1]], &x, &y))
		{
			gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
		}
	}

	VECTOR t;
	pathGetTargetPos(t, moby);
	if (gfxWorldSpaceToScreenSpace(t, &x, &y))
	{
		gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "+", -1, 4);
	}
}
#endif

//--------------------------------------------------------------------------
void swamperDoState(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? SWAMPER_TURN_RADIANS_PER_SEC : SWAMPER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? SWAMPER_MOVE_ACCELERATION : SWAMPER_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastState == SWAMPER_STATE_FLINCH || pvars->MobVars.LastState == SWAMPER_STATE_BIG_FLINCH);

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

#if DEBUG_PATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&swamperRenderPath, moby);
#endif

	switch (pvars->MobVars.State)
	{
	case SWAMPER_STATE_SPAWN:
	{
		mobTransAnim(moby, SWAMPER_ANIM_ROAR, 0);
		mobStand(moby);
		break;
	}
	case SWAMPER_STATE_FLINCH:
	case SWAMPER_STATE_BIG_FLINCH:
	{
		decTimerU8(&pvars->MobVars.Knockback.Ticks);
		int animFlinchId = pvars->MobVars.State == SWAMPER_STATE_BIG_FLINCH ? SWAMPER_ANIM_FALL_BACKWARDS : SWAMPER_ANIM_JUMP_BACKWARDS;

		mobTransAnim(moby, animFlinchId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.State == SWAMPER_STATE_BIG_FLINCH)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, SWAMPER_KNOCKBACK_MULTIPLIER);
			vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
		}
		else if (pvars->MobVars.MoveVars.Grounded)
		{
			mobStand(moby);
		}
		else if (pvars->MobVars.CurrentStateForTicks > (1 * TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter)
		{
			mobStand(moby);
		}
		break;
	}
	case SWAMPER_STATE_IDLE:
	{
		mobTransAnim(moby, SWAMPER_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case SWAMPER_STATE_JUMP:
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
			mobTransAnim(moby, SWAMPER_ANIM_JUMP, 5);
			mobResetSoundTrigger(moby);

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
				jumpSpeed = SWAMPER_DEFAULT_JUMP_SPEED;
			}

			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case SWAMPER_STATE_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
			mobTurnTowards(moby, target->Position, turnSpeed);
		break;
	}
	case SWAMPER_STATE_WALK:
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
		if (moby->AnimSeqId == SWAMPER_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded)
		{
			// wait for jump to land
		}
		else if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			swamperForceLocalState(moby, SWAMPER_STATE_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, SWAMPER_ANIM_RUN, 0);
		}
		else if (moby->AnimSeqId != SWAMPER_ANIM_RUN || pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, SWAMPER_ANIM_IDLE, 0);
		}
		break;
	}
	case SWAMPER_STATE_DIE:
	{
		mobStand(moby);
		break;
	}
	case SWAMPER_STATE_ATTACK:
	{
		int attack1AnimId = SWAMPER_ANIM_BITE;
		mobTransAnim(moby, attack1AnimId, 0);

		float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, SWAMPER_PARAM_BITE_DAMAGE_MULTIPLIER);
		float lungeMult = mobGetFloat(moby, SWAMPER_PARAM_BITE_LUNGE_MULTIPLIER);

		float t = moby->AnimSeqT / SWAMPER_BITE_ANIM_DURATION;
		float speedCurve = pvars->MobVars.Config.Speed * powf(clamp((1.5 - t) * 1.5, 0, 1.5), 2) * lungeMult;
		float speed = (moby->AnimSeqId == attack1AnimId && (moby->AnimSeqT < SWAMPER_BITE_LUNGE_FRAME_START || moby->AnimSeqT > SWAMPER_BITE_LUNGE_FRAME_END)) ? 0 : speedCurve;
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= SWAMPER_BITE_ATTACK_HIT_FRAME_START && moby->AnimSeqT < SWAMPER_BITE_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (!isInAirFromFlinching)
		{
			if (target)
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
	}

	pvars->MobVars.CurrentStateForTicks++;
}

//--------------------------------------------------------------------------
void swamperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, SWAMPER_SUBSKELETON_JOINT_JAW, 1, 0);
}

//--------------------------------------------------------------------------
void swamperForceLocalState(Moby *moby, int state)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float difficulty = 1;
	int stateCooldownTicks = SWAMPER_STATE_COOLDOWN_TICKS;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.State)
	{
	case SWAMPER_STATE_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case SWAMPER_STATE_DIE:
	{
		// can't undie
		return;
	}
	}

	// to
	switch (state)
	{
	case SWAMPER_STATE_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		stateCooldownTicks = 0;
		break;
	}
	case SWAMPER_STATE_WALK:
	{

		break;
	}
	case SWAMPER_STATE_DIE:
	{
		pvars->MobVars.Destroy = 1;
		break;
	}
	case SWAMPER_STATE_ATTACK:
	{
		swamperResetActionCooldownTicks(moby, SWAMPER_ACTION_BITE);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		mobResetMoveStep(moby); // force move step reset for accurate lunge
		stateCooldownTicks = 0;
		break;
	}
	case SWAMPER_STATE_FLINCH:
	case SWAMPER_STATE_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = SWAMPER_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
		break;
	}
	}

	//
	if (state != pvars->MobVars.State)
		pvars->MobVars.CurrentStateForTicks = 0;

	pvars->MobVars.State = state;
	pvars->MobVars.NextState = -1;
	pvars->MobVars.StateCooldownTicks = stateCooldownTicks;
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
	return pvars->MobVars.State == SWAMPER_STATE_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swamperCanNonOwnerTransitionToState(Moby *moby, int state)
{
	// always let non-owners simulate an state unless its the death state
	if (state == SWAMPER_STATE_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int swamperShouldForceStateUpdateOnState(Moby *moby, int state)
{
	// only send state updates at regular intervals, unless dying
	if (state == SWAMPER_STATE_DIE)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int swamperIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.State == SWAMPER_STATE_SPAWN && !pvars->MobVars.AnimationLooped;
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
	return pvars->MobVars.State == SWAMPER_STATE_DIE;
}
