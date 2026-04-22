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
#include "laserbeam.h"
#include "maputils.h"
#include "pathfind.h"

void leviathanPreUpdate(Moby *moby);
void leviathanPostUpdate(Moby *moby);
void leviathanPostDraw(Moby *moby);
void leviathanMove(Moby *moby);
int leviathanGetExtraDataSize(int spawnParamsIdx);
void leviathanOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void leviathanOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void leviathanOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void leviathanOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int leviathanOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void leviathanOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e);
Moby *leviathanGetNextTarget(Moby *moby);
enum LeviathanStates leviathanGetPreferredAttack(Moby *moby);
int leviathanGetPreferredState(Moby *moby, int *delayTicks);
void leviathanDoState(Moby *moby);
void leviathanDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void leviathanForceLocalState(Moby *moby, int state);
short leviathanGetArmor(Moby *moby);
int leviathanIsAttacking(Moby *moby);
int leviathanCanNonOwnerTransitionToState(Moby *moby, int state);
int leviathanShouldForceStateUpdateOnState(Moby *moby, int state);

int leviathanIsSpawning(struct MobPVar *pvars);
int leviathanIsIdling(struct MobPVar *pvars);
int leviathanCanAttack(struct MobPVar *pvars);
int leviathanIsFlinching(Moby *moby);
int leviathanIsDying(Moby *moby);
int leviathanShouldStrafe(Moby *moby);
int leviathanShouldChase(Moby *moby);
int leviathanGetLaserForTicks(Moby *moby);

struct MobVTable LeviathanVTable = {
		.PreUpdate = &leviathanPreUpdate,
		.PostUpdate = &leviathanPostUpdate,
		.PostDraw = &leviathanPostDraw,
		.Move = &leviathanMove,
		.GetExtraDataSize = &leviathanGetExtraDataSize,
		.OnSpawning = &leviathanOnSpawning,
		.OnSpawn = &leviathanOnSpawn,
		.OnDestroy = &leviathanOnDestroy,
		.OnDamage = &leviathanOnDamage,
		.OnLocalDamage = &leviathanOnLocalDamage,
		.OnFullStateUpdate = &leviathanOnFullStateUpdate,
		.GetNextTarget = &leviathanGetNextTarget,
		.GetPreferredState = &leviathanGetPreferredState,
		.ForceLocalState = &leviathanForceLocalState,
		.DoState = &leviathanDoState,
		.DoDamage = &leviathanDoDamage,
		.GetArmor = &leviathanGetArmor,
		.IsAttacking = &leviathanIsAttacking,
		.CanNonOwnerTransitionToState = &leviathanCanNonOwnerTransitionToState,
		.ShouldForceStateUpdateOnState = &leviathanShouldForceStateUpdateOnState,
};

//--------------------------------------------------------------------------
int leviathanIsBoss(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Config.MobAttribute == MOB_ATTRIBUTE_BOSS;
}

//--------------------------------------------------------------------------
int leviathanIsEvasive(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (!leviathanIsBoss(moby))
		return mobGetBehavior(moby) == LEVIATHAN_BEHAVIOR_RANGED;

	float healthPerc = (pvars->MobVars.Health / pvars->MobVars.Config.Health);
	if (healthPerc > 0.25 && healthPerc < 0.5)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int leviathanIsAggressive(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (!leviathanIsBoss(moby))
		return 0;

	float healthPerc = (pvars->MobVars.Health / pvars->MobVars.Config.Health);
	if (healthPerc < 0.25)
		return 1;
	if (healthPerc > 0.5 && healthPerc < 0.75)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
float leviathanGetScale(Moby *moby)
{
	return mobGetScaleMultiplier(moby);
}

//--------------------------------------------------------------------------
int leviathanGetLaserCooldownTicks(Moby *moby)
{
	int ticks = randRangeInt(LEVIATHAN_LASER_COOLDOWN_TICKS_MIN, LEVIATHAN_LASER_COOLDOWN_TICKS_MAX);

	// evasive so laser more often
	if (leviathanIsEvasive(moby))
		ticks /= 8;

	return ticks;
}

//--------------------------------------------------------------------------
void leviathanPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);

	if (!mobIsFrozen(moby))
		decTimerU32(&leviathanVars->AttackLaserCooldownTicks);
}

