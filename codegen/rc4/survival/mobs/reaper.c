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
#include "pathfind.h"
#include "maputils.h"
#include "utils.h"

void reaperPreUpdate(Moby *moby);
void reaperPostUpdate(Moby *moby);
void reaperPostDraw(Moby *moby);
void reaperMove(Moby *moby);
int reaperGetExtraDataSize(int spawnParamsIdx);
void reaperOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void reaperOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void reaperOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void reaperOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int reaperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void reaperOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e);
Moby *reaperGetNextTarget(Moby *moby);
int reaperGetPreferredState(Moby *moby, int *delayTicks);
void reaperDoState(Moby *moby);
void reaperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void reaperForceLocalState(Moby *moby, int state);
short reaperGetArmor(Moby *moby);
int reaperIsAttacking(Moby *moby);
int reaperCanNonOwnerTransitionToState(Moby *moby, int state);
int reaperShouldForceStateUpdateOnState(Moby *moby, int state);

int reaperIsSpawning(struct MobPVar *pvars);
int reaperCanAttack(struct MobPVar *pvars);
int reaperIsFlinching(Moby *moby);

struct MobVTable ReaperVTable = {
		.PreUpdate = &reaperPreUpdate,
		.PostUpdate = &reaperPostUpdate,
		.PostDraw = &reaperPostDraw,
		.Move = &reaperMove,
		.GetExtraDataSize = &reaperGetExtraDataSize,
		.OnSpawning = &reaperOnSpawning,
		.OnSpawn = &reaperOnSpawn,
		.OnDestroy = &reaperOnDestroy,
		.OnDamage = &reaperOnDamage,
		.OnLocalDamage = &reaperOnLocalDamage,
		.OnFullStateUpdate = &reaperOnFullStateUpdate,
		.GetNextTarget = &reaperGetNextTarget,
		.GetPreferredState = &reaperGetPreferredState,
		.ForceLocalState = &reaperForceLocalState,
		.DoState = &reaperDoState,
		.DoDamage = &reaperDoDamage,
		.GetArmor = &reaperGetArmor,
		.IsAttacking = &reaperIsAttacking,
		.CanNonOwnerTransitionToState = &reaperCanNonOwnerTransitionToState,
		.ShouldForceStateUpdateOnState = &reaperShouldForceStateUpdateOnState,
};

//--------------------------------------------------------------------------
void reaperPreUpdate(Moby *moby)
{
	mobDefaultPreUpdate(moby);
}

//--------------------------------------------------------------------------
void reaperPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float animSpeed = 1.5 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (reaperIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / REAPER_FLINCH_ANIM_DURATION, 2));
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.State == REAPER_STATE_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void reaperPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 1.45, color, REAPER_SUBSKELETON_HEAD);
}

//--------------------------------------------------------------------------
void reaperAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void reaperMove(Moby *moby)
{
	mobMove(moby);
}

//--------------------------------------------------------------------------
int reaperGetExtraDataSize(int spawnParamsIdx)
{
	return sizeof(ReaperMobVars_t);
}

//--------------------------------------------------------------------------
void reaperOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
}

//--------------------------------------------------------------------------
void reaperOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(ReaperMobVars_t));
	float scale = mobGetScaleMultiplier(moby);

	// set scale
	moby->Scale = 0.256339 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 0.75 + (scale * 0.25);
	pvars->MobVars.BlipType = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
}

//--------------------------------------------------------------------------
void reaperOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// limit corn spawning to prevent freezing/framelag
	if (MapConfig.State && MapConfig.State->MobStats.TotalAlive < 30)
	{
	}
}

//--------------------------------------------------------------------------
void reaperOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;
	float damage = e->DamageQuarters / 4.0;
	float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.State != REAPER_STATE_FLINCH && pvars->MobVars.State != REAPER_STATE_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & MOB_DAMAGE_FLAG_SHOCK;
	int isShortFreeze = e->DamageFlags & MOB_DAMAGE_FLAG_SHORT_FREEZE;

	// destroy
	if (newHp <= 0)
	{
		reaperForceLocalState(moby, REAPER_STATE_DIE);
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}

	// trigger aggro
	Player *sourcePlayer = playerGetFromUID(e->SourceUID);
	if (!reaperVars->AggroTriggered && sourcePlayer)
	{
		reaperVars->AggroTriggered = 1;
		reaperVars->AggroTriggeredBy = sourcePlayer;
		pvars->MobVars.Target = playerGetTargetMoby(sourcePlayer);
		DPRINTF("aggro triggered by %d\n", sourcePlayer->PlayerId);
	}

	float damageRatio = damage / pvars->MobVars.Config.Health;
	float pFactor = reaperVars->AggroTriggered ? 0.5 : 1;
	float powerFactor = REAPER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
	float probability = (pFactor * damageRatio * REAPER_FLINCH_PROBABILITY) + powerFactor;
	mobHandleFlinch(moby, e, canFlinch, isShock, probability, powerFactor, REAPER_STATE_FLINCH, REAPER_STATE_BIG_FLINCH);

	// short freeze
	if (isShortFreeze && pvars->MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS)
	{
		pvars->MobVars.SlowTicks = MOB_SHORT_FREEZE_DURATION_TICKS;
		mobResetMoveStep(moby);
	}
}

