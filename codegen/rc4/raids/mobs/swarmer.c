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
#include "spawner.h"
#include "shared.h"

void swarmerResetActionCooldownTicks(Moby *moby, enum SwarmerAttackActions action);
int swarmerGetActionReady(Moby *moby, enum SwarmerAttackActions action);
void swarmerPreUpdate(Moby *moby);
void swarmerPostUpdate(Moby *moby);
void swarmerPostDraw(Moby *moby);
void swarmerAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount);
void swarmerMove(Moby* moby);
void swarmerOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void swarmerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void swarmerOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int swarmerOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void swarmerOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *swarmerGetNextTarget(Moby *moby);
int swarmerGetPreferredAction(Moby *moby, int *delayTicks);
void swarmerRenderPath(Moby* moby);
void swarmerDoAction(Moby *moby);
void swarmerDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void swarmerForceLocalAction(Moby *moby, int action);
short swarmerGetArmor(Moby *moby);
int swarmerIsAttacking(Moby *moby);
int swarmerCanNonOwnerTransitionToAction(Moby *moby, int action);
int swarmerShouldForceStateUpdateOnAction(Moby *moby, int action);
int swarmerIsSpawning(struct MobPVar *pvars);
int swarmerCanAttack(struct MobPVar *pvars);
int swarmerGetSideFlipLeftOrRight(struct MobPVar *pvars);
int swarmerIsFlinching(Moby *moby);
int swarmerIsIdling(struct MobPVar* pvars);
int swarmerCreate(struct MobCreateArgs* args);
int swarmerIsRoaming(struct MobPVar* pvars);

struct MobVTable SwarmerVTable = {
  .PreUpdate = &swarmerPreUpdate,
  .PostUpdate = &swarmerPostUpdate,
  .PostDraw = &swarmerPostDraw,
  .Move = &swarmerMove,
  .OnSpawn = &swarmerOnSpawn,
  .OnDestroy = &swarmerOnDestroy,
  .OnDamage = &swarmerOnDamage,
  .OnLocalDamage = &swarmerOnLocalDamage,
  .OnStateUpdate = &swarmerOnStateUpdate,
  .GetNextTarget = &swarmerGetNextTarget,
  .GetPreferredAction = &swarmerGetPreferredAction,
  .ForceLocalAction = &swarmerForceLocalAction,
  .DoAction = &swarmerDoAction,
  .DoDamage = &swarmerDoDamage,
  .GetArmor = &swarmerGetArmor,
  .IsAttacking = &swarmerIsAttacking,
  .CanNonOwnerTransitionToAction = &swarmerCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &swarmerShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------

void swarmerResetActionCooldownTicks(Moby *moby, enum SwarmerAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwarmerMobVars_t *swarmerVars = (SwarmerMobVars_t *)pvars->AdditionalMobVarsPtr;
	swarmerVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	swarmerVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int swarmerGetActionReady(Moby *moby, enum SwarmerAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwarmerMobVars_t *swarmerVars = (SwarmerMobVars_t *)pvars->AdditionalMobVarsPtr;
	return swarmerVars->ActionQueuedForTicks[action] > 0;
}

//--------------------------------------------------------------------------
void swarmerPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	SwarmerMobVars_t *swarmerVars = (SwarmerMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);

	if (!mobIsFrozen(moby))
		mobTickActionCooldowns(moby, SWARMER_ATTACK_ACTION_COUNT, swarmerVars->ActionCooldownTicks, swarmerVars->ActionQueuedForTicks);
}

//--------------------------------------------------------------------------
void swarmerPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float baseSpeed = 0.9;
	float animSpeed = (baseSpeed / scale) * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) * (SWARMER_BASE_COLL_RADIUS / pvars->MobVars.Config.CollRadius);
	if (moby->AnimSeqId == SWARMER_ANIM_JUMP)
	{
		animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / SWARMER_JUMP_ANIM_DURATION, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
		}
	}
	else if (moby->AnimSeqId == SWARMER_ANIM_JUMP_AND_FALL)
	{
		// animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / 15, 2));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = baseSpeed;
		}
	}
	else if (swarmerIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / SWARMER_FLINCH_ANIM_AIR_DURATION, 2));
	}
	else if (moby->AnimSeqId == SWARMER_ANIM_JUMP_FORWARD_BITE)
	{
		animSpeed = baseSpeed * mobGetFloat(moby, SWARMER_PARAM_BITE_ATTACK_SPEED_MULTIPLIER);
	}

	if (moby->AnimSeqId == SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND)
	{
		animSpeed = 1;
	}

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == SWARMER_ACTION_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void swarmerPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 0.5, color, 0);
}

