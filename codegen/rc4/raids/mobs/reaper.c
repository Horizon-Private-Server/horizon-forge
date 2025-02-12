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
#include "spawner.h"
#include "maputils.h"
#include "shared.h"

void reaperPreUpdate(Moby* moby);
void reaperPostUpdate(Moby* moby);
void reaperPostDraw(Moby* moby);
void reaperMove(Moby* moby);
void reaperOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void reaperOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void reaperOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int reaperOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void reaperOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
Moby* reaperGetNextTarget(Moby* moby);
int reaperGetPreferredAction(Moby* moby, int * delayTicks);
void reaperDoAction(Moby* moby);
void reaperDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void reaperForceLocalAction(Moby* moby, int action);
short reaperGetArmor(Moby* moby);
int reaperIsAttacking(Moby* moby);
int reaperCanNonOwnerTransitionToAction(Moby* moby, int action);
int reaperShouldForceStateUpdateOnAction(Moby* moby, int action);

int reaperIsSpawning(struct MobPVar* pvars);
int reaperIsRoaming(struct MobPVar* pvars);
int reaperIsIdling(struct MobPVar* pvars);
int reaperCanAttack(struct MobPVar* pvars);
int reaperIsFlinching(Moby* moby);
int reaperIsDying(Moby* moby);

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
      MapConfig.PopulateSpawnArgsFunc(&spawnArgs, args->Config, args->SpawnParamsIdx, args->SpawnFromUID == -1, args->DifficultyMult);
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
void reaperPreUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // decrement path target pos ticker
  decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void reaperPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = pvars->MobVars.Config.Scale;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(REAPER_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(REAPER_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = REAPER_PRIMARY_COLOR;
  }

  // adjust animSpeed by speed and by animation
  float baseSpeed = 1.0;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;
  if (reaperIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (reaperIsDying(moby)) {
    animSpeed = baseSpeed;
  } else if (moby->AnimSeqId == REAPER_ANIM_RUN) {
    animSpeed *= mobGetCurrentMoveSpeed(moby) * 0.5;
  } else if (moby->AnimSeqId == REAPER_ANIM_WALK) {
    animSpeed *= mobGetCurrentMoveSpeed(moby);
  }

	if ((moby->DrawDist == 0 && !reaperIsAttacking(moby) && !reaperIsSpawning(pvars) && !reaperIsDying(moby) && !reaperIsFlinching(moby))) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void reaperPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = REAPER_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, REAPER_SUBSKELETON_HEAD);
}

//--------------------------------------------------------------------------
void reaperMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void reaperOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.256339 * scale;

  // colors by mob type
	moby->GlowRGBA = REAPER_GLOW_COLOR;
	moby->PrimaryColor = REAPER_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 0.75 + (scale * 0.25);
  
#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void reaperOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = REAPER_PRIMARY_COLOR;
  
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
int reaperOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void reaperOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby* reaperGetNextTarget(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  ReaperMobVars_t* reaperVars = (ReaperMobVars_t*)pvars->AdditionalMobVarsPtr;
	Moby * currentTarget = pvars->MobVars.MoveVars.Target;

  // target player who hit us
  Moby* aggroTriggeredByTarget = playerGetTargetMoby(reaperVars->AggroTriggeredBy);
  if (reaperVars->AggroTriggered && aggroTriggeredByTarget) {
    reaperVars->AggroTriggeredBy = NULL;
    return aggroTriggeredByTarget;
  }

  // don't change target when aggro
  if (pvars->MobVars.Action == REAPER_ACTION_AGGRO && currentTarget) {
    Player* currentPlayerTarget = guberMobyGetPlayerDamager(currentTarget);
    if (currentPlayerTarget && !playerIsDead(currentPlayerTarget)) {
      return currentTarget;
    }
  }

  // defer to default
  return mobGetNextTarget(moby);
}

