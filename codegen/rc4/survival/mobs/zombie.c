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
#include "utils.h"
#include "maputils.h"

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
void zombieSpawnThrowMoby(Moby* moby, float speed, int jointIdx);

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
int zombieGetThrowCooldownTicks(Moby *moby)
{
  // if ranged, trigger throw more frequently
  if (mobGetBehavior(moby) == ZOMBIE_BEHAVIOR_RANGED)
    return randRangeInt(ZOMBIE_THROW_COOLDOWN_TICKS_MIN, ZOMBIE_THROW_COOLDOWN_TICKS_MIN * 2);

	return randRangeInt(ZOMBIE_THROW_COOLDOWN_TICKS_MIN, ZOMBIE_THROW_COOLDOWN_TICKS_MAX);
}

//--------------------------------------------------------------------------
Moby* zombieGetThrownMoby(Moby* moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
  Moby* thrownMoby = zombieVars->ThrownMoby;

  if (!thrownMoby)
    return NULL;

  if (mobyIsDestroyed(thrownMoby))
    return NULL;

  if (thrownMoby->State == 1)
    return NULL;

  if (thrownMoby->OClass != ZOMBIE_THROW_MOBY_OCLASS)
    return NULL;

  ZombieThrownMobyVars_t* thrownMobyVars = (ZombieThrownMobyVars_t*)thrownMoby->PVar;
  if (!thrownMobyVars)
    return NULL;

  if (thrownMobyVars->ThrownBy != moby)
    return NULL;

  return thrownMoby;
}

//--------------------------------------------------------------------------
void zombiePreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);
  
	if (!mobIsFrozen(moby))
		decTimerU32(&zombieVars->AttackThrowCooldownTicks);
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
		animSpeed = 0.9 * (1 - powf(moby->AnimSeqT / ZOMBIE_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = 0.9;
		}
	}
	else if (zombieIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / ZOMBIE_FLINCH_ANIM_DURATION, 2));
	}
  else if (moby->AnimSeqId == ZOMBIE_ANIM_THROW_HEAD)
  {
    animSpeed *= 0.5;
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
	return sizeof(ZombieMobVars_t);
}

//--------------------------------------------------------------------------
void zombieOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
}

