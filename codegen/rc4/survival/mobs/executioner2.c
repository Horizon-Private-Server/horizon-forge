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
#include "utils.h"
#include "maputils.h"
#include "shared.h"

void executioner2PreUpdate(Moby* moby);
void executioner2PostUpdate(Moby* moby);
void executioner2PostDraw(Moby* moby);
void executioner2Move(Moby* moby);
void executioner2OnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void executioner2OnDestroy(Moby* moby, int killedByPlayerId, int weaponId);
void executioner2OnDamage(Moby* moby, struct MobDamageEventArgs* e);
int executioner2OnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void executioner2OnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
Moby* executioner2GetNextTarget(Moby* moby);
int executioner2GetPreferredAction(Moby* moby, int * delayTicks);
void executioner2DoAction(Moby* moby);
void executioner2DoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void executioner2ForceLocalAction(Moby* moby, int action);
short executioner2GetArmor(Moby* moby);
int executioner2IsAttacking(Moby* moby);
int executioner2CanNonOwnerTransitionToAction(Moby* moby, int action);
int executioner2ShouldForceStateUpdateOnAction(Moby* moby, int action);

int executioner2IsSpawning(struct MobPVar* pvars);
int executioner2CanAttack(struct MobPVar* pvars);
int executioner2IsFlinching(Moby* moby);

struct MobVTable Executioner2VTable = {
  .PreUpdate = &executioner2PreUpdate,
  .PostUpdate = &executioner2PostUpdate,
  .PostDraw = &executioner2PostDraw,
  .Move = &executioner2Move,
  .OnSpawn = &executioner2OnSpawn,
  .OnDestroy = &executioner2OnDestroy,
  .OnDamage = &executioner2OnDamage,
  .OnLocalDamage = &executioner2OnLocalDamage,
  .OnStateUpdate = &executioner2OnStateUpdate,
  .GetNextTarget = &executioner2GetNextTarget,
  .GetPreferredAction = &executioner2GetPreferredAction,
  .ForceLocalAction = &executioner2ForceLocalAction,
  .DoAction = &executioner2DoAction,
  .DoDamage = &executioner2DoDamage,
  .GetArmor = &executioner2GetArmor,
  .IsAttacking = &executioner2IsAttacking,
  .CanNonOwnerTransitionToAction = &executioner2CanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &executioner2ShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
int executioner2Create(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config)
{
	struct MobSpawnEventArgs args;
  struct MobSpawnParams* spawnParams = &MapConfig.DefaultSpawnParams[spawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar), &guberEvent, NULL);
	if (guberEvent)
	{
    if (MapConfig.PopulateSpawnArgsFunc) {
      MapConfig.PopulateSpawnArgsFunc(&args, config, spawnParamsIdx, spawnFromUID == -1, spawnFlags);
    }

		u8 random = (u8)rand(100);

    position[2] += 1; // spawn slightly above point
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, &yaw, 4);
		guberEventWrite(guberEvent, &spawnFromUID, 4);
		guberEventWrite(guberEvent, &spawnFlags, 4);
		guberEventWrite(guberEvent, &random, 1);
		guberEventWrite(guberEvent, &args, sizeof(struct MobSpawnEventArgs));
	}
	else
	{
		DPRINTF("failed to guberevent mob\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void executioner2PreUpdate(Moby* moby)
{
  int i;
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  Executioner2MobVars_t* executioner2Vars = (Executioner2MobVars_t*)pvars->AdditionalMobVarsPtr;
  
  // decrement tickers regardless of frozen state
  for (i = 0; i < GAME_MAX_LOCALS; ++i)
    decTimerU8(&executioner2Vars->LocalPlayerDamageHitInvTimer[i]);

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
void executioner2PostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = mobGetScaleMultiplier(moby);

  // adjust animSpeed by speed and by animation
	float animSpeed = 0.6 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
  if (moby->AnimSeqId == EXECUTIONER2_ANIM_JUMP) {
    animSpeed = 1 * (1 - powf(moby->AnimSeqT / 21, 1));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = 0.6;
    }
  } else if (executioner2IsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  }

	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == EXECUTIONER2_ACTION_WALK)) {
		moby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
	}
}

//--------------------------------------------------------------------------
void executioner2PostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  u32 color = EXECUTIONER2_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, 1);
}

