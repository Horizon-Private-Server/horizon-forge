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
#include "spawner.h"
#include "shared.h"

void zombieResetActionCooldownTicks(Moby *moby, enum ZombieAttackActions action);
int zombieGetActionReady(Moby *moby, enum ZombieAttackActions action);
Moby *zombieGetThrownMoby(Moby *moby);
void zombiePreUpdate(Moby *moby);
void zombiePostUpdate(Moby *moby);
void zombiePostDraw(Moby *moby);
void zombieAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount);
void zombieMove(Moby* moby);
void zombieOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void zombieOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void zombieOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int zombieOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void zombieOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *zombieGetNextTarget(Moby *moby);
int zombieGetPreferredAction(Moby *moby, int *delayTicks);
void zombieRenderPath(Moby* moby);
void zombieDoAction(Moby *moby);
void zombieThrowMobySpawnExplosion(Moby *moby);
void zombieThrowMobyUpdate(Moby *moby);
void zombieGetThrowVelocity(Moby *moby, VECTOR from, VECTOR to, float speed, VECTOR out);
void zombieSpawnThrowMoby(Moby *moby, float speed, int jointIdx);
void zombieDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void zombieForceLocalAction(Moby *moby, int action);
short zombieGetArmor(Moby *moby);
int zombieIsAttacking(Moby *moby);
int zombieCanNonOwnerTransitionToAction(Moby *moby, int action);
int zombieShouldForceStateUpdateOnAction(Moby *moby, int action);
int zombieIsSpawning(struct MobPVar *pvars);
int zombieCanAttack(struct MobPVar *pvars);
int zombieIsFlinching(Moby *moby);
int zombieIsIdling(struct MobPVar* pvars);
int zombieCreate(struct MobCreateArgs* args);
int zombieIsRoaming(struct MobPVar* pvars);

struct MobVTable ZombieVTable = {
  .PreUpdate = &zombiePreUpdate,
  .PostUpdate = &zombiePostUpdate,
  .PostDraw = &zombiePostDraw,
  .Move = &zombieMove,
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

void zombieResetActionCooldownTicks(Moby *moby, enum ZombieAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
	zombieVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	zombieVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int zombieGetActionReady(Moby *moby, enum ZombieAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
	return zombieVars->ActionQueuedForTicks[action] > 0;
}

//--------------------------------------------------------------------------
Moby *zombieGetThrownMoby(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *thrownMoby = zombieVars->ThrownMoby;

	if (!thrownMoby)
		return NULL;

	if (mobyIsDestroyed(thrownMoby))
		return NULL;

	if (thrownMoby->State == 1)
		return NULL;

	if (thrownMoby->OClass != ZOMBIE_THROW_MOBY_OCLASS)
		return NULL;

	ZombieThrownMobyVars_t *thrownMobyVars = (ZombieThrownMobyVars_t *)thrownMoby->PVar;
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
		mobTickActionCooldowns(moby, ZOMBIE_ATTACK_ACTION_COUNT, zombieVars->ActionCooldownTicks, zombieVars->ActionQueuedForTicks);
}

//--------------------------------------------------------------------------
void zombiePostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float baseSpeed = 0.9;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP)
	{
		animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / ZOMBIE_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
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
	else if (moby->AnimSeqId == ZOMBIE_ANIM_SLAP)
	{
		animSpeed = baseSpeed * mobGetFloat(moby, ZOMBIE_PARAM_MELEE_ATTACK_SPEED_MULTIPLIER);
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
	u32 color = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 1.0, color, 1);
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
void zombieMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void zombieOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(ZombieMobVars_t));
	float scale = mobGetScaleMultiplier(moby);

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
	int i;
	for (i = 0; i < ZOMBIE_ATTACK_ACTION_COUNT; ++i)
		zombieResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void zombieOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
  