//--------------------------------------------------------------------------
int reaperGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  ReaperMobVars_t* reaperVars = (ReaperMobVars_t*)pvars->AdditionalMobVarsPtr;
	VECTOR t;

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
		return reaperVars->AggroTriggered ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (mobHitWallShouldJump(moby, REAPER_MAX_WALKABLE_SLOPE)) {
    return REAPER_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return REAPER_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = mobGetNextTarget(moby);
	if (target) {
		vector_copy(t, target->Position);
		vector_subtract(t, t, moby->Position);
		float distSqr = vector_sqrmag(t);
		float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;

		if (distSqr <= attackRadiusSqr) {
			if (reaperCanAttack(pvars)) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return REAPER_ACTION_ATTACK;
      }
      
      // wait for sprint to finish
      if (pvars->MobVars.Action == REAPER_ACTION_AGGRO)
        return -1;

			return reaperVars->AggroTriggered ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
		} else {

      // wait for sprint to finish
      if (pvars->MobVars.Action == REAPER_ACTION_AGGRO)
        return -1;

			return reaperVars->AggroTriggered ? REAPER_ACTION_AGGRO : REAPER_ACTION_WALK;
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
void reaperDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  ReaperMobVars_t* reaperVars = (ReaperMobVars_t*)pvars->AdditionalMobVarsPtr;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.Config.TurnSpeed * (pvars->MobVars.MoveVars.Grounded ? REAPER_TURN_RADIANS_PER_SEC : REAPER_TURN_AIR_RADIANS_PER_SEC);
  float acceleration = pvars->MobVars.MoveVars.Grounded ? REAPER_MOVE_ACCELERATION : REAPER_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == REAPER_ACTION_FLINCH || pvars->MobVars.LastAction == REAPER_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&reaperRenderPath, moby);
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
    case REAPER_ACTION_AGGRO:
		{
      int nextAnimId = moby->AnimSeqId;
      switch (moby->AnimSeqId)
      {
        case REAPER_ANIM_AGGRO_ROAR:
        {
          if (pvars->MobVars.AnimationLooped) {
            nextAnimId = REAPER_ANIM_RUN;
          }
          break;
        }
      }
      
      // trigger roar
      if (!pvars->MobVars.CurrentActionForTicks && reaperVars->AggroRoarTriggered) {
        nextAnimId = REAPER_ANIM_AGGRO_ROAR;
        reaperVars->AggroRoarTriggered = 0;
      }

      mobTransAnim(moby, nextAnimId, 0);

      // let aggro roar finish before moving
      if (nextAnimId == REAPER_ANIM_AGGRO_ROAR) {
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed);
        }

        mobStand(moby);
        break;
      }
      
      // fall through to move
      speed = clamp(speed * REAPER_AGGRO_SPEED_MULTIPLIER, 0, MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx].Config.MaxSpeed);
      goto walk;
		}
    case REAPER_ACTION_WALK:
		{
      walk:;
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
		case REAPER_ACTION_ATTACK:
		{
      int attack1AnimId = REAPER_ANIM_SWING;
      int nextAnimId = mobGetAnimIf(moby, REAPER_ANIM_IDLE, attack1AnimId, !pvars->MobVars.CurrentActionForTicks, 1);
			mobTransAnim(moby, nextAnimId, 0);

      float damageMult = ((pvars->MobVars.LastAction == REAPER_ACTION_AGGRO) ? REAPER_AGGRO_DAMAGE_MULTIPLIER : 1);
			float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1, 1, 5);
			int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 14 && moby->AnimSeqT < 17;
			u32 damageFlags = 0x00081801;

      if (!isInAirFromFlinching) {
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed);
          mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, target->Position, speedMult * pvars->MobVars.Config.Speed, acceleration);
        } else {
          // stand
          mobStand(moby);
        }
      }

			if (swingAttackReady && damageFlags) {
				reaperDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage * damageMult, damageFlags, 0);
			}
			break;
		}
  }

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void reaperDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  ReaperMobVars_t* reaperVars = (ReaperMobVars_t*)pvars->AdditionalMobVarsPtr;

  int hitFlags = mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, REAPER_SUBSKELETON_LEFT_HAND, 1, 0);
  
  // aggro ends when we finally hit our target
  if (hitFlags & MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET) {
    reaperVars->AggroTriggered = 0;
  }
}

//--------------------------------------------------------------------------
void reaperForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
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
short reaperGetArmor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.Health;
  int bangles = pvars->MobVars.Config.Bangles;

  if (t < 0.3)
    return 0x0000;
  else if (t < 0.7)
    return bangles & 0x1f; // remove torso bangle

	return bangles;
}

//--------------------------------------------------------------------------
int reaperIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == REAPER_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reaperCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == REAPER_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int reaperShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == REAPER_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == REAPER_ACTION_ROAM || action == REAPER_ACTION_ROAM) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int reaperIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == REAPER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reaperIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == REAPER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int reaperIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == REAPER_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int reaperCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int reaperIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == REAPER_ANIM_FLINCH || moby->AnimSeqId == REAPER_ANIM_FLINCH_KNOCKBACK) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reaperIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == REAPER_ACTION_DIE;
}