//--------------------------------------------------------------------------
void executioner2AlterTarget(VECTOR out, Moby* moby, VECTOR forward, float amount)
{
	VECTOR up = {0,0,1,0};
	
	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void executioner2Move(Moby* moby)
{
  mobMove(moby);
}

//--------------------------------------------------------------------------
void executioner2OnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = mobGetScaleMultiplier(moby);

  // set scale
  moby->Scale = 0.35 * scale;

  // colors by mob type
	moby->GlowRGBA = EXECUTIONER2_GLOW_COLOR;
	moby->PrimaryColor = EXECUTIONER2_PRIMARY_COLOR;

  // targeting
	pvars->TargetVars.targetHeight = 1.5 + (scale * 0.5);
  pvars->MobVars.BlipType = 6;

#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // russion doll
  if (pvars->MobVars.SpawnFlags & MOB_SPAWN_FLAG_RUSSIAN_DOLL) {
    mobSetAction(moby, EXECUTIONER2_ACTION_BIG_FLINCH);
  }

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
}

//--------------------------------------------------------------------------
void executioner2OnDestroy(Moby* moby, int killedByPlayerId, int weaponId)
{
  VECTOR expOffset;
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = EXECUTIONER2_PRIMARY_COLOR;

  // spawn explosion
  u32 expColor = 0x801E70D6;
  vector_copy(expOffset, moby->Position);
  expOffset[2] += pvars->TargetVars.targetHeight;
  u128 expPos = vector_read(expOffset);
  mobySpawnExplosion
    (expPos, 0, 0, 0, 0, 16, 0, 16, 0, 1, 0, 0, 0, 0,
    0, 0, expColor, expColor, expColor, expColor, expColor, expColor, expColor, expColor,
    0, 0, 0, 0, 0, 1, 0, 0, 0);
}

