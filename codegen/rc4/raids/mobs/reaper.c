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
#include "spawner.h"
#include "shared.h"
#include "utils.h"

void reaperResetActionCooldownTicks(Moby *moby, enum ReaperAttackActions action);
int reaperGetActionReady(Moby *moby, enum ReaperAttackActions action);
void reaperPreUpdate(Moby *moby);
void reaperPostUpdate(Moby *moby);
void reaperPostDraw(Moby *moby);
void reaperAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount);
void reaperMove(Moby* moby);
void reaperOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void reaperOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void reaperOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int reaperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void reaperOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
Moby *reaperGetNextTarget(Moby *moby);
int reaperGetPreferredAction(Moby *moby, int *delayTicks);
void reaperRenderPath(Moby* moby);
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
int reaperIsRoaming(struct MobPVar* pvars);
int reaperCreate(struct MobCreateArgs* args);
int reaperIsIdling(struct MobPVar* pvars);

struct MobVTable ReaperVTable = {
  .PreUpdate = &reaperPreUpdate,
  .PostUpdate = &reaperPostUpdate,
  .PostDraw = &reaperPostDraw,
  .Move = &reaperMove,
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

void reaperResetActionCooldownTicks(Moby *moby, enum ReaperAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;
	reaperVars->ActionCooldownTicks[action] = mobGetActionCooldownTicks(moby, action);
	reaperVars->ActionQueuedForTicks[action] = 0;
}

//--------------------------------------------------------------------------
int reaperGetActionReady(Moby *moby, enum ReaperAttackActions action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;
	return reaperVars->ActionQueuedForTicks[action] > 0;
}

//--------------------------------------------------------------------------
void reaperPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;

	mobDefaultPreUpdate(moby);

	if (!mobIsFrozen(moby))
		mobTickActionCooldowns(moby, REAPER_ATTACK_ACTION_COUNT, reaperVars->ActionCooldownTicks, reaperVars->ActionQueuedForTicks);
}

//--------------------------------------------------------------------------
void reaperPostUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float baseSpeed = 1.5;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (reaperIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / REAPER_FLINCH_ANIM_DURATION, 2));
	}
	else if (moby->AnimSeqId == REAPER_ANIM_SWING)
	{
		animSpeed = baseSpeed * mobGetFloat(moby, REAPER_PARAM_MELEE_ATTACK_SPEED_MULTIPLIER);
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
	u32 color = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, mobGetSpawnParams(moby)->SpriteTexId, color, REAPER_SUBSKELETON_HEAD);
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
void reaperMove(Moby* moby)
{
  mobMove(moby);
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
	moby->GlowRGBA = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 0.75 + (scale * 0.25);
	pvars->MobVars.BlipType = MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

#if MOB_DAMAGETYPES
	pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

	// default move step
	pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;

	// initialize action cooldowns
	int i;
	for (i = 0; i < REAPER_ATTACK_ACTION_COUNT; ++i)
		reaperResetActionCooldownTicks(moby, i);
}