  // spawn corn
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void zombieOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != ZOMBIE_ACTION_FLINCH 
            && pvars->MobVars.Action != ZOMBIE_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    zombieForceLocalAction(moby, ZOMBIE_ACTION_DIE);
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

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, ZOMBIE_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, ZOMBIE_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, ZOMBIE_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, ZOMBIE_ACTION_FLINCH);
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
	return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int zombieGetPreferredAction(Moby *moby, int *delayTicks)
{
	VECTOR t;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int canRanged = mobGetBehavior(moby) != ZOMBIE_BEHAVIOR_MELEE;
	int preferRanged = mobGetBehavior(moby) == ZOMBIE_BEHAVIOR_RANGED;

	// no preferred action
	if (zombieIsAttacking(moby))
		return -1;

	if (zombieIsSpawning(pvars))
		return -1;

	if (zombieIsFlinching(moby))
		return -1;

  if (pvars->MobVars.Action == ZOMBIE_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == ZOMBIE_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return ZOMBIE_ACTION_WALK;
  }

	// jump if we've hit a slope and are grounded
	if (mobHitWallShouldJump(moby, ZOMBIE_MAX_WALKABLE_SLOPE))
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
		float rangedAttackRadius = mobGetFloat(moby, ZOMBIE_PARAM_THROW_MAX_DISTANCE);
		int isThrowState = pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK_THROW;

		// can't attack
		if (!zombieCanAttack(pvars))
			return preferRanged ? ZOMBIE_ACTION_LOOK_AT_TARGET : ZOMBIE_ACTION_WALK;

		if (dist <= attackRadius && zombieGetActionReady(moby, ZOMBIE_ATTACK_ACTION_MELEE))
		{
			if (delayTicks)
				*delayTicks = pvars->MobVars.Config.ReactionTickCount;
			return ZOMBIE_ACTION_ATTACK;
		}
		else if (!isThrowState && canRanged && pvars->MobVars.MoveVars.Grounded && dist <= rangedAttackRadius && mobCanSeeMoby(moby, target))
		{
			VECTOR dt;
			vector_subtract(dt, target->Position, moby->Position);
			float theta = acosf(vector_innerproduct(dt, moby->M0_03));
			if (fabsf(theta) < ZOMBIE_THROW_FIRE_AT_MAX_TURN_ANGLE && zombieGetActionReady(moby, ZOMBIE_ATTACK_ACTION_THROW))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return ZOMBIE_ACTION_ATTACK_THROW;
			}

			return preferRanged ? ZOMBIE_ACTION_LOOK_AT_TARGET : ZOMBIE_ACTION_WALK;
		}
		else
		{
			return ZOMBIE_ACTION_WALK;
		}
	}

	  // if roaming, then we want to periodically stop or reroute
  if (zombieIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return ZOMBIE_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (zombieIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return ZOMBIE_ACTION_IDLE;
  }
	
	return ZOMBIE_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void zombieRenderPath(Moby* moby)
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
#endif

//--------------------------------------------------------------------------
void zombieDoAction(Moby *moby)
{
  struct PathGraph* path = pathGetMobyPathGraph(moby, &((struct MobPVar*)moby->PVar)->MobVars.MoveVars);
  float speed = ((struct MobPVar*)moby->PVar)->MobVars.Config.Speed;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_TURN_RADIANS_PER_SEC : ZOMBIE_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_MOVE_ACCELERATION : ZOMBIE_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == ZOMBIE_ACTION_FLINCH || pvars->MobVars.LastAction == ZOMBIE_ACTION_BIG_FLINCH);

	turnSpeed *= pvars->MobVars.Config.TurnSpeed;

#if DEBUGPATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&zombieRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
	case ZOMBIE_ACTION_ATTACK:
	{
		int attack1AnimId = ZOMBIE_ANIM_SLAP;
		mobTransAnim(moby, attack1AnimId, 0);

		float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, ZOMBIE_PARAM_MELEE_DAMAGE_MULTIPLIER);
		float lungeMult = mobGetFloat(moby, ZOMBIE_PARAM_MELEE_LUNGE_MULTIPLIER);
		float speed = mobGetLungeSpeed(moby, attack1AnimId, 0, ZOMBIE_SLAP_ANIM_LUNGE_DURATION, lungeMult);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= ZOMBIE_ATTACK_HIT_FRAME_START && moby->AnimSeqT < ZOMBIE_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (!isInAirFromFlinching)
		{
			if (target && speed > 0)
			{
				mobTurnTowardsPredictiveWithSpeed(moby, target, ZOMBIE_TURN_LUNGE_RADIANS_PER_SEC, speed);
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
			zombieDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
		}
		break;
	}
	case ZOMBIE_ACTION_ATTACK_THROW:
	{
		int nextAnimId = moby->AnimSeqId;
		float projectileSpeed = MATH_DT * mobGetFloat(moby, ZOMBIE_PARAM_THROW_PROJECTILE_SPEED);

		switch (moby->AnimSeqId)
		{
		case ZOMBIE_ANIM_THROW_HEAD:
		{
			// turn towards player
			if (!pvars->MobVars.AnimationLooped && moby->AnimSeqT < ZOMBIE_ATTACK_THROW_SPAWN_FRAME_START)
			{
				// randomize thrown moby speed by 80-100%
				float randomizedSpeed = randRange(0.8, 1.0) * projectileSpeed;
				mobTurnTowardsPredictiveWithSpeed(moby, target, ZOMBIE_THROW_TURN_RADIANS_PER_SEC, randomizedSpeed);
			}

			// spawn throw moby when hand is fully extended forward
			if (!zombieGetThrownMoby(moby) && moby->AnimSeqT >= ZOMBIE_ATTACK_THROW_SPAWN_FRAME_START)
			{
				zombieSpawnThrowMoby(moby, projectileSpeed, ZOMBIE_SUBSKELETON_JOINT_RIGHT_HAND);
				nextAnimId = ZOMBIE_ANIM_IDLE;
			}
			break;
		}
		}

		// begin animation sequence
		if (!pvars->MobVars.CurrentActionForTicks)
			nextAnimId = ZOMBIE_ANIM_THROW_HEAD;

		// not moving in this action
		if (!isInAirFromFlinching)
			mobStand(moby);

		mobTransAnim(moby, nextAnimId, 0);
		break;
	}
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
      
			if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == ZOMBIE_ACTION_BIG_FLINCH) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, ZOMBIE_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case ZOMBIE_ACTION_IDLE:
		{
      if (pvars->MobVars.AnimationLooped || (moby->AnimSeqId != ZOMBIE_ANIM_IDLE && moby->AnimSeqId != ZOMBIE_ANIM_IDLE_2)) {
			  mobTransAnim(moby, (rand(1000) == 1) ? ZOMBIE_ANIM_IDLE : ZOMBIE_ANIM_IDLE_2, 0);
      } else {
        mobTransAnim(moby, moby->AnimSeqId, 0);
      }
      mobStand(moby);
      //mobResetSoundTrigger(moby);
			break;
		}
		case ZOMBIE_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    mobTransAnim(moby, ZOMBIE_ANIM_JUMP, 5);
          mobResetSoundTrigger(moby);

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
		case ZOMBIE_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case ZOMBIE_ACTION_ROAM:
    {
      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, 0);
      }

			// 
      if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        zombieForceLocalAction(moby, ZOMBIE_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, ZOMBIE_ANIM_WALK, 0);
      } else {
				mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
      }
      break;
    }
    case ZOMBIE_ACTION_WALK:
		{
      float dir = mobGetCurrentWalkAngle(moby);

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
      }

			// 
      if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        zombieForceLocalAction(moby, ZOMBIE_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, ZOMBIE_ANIM_RUN, 0);
      } else {
				mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
      }
			break;
		}
    case ZOMBIE_ACTION_DIE:
    {
      mobStand(moby);
      break;
    }
		}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void zombieThrowMobySpawnExplosion(Moby *moby)
{
	ZombieThrownMobyVars_t *pvars = (ZombieThrownMobyVars_t *)moby->PVar;
	mobySpawnExplosion(vector_read(pvars->HeadPos), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0x404080, 0, NULL, NULL, 0, ZOMBIE_THROW_HIT_RADIUS, 0, 0, 0);
}