//--------------------------------------------------------------------------
void zombieOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(ZombieMobVars_t));
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

  // prevent spawning and throwing immediately
  zombieVars->AttackThrowCooldownTicks = zombieGetThrowCooldownTicks(moby);
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

	int isShock = e->DamageFlags & MOB_DAMAGE_FLAG_SHOCK;
	int isShortFreeze = e->DamageFlags & MOB_DAMAGE_FLAG_SHORT_FREEZE;

	// destroy
	if (newHp <= 0)
	{
		if (pvars->MobVars.Action == ZOMBIE_ACTION_TIME_BOMB && moby->AnimSeqId == ZOMBIE_ANIM_CROUCH && moby->AnimSeqT > ZOMBIE_CROUCH_ANIM_MIN_T_FOR_EXPLOSION)
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

	float damageRatio = damage / pvars->MobVars.Config.Health;
	float powerFactor = ZOMBIE_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
	float probability = clamp((damageRatio * ZOMBIE_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);
	mobHandleFlinch(moby, e, canFlinch, isShock, probability, powerFactor, ZOMBIE_ACTION_FLINCH, ZOMBIE_ACTION_BIG_FLINCH);

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
	return mobDefaultOnLocalDamage(moby, e);
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
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
	int canRanged = mobGetBehavior(moby) != ZOMBIE_BEHAVIOR_MELEE;
  int preferRanged = mobGetBehavior(moby) == ZOMBIE_BEHAVIOR_RANGED;

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
	  float rangedAttackRadius = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].RangedAttackDistance;
	  int isThrowAction = pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK_THROW;
    
    // ranged should stop before getting too close to target
    int deferredAction = (preferRanged && (dist <= rangedAttackRadius * 0.9)) ? ZOMBIE_ACTION_LOOK_AT_TARGET : ZOMBIE_ACTION_WALK;
		
    if (dist <= attackRadius)
		{
			if (zombieCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return pvars->MobVars.Config.MobAttribute != MOB_ATTRIBUTE_EXPLODE ? ZOMBIE_ACTION_ATTACK : ZOMBIE_ACTION_TIME_BOMB;
			}
			return deferredAction;
		}
    else if (!isThrowAction && canRanged && pvars->MobVars.MoveVars.Grounded && dist <= rangedAttackRadius && zombieVars->AttackThrowCooldownTicks == 0)
    {
      VECTOR dt;
	    vector_subtract(dt, target->Position, moby->Position);
			float theta = acosf(vector_innerproduct(dt, moby->M0_03));
      if (fabsf(theta) < ZOMBIE_THROW_FIRE_AT_MAX_TURN_ANGLE && mobCanSeeMoby(moby, target))
      {
        if (delayTicks)
          *delayTicks = pvars->MobVars.Config.ReactionTickCount;
        return ZOMBIE_ACTION_ATTACK_THROW;
      }

			return deferredAction;
    }
		else
		{
			return deferredAction;
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
				jumpSpeed = ZOMBIE_DEFAULT_JUMP_SPEED;
			}

			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case ZOMBIE_ACTION_LOOK_AT_TARGET:
	{
    mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
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

		float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < ZOMBIE_SLAP_ANIM_LUNGE_DURATION) ? (difficulty * 2) : 1, 1, 5);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= ZOMBIE_ATTACK_HIT_FRAME_START && moby->AnimSeqT < ZOMBIE_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

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

		if (swingAttackReady && damageFlags)
		{
			zombieDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
		}
		break;
	}
  case ZOMBIE_ACTION_ATTACK_THROW:
  {
		int nextAnimId = moby->AnimSeqId;

		switch (moby->AnimSeqId)
    {
      case ZOMBIE_ANIM_THROW_HEAD:
      {
        // turn towards player
        if (!pvars->MobVars.AnimationLooped && moby->AnimSeqT < ZOMBIE_ATTACK_THROW_SPAWN_FRAME_START)
        {
          // randomize thrown moby speed by 80-100%
          float randomizedSpeed = randRange(0.8, 1.0) * ZOMBIE_THROW_SPEED;
          mobTurnTowardsPredictiveWithSpeed(moby, target, ZOMBIE_THROW_TURN_RADIANS_PER_SEC, randomizedSpeed);
        }

        // spawn throw moby when hand is fully extended forward
        if (!zombieGetThrownMoby(moby) && moby->AnimSeqT >= ZOMBIE_ATTACK_THROW_SPAWN_FRAME_START)
        {
          zombieSpawnThrowMoby(moby, ZOMBIE_THROW_SPEED, ZOMBIE_SUBSKELETON_JOINT_RIGHT_HAND);
          nextAnimId = ZOMBIE_ANIM_IDLE;
        }
        break;
      }
    }
    
		// begin animation sequence
		if (!pvars->MobVars.CurrentActionForTicks)
			nextAnimId = ZOMBIE_ANIM_THROW_HEAD;

    // not moving in this state
		if (!isInAirFromFlinching)
			mobStand(moby);

		mobTransAnim(moby, nextAnimId, 0);
    break;
  }
	}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void zombieThrowMobySpawnExplosion(Moby* moby)
{
  ZombieThrownMobyVars_t* pvars = (ZombieThrownMobyVars_t*)moby->PVar;
  mobySpawnExplosion(vector_read(pvars->HeadPos)
    , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0
    , 0x404080, 0, NULL, NULL, 0, ZOMBIE_THROW_HIT_RADIUS, 0, 0, 0);
}