//--------------------------------------------------------------------------
void reaperOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = mobGetSpawnParams(moby)->BaseColor;
  
  // spawn corn
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
void reaperOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  ReaperMobVars_t* reaperVars = (ReaperMobVars_t*)pvars->AdditionalMobVarsPtr;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != REAPER_ACTION_FLINCH 
            && pvars->MobVars.Action != REAPER_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    reaperForceLocalAction(moby, REAPER_ACTION_DIE);
	}

	// knockback
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		memcpy(&pvars->MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
	}

  // trigger aggro
  Player* sourcePlayer = playerGetFromUID(e->SourceUID);
  if (!reaperVars->AggroTriggered && sourcePlayer) {
    reaperVars->AggroTriggered = 1;
    reaperVars->AggroRoarTriggered = 1;
    reaperVars->AggroTriggeredBy = sourcePlayer;
    pvars->MobVars.MoveVars.Target = playerGetTargetMoby(sourcePlayer);
    DPRINTF("aggro triggered by %d\n", sourcePlayer->PlayerId);
  }

  // flinch
	if (mobAmIOwner(moby))
	{
		float damageRatio = damage / pvars->MobVars.Config.Health;
    float pFactor = reaperVars->AggroTriggered ? 0.5 : 1;
    float powerFactor = REAPER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((pFactor * damageRatio * REAPER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, REAPER_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, REAPER_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, REAPER_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, REAPER_ACTION_FLINCH);
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
int reaperOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	return mobDefaultOnLocalDamage(moby, e);
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
	Moby *currentTarget = pvars->MobVars.MoveVars.Target;

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

	return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int reaperGetPreferredAction(Moby *moby, int *delayTicks)
{
	VECTOR t;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReaperMobVars_t *reaperVars = (ReaperMobVars_t *)pvars->AdditionalMobVarsPtr;

	// no preferred action
	if (reaperIsAttacking(moby))
		return -1;

	if (reaperIsSpawning(pvars))
		return -1;

	if (reaperIsFlinching(moby))
		return -1;

  if (pvars->MobVars.Action == REAPER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == REAPER_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return (reaperVars->AggroTriggered && reaperGetActionReady(moby, REAPER_ATTACK_ACTION_AGGRO)) ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
  }

	// jump if we've hit a slope and are grounded
	if (mobHitWallShouldJump(moby, REAPER_MAX_WALKABLE_SLOPE))
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
			if (reaperCanAttack(pvars) && reaperGetActionReady(moby, REAPER_ATTACK_ACTION_MELEE))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return REAPER_ACTION_ATTACK;
			}

			// wait for sprint to finish
			if (pvars->MobVars.Action == REAPER_ACTION_AGGRO)
				return -1;

			return (reaperVars->AggroTriggered && reaperGetActionReady(moby, REAPER_ATTACK_ACTION_AGGRO)) ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
		}
		else
		{

			// wait for sprint to finish
			if (pvars->MobVars.Action == REAPER_ACTION_AGGRO)
				return -1;

			return (reaperVars->AggroTriggered && reaperGetActionReady(moby, REAPER_ATTACK_ACTION_AGGRO)) ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
		}
	}

	  // if roaming, then we want to periodically stop or reroute
  if (reaperIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;
    
    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return REAPER_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (reaperIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return REAPER_ACTION_IDLE;
  }
	
	return REAPER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void reaperRenderPath(Moby* moby)
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
void reaperDoAction(Moby *moby)
{
  struct PathGraph* path = pathGetMobyPathGraph(moby, &((struct MobPVar*)moby->PVar)->MobVars.MoveVars);
  float speed = ((struct MobPVar*)moby->PVar)->MobVars.Config.Speed;
  int isInAirFromFlinching = !((struct MobPVar*)moby->PVar)->MobVars.MoveVars.Grounded && (((struct MobPVar*)moby->PVar)->MobVars.LastAction == REAPER_ACTION_FLINCH || ((struct MobPVar*)moby->PVar)->MobVars.LastAction == REAPER_ACTION_BIG_FLINCH);

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? REAPER_TURN_RADIANS_PER_SEC : REAPER_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? REAPER_MOVE_ACCELERATION : REAPER_MOVE_AIR_ACCELERATION;
	turnSpeed *= pvars->MobVars.Config.TurnSpeed;
	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&reaperRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
case REAPER_ACTION_AGGRO:
	{
		int nextAnimId = moby->AnimSeqId;

		float speed = pvars->MobVars.Config.Speed * mobGetFloat(moby, REAPER_PARAM_MELEE_AGGRO_MOVE_SPEED_MULTIPLIER);

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
			if (mobGetPathTargetPos(t, moby) && mobAmIOwner(moby))
				pvars->MobVars.Dirty = 1; // new path, sync with other clients
			mobMoveTowards(moby, t, speed, turnSpeed, acceleration, mobGetCurrentWalkAngle(moby));
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
	case REAPER_ACTION_ATTACK:
	{
		int attack1AnimId = REAPER_ANIM_SWING;
		mobTransAnim(moby, attack1AnimId, 0);

		float lungeMult = mobGetFloat(moby, REAPER_PARAM_MELEE_LUNGE_MULTIPLIER);
		float damageMultiplier = mobGetFloat(moby, (pvars->MobVars.LastAction == REAPER_ACTION_AGGRO) ? REAPER_PARAM_MELEE_AGGRO_DAMAGE_MULTIPLIER : REAPER_PARAM_MELEE_DAMAGE_MULTIPLIER);
		float damage = pvars->MobVars.Config.Damage * damageMultiplier;

		int lungeActive = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < REAPER_ATTACK_ANIM_LUNGE_DURATION;
		float speed = lungeMult * clamp(difficulty * 2, 1, 5);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= REAPER_ATTACK_HIT_FRAME_START && moby->AnimSeqT < REAPER_ATTACK_HIT_FRAME_END;
		u32 damageFlags = mobGetDamageFlags(moby, MOB_DAMAGE_FLAG_BASE);

		if (target && lungeActive)
		{
			mobTurnTowardsPredictiveWithSpeed(moby, target, REAPER_TURN_LUNGE_RADIANS_PER_SEC, speed);
			mobMoveTowards(moby, target->Position, speed, 0, acceleration, 0);
		}
		else
		{
			mobStand(moby);
		}

		if (swingAttackReady && damageFlags)
		{
			reaperDoDamage(
					moby,
					pvars->MobVars.Config.HitRadius,
					damage,
					damageFlags,
					0);
		}
		break;
	}
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
      
			if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == REAPER_ACTION_BIG_FLINCH) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, REAPER_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
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
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
			    //mobTransAnim(moby, REAPER_ANIM_JUMP, 5);
          //mobResetSoundTrigger(moby);

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
		case REAPER_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case REAPER_ACTION_ROAM:
    {
      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, 0);
      }

			// 
      if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        reaperForceLocalAction(moby, REAPER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, REAPER_ANIM_WALK, 0);
      } else {
				mobTransAnim(moby, REAPER_ANIM_IDLE, 0);
      }
      break;
    }
    case REAPER_ACTION_WALK:
		{
      float dir = mobGetCurrentWalkAngle(moby);

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
      }

			// 
      if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        reaperForceLocalAction(moby, REAPER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, REAPER_ANIM_RUN, 0);
      } else {
				mobTransAnim(moby, REAPER_ANIM_IDLE, 0);
      }
			break;
		}
    case REAPER_ACTION_DIE:
    {
      mobTransAnim(moby, REAPER_ANIM_FALL_AND_DIE, 0);
      if (moby->AnimSeqId == REAPER_ANIM_FALL_AND_DIE && pvars->MobVars.AnimationLooped) {
        pvars->MobVars.Destroy = 1;
      }

      mobStand(moby);
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
	if (mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, REAPER_SUBSKELETON_LEFT_HAND, 1, 0) & MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET)
	{
		reaperVars->AggroTriggered = 0;
	}
}