//--------------------------------------------------------------------------
void zombieThrowMobyUpdate(Moby *moby)
{
	ZombieThrownMobyVars_t *pvars = (ZombieThrownMobyVars_t *)moby->PVar;
	if (!pvars)
		return;

	// detect when parent dies
	Moby *parentMoby = pvars->ThrownBy;
	if (!parentMoby || mobyIsDestroyed(parentMoby))
	{
		mobyDestroy(moby);
		return;
	}

	if (moby->State == 1)
	{
		mobyDestroy(moby);
		return;
	}

	// seed initial action
	if ((moby->Triggers & 1) == 0)
	{
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
	if (CollLine_Fix(startHeadPos, nextHeadPos, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, pvars->ThrownBy, NULL))
	{
		vector_copy(pvars->HeadPos, CollLine_Fix_GetHitPosition());
		zombieThrowMobySpawnExplosion(moby);
		pvars->LifeTicks = 0;
	}

	// apply gravity to velocity
	VECTOR gravity = {0, 0, ZOMBIE_THROW_GRAVITY, 0};
	vector_add(pvars->Velocity, pvars->Velocity, gravity);

	// spin
	VECTOR angularVelocity;
	vector_normalize(angularVelocity, pvars->Velocity);
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
	struct MobPVar *mobPvars = (struct MobPVar *)parentMoby->PVar;
	float damage = mobPvars->MobVars.Config.Damage * mobGetFloat(parentMoby, ZOMBIE_PARAM_THROW_DAMAGE_MULTIPLIER);
	float throwHitRadius = ZOMBIE_THROW_HIT_RADIUS * mobGetScaleMultiplier(parentMoby);
	u32 damageFlags = mobGetDamageFlags(parentMoby, MOB_DAMAGE_FLAG_BASE);
	if (mobDoSweepDamage(pvars->ThrownBy, pvars->ThrownBy, startHeadPos, pvars->HeadPos, throwHitRadius * 0.5, throwHitRadius, damage, damageFlags, 0, 0, 1))
	{
		zombieThrowMobySpawnExplosion(moby);
		pvars->LifeTicks = 0;
	}

	// kill when life hits 0
	--pvars->LifeTicks;
	if (pvars->LifeTicks <= 0)
	{
		mobySetState(moby, 1, -1);
	}
}

//--------------------------------------------------------------------------
void zombieGetThrowVelocity(Moby *moby, VECTOR from, VECTOR to, float speed, VECTOR out)
{
	// Calculate delta positions
	float dx = to[0] - from[0];
	float dy = to[1] - from[1];
	float dz = to[2] - from[2];

	// Calculate horizontal distance in XY plane
	float horizontal_dist_sq = dx * dx + dy * dy;
	float horizontal_dist = sqrtf(horizontal_dist_sq);

	// Handle case where target is directly above/below
	if (horizontal_dist < 0.001f)
	{
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

	float discriminant = b * b - 4 * a * c;
	if (discriminant < 0)
	{
		// Target unreachable with given speed, fall back to simple direction
		vector_fromyaw(out, moby->Rotation[2]);
		out[2] = 0.1f;
		vector_normalize(out, out);
		vector_scale(out, out, speed);
		return;
	}

	// For low arc (shorter flight time), use the smaller u value
	float u = (-b - sqrtf(discriminant)) / (2 * a);

	if (u <= 0)
	{
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
void zombieSpawnThrowMoby(Moby *moby, float speed, int jointIdx)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;

	// get position to spawn moby from joint
	MATRIX jointMtx;
	VECTOR spawnAt, spawnVelocity;
	mobyGetJointMatrix(moby, jointIdx, jointMtx);
	vector_copy(spawnAt, &jointMtx[12]);

	// get target position
	VECTOR targetCenter;
	mobGetTargetCenter(pvars->MobVars.MoveVars.Target, targetCenter);
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
	VECTOR groundCheckFrom = {0, 0, 2, 0};
	VECTOR groundCheckTo = {0, 0, 0, 0};
	vector_add(groundCheckFrom, groundCheckFrom, spawnAt);
	vector_add(groundCheckTo, groundCheckTo, spawnAt);
	if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL))
		vector_add(spawnAt, CollLine_Fix_GetHitPosition(), (VECTOR){0, 0, 0.1, 0});

	// spawn throw moby
	// use custom update function to drive mob damage
	Moby *spawnedThrownMoby = zombieVars->ThrownMoby = mobySpawn(ZOMBIE_THROW_MOBY_OCLASS, sizeof(ZombieThrownMobyVars_t));
	if (!spawnedThrownMoby)
		return;

	ZombieThrownMobyVars_t *thrownMobyVars = (ZombieThrownMobyVars_t *)spawnedThrownMoby->PVar;
	spawnedThrownMoby->PUpdate = &zombieThrowMobyUpdate;
	spawnedThrownMoby->Bangles = 0x8000 | (moby->Bangles & 0x1f); // head only
	spawnedThrownMoby->CollActive = -1;														// disable collision
	spawnedThrownMoby->AnimSpeed = 0;
	spawnedThrownMoby->UpdateDist = -1;
	spawnedThrownMoby->DrawDist = 64;
	spawnedThrownMoby->ModeBits = MOBY_MODE_BIT_HAS_GLOW;
	spawnedThrownMoby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
	spawnedThrownMoby->GlowRGBA = mobGetSpawnParams(moby)->GlowColor;
	spawnedThrownMoby->Scale *= mobGetScaleMultiplier(moby);
	thrownMobyVars->ThrownBy = moby;
	vector_copy(thrownMobyVars->HeadPos, spawnAt);
	vector_copy(thrownMobyVars->Velocity, spawnVelocity);
}

//--------------------------------------------------------------------------
void zombieDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, ZOMBIE_SUBSKELETON_JOINT_LEFT_HAND, 1, 0);
}

