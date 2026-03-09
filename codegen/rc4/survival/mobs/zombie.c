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
#include "utils.h"
#include "maputils.h"
#include "shared.h"

void zombiePreUpdate(Moby *moby);
void zombiePostUpdate(Moby *moby);
void zombiePostDraw(Moby *moby);
void zombieMove(Moby *moby);
int zombieGetExtraDataSize(int spawnParamsIdx);
void zombieOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void zombieOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void zombieOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void zombieOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int zombieOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void zombieOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *zombieGetNextTarget(Moby *moby);
int zombieGetPreferredAction(Moby *moby, int *delayTicks);
void zombieDoAction(Moby *moby);
void zombieDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void zombieForceLocalAction(Moby *moby, int action);
short zombieGetArmor(Moby *moby);
int zombieIsAttacking(Moby *moby);
int zombieCanNonOwnerTransitionToAction(Moby *moby, int action);
int zombieShouldForceStateUpdateOnAction(Moby *moby, int action);

int zombieIsSpawning(struct MobPVar *pvars);
int zombieCanAttack(struct MobPVar *pvars);
int zombieIsFlinching(Moby *moby);

struct MobVTable ZombieVTable = {
		.PreUpdate = &zombiePreUpdate,
		.PostUpdate = &zombiePostUpdate,
		.PostDraw = &zombiePostDraw,
		.Move = &zombieMove,
		.GetExtraDataSize = &zombieGetExtraDataSize,
		.OnSpawning = &zombieOnSpawning,
		.OnSpawn = &zombieOnSpawn,
		.OnDestroy = &zombieOnDestroy,
		.OnDamage = &zombieOnDamage,
		.OnLocalDamage = &zombieOnLocalDamage,
		.OnStateUpdate = &zombieOnStateUpdate,
		.GetNextTarget = &zombieGetNextTarget,
		.GetPreferredAction = &zombieGetPreferredAction,
		.ForceLocalAction = &zombieForceLocalAction,
		.DoAction = &zombieDoAction,
		.DoDamage = &zombieDoDamage,
		.GetArmor = &zombieGetArmor,
		.IsAttacking = &zombieIsAttacking,
		.CanNonOwnerTransitionToAction = &zombieCanNonOwnerTransitionToAction,
		.ShouldForceStateUpdateOnAction = &zombieShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
void zombiePreUpdate(Moby *moby)
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
void zombiePostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float animSpeed = 0.9 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP)
	{
		animSpeed = 0.9 * (1 - powf(moby->AnimSeqT / 35, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = 0.9;
		}
	}
	else if (zombieIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == ZOMBIE_ACTION_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void zombiePostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 1, color, 1);
}

//--------------------------------------------------------------------------
void zombieAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void zombieMove(Moby *moby)
{
	mobMove(moby);
}

//--------------------------------------------------------------------------
int zombieGetExtraDataSize(int spawnParamsIdx)
{
	return 0;
}

//--------------------------------------------------------------------------
void zombieOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
}

//--------------------------------------------------------------------------
void zombieOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
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
void zombieOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// limit corn spawning to prevent freezing/framelag
	if (MapConfig.State && MapConfig.State->MobStats.TotalAlive < 30 && mobyGetNumSpawnableMobys() > 100)
	{
		// mobSpawnCorn(moby, ZOMBIE_BANGLE_LARM | ZOMBIE_BANGLE_RARM | ZOMBIE_BANGLE_LLEG | ZOMBIE_BANGLE_RLEG | ZOMBIE_BANGLE_RFOOT | ZOMBIE_BANGLE_HIPS);
	}
}