//--------------------------------------------------------------------------
void reaperForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float difficulty = 1;
	int stateCooldownTicks = REAPER_ACTION_COOLDOWN_TICKS;

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
	case REAPER_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
	case REAPER_ACTION_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		stateCooldownTicks = 0;
		break;
	}
	case REAPER_ACTION_AGGRO:
	{
        reaperResetActionCooldownTicks(moby, REAPER_ATTACK_ACTION_AGGRO);
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
		reaperResetActionCooldownTicks(moby, REAPER_ATTACK_ACTION_MELEE);
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		stateCooldownTicks = 0;
		break;
	}
	case REAPER_ACTION_FLINCH:
	case REAPER_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = REAPER_FLINCH_COOLDOWN_TICKS;
		stateCooldownTicks = 0;
		break;
	}
	default:
	{
		break;
	}
	case REAPER_ACTION_ROAM:
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
  if (((struct MobPVar*)moby->PVar)->MobVars.Action == REAPER_ACTION_ROAM || action == REAPER_ACTION_ROAM) return 1;

	// only send action updates at regular intervals, unless dying or flinching
	if (action == REAPER_ACTION_DIE || action == REAPER_ACTION_FLINCH || action == REAPER_ACTION_BIG_FLINCH)
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

//--------------------------------------------------------------------------
int reaperIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == REAPER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int reaperCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(ReaperMobVars_t), &guberEvent, NULL);
	if (guberEvent)
	{
    if (MapConfig.PopulateSpawnArgsFunc) {
      MapConfig.PopulateSpawnArgsFunc(&spawnArgs, args);
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
int reaperIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == REAPER_ACTION_IDLE;
}