//--------------------------------------------------------------------------
void zombieThrowMobyUpdate(Moby* moby)
{
  ZombieThrownMobyVars_t* pvars = (ZombieThrownMobyVars_t*)moby->PVar;
  if (!pvars) return;
  
  // detect when parent dies
  Moby* parentMoby = pvars->ThrownBy;
  if (!parentMoby || mobyIsDestroyed(parentMoby)) {
    mobyDestroy(moby);
    return;
  }

  if (moby->State == 1) {
    mobyDestroy(moby);
    return;
  }

  // seed initial state
  if ((moby->Triggers & 1) == 0) {
    moby->Triggers |= 1;
    pvars->LifeTicks = ZOMBIE_THROW_DURATION_TICKS;
  }

  // simulate
  VECTOR startHeadPos, nextHeadPos;
  vector_copy(startHeadPos, pvars->HeadPos);
  vector_add(nextHeadPos, pvars->HeadPos, pvars->Velocity);
  vector_copy(pvars->HeadPos, nextHeadPos);

  // check for collision
  // if hit anything, destroy
  if (CollLine_Fix(startHeadPos, nextHeadPos, COLLISION_FLAG_IGNORE_DYNAMIC, pvars->ThrownBy, NULL)) {
    vector_copy(pvars->HeadPos, CollLine_Fix_GetHitPosition());
    zombieThrowMobySpawnExplosion(moby);
    pvars->LifeTicks = 0;
  }

  // apply gravity to velocity
  VECTOR gravity = {0,0,ZOMBIE_THROW_GRAVITY,0};
  vector_add(pvars->Velocity, pvars->Velocity, gravity);

  // spin
  VECTOR angularVelocity;
  vector_scale(angularVelocity, pvars->Velocity, 8.0f);
  vector_add(moby->Rotation, moby->Rotation, angularVelocity);
  vector_clampeuler(moby->Rotation, moby->Rotation);
  mobyUpdateTransform(moby);

  // update position
  MATRIX headJointMtx;
  VECTOR pos;
  mobyGetJointMatrix(moby, ZOMBIE_SUBSKELETON_JOINT_HEAD, headJointMtx);
  vector_subtract(pos, &headJointMtx[12], moby->Position);
  vector_subtract(moby->Position, pvars->HeadPos, pos);

  // damage
  // destroy on hit
  struct MobPVar* mobPvars = (struct MobPVar*)parentMoby->PVar;
  u32 damageFlags = mobGetDamageFlags(parentMoby, MOB_DAMAGE_FLAG_BASE);
  if (mobDoSweepDamage(pvars->ThrownBy, startHeadPos, pvars->HeadPos, 1, ZOMBIE_THROW_HIT_RADIUS, mobPvars->MobVars.Config.Damage * ZOMBIE_THROW_DAMAGE_MULT, damageFlags, 0, 0, 1)) {
    zombieThrowMobySpawnExplosion(moby);
    pvars->LifeTicks = 0;
  }

  // kill when life hits 0
  --pvars->LifeTicks;
  if (pvars->LifeTicks <= 0) {
    mobySetState(moby, 1, -1);
  }
}

//--------------------------------------------------------------------------
void zombieGetThrowVelocity(Moby* moby, VECTOR from, VECTOR to, float speed, VECTOR out)
{
  // Calculate delta positions
  float dx = to[0] - from[0];
  float dy = to[1] - from[1];
  float dz = to[2] - from[2];
  
  // Calculate horizontal distance in XY plane
  float horizontal_dist_sq = dx*dx + dy*dy;
  float horizontal_dist = sqrtf(horizontal_dist_sq);
  
  // Handle case where target is directly above/below
  if (horizontal_dist < 0.001f) {
    // Just throw upward towards target
    out[0] = 0;
    out[1] = 0;
    out[2] = speed;
    return;
  }
  
  // Get gravity value (negative)
  float g = ZOMBIE_THROW_GRAVITY;
  
  // Solve projectile motion equations to find time of flight
  // We solve: 0.25*g²*t⁴ - (g*dz + speed²)*t² + (horizontal_dist² + dz²) = 0
  // This is a quadratic in u=t²
  
  float a = 0.25f * g * g;
  float b = -(g * dz + speed * speed);
  float c = horizontal_dist_sq + dz * dz;
  
  float discriminant = b*b - 4*a*c;
  if (discriminant < 0) {
    // Target unreachable with given speed, fall back to simple direction
    vector_fromyaw(out, moby->Rotation[2]);
    out[2] = 0.1f;
    vector_normalize(out, out);
    vector_scale(out, out, speed);
    return;
  }
  
  // For low arc (shorter flight time), use the smaller u value
  float u = (-b - sqrtf(discriminant)) / (2*a);
  
  if (u <= 0) {
    // Shouldn't happen if discriminant is valid, but fallback just in case
    vector_fromyaw(out, moby->Rotation[2]);
    out[2] = 0.1f;
    vector_normalize(out, out);
    vector_scale(out, out, speed);
    return;
  }
  
  float t = sqrtf(u);
  
  // Calculate velocity components
  float horizontal_speed = horizontal_dist / t;
  float vz = (dz - 0.5f * g * t * t) / t;
  
  // Normalize horizontal direction and scale by horizontal speed
  float hx = (dx / horizontal_dist) * horizontal_speed;
  float hy = (dy / horizontal_dist) * horizontal_speed;
  
  out[0] = hx;
  out[1] = hy;
  out[2] = vz;
}