//--------------------------------------------------------------------------
void leviathanPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	// float scale = leviathanGetScale(moby);
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;

	//
	if (leviathanVars->AttackLaserCooldownTicks == 0)
	{
		leviathanVars->AttackLaserCooldownTicks = leviathanGetLaserCooldownTicks(moby);
	}

	// adjust animSpeed by speed and by animation
	float baseSpeed = 0.5;
	float animSpeed = baseSpeed;

	if (moby->AnimSeqId == LEVIATHAN_ANIM_JUMP)
	{
		animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / LEVIATHAN_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
		}
	}
	else if (leviathanIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / LEVIATHAN_FLINCH_ANIM_DURATION, 2));
	}
	else if (leviathanIsDying(moby))
	{
		animSpeed = 1;
	}
	else if (moby->AnimSeqId == LEVIATHAN_ANIM_WALK || moby->AnimSeqId == LEVIATHAN_ANIM_WALK_LEFT || moby->AnimSeqId == LEVIATHAN_ANIM_WALK_RIGHT)
	{
		animSpeed *= mobGetCurrentMoveSpeed(moby);
	}

	// scale up attack and walk animations by speed
	switch (moby->AnimSeqId)
	{
	case LEVIATHAN_ANIM_SWING:
	case LEVIATHAN_ANIM_STAB_DOWN:
	case LEVIATHAN_ANIM_WALK:
	case LEVIATHAN_ANIM_WALK_LEFT:
	case LEVIATHAN_ANIM_WALK_RIGHT:
	{
		animSpeed *= (pvars->MobVars.Config.Speed / MOB_BASE_SPEED);
		break;
	}
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && !leviathanIsAttacking(moby) && !leviathanIsSpawning(pvars) && !leviathanIsDying(moby) && !leviathanIsFlinching(moby)))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void leviathanPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 2.2, color, LEVIATHAN_SUBSKELETON_JOINT_BODY);
}

//--------------------------------------------------------------------------
void leviathanMove(Moby *moby)
{
	mobMove(moby);
}

//--------------------------------------------------------------------------
int leviathanGetExtraDataSize(int spawnParamsIdx)
{
	return sizeof(LeviathanMobVars_t);
}

//--------------------------------------------------------------------------
void leviathanOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
}