//--------------------------------------------------------------------------
void executioner2OnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != EXECUTIONER2_ACTION_FLINCH 
            && pvars->MobVars.Action != EXECUTIONER2_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    executioner2ForceLocalAction(moby, EXECUTIONER2_ACTION_DIE);
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
    float powerFactor = EXECUTIONER2_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * EXECUTIONER2_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, EXECUTIONER2_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, EXECUTIONER2_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, EXECUTIONER2_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, EXECUTIONER2_ACTION_FLINCH);
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
int executioner2OnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // we want to give each local player a cooldown on damage they can apply to reactor
  if (!e->PlayerDamager) return 1;
  if (!e->PlayerDamager->IsLocal) return 1;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  Executioner2MobVars_t* executioner2Vars = (Executioner2MobVars_t*)pvars->AdditionalMobVarsPtr;

  // only accept local damage when timer is 0
  int timer = executioner2Vars->LocalPlayerDamageHitInvTimer[e->PlayerDamager->LocalPlayerIndex];
  if (timer == 0) {
    executioner2Vars->LocalPlayerDamageHitInvTimer[e->PlayerDamager->LocalPlayerIndex] = EXECUTIONER2_HIT_INV_TICKS;
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
void executioner2OnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby* executioner2GetNextTarget(Moby* moby)
{
  return mobGetNextTarget(moby, EXECUTIONER2_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int executioner2GetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	VECTOR t;

	// no preferred action
	if (executioner2IsAttacking(moby))
		return -1;

	if (executioner2IsSpawning(pvars))
		return -1;

  if (executioner2IsFlinching(moby))
    return -1;

	if (pvars->MobVars.Action == EXECUTIONER2_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded) {
		return EXECUTIONER2_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > EXECUTIONER2_MAX_WALKABLE_SLOPE) {
    return EXECUTIONER2_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return EXECUTIONER2_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = executioner2GetNextTarget(moby);
	if (target) {
		vector_copy(t, target->Position);
		vector_subtract(t, t, moby->Position);
		float distSqr = vector_sqrmag(t);
		float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;
		float rangedAttackRadiusSqr = EXECUTIONER2_VISION_RANGE * EXECUTIONER2_VISION_RANGE;
    int canRanged = pvars->MobVars.Config.MobAttribute == MOB_ATTRIBUTE_RANGED_ATTACK || pvars->MobVars.Config.MobAttribute == MOB_ATTRIBUTE_BOSS;

    if (1) {
      if (distSqr <= attackRadiusSqr) {
        // near target, swing
        if (executioner2CanAttack(pvars) && distSqr > (EXECUTIONER2_TOO_CLOSE_TO_TARGET_RADIUS*EXECUTIONER2_TOO_CLOSE_TO_TARGET_RADIUS)) {
          if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
          return EXECUTIONER2_ACTION_ATTACK;
        }
        return EXECUTIONER2_ACTION_WALK;
      } else if (canRanged && distSqr <= rangedAttackRadiusSqr && mobCanSeeMoby(moby, target)) {
        // away from target
        // check if facing
        // and fire
        t[2] = 0;
        float theta = acosf(vector_innerproduct(t, moby->M0_03));
        if (fabsf(theta) < (30 * MATH_DEG2RAD))
          return EXECUTIONER2_ACTION_FIRE;
      }
    }

    return EXECUTIONER2_ACTION_WALK;
	}
	
	return EXECUTIONER2_ACTION_IDLE;
}

//--------------------------------------------------------------------------
Moby* executioner2FireShot(Moby* moby, Moby* target)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int jointId = EXECUTIONER2_SUBSKELETON_JOINT_STAFF_END;

  VECTOR from, to={0,0,1,0}, dir, vel, offset;
  MATRIX m;
  mobyGetJointMatrix(moby, jointId, m);
  vector_copy(from, &m[12]);
  vector_copy(vel, &m[0]);
  
  // move shot from forward
  //vector_scale(offset, &m[0], 0.15);
  //vector_add(from, from, offset);

  if (target) {

    // determine if we should shoot directly towards target
    vector_subtract(dir, target->Position, moby->Position);
    vector_normalize(dir, dir);
    VECTOR planarForward;
    vector_projectonplane(planarForward, dir, moby->M2_03);
    float angle = acosf(vector_innerproduct(planarForward, moby->M0_03));
    if (angle < (1*MATH_DEG2RAD)) {
      mobGetTargetCenter(target, to);
      vector_subtract(vel, to, from);
      vector_normalize(vel, vel);
    } else {
      vector_subtract(dir, dir, planarForward);
      vector_projectonplane(vel, vel, moby->M2_03);
      vector_normalize(vel, vel);
      vector_add(vel, vel, dir);
    }
  }

  vector_scale(vel, vel, 0.5);

  // fire shot
  Moby* shotMoby = ((Moby* (*)(float, float, VECTOR, VECTOR, Moby*, int, int, int, int))0x0045d598)(4, pvars->MobVars.Config.Damage, from, vel, moby, 1, 0x222124, -1, 0);
  if (shotMoby) {
    ((void (*)(Moby*, int))0x0045d758)(shotMoby, 2); // shot type
    ((void (*)(Moby*, int))0x0045d788)(shotMoby, TEAM_RED); // shot color
    ((void (*)(Moby*, int))0x0045d7A8)(shotMoby, 1); // hit flag
    ((void (*)(Moby*, int))0x0045d798)(shotMoby, 2*TPS + (int)EXECUTIONER2_VISION_RANGE); // shot life (ticks)
    shotMoby->Bolts = -1; // indicate to gamemode shot can damage player
    shotMoby->PParent = moby;
  }

  // spawn flare
  //((void (*)(float, float, float, Moby*, int, int, int))0x0042c178)(0.75, 0.75, 1.0, moby, 0, 0, jointId);
  
  // play sound
  //mobyPlaySoundByClass(1, 0, moby, MOBY_ID_LANDSTALKER);
}

//--------------------------------------------------------------------------
void executioner2DoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.Target;
  Executioner2MobVars_t* executioner2Vars = (Executioner2MobVars_t*)pvars->AdditionalMobVarsPtr;
	VECTOR t, t2;
  float difficulty = 1;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER2_TURN_RADIANS_PER_SEC : EXECUTIONER2_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? EXECUTIONER2_MOVE_ACCELERATION : EXECUTIONER2_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == EXECUTIONER2_ACTION_FLINCH || pvars->MobVars.LastAction == EXECUTIONER2_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	switch (pvars->MobVars.Action)
	{
		case EXECUTIONER2_ACTION_SPAWN:
		{
      mobTransAnim(moby, EXECUTIONER2_ANIM_SPAWN, 0);
      mobStand(moby);
			break;
		}
		case EXECUTIONER2_ACTION_FLINCH:
		case EXECUTIONER2_ACTION_BIG_FLINCH:
		{
      int animFlinchId = pvars->MobVars.Action == EXECUTIONER2_ACTION_BIG_FLINCH ? EXECUTIONER2_ANIM_BIG_FLINCH : EXECUTIONER2_ANIM_FLINCH;

      mobTransAnim(moby, animFlinchId, 0);
      
			if (pvars->MobVars.Knockback.Ticks > 0) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, EXECUTIONER2_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        mobStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        mobStand(moby);
      }
			break;
		}
		case EXECUTIONER2_ACTION_IDLE:
		{
			mobTransAnim(moby, EXECUTIONER2_ANIM_IDLE, 0);
      mobStand(moby);
			break;
		}
		case EXECUTIONER2_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (target) {
            if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
              pvars->MobVars.Dirty = 1; // new path, sync with other clients
            mobJumpTowards(moby, t);
          } else {
            mobStand(moby);
          }
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded) {
			    mobTransAnim(moby, EXECUTIONER2_ANIM_JUMP, 5);

          // check if we're near last jump pos
          // if so increment StuckJumpCount
          if (pvars->MobVars.MoveVars.IsStuck) {
            if (pvars->MobVars.MoveVars.StuckJumpCount < 255)
              pvars->MobVars.MoveVars.StuckJumpCount++;
          }

          // use delta height between target as base of jump speed
          // with min speed
          float jumpSpeed = pvars->MobVars.MoveVars.QueueJumpSpeed;
          if (jumpSpeed <= 0 && target) {
            jumpSpeed = 8; //clamp(2 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->MobVars.MoveVars.WallSlope) * 2, 3, 15);
          }

          pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
          pvars->MobVars.MoveVars.Grounded = 0;
          pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
        }
				break;
			}
		case EXECUTIONER2_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target) {
        mobTurnTowards(moby, target->Position, turnSpeed);
      }
      break;
    }
    case EXECUTIONER2_ACTION_WALK:
		{
      int walkBackwards = 0;

      if (!isInAirFromFlinching) {
        if (target) {

          float dir = mobGetCurrentWalkAngle(moby);

          // determine next position
          vector_copy(t, target->Position);
          vector_subtract(t, t, moby->Position);
          float dist = vector_length(t);

          // walk backwards if too close
          if (dist < EXECUTIONER2_TOO_CLOSE_TO_TARGET_RADIUS) {
            walkBackwards = 1;

            vector_normalize(t, t);
            vector_scale(t, t, 5);
            vector_subtract(t, target->Position, t);
            mobMoveTowards(moby, t, pvars->MobVars.Config.Speed, turnSpeed, acceleration, dir);
            DPRINTF("%f\n", vector_length(pvars->MobVars.MoveVars.Velocity));
          }
          else if (dist > (pvars->MobVars.Config.AttackRadius - pvars->MobVars.Config.HitRadius)) {

            if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
              pvars->MobVars.Dirty = 1; // new path, sync with other clients
            mobMoveTowards(moby, t, pvars->MobVars.Config.Speed, turnSpeed, acceleration, dir);

          } else {
            mobStand(moby);
          }
        } else {
          mobStand(moby);
        }
      }

			// 
      if (moby->AnimSeqId == EXECUTIONER2_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        executioner2ForceLocalAction(moby, EXECUTIONER2_ACTION_JUMP);
			} else if (mobHasVelocity(pvars)) {
				mobTransAnim(moby, walkBackwards ? EXECUTIONER2_ANIM_WALK_BACKWARD : EXECUTIONER2_ANIM_RUN, 0);
			} else if (moby->AnimSeqId != EXECUTIONER2_ANIM_WALK_BACKWARD || moby->AnimSeqId != EXECUTIONER2_ANIM_RUN || pvars->MobVars.AnimationLooped) {
				mobTransAnim(moby, EXECUTIONER2_ANIM_IDLE, 0);
      }
			break;
		}
    case EXECUTIONER2_ACTION_DIE:
    {
      mobTransAnimLerp(moby, EXECUTIONER2_ANIM_BIG_FLINCH, 5, 0);

      if (moby->AnimSeqId == EXECUTIONER2_ANIM_BIG_FLINCH && moby->AnimSeqT > 3) {
        pvars->MobVars.Destroy = 1;
      }

      mobStand(moby);
      break;
    }
		case EXECUTIONER2_ACTION_ATTACK:
		{
      int attack1AnimId = EXECUTIONER2_ANIM_SWING;
			mobTransAnim(moby, attack1AnimId, 0);

			float speedMult = 0; // (moby->AnimSeqId == attack1AnimId && moby->AnimSeqT < 5) ? (difficulty * 2) : 1;
			int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 4 && moby->AnimSeqT < 10;
			u32 damageFlags = 0x00081801;

      if (!isInAirFromFlinching) {
        if (target) {
          mobMoveTowards(moby, target->Position, speedMult * pvars->MobVars.Config.Speed, turnSpeed, acceleration, 0);
        } else {
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
			
			if (swingAttackReady && damageFlags) {
				executioner2DoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
			}
			break;
		}
    case EXECUTIONER2_ACTION_FIRE:
    {
      int animId = EXECUTIONER2_ANIM_FIRE;

      if (!isInAirFromFlinching) {
        mobStand(moby);
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed*0.5);

          if (moby->AnimSeqId == animId && moby->AnimSeqT >= 31.5 && moby->AnimSeqT < 33 && executioner2Vars->AnimationLoopLastFire != pvars->MobVars.AnimationLooped) {
            executioner2FireShot(moby, target);
            executioner2Vars->AnimationLoopLastFire = pvars->MobVars.AnimationLooped;
          }
        }
      }

      mobTransAnim(moby, animId, 0);
      break;
    }
	}

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void executioner2DoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  mobDoDamage(moby, radius, amount, damageFlags, friendlyFire, 6, 1, 0);
}

