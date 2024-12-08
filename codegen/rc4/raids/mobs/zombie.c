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

void zombiePreUpdate(Moby* moby);
void zombiePostUpdate(Moby* moby);
void zombiePostDraw(Moby* moby);
void zombieMove(Moby* moby);
void zombieOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void zombieOnDestroy(Moby* moby, int killedByPlayerId, int weaponId);
void zombieOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int zombieOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void zombieOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
Moby* zombieGetNextTarget(Moby* moby);
int zombieGetPreferredAction(Moby* moby, int * delayTicks);
void zombieDoAction(Moby* moby);
void zombieDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void zombieForceLocalAction(Moby* moby, int action);
short zombieGetArmor(Moby* moby);
int zombieIsAttacking(Moby* moby);
int zombieCanNonOwnerTransitionToAction(Moby* moby, int action);
int zombieShouldForceStateUpdateOnAction(Moby* moby, int action);

int zombieIsSpawning(struct MobPVar* pvars);
int zombieIsRoaming(struct MobPVar* pvars);
int zombieIsIdling(struct MobPVar* pvars);
int zombieCanAttack(struct MobPVar* pvars);
int zombieIsFlinching(Moby* moby);
int zombieIsDying(Moby* moby);

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
int zombieCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar), &guberEvent, NULL);
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
void zombiePreUpdate(Moby* moby)
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
void zombiePostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(ZOMBIE_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(ZOMBIE_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = ZOMBIE_PRIMARY_COLOR;
  }

  // adjust animSpeed by speed and by animation
	float animSpeed = 0.7 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED);
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;
  if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP) {
    animSpeed = 0.9 * (1 - powf(moby->AnimSeqT / 35, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = 0.9;
    }
  } else if (zombieIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (zombieIsDying(moby)) {
    animSpeed = 0.9;
  }

	if ((moby->DrawDist == 0 && pvars->MobVars.Action == ZOMBIE_ACTION_WALK)) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void zombiePostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = ZOMBIE_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, 1);
}

//--------------------------------------------------------------------------
void zombieMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void zombieOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.256339 * scale;

  // colors by mob type
	moby->GlowRGBA = ZOMBIE_GLOW_COLOR;
	moby->PrimaryColor = ZOMBIE_PRIMARY_COLOR;

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
void zombieOnDestroy(Moby* moby, int killedByPlayerId, int weaponId)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = ZOMBIE_PRIMARY_COLOR;
  
	// limit corn spawning to prevent freezing/framelag
	if (MapConfig.State && MapConfig.State->MobStats.TotalAlive < 30 && killedByPlayerId >= 0) {
		mobSpawnCorn(moby, ZOMBIE_BANGLE_LARM | ZOMBIE_BANGLE_RARM | ZOMBIE_BANGLE_LLEG | ZOMBIE_BANGLE_RLEG | ZOMBIE_BANGLE_RFOOT | ZOMBIE_BANGLE_HIPS);
	}
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
int zombieOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void zombieOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby* zombieGetNextTarget(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Player ** players = playerGetAll();
	int i;
	VECTOR delta;
  VECTOR forward;
	Moby * currentTarget = pvars->MobVars.MoveVars.Target;
	Player * closestPlayer = NULL;
	float closestPlayerDist = 100000;

  vector_fromyaw(forward, moby->Rotation[2]);
	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player * p = players[i];
		if (p && p->SkinMoby && !playerIsDead(p) && p->Health > 0 && p->SkinMoby->Opacity >= 0x80) {
			vector_subtract(delta, p->PlayerPosition, moby->Position);
			float dist = vector_length(delta);
      Moby* pTargetMoby = playerGetTargetMoby(p);
      int isCurrentTarget = pTargetMoby == currentTarget;
      
      // determine angle from mob forward to player
      float theta = acosf(vector_innerproduct(forward, delta));
			if (dist < 300) {

        // skip if not in sight or aggro zone, unless already targeted
        if (!isCurrentTarget) {
          if (dist > pvars->MobVars.Config.AutoAggroMaxRange && (dist > pvars->MobVars.Config.VisionRange || fabsf(theta) > pvars->MobVars.Config.PeripheryRangeTheta)) continue;
          if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS && !spawnerOnChildConsiderTarget(moby->PParent, moby, pvars->MobVars.Userdata, pTargetMoby)) continue;
        }

				// favor existing target
				if (isCurrentTarget)
					dist *= (1.0 / ZOMBIE_TARGET_KEEP_CURRENT_FACTOR);
				
				// pick closest target
				if (dist < closestPlayerDist) {
					closestPlayer = p;
					closestPlayerDist = dist;
				}
			}
		}
	}

	if (closestPlayer)
		return playerGetTargetMoby(closestPlayer);

	return NULL;
}