//--------------------------------------------------------------------------
void zombieOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
	float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != ZOMBIE_ACTION_FLINCH && pvars->MobVars.Action != ZOMBIE_ACTION_BIG_FLINCH && pvars->MobVars.Action != ZOMBIE_ACTION_TIME_BOMB && pvars->MobVars.Action != ZOMBIE_ACTION_TIME_BOMB_EXPLODE && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & 0x40;
	int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0)
	{
		if (pvars->MobVars.Action == ZOMBIE_ACTION_TIME_BOMB && moby->AnimSeqId == ZOMBIE_ANIM_CROUCH && moby->AnimSeqT > 3)
		{
			// explode
			// zombieForceLocalAction(moby, ZOMBIE_ACTION_TIME_BOMB_EXPLODE);
			zombieForceLocalAction(moby, ZOMBIE_ACTION_DIE);
		}
		else
		{
			zombieForceLocalAction(moby, ZOMBIE_ACTION_DIE);
		}

		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
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
		float powerFactor = ZOMBIE_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
		float probability = clamp((damageRatio * ZOMBIE_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
		probability = 2;
		powerFactor = 2;
#endif

		if (canFlinch)
		{
			if (e->Knockback.Force)
			{
				mobSetAction(moby, ZOMBIE_ACTION_BIG_FLINCH);
			}
			else if (isShock)
			{
				mobSetAction(moby, ZOMBIE_ACTION_FLINCH);
			}
			else if (randRange(0, 1) < probability)
			{
				if (randRange(0, 1) < powerFactor)
				{
					mobSetAction(moby, ZOMBIE_ACTION_BIG_FLINCH);
				}
				else
				{
					mobSetAction(moby, ZOMBIE_ACTION_FLINCH);
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
int zombieOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	// we want to give each local player a cooldown on damage they can apply to zombie
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
void zombieOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *zombieGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby, ZOMBIE_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int zombieGetPreferredAction(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// no preferred action
	if (zombieIsAttacking(moby))
		return -1;

	if (zombieIsSpawning(pvars))
		return -1;

	if (zombieIsFlinching(moby))
		return -1;

	if (pvars->MobVars.Action == ZOMBIE_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return ZOMBIE_ACTION_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > ZOMBIE_MAX_WALKABLE_SLOPE)
	{
		return ZOMBIE_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return ZOMBIE_ACTION_JUMP;
	}

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby *target = zombieGetNextTarget(moby);
	if (target)
	{
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		if (dist <= attackRadius)
		{
			if (zombieCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return pvars->MobVars.Config.MobAttribute != MOB_ATTRIBUTE_EXPLODE ? ZOMBIE_ACTION_ATTACK : ZOMBIE_ACTION_TIME_BOMB;
			}
			return ZOMBIE_ACTION_WALK;
		}
		else
		{
			return ZOMBIE_ACTION_WALK;
		}
	}

	return ZOMBIE_ACTION_IDLE;
}

//--------------------------------------------------------------------------
#if DEBUG_PATH
void zombieRenderPath(Moby *moby)
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
void zombieDoAction(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_TURN_RADIANS_PER_SEC : ZOMBIE_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_MOVE_ACCELERATION : ZOMBIE_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == ZOMBIE_ACTION_FLINCH || pvars->MobVars.LastAction == ZOMBIE_ACTION_BIG_FLINCH);

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

#if DEBUG_PATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&zombieRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
	case ZOMBIE_ACTION_SPAWN:
	{
		mobTransAnim(moby, ZOMBIE_ANIM_CRAWL_OUT_OF_GROUND, 0);
		mobStand(moby);
		break;
	}
	case ZOMBIE_ACTION_FLINCH:
	case ZOMBIE_ACTION_BIG_FLINCH:
	{
		decTimerU8(&pvars->MobVars.Knockback.Ticks);
		int animFlinchId = pvars->MobVars.Action == ZOMBIE_ACTION_BIG_FLINCH ? ZOMBIE_ANIM_BIG_FLINCH : ZOMBIE_ANIM_BIG_FLINCH;

		mobTransAnim(moby, animFlinchId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == ZOMBIE_ACTION_BIG_FLINCH)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, ZOMBIE_KNOCKBACK_MULTIPLIER);
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
	case ZOMBIE_ACTION_IDLE:
	{
		mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case ZOMBIE_ACTION_JUMP:
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
			mobTransAnim(moby, ZOMBIE_ANIM_JUMP, 5);
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
				jumpSpeed = 8; // clamp(0 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->MobVars.MoveVars.WallSlope) * 1, 3, 15);
			}

			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case ZOMBIE_ACTION_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
			mobTurnTowards(moby, target->Position, turnSpeed);
		break;
	}
	case ZOMBIE_ACTION_WALK:
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
		if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded)
		{
			// wait for jump to land
		}
		else if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			zombieForceLocalAction(moby, ZOMBIE_ACTION_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, ZOMBIE_ANIM_RUN, 0);
		}
		else if (moby->AnimSeqId != ZOMBIE_ANIM_RUN || pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
		}
		break;
	}
	case ZOMBIE_ACTION_DIE:
	{
		mobStand(moby);
		break;
	}
	case ZOMBIE_ACTION_TIME_BOMB_EXPLODE:
	{

		break;
	}
	case ZOMBIE_ACTION_TIME_BOMB:
	{
		mobTransAnim(moby, ZOMBIE_ANIM_CROUCH, 0);

		if (pvars->MobVars.TimeBombTicks == 0)
		{
			moby->Opacity = 0x80;
			pvars->MobVars.OpacityFlickerDirection = 0;
			mobSetAction(moby, ZOMBIE_ACTION_TIME_BOMB_EXPLODE);
		}
		else
		{

			// cycle opacity from 1.0 to 2.0
			int newOpacity = (u8)moby->Opacity + pvars->MobVars.OpacityFlickerDirection;
			if (newOpacity <= 0x80)
			{
				pvars->MobVars.OpacityFlickerDirection *= -1.25;
				newOpacity = 0x80;
			}
			else if (newOpacity >= 0xFF)
			{
				pvars->MobVars.OpacityFlickerDirection *= -1.25;
				newOpacity = 0xFF;
			}

			// limit cycle rate
			if (pvars->MobVars.OpacityFlickerDirection < -0x40)
				pvars->MobVars.OpacityFlickerDirection = -0x40;
			if (pvars->MobVars.OpacityFlickerDirection > 0x40)
				pvars->MobVars.OpacityFlickerDirection = 0x40;

			moby->Opacity = (u8)newOpacity;
			mobStand(moby);
		}
		break;
	}
	case ZOMBIE_ACTION_ATTACK:
	{
		int attack1AnimId = ZOMBIE_ANIM_SLAP;
		mobTransAnim(moby, attack1AnimId, 0);

		float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1, 1, 5);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 11 && moby->AnimSeqT < 12;
		u32 damageFlags = 0x00081801;

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
			zombieDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
		}
		break;
	}
	}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void zombieDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, ZOMBIE_SUBSKELETON_JOINT_LEFT_HAND, 1, 0);
}