//--------------------------------------------------------------------------
void swarmerAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void swarmerMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void swarmerOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(SwarmerMobVars_t));

	// set scale
	float scale = mobGetScaleMultiplier(moby);
	moby->Scale = 0.256339 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 1 + (scale * 0.25);
	pvars->MobVars.BlipType = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;

	// initialize action cooldowns
	int i;
	for (i = 0; i < SWARMER_ATTACK_ACTION_COUNT; ++i)
		swarmerResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void swarmerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
  
  // spawn corn
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void swarmerOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != SWARMER_ACTION_FLINCH 
            && pvars->MobVars.Action != SWARMER_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    swarmerForceLocalAction(moby, SWARMER_ACTION_DIE);
	}

	// knockback
  // swarmers always have knockback
  if (e->Knockback.Power < 3) e->Knockback.Power = 3;
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		memcpy(&pvars->MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
	}

  // flinch
	if (mobAmIOwner(moby))
	{
		float damageRatio = damage / pvars->MobVars.Config.Health;
    float powerFactor = SWARMER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * SWARMER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, SWARMER_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, SWARMER_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, SWARMER_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, SWARMER_ACTION_FLINCH);
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
int swarmerOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
}

//--------------------------------------------------------------------------
void swarmerOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby *swarmerGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int swarmerGetPreferredAction(Moby *moby, int *delayTicks)
{
	VECTOR t;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// no preferred action
	if (swarmerIsAttacking(moby))
		return -1;

	if (swarmerIsSpawning(pvars))
		return -1;

	if (swarmerIsFlinching(moby))
		return -1;

	if (pvars->MobVars.Action == SWARMER_ACTION_DODGE)
	{
		return -1;
	}

  if (pvars->MobVars.Action == SWARMER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == SWARMER_ACTION_DODGE) {
		return -1;
  }

	if (pvars->MobVars.Action == SWARMER_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return SWARMER_ACTION_WALK;
  }

	// jump if we've hit a slope and are grounded
	if (mobHitWallShouldJump(moby, SWARMER_MAX_WALKABLE_SLOPE))
	{
		return SWARMER_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return SWARMER_ACTION_JUMP;
	}

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// check if a projectile is coming towards us
	if (pvars->MobVars.MoveVars.Grounded && mobIsProjectileComing(moby) && swarmerGetActionReady(moby, SWARMER_ATTACK_ACTION_DODGE))
	{
		return SWARMER_ACTION_DODGE;
	}

	// get next target
	Moby *target = swarmerGetNextTarget(moby);
	if (target)
	{
		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		if (dist <= attackRadius && swarmerGetActionReady(moby, SWARMER_ATTACK_ACTION_BITE))
		{
			if (swarmerCanAttack(pvars))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return SWARMER_ACTION_ATTACK;
			}
			return SWARMER_ACTION_WALK;
		}
		else
		{
			return SWARMER_ACTION_WALK;
		}
	}

	  // if roaming, then we want to periodically stop or reroute
  if (swarmerIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return SWARMER_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (swarmerIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return SWARMER_ACTION_IDLE;
  }
	
	return SWARMER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void swarmerRenderPath(Moby* moby)
{
  int x,y;
  int i;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* pathGraph = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
  u8* path = (u8*)pvars->MobVars.MoveVars.CurrentPath;
  int pathLen = pvars->MobVars.MoveVars.PathEdgeCount;
  int pathIdx = pvars->MobVars.MoveVars.PathEdgeCurrent;

  if (pathLen > 0) {
    u8* edge = pathGraph->Edges[path[0]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[0]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }
  for (i = 0; i < pathLen; ++i) {
    u8* edge = pathGraph->Edges[path[i]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[1]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }

  VECTOR t;
  if (pathGetTargetPos(pathGraph, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
    pvars->MobVars.Dirty = 1;
  if (gfxWorldSpaceToScreenSpace(t, &x, &y)) {
    gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "+", -1, 4);
  }

  /*int edgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
  if (edgeIdx < 255) {
    u8* edge = pathGraph->Edges[edgeIdx];
    float jumpAt = pathGraph->EdgesJumpAt[edgeIdx] / 255.0;
    VECTOR dt;
    vector_subtract(dt, pathGraph->Nodes[edge[1]], pathGraph->Nodes[edge[0]]);
    vector_scale(t, dt, pvars->MobVars.MoveVars.PathEdgeAlpha);
    vector_add(t, t, pathGraph->Nodes[edge[0]]);
    t[3] = 0;

    VECTOR a1;
    VECTOR startToMoby;
    vector_subtract(startToMoby, moby->Position, pathGraph->Nodes[edge[0]]);
    startToMoby[3] = 0;
    float len = vector_length(dt);
    vector_normalize(dt, dt);
    vector_project(a1, startToMoby, dt);
    DPRINTF("%f / %f\n", vector_length(a1) / len, pvars->MobVars.MoveVars.PathEdgeAlpha);
    vector_add(a1, a1, pathGraph->Nodes[edge[0]]);
    a1[3] = 0;
    if (gfxWorldSpaceToScreenSpace(a1, &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "I", -1, 4);
    }
  }*/
}
#endif

//--------------------------------------------------------------------------
void swarmerDoAction(Moby *moby)
{
  struct PathGraph* path = pathGetMobyPathGraph(moby, &((struct MobPVar*)moby->PVar)->MobVars.MoveVars);
  float speed = ((struct MobPVar*)moby->PVar)->MobVars.Config.Speed;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? SWARMER_TURN_RADIANS_PER_SEC : SWARMER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? SWARMER_MOVE_ACCELERATION : SWARMER_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == SWARMER_ACTION_FLINCH || pvars->MobVars.LastAction == SWARMER_ACTION_BIG_FLINCH);

	turnSpeed *= pvars->MobVars.Config.TurnSpeed;
	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&swarmerRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
case SWARMER_ACTION_DODGE:
	{
		float dodgeSpeedMult = mobGetFloat(moby, SWARMER_PARAM_DODGE_VELOCITY_MULTIPLIER);

		if (!pvars->MobVars.CurrentActionForTicks)
		{
			// determine left or right flip
			int leftOrRight = swarmerGetSideFlipLeftOrRight(pvars);
			float dir = leftOrRight * 2 - 1;

			// compute velocity from forward and direction (strafe)
			vector_fromyaw(t, moby->Rotation[2] + (MATH_PI / 2) * dir);
			vector_scale(t, t, pvars->MobVars.Config.Speed * 4 * MATH_DT);
			t[2] = 4 * MATH_DT;
			vector_scale(t, t, dodgeSpeedMult);
			vector_copy(pvars->MobVars.MoveVars.Velocity, t);
			pvars->MobVars.MoveVars.Grounded = 0;

			mobTransAnim(moby, SWARMER_ANIM_SIDE_FLIP, 0);
		}

		// face target
		if (target)
			mobTurnTowards(moby, target->Position, SWARMER_TURN_RADIANS_PER_SEC);

		// when we land, transition to idle
		// if (pvars->MobVars.AnimationLooped && (moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND || moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND2)) {
		if (pvars->MobVars.MoveVars.Grounded)
		{
			mobTransAnim(moby, SWARMER_ANIM_IDLE, 0);
			swarmerForceLocalAction(moby, SWARMER_ACTION_WALK);
			break;
		}

		if (moby->AnimSeqId == SWARMER_ANIM_SIDE_FLIP && pvars->MobVars.AnimationLooped)
		{
			mobTransAnim(moby, SWARMER_ANIM_JUMP_AND_FALL, 5);
			break;
		}

		if (moby->AnimSeqId == SWARMER_ANIM_SIDE_FLIP)
		{
			mobTransAnim(moby, SWARMER_ANIM_SIDE_FLIP, 0);
		}
		break;
	}
	case SWARMER_ACTION_ATTACK:
	{
		int attack1AnimId = SWARMER_ANIM_JUMP_FORWARD_BITE;
		mobTransAnim(moby, attack1AnimId, 0);

		float damage = pvars->MobVars.Config.Damage * mobGetFloat(moby, SWARMER_PARAM_BITE_DAMAGE_MULTIPLIER);
		float lungeMult = mobGetFloat(moby, SWARMER_PARAM_BITE_LUNGE_MULTIPLIER);

		float speedCurve = pvars->MobVars.Config.Speed * powf(clamp(SWARMER_ATTACK_ANIM_LUNGE_DURATION - moby->AnimSeqT, 1, 2.25), 2) * lungeMult;
		float speed = (moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < SWARMER_ATTACK_ANIM_LUNGE_DURATION) ? speedCurve : 1;
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= SWARMER_ATTACK_HIT_FRAME_START && moby->AnimSeqT < SWARMER_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		// printf("speed:%f lunge:%f final:%f\n", pvars->MobVars.Config.Speed, lungeMult, speed);

		if (!isInAirFromFlinching)
		{
			if (target)
			{
				mobTurnTowardsPredictiveWithSpeed(moby, target, SWARMER_TURN_LUNGE_RADIANS_PER_SEC, speed);
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
			swarmerDoDamage(moby, pvars->MobVars.Config.HitRadius, damage, damageFlags, 0);
		}
		break;
	}
	case SWARMER_ACTION_SPAWN:
		{
      mobTransAnim(moby, SWARMER_ANIM_ROAR, 0);
      //mobStand(moby);
			break;
		}
		case SWARMER_ACTION_FLINCH:
		case SWARMER_ACTION_BIG_FLINCH:
		{
      int animFlinchId = pvars->MobVars.Action == SWARMER_ACTION_BIG_FLINCH ? SWARMER_ANIM_FLINCH_SPIN_AND_STAND2 : SWARMER_ANIM_FLINCH_SPIN_AND_STAND;

      mobTransAnim(moby, animFlinchId, 0);

      if (pvars->MobVars.Knockback.Ticks > 0) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, SWARMER_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
      } else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case SWARMER_ACTION_IDLE:
		{
			mobTransAnim(moby, SWARMER_ANIM_IDLE, 0);
      mobStand(moby);
			break;
		}
		case SWARMER_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    mobTransAnim(moby, SWARMER_ANIM_JUMP_AND_FALL, 5);

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

          //DPRINTF("jump %f\n", jumpSpeed);
          vector_write(pvars->MobVars.MoveVars.Velocity, 0);
          pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
          pvars->MobVars.MoveVars.Grounded = 0;
          pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
          pvars->MobVars.MoveVars.JumpedThisAction = 1;
          mobResetMoveStep(moby);
        }
				break;
			}
		case SWARMER_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case SWARMER_ACTION_ROAM:
    case SWARMER_ACTION_WALK:
		{
      float dir = mobGetCurrentWalkAngle(moby);

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
      }

      
			// 
      if (moby->AnimSeqId == SWARMER_ANIM_JUMP_AND_FALL && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        swarmerForceLocalAction(moby, SWARMER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, SWARMER_ANIM_WALK, 0);
      } else {
				mobTransAnim(moby, SWARMER_ANIM_IDLE, 0);
      }
			break;
		}
    case SWARMER_ACTION_DIE:
    {
      // on destroy, give some upwards velocity for the backflip
      if (moby->AnimSeqId != SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND) {
        vector_fromyaw(t, pvars->MobVars.Knockback.Angle / 1000.0);
        t[2] = 1.0;
        vector_scale(t, t, 4 * 2 * MATH_DT);
        vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
      }

      mobTransAnimLerp(moby, SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND, 5, 0, &pvars->MobVars.AnimationReset, &pvars->MobVars.AnimationLooped);
      if (moby->AnimSeqId == SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND && moby->AnimSeqT > 25) {
        pvars->MobVars.Destroy = 1;
      }

      //mobStand(moby);
      break;
    }
		}

	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void swarmerDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, SWARMER_SUBSKELETON_JOINT_JAW, 1, 0);
}

