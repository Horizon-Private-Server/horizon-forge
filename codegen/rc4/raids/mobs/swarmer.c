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
#include "mob.h"
#include "pathfind.h"
#include "spawner.h"
#include "maputils.h"
#include "shared.h"

void swarmerPreUpdate(Moby* moby);
void swarmerPostUpdate(Moby* moby);
void swarmerPostDraw(Moby* moby);
void swarmerMove(Moby* moby);
void swarmerOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void swarmerOnDestroy(Moby* moby, int killedByPlayerId, int weaponId);
void swarmerOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int swarmerOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void swarmerOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
Moby* swarmerGetNextTarget(Moby* moby);
int swarmerGetPreferredAction(Moby* moby, int * delayTicks);
void swarmerDoAction(Moby* moby);
void swarmerDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void swarmerForceLocalAction(Moby* moby, int action);
short swarmerGetArmor(Moby* moby);
int swarmerIsAttacking(Moby* moby);
int swarmerCanNonOwnerTransitionToAction(Moby* moby, int action);
int swarmerShouldForceStateUpdateOnAction(Moby* moby, int action);

int swarmerIsSpawning(struct MobPVar* pvars);
int swarmerIsRoaming(struct MobPVar* pvars);
int swarmerIsIdling(struct MobPVar* pvars);
int swarmerCanAttack(struct MobPVar* pvars);
int swarmerGetSideFlipLeftOrRight(struct MobPVar* pvars);
int swarmerIsFlinching(Moby* moby);
int swarmerIsDying(Moby* moby);
float swarmerGetDodgeProbability(Moby* moby);

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
int swarmerCreate(struct MobCreateArgs* args)
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
		guberEventWrite(guberEvent, &spawnArgs, sizeof(struct MobSpawnEventArgs));
	}
	else
	{
		DPRINTF("failed to guberevent mob\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void swarmerPreUpdate(Moby* moby)
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
void swarmerPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = pvars->MobVars.Config.Scale;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(SWARMER_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    moby->PrimaryColor = colorLerp(SWARMER_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = SWARMER_PRIMARY_COLOR;
  }

  // adjust animSpeed by speed and by animation
	float animSpeed = 0.9 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) * scale;
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;
  if (moby->AnimSeqId == SWARMER_ANIM_JUMP) {
    animSpeed = 0.9 * (1 - powf(moby->AnimSeqT / 35, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = 0.9;
    }
  } else if (moby->AnimSeqId == SWARMER_ANIM_JUMP_AND_FALL) {
    //animSpeed = 0.9 * (1 - powf(moby->AnimSeqT / 15, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = 0.9;
    }
  } else if (swarmerIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (swarmerIsDying(moby)) {
    animSpeed = 0.9;
  }

  if (moby->AnimSeqId == SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND) {
    animSpeed = 1;
  }

	if ((moby->DrawDist == 0 && pvars->MobVars.Action == SWARMER_ACTION_WALK)) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void swarmerPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = SWARMER_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, 0);
}

//--------------------------------------------------------------------------
void swarmerMove(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void swarmerOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
  
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.256339 * scale;

  // colors by mob type
	moby->GlowRGBA = SWARMER_GLOW_COLOR;
	moby->PrimaryColor = SWARMER_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 1 + (scale * 0.25);
  pvars->MobVars.BlipType = 4;
  pvars->MobVars.BlipTeam = TEAM_RED;

#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void swarmerOnDestroy(Moby* moby, int killedByPlayerId, int weaponId)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = SWARMER_PRIMARY_COLOR;
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
int swarmerOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void swarmerOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby* swarmerGetNextTarget(Moby* moby)
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
					dist *= (1.0 / SWARMER_TARGET_KEEP_CURRENT_FACTOR);
				
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
int swarmerGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	VECTOR t;

	// no preferred action
	if (swarmerIsAttacking(moby))
		return -1;

	if (swarmerIsSpawning(pvars))
		return -1;

  if (swarmerIsFlinching(moby))
    return -1;

	if (pvars->MobVars.Action == SWARMER_ACTION_DODGE) {
		return -1;
  }

	if (pvars->MobVars.Action == SWARMER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded) {
		return SWARMER_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > SWARMER_MAX_WALKABLE_SLOPE) {
    return SWARMER_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return SWARMER_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

  // check if a projectile is coming towards us
  if (pvars->MobVars.MoveVars.Grounded && mobIsProjectileComing(moby) && randRange(0, 1) <= swarmerGetDodgeProbability(moby)) {
    return SWARMER_ACTION_DODGE;
  }

	// get next target
	Moby * target = swarmerGetNextTarget(moby);
	if (target) {
		vector_copy(t, target->Position);
		vector_subtract(t, t, moby->Position);
		float distSqr = vector_sqrmag(t);
		float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;

		if (distSqr <= attackRadiusSqr) {
			if (swarmerCanAttack(pvars)) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return SWARMER_ACTION_ATTACK;
      }
			return SWARMER_ACTION_WALK;
		} else {
			return SWARMER_ACTION_WALK;
		}
	}
	
  // if roaming, then we want to periodically stop or reroute
  if (swarmerIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    float dist = vector_length(t);
    
    // idle if near target or randomly
    if (dist < pvars->MobVars.Config.AttackRadius || rand(10007) == 0) {
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
}
#endif

//--------------------------------------------------------------------------
void swarmerDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? SWARMER_TURN_RADIANS_PER_SEC : SWARMER_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? SWARMER_MOVE_ACCELERATION : SWARMER_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == SWARMER_ACTION_FLINCH || pvars->MobVars.LastAction == SWARMER_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&swarmerRenderPath, moby);
#endif

	switch (pvars->MobVars.Action)
	{
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
          mobTurnTowards(moby, t, turnSpeed);
          mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, pvars->MobVars.Config.Speed, acceleration);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded) {
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
          pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
          pvars->MobVars.MoveVars.Grounded = 0;
          pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
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
      float dir = 0;
      if (target) {
        dir = ((pvars->MobVars.ActionId + pvars->MobVars.Random) % 3) - 1;
      }

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
      } else if (moby->AnimSeqId != SWARMER_ANIM_WALK || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, SWARMER_ANIM_IDLE, 0);
      }
			break;
		}
    case SWARMER_ACTION_DODGE:
    {
      if (!pvars->MobVars.CurrentActionForTicks) {
        // determine left or right flip
        int leftOrRight = swarmerGetSideFlipLeftOrRight(pvars);
        float dir = leftOrRight*2 - 1;

        // compute velocity from forward and direction (strafe)
        vector_fromyaw(t, moby->Rotation[2] + (MATH_PI/2)*dir);
        vector_scale(t, t, pvars->MobVars.Config.Speed * 4 * MATH_DT);
        t[2] = 4 * MATH_DT;
        vector_copy(pvars->MobVars.MoveVars.Velocity, t);
        pvars->MobVars.MoveVars.Grounded = 0;
        
        mobTransAnim(moby, SWARMER_ANIM_SIDE_FLIP, 0);
      }
      
      // face target
      if (target)
        mobTurnTowards(moby, target->Position, SWARMER_TURN_RADIANS_PER_SEC);

      // when we land, transition to idle
      //if (pvars->MobVars.AnimationLooped && (moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND || moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND2)) {
      if (pvars->MobVars.MoveVars.Grounded) {
        mobTransAnim(moby, SWARMER_ANIM_IDLE, 0);
        swarmerForceLocalAction(moby, SWARMER_ACTION_WALK);
        break;
      }

      if (moby->AnimSeqId == SWARMER_ANIM_SIDE_FLIP && pvars->MobVars.AnimationLooped) {
        mobTransAnim(moby, SWARMER_ANIM_JUMP_AND_FALL, 5);
        break;
      }

      if (moby->AnimSeqId == SWARMER_ANIM_SIDE_FLIP) {
        mobTransAnim(moby, SWARMER_ANIM_SIDE_FLIP, 0);
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

      mobTransAnimLerp(moby, SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND, 5, 0);
      if (moby->AnimSeqId == SWARMER_ANIM_FLINCH_BACKFLIP_AND_STAND && moby->AnimSeqT > 25) {
        pvars->MobVars.Destroy = 1;
      }

      //mobStand(moby);
      break;
    }
		case SWARMER_ACTION_ATTACK:
		{
      int attack1AnimId = SWARMER_ANIM_JUMP_FORWARD_BITE;
			mobTransAnim(moby, attack1AnimId, 0);

      float speedCurve = powf(clamp(5 - moby->AnimSeqT, 1, 2.25), 2);
			float speedMult = (moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? speedCurve : 1;
			int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 5 && moby->AnimSeqT < 8;
			u32 damageFlags = 0x00081801;

      if (speedMult < 1)
				speedMult = 1;

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
				swarmerDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
			}
			break;
		}
	}

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void swarmerDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, SWARMER_SUBSKELETON_JOINT_JAW, 1, 0);
}

