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
#include "maputils.h"
#include "utils.h"
#include "shared.h"

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
void reaperOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *reaperGetNextTarget(Moby *moby);
int reaperGetPreferredAction(Moby *moby, int *delayTicks);
void reaperDoAction(Moby *moby);
void reaperDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void reaperForceLocalAction(Moby *moby, int action);
short reaperGetArmor(Moby *moby);
int reaperIsAttacking(Moby *moby);
int reaperCanNonOwnerTransitionToAction(Moby *moby, int action);
int reaperShouldForceStateUpdateOnAction(Moby *moby, int action);

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
		.OnStateUpdate = &reaperOnStateUpdate,
		.GetNextTarget = &reaperGetNextTarget,
		.GetPreferredAction = &reaperGetPreferredAction,
		.ForceLocalAction = &reaperForceLocalAction,
		.DoAction = &reaperDoAction,
		.DoDamage = &reaperDoDamage,
		.GetArmor = &reaperGetArmor,
		.IsAttacking = &reaperIsAttacking,
		.CanNonOwnerTransitionToAction = &reaperCanNonOwnerTransitionToAction,
		.ShouldForceStateUpdateOnAction = &reaperShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
void reaperPreUpdate(Moby *moby)
{
	int i;
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// decrement tickers regardless of frozen state
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
		decTimerU16(&pvars->MobVars.LocalPlayerDamageHitInvTimer[i]);

	if (mobIsFrozen(moby))
		return;

	// decrement path target pos ticker
	decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);

	mobPreUpdate(moby);
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
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == REAPER_ACTION_WALK))
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

	int canFlinch = pvars->MobVars.Action != REAPER_ACTION_FLINCH && pvars->MobVars.Action != REAPER_ACTION_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & 0x40;
	int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0)
	{
		reaperForceLocalAction(moby, REAPER_ACTION_DIE);
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}

	// knockback
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		memcpy(&pvars->MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
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

	// flinch
	if (mobAmIOwner(moby))
	{
		float damageRatio = damage / pvars->MobVars.Config.Health;
		float pFactor = reaperVars->AggroTriggered ? 0.5 : 1;
		float powerFactor = REAPER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
		float probability = (pFactor * damageRatio * REAPER_FLINCH_PROBABILITY) + powerFactor;

#if ALWAYS_FLINCH
		probability = 2;
		powerFactor = 2;
#endif

		if (canFlinch)
		{
			if (e->Knockback.Force)
			{
				mobSetAction(moby, REAPER_ACTION_BIG_FLINCH);
			}
			else if (isShock)
			{
				mobSetAction(moby, REAPER_ACTION_FLINCH);
			}
			else if (randRange(0, 1) < probability)
			{
				if (randRange(0, 1) < powerFactor)
				{
					mobSetAction(moby, REAPER_ACTION_BIG_FLINCH);
				}
				else
				{
					mobSetAction(moby, REAPER_ACTION_FLINCH);
				}
			}
		}
	}

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
	// we want to give each local player a cooldown on damage they can apply to reaper
	if (!e->PlayerDamager)
		return 1;
	if (!e->PlayerDamager->IsLocal)
		return 1;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// only accept local damage when timer is 0
	int timer = pvars->MobVars.LocalPlayerDamageHitInvTimer[e->PlayerDamager->LocalPlayerIndex];
	if (timer == 0)
	{
		pvars->MobVars.LocalPlayerDamageHitInvTimer[e->PlayerDamager->LocalPlayerIndex] = pvars->MobVars.Config.DamageCooldownTickCount;
		return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
void reaperOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
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
	if (pvars->MobVars.Action == REAPER_ACTION_AGGRO && currentTarget)
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
int reaperGetPreferredAction(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;

	// no preferred action
	if (reaperIsAttacking(moby))
		return -1;

	if (reaperIsSpawning(pvars))
		return -1;

	if (reaperIsFlinching(moby))
		return -1;

	if (pvars->MobVars.Action == REAPER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return REAPER_ACTION_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > REAPER_MAX_WALKABLE_SLOPE)
	{
		return REAPER_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return REAPER_ACTION_JUMP;
	}

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
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
				return REAPER_ACTION_ATTACK;
			}

			// wait for sprint to finish
			if (pvars->MobVars.Action == REAPER_ACTION_AGGRO)
				return -1;

			return reaperVars->AggroTriggered ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
		}
		else
		{

			// wait for sprint to finish
			if (pvars->MobVars.Action == REAPER_ACTION_AGGRO)
				return -1;

			return reaperVars->AggroTriggered ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
		}
	}

	return REAPER_ACTION_IDLE;
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
void reaperDoAction(Moby *moby)
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

	switch (pvars->MobVars.Action)
	{
	case REAPER_ACTION_SPAWN:
	{
		mobTransAnim(moby, REAPER_ANIM_SPAWN, 0);
		mobStand(moby);
		break;
	}
	case REAPER_ACTION_FLINCH:
	case REAPER_ACTION_BIG_FLINCH:
	{
		int animFlinchId = pvars->MobVars.Action == REAPER_ACTION_BIG_FLINCH ? REAPER_ANIM_FLINCH_KNOCKBACK : REAPER_ANIM_FLINCH;

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
		else if (pvars->MobVars.CurrentActionForTicks > (1 * TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter)
		{
			mobStand(moby);
		}
		break;
	}
	case REAPER_ACTION_IDLE:
	{
		mobTransAnim(moby, REAPER_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case REAPER_ACTION_JUMP:
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
				jumpSpeed = 8; // clamp(0 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->MobVars.MoveVars.WallSlope) * 1, 3, 15);
			}

			// DPRINTF("jump %f\n", jumpSpeed);
			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case REAPER_ACTION_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
			mobTurnTowards(moby, target->Position, turnSpeed);
		break;
	}
	case REAPER_ACTION_AGGRO:
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

		if (!pvars->MobVars.CurrentActionForTicks)
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
			reaperForceLocalAction(moby, REAPER_ACTION_JUMP);
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
	case REAPER_ACTION_WALK:
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
			reaperForceLocalAction(moby, REAPER_ACTION_JUMP);
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
	case REAPER_ACTION_DIE:
	{
		mobTransAnim(moby, REAPER_ANIM_FALL_AND_DIE, 0);
		if (moby->AnimSeqId == REAPER_ANIM_FALL_AND_DIE && pvars->MobVars.AnimationLooped)
		{
			pvars->MobVars.Destroy = 1;
		}

		mobStand(moby);
		break;
	}
	case REAPER_ACTION_ATTACK:
	{
		int attack1AnimId = REAPER_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1, 1, 5);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 14 && moby->AnimSeqT < 17;
		u32 damageFlags = 0x00081801;

		if (target)
		{
			mobMoveTowards(moby, target->Position, speedMult * pvars->MobVars.Config.Speed, turnSpeed, acceleration, 0);
		}
		else
		{
			mobStand(moby);
		}

		// attribute damage
		switch (pvars->MobVars.Config.MobAttribute)
		{
		case MOB_ATTRIBUTE_FREEZE:
		{
			damageFlags |= 0x00800000;
			break;
		}
		case MOB_ATTRIBUTE_ACID:
		{
			damageFlags |= 0x00000080;
			break;
		}
		}

		if (swingAttackReady && damageFlags)
		{
			// aggro does more damage
			reaperDoDamage(
					moby,
					pvars->MobVars.Config.HitRadius,
					pvars->MobVars.Config.Damage * ((pvars->MobVars.LastAction == REAPER_ACTION_AGGRO) ? REAPER_AGGRO_DAMAGE_MULTIPLIER : 1),
					damageFlags,
					0);
		}
		break;
	}
	}

	pvars->MobVars.CurrentActionForTicks++;
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
void reaperForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float difficulty = 1;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
	case REAPER_ACTION_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case REAPER_ACTION_DIE:
	{
		// can't undie
		return;
	}
	}

	// to
	switch (action)
	{
	case REAPER_ACTION_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case REAPER_ACTION_AGGRO:
	{
		break;
	}
	case REAPER_ACTION_WALK:
	{

		break;
	}
	case REAPER_ACTION_DIE:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case REAPER_ACTION_ATTACK:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		break;
	}
	case REAPER_ACTION_FLINCH:
	case REAPER_ACTION_BIG_FLINCH:
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
	if (action != pvars->MobVars.Action)
		pvars->MobVars.CurrentActionForTicks = 0;

	pvars->MobVars.Action = action;
	pvars->MobVars.NextAction = -1;
	pvars->MobVars.ActionCooldownTicks = REAPER_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short reaperGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;
	int bangles = pvars->MobVars.Config.Bangles;

	if (t < 0.3)
		return 0x0000;
	else if (t < 0.7)
		return bangles & 0x1f; // remove torso bangle

	return bangles;
}

//--------------------------------------------------------------------------
int reaperIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == REAPER_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reaperCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// always let non-owners simulate an action unless its the death action
	if (action == REAPER_ACTION_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int reaperShouldForceStateUpdateOnAction(Moby *moby, int action)
{
	// only send state updates at regular intervals, unless dying
	if (action == REAPER_ACTION_DIE)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int reaperIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == REAPER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
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