//--------------------------------------------------------------------------
void swarmerForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float difficulty = 1;
	int stateCooldownTicks = SWARMER_ACTION_COOLDOWN_TICKS;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
	case SWARMER_ACTION_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case SWARMER_ACTION_DIE:
	{
		// can't undie
		return;
	}
	case SWARMER_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
	case SWARMER_ACTION_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		stateCooldownTicks = 0;
		break;
	}
	case SWARMER_ACTION_WALK:
	{

		break;
	}
	case SWARMER_ACTION_DIE:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case SWARMER_ACTION_ATTACK:
	{
		swarmerResetActionCooldownTicks(moby, SWARMER_ATTACK_ACTION_BITE);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		stateCooldownTicks = 0;
		break;
	}
	case SWARMER_ACTION_DODGE:
	{
		swarmerResetActionCooldownTicks(moby, SWARMER_ATTACK_ACTION_DODGE);
		break;
	}
	case SWARMER_ACTION_FLINCH:
	case SWARMER_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = SWARMER_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
		break;
	}
	case SWARMER_ACTION_ROAM:
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
short swarmerGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Config.Bangles;
}

//--------------------------------------------------------------------------
int swarmerIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (pvars->MobVars.Action == SWARMER_ACTION_ATTACK && !pvars->MobVars.AnimationLooped);
}