//--------------------------------------------------------------------------
int reaperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
}

//--------------------------------------------------------------------------
void reaperOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e)
{
	mobOnFullStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *reaperGetNextTarget(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *currentTarget = pvars->MobVars.Target;

	// target player who hit us
	Moby *aggroTriggeredByTarget = playerGetTargetMoby(reaperVars->AggroTriggeredBy);
	if (reaperVars->AggroTriggered && aggroTriggeredByTarget)
	{
		reaperVars->AggroTriggeredBy = NULL;
		return aggroTriggeredByTarget;
	}

	// don't change target when aggro
	if (pvars->MobVars.State == REAPER_STATE_AGGRO && currentTarget)
	{
		Player *currentPlayerTarget = guberMobyGetPlayerDamager(currentTarget);
		if (currentPlayerTarget && !playerIsDead(currentPlayerTarget))
		{
			return currentTarget;
		}
	}

	return mobGetNextTarget(moby, REAPER_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int reaperGetPreferredState(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;

	// no preferred state
	if (reaperIsAttacking(moby))
		return -1;

	if (reaperIsSpawning(pvars))
		return -1;

	if (reaperIsFlinching(moby))
		return -1;

	if (pvars->MobVars.State == REAPER_STATE_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return REAPER_STATE_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > REAPER_MAX_WALKABLE_SLOPE)
	{
		return REAPER_STATE_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return REAPER_STATE_JUMP;
	}

	// prevent state changing too quickly
	if (pvars->MobVars.StateCooldownTicks)
		return -1;

	// get next target
	Moby *target = reaperGetNextTarget(moby);
	if (target)
	{
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		if (dist <= attackRadius)
		{
			if (reaperCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return REAPER_STATE_ATTACK;
			}

			// wait for sprint to finish
			if (pvars->MobVars.State == REAPER_STATE_AGGRO)
				return -1;

			return reaperVars->AggroTriggered ? REAPER_STATE_AGGRO : REAPER_STATE_WALK;
		}
		else
		{

			// wait for sprint to finish
			if (pvars->MobVars.State == REAPER_STATE_AGGRO)
				return -1;

			return reaperVars->AggroTriggered ? REAPER_STATE_AGGRO : REAPER_STATE_WALK;
		}
	}

	return REAPER_STATE_IDLE;
}

//--------------------------------------------------------------------------
#if DEBUG_PATH
void reaperRenderPath(Moby *moby)
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
void reaperDoState(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? REAPER_TURN_RADIANS_PER_SEC : REAPER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? REAPER_MOVE_ACCELERATION : REAPER_MOVE_AIR_ACCELERATION;
	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

#if DEBUG_PATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&reaperRenderPath, moby);
#endif

	switch (pvars->MobVars.State)
	{
	case REAPER_STATE_SPAWN:
	{
		mobTransAnim(moby, REAPER_ANIM_SPAWN, 0);
		mobStand(moby);
		break;
	}
	case REAPER_STATE_FLINCH:
	case REAPER_STATE_BIG_FLINCH:
	{
		decTimerU8(&pvars->MobVars.Knockback.Ticks);
		int animFlinchId = pvars->MobVars.State == REAPER_STATE_BIG_FLINCH ? REAPER_ANIM_FLINCH_KNOCKBACK : REAPER_ANIM_FLINCH;

		mobTransAnim(moby, animFlinchId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, REAPER_KNOCKBACK_MULTIPLIER);
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
	case REAPER_STATE_IDLE:
	{
		mobTransAnim(moby, REAPER_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case REAPER_STATE_JUMP:
	{
		// move
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

		// handle jumping
		if (pvars->MobVars.MoveVars.Grounded)
		{
			// mobTransAnim(moby, jumpAnimId, 5);

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
				jumpSpeed = REAPER_DEFAULT_JUMP_SPEED;
			}

			// DPRINTF("jump %f\n", jumpSpeed);
			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case REAPER_STATE_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
			mobTurnTowards(moby, target->Position, turnSpeed);
		break;
	}
	case REAPER_STATE_AGGRO:
	{
		int nextAnimId = moby->AnimSeqId;

		switch (moby->AnimSeqId)
		{
		case REAPER_ANIM_AGGRO_ROAR:
		{
			if (pvars->MobVars.AnimationLooped)
			{
				nextAnimId = REAPER_ANIM_RUN;
			}
			break;
		}
		}

		if (!pvars->MobVars.CurrentStateForTicks)
		{
			nextAnimId = REAPER_ANIM_AGGRO_ROAR;
		}

		mobTransAnim(moby, nextAnimId, 0);

		// let aggro roar finish before moving
		if (nextAnimId == REAPER_ANIM_AGGRO_ROAR)
		{
			if (target)
			{
				mobTurnTowards(moby, target->Position, turnSpeed);
			}

			mobStand(moby);
			break;
		}

		if (target)
		{
			if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
				pvars->MobVars.Dirty = 1; // new path, sync with other clients
			mobMoveTowards(moby, t, pvars->MobVars.Config.Speed * 3, turnSpeed, acceleration, mobGetCurrentWalkAngle(moby));
		}
		else
		{
			// stand
			mobStand(moby);
		}

		//
		if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			reaperForceLocalState(moby, REAPER_STATE_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, REAPER_ANIM_RUN, 0);
		}
		else if (moby->AnimSeqId != REAPER_ANIM_RUN || pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, REAPER_ANIM_IDLE, 0);
		}
		break;
	}
	case REAPER_STATE_WALK:
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

		//
		if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			reaperForceLocalState(moby, REAPER_STATE_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, REAPER_ANIM_WALK, 0);
		}
		else if (moby->AnimSeqId != REAPER_ANIM_WALK || pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, REAPER_ANIM_IDLE, 0);
		}
		break;
	}
	case REAPER_STATE_DIE:
	{
		mobTransAnim(moby, REAPER_ANIM_FALL_AND_DIE, 0);
		if (moby->AnimSeqId == REAPER_ANIM_FALL_AND_DIE && pvars->MobVars.AnimationLooped)
		{
			pvars->MobVars.Destroy = 1;
		}

		mobStand(moby);
		break;
	}
	case REAPER_STATE_ATTACK:
	{
		int attack1AnimId = REAPER_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < REAPER_ATTACK_EARLY_PHASE_FRAME_END) ? (difficulty * 2) : 1, 1, 5);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= REAPER_ATTACK_HIT_FRAME_START && moby->AnimSeqT < REAPER_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (target)
		{
			mobMoveTowards(moby, target->Position, speedMult * pvars->MobVars.Config.Speed, turnSpeed, acceleration, 0);
		}
		else
		{
			mobStand(moby);
		}

		if (swingAttackReady && damageFlags)
		{
			// aggro does more damage
			reaperDoDamage(
					moby,
					pvars->MobVars.Config.HitRadius,
					pvars->MobVars.Config.Damage * ((pvars->MobVars.LastState == REAPER_STATE_AGGRO) ? REAPER_AGGRO_DAMAGE_MULTIPLIER : 1),
					damageFlags,
					0);
		}
		break;
	}
	}

	pvars->MobVars.CurrentStateForTicks++;
}