//--------------------------------------------------------------------------
void leviathanOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(LeviathanMobVars_t));

	// set scale
	float scale = leviathanGetScale(moby);
	moby->Scale = 0.256339 * scale;

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
	vector_copy(pvars->MobVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void leviathanOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	if (leviathanVars->LaserbeamMoby)
	{
		laserbeamDestroy(leviathanVars->LaserbeamMoby);
		leviathanVars->LaserbeamMoby = NULL;
	}

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// spawn corn
	// mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void leviathanOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	float damage = e->DamageQuarters / 4.0;

	// take more damage in exhausted state
	if ((pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER || pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER_LOCKON) && moby->AnimSeqId == LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED)
	{
		e->DamageQuarters *= 2;
		damage *= 2;
	}

	float newHp = pvars->MobVars.Health - damage;
	int canFlinch = pvars->MobVars.State != LEVIATHAN_STATE_FLINCH && pvars->MobVars.State != LEVIATHAN_STATE_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

	// every quarter health reset laser cooldown
	if ((int)(newHp * 4) != (int)(pvars->MobVars.Health * 4))
	{
		leviathanVars->AttackLaserCooldownTicks = 1;
	}

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & MOB_DAMAGE_FLAG_SHOCK;
	int isShortFreeze = e->DamageFlags & MOB_DAMAGE_FLAG_SHORT_FREEZE;

	// destroy
	if (newHp <= 0)
	{
		leviathanForceLocalState(moby, LEVIATHAN_STATE_DIE);
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}

	float damageRatio = damage / pvars->MobVars.Config.Health;
	float powerFactor = LEVIATHAN_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
	float probability = clamp((damageRatio * LEVIATHAN_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);
	mobHandleFlinch(moby, e, canFlinch, isShock, probability, powerFactor, LEVIATHAN_STATE_FLINCH, LEVIATHAN_STATE_BIG_FLINCH);

	// auto aggro (owner-only)
	if (mobAmIOwner(moby))
	{
		// auto aggro
		if (!pvars->MobVars.Target)
		{
			Guber *guber = guberGetObjectByUID(e->SourceUID);
			Moby *moby = guber ? guber->VTable->GetMoby(guber) : NULL;
			Player *target = mobyGetPlayer(moby);
			if (target)
			{
				Moby *targetMoby = playerGetTargetMoby(target);
				if (targetMoby)
				{
					pvars->MobVars.Target = targetMoby;
					pvars->MobVars.Dirty = 1;
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
int leviathanOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
}

//--------------------------------------------------------------------------
void leviathanOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e)
{
	mobOnFullStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *leviathanGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby, LEVIATHAN_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
void leviathanComputeLaserTargets(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *target = pvars->MobVars.Target;

	VECTOR offset, dir, ndir, targetPos;
	if (target)
	{
		mobGetTargetCenter(target, targetPos);

		// if player, try and stay near their ground
		Player *targetPlayer = guberMobyGetPlayerDamager(target);
		if (targetPlayer)
		{
			vector_add(targetPos, targetPlayer->Ground.point, target->M2_03);
		}
	}
	else
	{
		vector_scale(dir, moby->M0_03, 25);
		vector_add(targetPos, moby->Position, dir);
	}
	vector_subtract(dir, targetPos, moby->Position);
	float dist = vector_length(dir);
	vector_scale(ndir, dir, 1 / dist);
	vector_outerproduct(offset, moby->M2_03, ndir);
	vector_scale(offset, offset, sinf(30 * MATH_DEG2RAD) * dist);
	vector_add(leviathanVars->LaserbeamTarget1, targetPos, offset);
	vector_subtract(leviathanVars->LaserbeamTarget2, targetPos, offset);
}

//--------------------------------------------------------------------------
enum LeviathanStates leviathanGetPreferredAttack(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *target = pvars->MobVars.Target;
	int isLaserState = pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER || pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER_LOCKON;
	if (!target)
		return -1;

	// check if target is within range
	VECTOR dt;
	vector_subtract(dt, target->Position, moby->Position);
	float distSqr = vector_sqrmag(dt);
	float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;
	float rangedAttackRadiusSqr = powf(MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].RangedAttackDistance, 2);
	int canRanged = mobGetBehavior(moby) != LEVIATHAN_BEHAVIOR_MELEE;
	if (distSqr > attackRadiusSqr && canRanged)
	{

		// check if moby is looking at (close to) target
		if (distSqr <= rangedAttackRadiusSqr && leviathanVars->AttackLaserCooldownTicks == 0)
		{
			float theta = acosf(vector_innerproduct(dt, moby->M0_03));
			if (!isLaserState && fabsf(theta) < (30 * MATH_DEG2RAD))
				return LEVIATHAN_STATE_ATTACK_LASER;
		}

		return -1;
	}

	// if target is right in front try stab
	VECTOR inFrontPos;

	vector_scale(dt, moby->M0_03, 1 * 2);
	vector_add(inFrontPos, moby->Position, dt);
	vector_subtract(dt, inFrontPos, target->Position);
	float dist = vector_length(dt) - mobGetTargetRadius(target);
	if (dist < 1)
	{
		return LEVIATHAN_STATE_ATTACK_STAB;
	}
	else if (dist < pvars->MobVars.Config.AttackRadius)
	{
		return LEVIATHAN_STATE_ATTACK_SWING;
	}

	return -1;
}

//--------------------------------------------------------------------------
int leviathanGetPreferredState(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// no preferred state
	if (leviathanIsAttacking(moby))
		return -1;

	if (leviathanIsSpawning(pvars))
		return -1;

	if (leviathanIsFlinching(moby))
		return -1;

	// if (pvars->MobVars.State == LEVIATHAN_STATE_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
	//   return -1;

	if (pvars->MobVars.State == LEVIATHAN_STATE_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return LEVIATHAN_STATE_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (!leviathanIsEvasive(moby) && pvars->MobVars.State != LEVIATHAN_STATE_STRAFE && mobHitWallShouldJump(moby, LEVIATHAN_MAX_WALKABLE_SLOPE))
	{
		return LEVIATHAN_STATE_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return LEVIATHAN_STATE_JUMP;
	}

	// prevent state changing too quickly
	if (pvars->MobVars.StateCooldownTicks)
		return -1;

	// get next target
	Moby *target = mobGetNextTarget(moby, LEVIATHAN_TARGET_KEEP_CURRENT_FACTOR);
	if (target)
	{
		if (leviathanCanAttack(pvars))
		{
			int preferredAttack = leviathanGetPreferredAttack(moby);
			if (preferredAttack >= 0)
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return preferredAttack;
			}
		}

		if (leviathanShouldChase(moby))
			return LEVIATHAN_STATE_CHASE;

		if (leviathanShouldStrafe(moby))
			return LEVIATHAN_STATE_STRAFE;

		return LEVIATHAN_STATE_WALK;
	}

	// idle for 3 seconds
	if (leviathanIsIdling(pvars) && pvars->MobVars.CurrentStateForTicks < TPS * 3)
	{
		return LEVIATHAN_STATE_IDLE;
	}

	return LEVIATHAN_STATE_IDLE;
}

//--------------------------------------------------------------------------
#if DEBUG_PATH
void leviathanRenderPath(Moby *moby)
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
int leviathanDoStateMove(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	// LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *target = pvars->MobVars.Target;
	VECTOR t;
	int evasive = leviathanIsEvasive(moby);
	// int aggressive = leviathanIsAggressive(moby);
	int strafe = evasive || pvars->MobVars.State == LEVIATHAN_STATE_STRAFE;
	float speed = pvars->MobVars.Config.Speed;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_TURN_RADIANS_PER_SEC : LEVIATHAN_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_MOVE_ACCELERATION : LEVIATHAN_MOVE_AIR_ACCELERATION;

	//
	VECTOR dt;
	vector_subtract(dt, target->Position, moby->Position);
	float sqrDistToTarget = vector_sqrmag(dt);
	float dir = mobGetCurrentWalkAngle(moby);

	// chase goes directly towards target, quickly
	if (pvars->MobVars.State == LEVIATHAN_STATE_CHASE)
	{
		speed *= LEVIATHAN_CHASE_SPEED_MULT;
		turnSpeed *= LEVIATHAN_CHASE_SPEED_MULT;
		strafe = 0;
		dir = 0;
	}
	else if (evasive)
	{
		speed *= LEVIATHAN_CHASE_SPEED_MULT;
		turnSpeed *= LEVIATHAN_CHASE_SPEED_MULT;
	}

	pvars->MobVars.MoveVars.ForceUseTargetPosition = strafe;
	float strafeDir = (pvars->MobVars.DynamicRandom % 2) ? 1 : -1;
#if DEBUG_MOVE
	strafeDir = fabsf(strafeDir);
#endif

	if (strafe)
	{
		VECTOR strafeVec, strafeFwd;
		vector_scale(strafeVec, moby->M1_03, 5 * strafeDir);
		if (dir != 0 && sqrDistToTarget < (LEVIATHAN_CHASE_TARGET_RADIUS * LEVIATHAN_CHASE_TARGET_RADIUS))
		{
			// move towards target if normal/aggro
			// move away if evasive
			vector_scale(strafeFwd, moby->M0_03, evasive ? -5 : 5);
			vector_add(strafeVec, strafeVec, strafeFwd);
		}

		vector_copy(pvars->MobVars.TargetPosition, moby->Position);
		vector_add(pvars->MobVars.TargetPosition, pvars->MobVars.TargetPosition, strafeVec);
	}

	if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
		pvars->MobVars.Dirty = 1; // new path, sync with other clients

	if (strafe)
	{
		VECTOR dt;
		vector_subtract(dt, t, moby->Position);
		float yaw = atan2f(dt[1], dt[0]);
		if (fabsf(mobTurnTowards(moby, target->Position, turnSpeed)) < (5 * MATH_DEG2RAD))
		{
			mobGetVelocityToTargetWithDirection(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, yaw, speed * LEVIATHAN_MOVE_STRAFE_MULT, acceleration);
		}
		return strafeDir > 0 ? LEVIATHAN_ANIM_WALK_LEFT : LEVIATHAN_ANIM_WALK_RIGHT;
	}
	else
	{
		mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
		return LEVIATHAN_ANIM_WALK;
	}
}

//--------------------------------------------------------------------------
void leviathanDoState(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *target = pvars->MobVars.Target;
	Moby *laserbeamMoby = leviathanVars->LaserbeamMoby;
	VECTOR t;
	int isBoss = leviathanIsBoss(moby);
	float difficulty = 1;
	// float speed = pvars->MobVars.Config.Speed;
	float turnSpeed = 1 * (pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_TURN_RADIANS_PER_SEC : LEVIATHAN_TURN_AIR_RADIANS_PER_SEC);
	// float acceleration = pvars->MobVars.MoveVars.Grounded ? LEVIATHAN_MOVE_ACCELERATION : LEVIATHAN_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastState == LEVIATHAN_STATE_FLINCH || pvars->MobVars.LastState == LEVIATHAN_STATE_BIG_FLINCH);

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

#if DEBUG_PATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&leviathanRenderPath, moby);
#endif

	// static int aaa = 0;
	// if (padGetButtonDown(0, PAD_RIGHT) && aaa > 0) {
	//   --aaa;
	//   DPRINTF("%d\n", aaa);
	//   mobTransAnim(moby, aaa, 0);
	// } else if (padGetButtonDown(0, PAD_DOWN) && aaa < (0x24 - 1)) {
	//   ++aaa;
	//   DPRINTF("%d\n", aaa);
	//   mobTransAnim(moby, aaa, 0);
	// }

	// mobStand(moby);
	// return;

	// default deactivate laserbeam
	// will get set to activated below
	if (laserbeamMoby)
	{
		laserbeamMoby->State = LASERBEAM_STATE_DEACTIVATED;
	}

	switch (pvars->MobVars.State)
	{
	case LEVIATHAN_STATE_SPAWN:
	{
		mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case LEVIATHAN_STATE_FLINCH:
	case LEVIATHAN_STATE_BIG_FLINCH:
	{
		decTimerU8(&pvars->MobVars.Knockback.Ticks);
		int animFlinchId = pvars->MobVars.State == LEVIATHAN_STATE_BIG_FLINCH ? LEVIATHAN_ANIM_BIG_FLINCH : LEVIATHAN_ANIM_BIG_FLINCH;

		mobTransAnim(moby, animFlinchId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.State == LEVIATHAN_STATE_BIG_FLINCH)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, LEVIATHAN_KNOCKBACK_MULTIPLIER);
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
	case LEVIATHAN_STATE_IDLE:
	{
		if (pvars->MobVars.AnimationLooped || (moby->AnimSeqId != LEVIATHAN_ANIM_IDLE))
		{
			mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
		}
		else
		{
			mobTransAnim(moby, moby->AnimSeqId, 0);
		}
		mobStand(moby);
		// mobResetSoundTrigger(moby);
		break;
	}
	case LEVIATHAN_STATE_JUMP:
	{
		// move
		if (!isInAirFromFlinching)
		{
			if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
				pvars->MobVars.Dirty = 1; // new path, sync with other clients
			mobJumpTowards(moby, t);
		}

		// handle jumping
		if (pvars->MobVars.MoveVars.Grounded)
		{
			mobTransAnim(moby, LEVIATHAN_ANIM_JUMP, 5);
			// mobResetSoundTrigger(moby);

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
			if (jumpSpeed <= 0)
			{
				jumpSpeed = LEVIATHAN_DEFAULT_JUMP_SPEED; // clamp(0 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->MobVars.MoveVars.WallSlope) * 1, 3, 15);
			}

			vector_write(pvars->MobVars.MoveVars.Velocity, 0);
			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
			mobResetMoveStep(moby);
		}
		break;
	}
	case LEVIATHAN_STATE_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
			mobTurnTowards(moby, target->Position, turnSpeed);
		break;
	}
	case LEVIATHAN_STATE_CHASE:
	case LEVIATHAN_STATE_WALK:
	case LEVIATHAN_STATE_STRAFE:
	{
		int walkAnimId = LEVIATHAN_ANIM_WALK;
		if (!isInAirFromFlinching && target)
		{
			walkAnimId = leviathanDoStateMove(moby);
		}

		//
		if (moby->AnimSeqId == LEVIATHAN_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded)
		{
			// wait for jump to land
		}
		else if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			leviathanForceLocalState(moby, LEVIATHAN_STATE_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			mobTransAnim(moby, walkAnimId, 0);
		}
		else
		{
			mobTransAnim(moby, LEVIATHAN_ANIM_IDLE, 0);
		}
		break;
	}
	case LEVIATHAN_STATE_DIE:
	{
		mobTransAnim(moby, LEVIATHAN_ANIM_DIE, 0);
		mobStand(moby);

		if (moby->AnimSeqId == LEVIATHAN_ANIM_DIE && pvars->MobVars.AnimationLooped)
		{
			pvars->MobVars.Destroy = 1;
		}
		break;
	}
	case LEVIATHAN_STATE_ATTACK_SWING:
	case LEVIATHAN_STATE_ATTACK_STAB:
	{
		int attackAnimId = LEVIATHAN_ANIM_SWING;
		float attackAnimHitStart = LEVIATHAN_SWING_ATTACK_HIT_FRAME_START;
		float attackAnimHitEnd = LEVIATHAN_SWING_ATTACK_HIT_FRAME_END;
		float damage = pvars->MobVars.Config.Damage;

		if (pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_STAB)
		{
			attackAnimId = LEVIATHAN_ANIM_STAB_DOWN;
			attackAnimHitStart = LEVIATHAN_STAB_ATTACK_HIT_FRAME_START;
			attackAnimHitEnd = LEVIATHAN_STAB_ATTACK_HIT_FRAME_END;
			damage *= 1.5; // more damage
		}

		mobTransAnim(moby, attackAnimId, 0);
		int swingAttackReady = moby->AnimSeqId == attackAnimId && moby->AnimSeqT >= attackAnimHitStart && moby->AnimSeqT < attackAnimHitEnd;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (!isInAirFromFlinching)
		{
			if (target)
			{
				mobTurnTowards(moby, target->Position, turnSpeed);
			}

			mobStand(moby);
		}

		if (swingAttackReady && damageFlags)
		{
			leviathanDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
		}
		break;
	}
	case LEVIATHAN_STATE_ATTACK_LASER:
	case LEVIATHAN_STATE_ATTACK_LASER_LOCKON:
	{
		int nextAnimId = moby->AnimSeqId;
		int isLockOn = pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER_LOCKON;

		switch (moby->AnimSeqId)
		{
		case LEVIATHAN_ANIM_LASER_FIRE_BEGIN:
		{
			if (pvars->MobVars.AnimationLooped)
			{

				// set initial direction to tail forward
				MATRIX mtxTailHead;
				mobyGetJointMatrix(moby, LEVIATHAN_SUBSKELETON_JOINT_TAIL_HEAD, mtxTailHead);
				// vector_copy(leviathanVars->LaserbeamDirection, &mtxTailHead[8]);
				vector_subtract(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamTarget1, &mtxTailHead[12]);
				leviathanVars->LaserAtTicks = pvars->MobVars.CurrentStateForTicks;

				nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_ACTIVE;
			}
			break;
		}
		case LEVIATHAN_ANIM_LASER_FIRE_ACTIVE:
		case LEVIATHAN_ANIM_LASER_FIRE_ACTIVE_WALK:
		{
			float damage = pvars->MobVars.Config.Damage;
			int laserForTicks = pvars->MobVars.CurrentStateForTicks - leviathanVars->LaserAtTicks;

			// save player from instant hit on laser start (annoying)
			// if (laserForTicks < 10) damage = 0;

			// get or create laserbeam
			if (!laserbeamMoby)
			{
				laserbeamMoby = leviathanVars->LaserbeamMoby = laserbeamCreate(moby);
			}

			// get fire from position
			MATRIX mtxTailHead;
			mobyGetJointMatrix(moby, LEVIATHAN_SUBSKELETON_JOINT_TAIL_HEAD, mtxTailHead);

			// move direction towards target
			VECTOR targetPos, idealDir, dt;

			if (isLockOn && target)
			{
				mobGetTargetCenter(target, targetPos);
			}
			else
			{
				leviathanComputeLaserTargets(moby);
				float t = (-cosf(laserForTicks / (float)LEVIATHAN_LASER_FIRE_CYCLE_TICKS) + 1) / 2;
				vector_lerp(targetPos, leviathanVars->LaserbeamTarget1, leviathanVars->LaserbeamTarget2, t);
			}

			// mobGetTargetCenter(target, targetPos);
			vector_subtract(idealDir, targetPos, &mtxTailHead[12]);
			vector_subtract(dt, idealDir, leviathanVars->LaserbeamDirection);

			vector_normalize(dt, dt);
			vector_scale(dt, dt, MATH_DT * 5);
			vector_add(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamDirection, dt);

			// vector_lerp(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamDirection, idealDir, MATH_DT * 0.2);
			vector_normalize(leviathanVars->LaserbeamDirection, leviathanVars->LaserbeamDirection);

			// stop if target is behind moby
			if (fabsf(acosf(vector_innerproduct(idealDir, moby->M0_03))) > LEVIATHAN_LASER_MAX_ANGLE)
			{
				nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED;
			}

			// update laserbeam
			if (laserbeamMoby)
			{
				laserbeamMoby->State = LASERBEAM_STATE_ACTIVATED;
				float width = 0.3;
				u32 colorBeam = 0x80208040;
				u32 colorGlow = 0x3020FF20;
				if (isBoss)
				{
					width = 0.7;
					colorBeam = 0x8010C020;
					colorGlow = 0x5020FF20;
				}
				laserbeamSet(laserbeamMoby, &mtxTailHead[12], leviathanVars->LaserbeamDirection, 100, width, damage, MOB_DAMAGE_FLAG_BASE, colorBeam, colorGlow, 0x00ff00, 0x00ff00, 0x45, 0x0E);
			}

			// check for laser hit target
			if (mobAmIOwner(moby) && laserbeamMoby)
			{
				struct LaserbeamPVar *pvars = (struct LaserbeamPVar *)laserbeamMoby->PVar;
				if (pvars->Hit && pvars->HitMoby == target && pvars->Damage > 0)
				{
					leviathanForceLocalState(moby, LEVIATHAN_STATE_WALK);
				}
			}

			// stop after n seconds
			if (laserForTicks > leviathanGetLaserForTicks(moby))
			{
				nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED;
			}
			break;
		}
		case LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED:
		{
			break;
		}
		}

		// first animation is begin
		if (!pvars->MobVars.CurrentStateForTicks)
		{
			nextAnimId = LEVIATHAN_ANIM_LASER_FIRE_BEGIN;
		}

		if (!isInAirFromFlinching)
		{
			mobStand(moby);
		}

		mobTransAnim(moby, nextAnimId, 0);
		break;
	}
	}

	// DPRINTF("(%d) %d:%f\n", pvars->MobVars.State, moby->AnimSeqId, moby->AnimSeqT);

	pvars->MobVars.CurrentStateForTicks++;
}

//--------------------------------------------------------------------------
void leviathanDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, LEVIATHAN_SUBSKELETON_JOINT_TAIL_HEAD, 1, 0);
}

//--------------------------------------------------------------------------
void leviathanForceLocalState(Moby *moby, int state)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	LeviathanMobVars_t *leviathanVars = (LeviathanMobVars_t *)pvars->AdditionalMobVarsPtr;
	float difficulty = 1;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.State)
	{
	case LEVIATHAN_STATE_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case LEVIATHAN_STATE_DIE:
	{
		// can't undie
		return;
	}
	case LEVIATHAN_STATE_JUMP:
	{
		// pvars->MobVars.MoveVars.JumpedThisState = 0;
		break;
	}
	}

	// to
	switch (state)
	{
	case LEVIATHAN_STATE_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case LEVIATHAN_STATE_WALK:
	{

		break;
	}
	case LEVIATHAN_STATE_DIE:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case LEVIATHAN_STATE_ATTACK_LASER:
	case LEVIATHAN_STATE_ATTACK_LASER_LOCKON:
	case LEVIATHAN_STATE_ATTACK_PROJECTILE:
	case LEVIATHAN_STATE_ATTACK_SWING:
	case LEVIATHAN_STATE_ATTACK_STAB:
	{
		leviathanComputeLaserTargets(moby);
		leviathanVars->AttackLaserCooldownTicks = leviathanGetLaserCooldownTicks(moby);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		break;
	}
	case LEVIATHAN_STATE_FLINCH:
	case LEVIATHAN_STATE_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = LEVIATHAN_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.StateCooldownTicks = LEVIATHAN_STATE_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short leviathanGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Config.Bangles;
}

//--------------------------------------------------------------------------
int leviathanIsExhausted(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int exhaustedLoopCount = LEVIATHAN_LASER_EXHAUSTED_ANIM_LOOP;
	return moby->AnimSeqId == LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED && pvars->MobVars.AnimationLooped < exhaustedLoopCount;
}

//--------------------------------------------------------------------------
int leviathanIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	switch (pvars->MobVars.State)
	{
	case LEVIATHAN_STATE_ATTACK_LASER:
	case LEVIATHAN_STATE_ATTACK_LASER_LOCKON:
		// stop after fire exhausted
		return moby->AnimSeqId != LEVIATHAN_ANIM_LASER_FIRE_EXHAUSTED || leviathanIsExhausted(moby);
	case LEVIATHAN_STATE_ATTACK_SWING:
	case LEVIATHAN_STATE_ATTACK_STAB:
		return !pvars->MobVars.AnimationLooped;
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int leviathanCanNonOwnerTransitionToState(Moby *moby, int state)
{
	// only let owner choose states for boss
	// since for this boss it is critical it syncs well
	if (leviathanIsBoss(moby))
		return 0;

	// always let non-owners simulate an state unless its the death state
	if (state == LEVIATHAN_STATE_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int leviathanShouldForceStateUpdateOnState(Moby *moby, int state)
{
	// only let owner choose states for boss
	// since for this boss it is critical it syncs well
	if (leviathanIsBoss(moby))
		return 1;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// only send state updates at regular intervals, unless dying
	// or if we're entering/leaving the roaming/laser/chase states
	if (state == LEVIATHAN_STATE_DIE)
		return 1;
	if (state == LEVIATHAN_STATE_FLINCH || state == LEVIATHAN_STATE_BIG_FLINCH)
		return 1;
	if (pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER || state == LEVIATHAN_STATE_ATTACK_LASER)
		return 1;
	if (pvars->MobVars.State == LEVIATHAN_STATE_ATTACK_LASER_LOCKON || state == LEVIATHAN_STATE_ATTACK_LASER_LOCKON)
		return 1;
	if (pvars->MobVars.State == LEVIATHAN_STATE_CHASE || state == LEVIATHAN_STATE_CHASE)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int leviathanIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.State == LEVIATHAN_STATE_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int leviathanIsIdling(struct MobPVar *pvars)
{
	return pvars->MobVars.State == LEVIATHAN_STATE_IDLE;
}

//--------------------------------------------------------------------------
int leviathanCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int leviathanIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == LEVIATHAN_ANIM_FLINCH || moby->AnimSeqId == LEVIATHAN_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int leviathanIsDying(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.State == LEVIATHAN_STATE_DIE;
}

//--------------------------------------------------------------------------
int leviathanShouldStrafe(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;

	if (!target)
		return 0;

	// get distance to target
	VECTOR dt;
	vector_subtract(dt, target->Position, moby->Position);
	float sqrDistToTarget = vector_sqrmag(dt);
	float maxDistSqr = powf(MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].RangedAttackDistance, 2);
	if (sqrDistToTarget > maxDistSqr)
		return 0;

	int strafe = 0;
	if (mobCanSeeMoby(moby, target) && sqrDistToTarget < (LEVIATHAN_CHASE_TARGET_RADIUS * LEVIATHAN_CHASE_TARGET_RADIUS))
	{
		strafe = 1;
	}

	// if mob is ready to attack
	// and we're not already strafing (if we are timeout at 15 seconds)
	// or if target is looking away, rush at them
	if (!leviathanIsEvasive(moby) && pvars->MobVars.AttackCooldownTicks <= 10 && (pvars->MobVars.State != LEVIATHAN_STATE_STRAFE || pvars->MobVars.CurrentStateForTicks > (15 * TPS) || vector_innerproduct_unscaled(moby->M0_03, target->M0_03) >= 0))
	{
		strafe = 0;
	}

	// if stuck, return to walk state
	if (pvars->MobVars.MoveVars.IsStuck)
	{
		strafe = 0;
	}

	return strafe;
}

//--------------------------------------------------------------------------
int leviathanShouldChase(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	int chase = pvars->MobVars.State == LEVIATHAN_STATE_CHASE;

	if (!target)
		return 0;
	if (leviathanIsEvasive(moby))
		return 0;
	if (leviathanIsAggressive(moby))
		return 1;
	if (!chase && rand(101))
		return 0;

	// get distance to target
	VECTOR dt;
	vector_subtract(dt, target->Position, moby->Position);
	float sqrDistToTarget = vector_sqrmag(dt);

	// if mob is ready to attack
	// and we're not already strafing (if we are timeout at 15 seconds)
	// or if target is looking away, rush at them
	if (pvars->MobVars.AttackCooldownTicks <= 10 && (pvars->MobVars.State != LEVIATHAN_STATE_CHASE || pvars->MobVars.CurrentStateForTicks > (15 * TPS)))
	{
		chase = 0;
	}

	if (sqrDistToTarget > (LEVIATHAN_CHASE_TARGET_RADIUS * LEVIATHAN_CHASE_TARGET_RADIUS))
	{
		chase = 1;
	}

	// if stuck, return to walk state
	if (pvars->MobVars.MoveVars.IsStuck)
	{
		chase = 0;
	}

	return chase;
}

//--------------------------------------------------------------------------
int leviathanGetLaserForTicks(Moby *moby)
{
	if (leviathanIsBoss(moby))
		return LEVIATHAN_LASER_FIRE_FOR_TICKS * 3;

	return LEVIATHAN_LASER_FIRE_FOR_TICKS;
}