//--------------------------------------------------------------------------
int swarmerCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// always let non-owners simulate an action unless its the death action
	if (action == SWARMER_ACTION_DIE)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
int swarmerShouldForceStateUpdateOnAction(Moby *moby, int action)
{
  if (((struct MobPVar*)moby->PVar)->MobVars.Action == SWARMER_ACTION_ROAM || action == SWARMER_ACTION_ROAM) return 1;

	// only send action updates at regular intervals, unless dying or flinching
	if (action == SWARMER_ACTION_DIE || action == SWARMER_ACTION_FLINCH || action == SWARMER_ACTION_BIG_FLINCH)
		return 1;

	return 0;
}

//--------------------------------------------------------------------------
int swarmerIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == SWARMER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swarmerCanAttack(struct MobPVar *pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int swarmerGetSideFlipLeftOrRight(struct MobPVar *pvars)
{
	int seed = pvars->MobVars.Random + pvars->MobVars.ActionId;
	sha1(&seed, 4, &seed, 4);
	return seed % 2;
}

//--------------------------------------------------------------------------
int swarmerIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND || moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND2) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swarmerIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWARMER_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int swarmerCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(SwarmerMobVars_t), &guberEvent, NULL);
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
int swarmerIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWARMER_ACTION_ROAM;
}