//--------------------------------------------------------------------------
void zombieForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float difficulty = 1;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
	case ZOMBIE_ACTION_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case ZOMBIE_ACTION_DIE:
	{
		// can't undie
		return;
	}
	}

	// to
	switch (action)
	{
	case ZOMBIE_ACTION_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case ZOMBIE_ACTION_WALK:
	{

		break;
	}
	case ZOMBIE_ACTION_DIE:
	{
		pvars->MobVars.Destroy = 1;
		break;
	}
	case ZOMBIE_ACTION_ATTACK:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		break;
	}
	case ZOMBIE_ACTION_TIME_BOMB_EXPLODE:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;

		u32 damageFlags = 0x00008801;
		u32 color = 0x403064FF;

		// attribute damage
		switch (pvars->MobVars.Config.MobAttribute)
		{
		case MOB_ATTRIBUTE_FREEZE:
		{
			color = 0x40FF6430;
			damageFlags |= 0x00800000;
			break;
		}
		case MOB_ATTRIBUTE_ACID:
		{
			color = 0x4064FF30;
			damageFlags |= 0x00000080;
			break;
		}
		}

		mobyPlaySoundByClass(0, 0, mobySpawnExplosion(vector_read(moby->Position), 0, 0, 0, 0, 16, 0, 16, 0, 1, 0, 0, 0, 0, 0, 0, color, color, color, color, color, color, color, color, color, 0, 0, 0, 0, ZOMBIE_EXPLODE_HIT_RADIUS / 2.5, 0, 0, 0), MOBY_ID_ARBITER_ROCKET0);
		mobDoDamage(moby, ZOMBIE_EXPLODE_HIT_RADIUS, pvars->MobVars.Config.Damage, damageFlags, 1, ZOMBIE_SUBSKELETON_JOINT_HIPS, 1, 1);
		pvars->MobVars.Destroy = 1;
		pvars->MobVars.LastHitBy = -1;
		break;
	}
	case ZOMBIE_ACTION_TIME_BOMB:
	{
		pvars->MobVars.OpacityFlickerDirection = 4;
		pvars->MobVars.TimeBombTicks = ZOMBIE_TIMEBOMB_TICKS;
		break;
	}
	case ZOMBIE_ACTION_FLINCH:
	case ZOMBIE_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = ZOMBIE_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = ZOMBIE_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short zombieGetArmor(Moby *moby)
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
int zombieIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == ZOMBIE_ACTION_TIME_BOMB || pvars->MobVars.Action == ZOMBIE_ACTION_TIME_BOMB_EXPLODE || (pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK && !pvars->MobVars.AnimationLooped);
}

//--------------------------------------------------------------------------
int zombieCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// always let non-owners simulate an action unless its the death action
	if (action == ZOMBIE_ACTION_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int zombieShouldForceStateUpdateOnAction(Moby *moby, int action)
{
	// only send state updates at regular intervals, unless dying or flinching
	if (action == ZOMBIE_ACTION_DIE || action == ZOMBIE_ACTION_FLINCH || action == ZOMBIE_ACTION_BIG_FLINCH)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int zombieIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == ZOMBIE_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int zombieCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int zombieIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == ZOMBIE_ANIM_FLINCH || moby->AnimSeqId == ZOMBIE_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
}