//--------------------------------------------------------------------------
void executioner2ForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  Executioner2MobVars_t* executioner2Vars = (Executioner2MobVars_t*)pvars->AdditionalMobVarsPtr;
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
		case EXECUTIONER2_ACTION_SPAWN:
		{
			// enable collision
			moby->CollActive = 0;
			break;
		}
		case EXECUTIONER2_ACTION_DIE:
		{
      // can't undie
      return;
		}
	}

	// to
	switch (action)
	{
		case EXECUTIONER2_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
		case EXECUTIONER2_ACTION_WALK:
		{
			
			break;
		}
		case EXECUTIONER2_ACTION_DIE:
		{
			
			break;
		}
    case EXECUTIONER2_ACTION_FIRE:
    {
      executioner2Vars->AnimationLoopLastFire = -1;
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
      break;
    }
		case EXECUTIONER2_ACTION_ATTACK:
		{
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
			break;
		}
		case EXECUTIONER2_ACTION_FLINCH:
		case EXECUTIONER2_ACTION_BIG_FLINCH:
		{
			pvars->MobVars.FlinchCooldownTicks = EXECUTIONER2_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = EXECUTIONER2_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short executioner2GetArmor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;
  int bangles = pvars->MobVars.Config.Bangles;

  if (t < 0.3)
    return 0x0000;
  else if (t < 0.7)
    return bangles & 0x1f; // remove torso bangle

	return bangles;
}

//--------------------------------------------------------------------------
int executioner2IsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  switch (pvars->MobVars.Action)
  {
    case EXECUTIONER2_ACTION_FIRE: return !pvars->MobVars.AnimationLooped;
    case EXECUTIONER2_ACTION_ATTACK: return !pvars->MobVars.AnimationLooped;
    default: return 0;
  }
}

//--------------------------------------------------------------------------
int executioner2CanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == EXECUTIONER2_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int executioner2ShouldForceStateUpdateOnAction(Moby* moby, int action)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // only send state updates at regular intervals, unless dying
  if (action == EXECUTIONER2_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == EXECUTIONER2_ACTION_FIRE || action == EXECUTIONER2_ACTION_FIRE) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int executioner2IsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == EXECUTIONER2_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int executioner2CanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int executioner2IsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == EXECUTIONER2_ANIM_FLINCH || moby->AnimSeqId == EXECUTIONER2_ANIM_BIG_FLINCH) && !pvars->MobVars.AnimationLooped;
}