//--------------------------------------------------------------------------
int zombieGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	VECTOR t;

	// no preferred action
	if (zombieIsAttacking(moby))
		return -1;

	if (zombieIsSpawning(pvars))
		return -1;

  if (zombieIsFlinching(moby))
    return -1;

	if (pvars->MobVars.Action == ZOMBIE_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded) {
		return ZOMBIE_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > ZOMBIE_MAX_WALKABLE_SLOPE) {
    return ZOMBIE_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return ZOMBIE_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = zombieGetNextTarget(moby);
	if (target) {
		vector_copy(t, target->Position);
		vector_subtract(t, t, moby->Position);
		float distSqr = vector_sqrmag(t);
		float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;

		if (distSqr <= attackRadiusSqr) {
			if (zombieCanAttack(pvars)) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return ZOMBIE_ACTION_ATTACK;
      }
			return ZOMBIE_ACTION_WALK;
		} else {
			return ZOMBIE_ACTION_WALK;
		}
	}

  // if roaming, then we want to periodically stop or reroute
  if (zombieIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    float dist = vector_length(t);
    
    // idle if near target or randomly
    if (dist < pvars->MobVars.Config.AttackRadius || rand(10007) == 0) {
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
void zombieDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_TURN_RADIANS_PER_SEC : ZOMBIE_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_MOVE_ACCELERATION : ZOMBIE_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == ZOMBIE_ACTION_FLINCH || pvars->MobVars.LastAction == ZOMBIE_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&zombieRenderPath, moby);
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
          mobTurnTowards(moby, t, turnSpeed);
          mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, pvars->MobVars.Config.Speed, acceleration);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded) {
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
      } else if (moby->AnimSeqId != ZOMBIE_ANIM_WALK || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
      }
      break;
    }
    case ZOMBIE_ACTION_WALK:
		{
      float dir = 0;
      if (target) {
        dir = ((pvars->MobVars.ActionId + pvars->MobVars.Random) % 3) - 1;
      }

      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, dir);
      }

			// 
      if (moby->AnimSeqId == ZOMBIE_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        zombieForceLocalAction(moby, ZOMBIE_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, ZOMBIE_ANIM_RUN, 0);
      } else if (moby->AnimSeqId != ZOMBIE_ANIM_RUN || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, ZOMBIE_ANIM_IDLE, 0);
      }
			break;
		}
    case ZOMBIE_ACTION_DIE:
    {
      mobStand(moby);
      break;
    }
		case ZOMBIE_ACTION_ATTACK:
		{
      int attack1AnimId = ZOMBIE_ANIM_SLAP;
			mobTransAnim(moby, attack1AnimId, 0);

			float speedMult = clamp((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1, 1, 5);
			int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 11 && moby->AnimSeqT < 12;
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
				zombieDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
			}
			break;
		}
  }

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void zombieDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, ZOMBIE_SUBSKELETON_JOINT_LEFT_HAND, 1, 0);
}

//--------------------------------------------------------------------------
void zombieForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
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
    case ZOMBIE_ACTION_ROAM:
    {
      struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
      if (path && path->NumNodes > 0 && mobAmIOwner(moby)) {

        int r = rand(path->NumNodes);
        int count = 0;

        // if we're in a spawner
        // then try and find a node thats in a habitable cuboid
        if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
          while (count < path->NumNodes && !spawnerOnChildConsiderRoamTarget(moby->PParent, moby, pvars->MobVars.Userdata, path->Nodes[r])) {
            r = (r + 1) % path->NumNodes;
            ++count;
          }
        }

        vector_copy(pvars->MobVars.MoveVars.TargetPosition, path->Nodes[r]);
        pvars->MobVars.MoveVars.TargetPosition[3] = 0;
      }
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
short zombieGetArmor(Moby* moby)
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
int zombieIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == ZOMBIE_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int zombieCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == ZOMBIE_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int zombieShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == ZOMBIE_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == ZOMBIE_ACTION_ROAM || action == ZOMBIE_ACTION_ROAM) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int zombieIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == ZOMBIE_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int zombieIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == ZOMBIE_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int zombieIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == ZOMBIE_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int zombieCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int zombieIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == ZOMBIE_ANIM_FLINCH || moby->AnimSeqId == ZOMBIE_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int zombieIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == ZOMBIE_ACTION_DIE;
}