//--------------------------------------------------------------------------
void zombieSpawnThrowMoby(Moby* moby, float speed, int jointIdx)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;

  // get position to spawn moby from joint
  MATRIX jointMtx;
  VECTOR spawnAt, spawnVelocity;
  mobyGetJointMatrix(moby, jointIdx, jointMtx);
  vector_copy(spawnAt, &jointMtx[12]);

  // get target position
  VECTOR targetCenter;
  mobGetTargetCenter(pvars->MobVars.Target, targetCenter);
  zombieGetThrowVelocity(moby, spawnAt, targetCenter, speed, spawnVelocity);

  // align velocity to mob forward direction
  VECTOR spawnVelocityHorizontal;
  vector_projectonhorizontal(spawnVelocityHorizontal, spawnVelocity);
  float spawnVelocityHorizontalMagnitude = vector_length(spawnVelocityHorizontal);
  vector_fromyaw(spawnVelocityHorizontal, moby->Rotation[2]);
  vector_scale(spawnVelocityHorizontal, spawnVelocityHorizontal, spawnVelocityHorizontalMagnitude);
  spawnVelocity[0] = spawnVelocityHorizontal[0];
  spawnVelocity[1] = spawnVelocityHorizontal[1];

  // force velocity to target based on mob yaw
  // VECTOR spawnAtToTarget;
  // MATRIX rotateTargetAlignYawMtx;
  // vector_subtract(spawnAtToTarget, targetCenter, spawnAt);
  // float distToTarget = vector_length(spawnAtToTarget);
  // float distToTargetTheta = atan2f(spawnAtToTarget[1] / distToTarget, spawnAtToTarget[0] / distToTarget);
  // matrix_unit(rotateTargetAlignYawMtx);
  // matrix_rotate_z(rotateTargetAlignYawMtx, rotateTargetAlignYawMtx, clampAngle(moby->Rotation[2] - distToTargetTheta));
  // VECTOR spawnAtToTargetAligned;
  // vector_apply(spawnAtToTargetAligned, spawnAtToTarget, rotateTargetAlignYawMtx);
  // vector_add(targetCenter, spawnAtToTargetAligned, spawnAt);

  // find ground
  VECTOR groundCheckFrom = {0,0,2,0};
  VECTOR groundCheckTo = {0,0,0,0};
  vector_add(groundCheckFrom, groundCheckFrom, spawnAt);
  vector_add(groundCheckTo, groundCheckTo, spawnAt);
  if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
    vector_add(spawnAt, CollLine_Fix_GetHitPosition(), (VECTOR){0,0,0.1,0});

  // spawn throw moby
  // use custom update function to drive mob damage
  Moby* spawnedThrownMoby = zombieVars->ThrownMoby = mobySpawn(ZOMBIE_THROW_MOBY_OCLASS, sizeof(ZombieThrownMobyVars_t));
  if (!spawnedThrownMoby)
    return;

  ZombieThrownMobyVars_t* thrownMobyVars = (ZombieThrownMobyVars_t*)spawnedThrownMoby->PVar;
  spawnedThrownMoby->PUpdate = &zombieThrowMobyUpdate;
  spawnedThrownMoby->Bangles = 0x8000 | (moby->Bangles & 0x1f); // head only
  spawnedThrownMoby->CollActive = -1; // disable collision
  spawnedThrownMoby->AnimSpeed = 0;
  spawnedThrownMoby->UpdateDist = -1;
  spawnedThrownMoby->DrawDist = 64;
  spawnedThrownMoby->ModeBits = MOBY_MODE_BIT_HAS_GLOW;
  thrownMobyVars->ThrownBy = moby;
  vector_copy(thrownMobyVars->HeadPos, spawnAt);
  vector_copy(thrownMobyVars->Velocity, spawnVelocity);
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
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
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
  case ZOMBIE_ACTION_ATTACK_THROW:
  {
    // reset ptr to thrown moby
    zombieVars->ThrownMoby = NULL;
    break;
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
	case ZOMBIE_ACTION_ATTACK_THROW:
	{
		zombieVars->AttackThrowCooldownTicks = zombieGetThrowCooldownTicks(moby);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		break;
	}
	case ZOMBIE_ACTION_TIME_BOMB_EXPLODE:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_EXPLODE_BASE);
		u32 color = 0x403064FF;

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

	if (t < MOB_ARMOR_THRESHOLD_LOW)
		return 0x0000;
	else if (t < MOB_ARMOR_THRESHOLD_HIGH)
		return bangles & 0x1f; // remove torso bangle

	return bangles;
}

//--------------------------------------------------------------------------
int zombieIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == ZOMBIE_ACTION_TIME_BOMB
      || pvars->MobVars.Action == ZOMBIE_ACTION_TIME_BOMB_EXPLODE
      || (pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK && !pvars->MobVars.AnimationLooped)
      || (pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK_THROW && !pvars->MobVars.AnimationLooped);
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