//--------------------------------------------------------------------------
void zombieForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ZombieMobVars_t *zombieVars = (ZombieMobVars_t *)pvars->AdditionalMobVarsPtr;
	int stateCooldownTicks = ZOMBIE_ACTION_COOLDOWN_TICKS;

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
	case ZOMBIE_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
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
		stateCooldownTicks = 0;
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
		zombieResetActionCooldownTicks(moby, ZOMBIE_ATTACK_ACTION_MELEE);
		zombieResetActionCooldownTicks(moby, ZOMBIE_ATTACK_ACTION_THROW);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		stateCooldownTicks = 0;
		break;
	}
	case ZOMBIE_ACTION_FLINCH:
	case ZOMBIE_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = ZOMBIE_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
		break;
	}
	case ZOMBIE_ACTION_ROAM:
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
	return (pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK && !pvars->MobVars.AnimationLooped) || (pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK_THROW && !pvars->MobVars.AnimationLooped);
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
  if (((struct MobPVar*)moby->PVar)->MobVars.Action == ZOMBIE_ACTION_ROAM || action == ZOMBIE_ACTION_ROAM) return 1;

	// only send action updates at regular intervals, unless dying or flinching
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

//--------------------------------------------------------------------------
int zombieIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == ZOMBIE_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int zombieCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(ZombieMobVars_t), &guberEvent, NULL);
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
int zombieIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == ZOMBIE_ACTION_ROAM;
}