//--------------------------------------------------------------------------
void swarmerForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float difficulty = 1;

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
	}

	// to
	switch (action)
	{
		case SWARMER_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case SWARMER_ACTION_ROAM:
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
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
			break;
		}
		case SWARMER_ACTION_FLINCH:
		case SWARMER_ACTION_BIG_FLINCH:
		{
			pvars->MobVars.FlinchCooldownTicks = SWARMER_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = SWARMER_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short swarmerGetArmor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return pvars->MobVars.Config.Bangles;
}

//--------------------------------------------------------------------------
int swarmerIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == SWARMER_ACTION_ATTACK && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swarmerCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == SWARMER_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int swarmerShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == SWARMER_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == SWARMER_ACTION_ROAM || action == SWARMER_ACTION_ROAM) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int swarmerIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWARMER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swarmerIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWARMER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int swarmerIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == SWARMER_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int swarmerCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int swarmerGetSideFlipLeftOrRight(struct MobPVar* pvars)
{
  int seed = pvars->MobVars.Random + pvars->MobVars.ActionId;
  sha1(&seed, 4, &seed, 4);
  return seed % 2;
}

//--------------------------------------------------------------------------
int swarmerIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND || moby->AnimSeqId == SWARMER_ANIM_FLINCH_SPIN_AND_STAND2) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int swarmerIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == SWARMER_ACTION_DIE;
}

//--------------------------------------------------------------------------
float swarmerGetDodgeProbability(Moby* moby)
{
  //int roundNo = 0;
  //if (MapConfig.State) roundNo = MapConfig.State->RoundNumber;

  float factor = clamp(powf(25.0 / 100.0, 2), 0, 1);
  return lerpf(0.03, 0.25, factor);
}