//--------------------------------------------------------------------------
void reaperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;

	// aggro ends when we finally hit our target
	if (mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, REAPER_SUBSKELETON_LEFT_HAND, 1, 0) & MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET)
	{
		reaperVars->AggroTriggered = 0;
	}
}

//--------------------------------------------------------------------------
void reaperForceLocalState(Moby *moby, int state)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float difficulty = 1;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.State)
	{
	case REAPER_STATE_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case REAPER_STATE_DIE:
	{
		// can't undie
		return;
	}
	}

	// to
	switch (state)
	{
	case REAPER_STATE_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case REAPER_STATE_AGGRO:
	{
		break;
	}
	case REAPER_STATE_WALK:
	{

		break;
	}
	case REAPER_STATE_DIE:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case REAPER_STATE_ATTACK:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		break;
	}
	case REAPER_STATE_FLINCH:
	case REAPER_STATE_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = REAPER_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.StateCooldownTicks = REAPER_STATE_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short reaperGetArmor(Moby *moby)
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
int reaperIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.State == REAPER_STATE_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reaperCanNonOwnerTransitionToState(Moby *moby, int state)
{
	// always let non-owners simulate an state unless its the death state
	if (state == REAPER_STATE_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int reaperShouldForceStateUpdateOnState(Moby *moby, int state)
{
	// only send state updates at regular intervals, unless dying or flinching
	if (state == REAPER_STATE_DIE || state == REAPER_STATE_FLINCH || state == REAPER_STATE_BIG_FLINCH)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int reaperIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.State == REAPER_STATE_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reaperCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int reaperIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == REAPER_ANIM_FLINCH || moby->AnimSeqId == REAPER_ANIM_FLINCH_KNOCKBACK) && !pvars->MobVars.AnimationLooped;
}
