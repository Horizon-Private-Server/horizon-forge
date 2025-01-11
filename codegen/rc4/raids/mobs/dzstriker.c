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

void dzstrikerPreUpdate(Moby* moby);
void dzstrikerPostUpdate(Moby* moby);
void dzstrikerPostDraw(Moby* moby);
void dzstrikerMove(Moby* moby);
void dzstrikerOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void dzstrikerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void dzstrikerOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int dzstrikerOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void dzstrikerOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
enum DZStrikerAction dzstrikerGetPreferredAttack(Moby* moby);
int dzstrikerGetPreferredAction(Moby* moby, int * delayTicks);
void dzstrikerDoAction(Moby* moby);
void dzstrikerDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void dzstrikerForceLocalAction(Moby* moby, int action);
short dzstrikerGetArmor(Moby* moby);
int dzstrikerIsAttacking(Moby* moby);
int dzstrikerCanNonOwnerTransitionToAction(Moby* moby, int action);
int dzstrikerShouldForceStateUpdateOnAction(Moby* moby, int action);
int dzstrikerGetSeeFromPosition(Moby* moby, VECTOR out);

int dzstrikerIsSpawning(struct MobPVar* pvars);
int dzstrikerIsRoaming(struct MobPVar* pvars);
int dzstrikerIsIdling(struct MobPVar* pvars);
int dzstrikerCanAttack(struct MobPVar* pvars);
int dzstrikerIsFlinching(Moby* moby);
int dzstrikerIsDying(Moby* moby);
int dzstrikerShouldStrafe(Moby* moby);
int dzstrikerIsSniper(Moby* moby);

int dzstrikerTorsoOnSpawn(Moby* mobMoby, Moby* torsoMoby);

struct MobVTable DZStrikerVTable = {
  .PreUpdate = &dzstrikerPreUpdate,
  .PostUpdate = &dzstrikerPostUpdate,
  .PostDraw = &dzstrikerPostDraw,
  .Move = &dzstrikerMove,
  .OnSpawn = &dzstrikerOnSpawn,
  .OnDestroy = &dzstrikerOnDestroy,
  .OnDamage = &dzstrikerOnDamage,
  .OnLocalDamage = &dzstrikerOnLocalDamage,
  .OnStateUpdate = &dzstrikerOnStateUpdate,
  .GetNextTarget = &mobGetNextTarget,
  .GetPreferredAction = &dzstrikerGetPreferredAction,
  .ForceLocalAction = &dzstrikerForceLocalAction,
  .DoAction = &dzstrikerDoAction,
  .DoDamage = &dzstrikerDoDamage,
  .GetArmor = &dzstrikerGetArmor,
  .IsAttacking = &dzstrikerIsAttacking,
  .CanNonOwnerTransitionToAction = &dzstrikerCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &dzstrikerShouldForceStateUpdateOnAction,
  .GetSeeFromPosition = &dzstrikerGetSeeFromPosition,
};

//--------------------------------------------------------------------------
DZStrikerMobVars_t* dzstrikerGetExtraVars(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return (DZStrikerMobVars_t*)pvars->AdditionalMobVarsPtr;
}

//--------------------------------------------------------------------------
int dzstrikerCreate(struct MobCreateArgs* args)
{
  VECTOR position = {0,0,1,0};
	struct MobSpawnEventArgs spawnArgs;

  struct MobSpawnParams* spawnParams = &MapConfig.MobSpawnParams[args->SpawnParamsIdx];
  
	// create guber object
	GuberEvent * guberEvent = 0;
	int guberUid = guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + sizeof(DZStrikerMobVars_t), &guberEvent, NULL);
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
void dzstrikerPreUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);

  // decrement path target pos ticker
  decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
  decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void dzstrikerPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float scale = pvars->MobVars.Config.Scale;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* torsoMoby = dzstrikerVars->TorsoMoby;
  if (!torsoMoby) return;

  // apply omega mod FX to color
  if (pvars->MobVars.AcidEffectActiveTicks > 0) {
    torsoMoby->PrimaryColor = moby->PrimaryColor = colorLerp(DZSTRIKER_PRIMARY_COLOR, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks > 0) {
    torsoMoby->PrimaryColor = moby->PrimaryColor = colorLerp(DZSTRIKER_PRIMARY_COLOR, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    torsoMoby->PrimaryColor = moby->PrimaryColor = DZSTRIKER_PRIMARY_COLOR;
  }
  
  // apply draw dist
  torsoMoby->DrawDist = moby->DrawDist;

  // adjust animSpeed by speed and by animation
  float baseSpeed = 0.5;
	float animSpeed = baseSpeed * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
  if (pvars->MobVars.FreezeEffectActiveTicks > 0) animSpeed *= MOB_POSTFX_FREEZE_FACTOR;
  if (moby->AnimSeqId == DZSTRIKER_LEGS_ANIM_JUMP) {
    animSpeed = baseSpeed * (1 - powf(moby->AnimSeqT / 35, 2));
    if (pvars->MobVars.MoveVars.Grounded) {
      animSpeed = baseSpeed;
    }
  } else if (dzstrikerIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded) {
    animSpeed = baseSpeed * 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
  } else if (dzstrikerIsDying(moby)) {
    animSpeed = baseSpeed;
  }

	if ((moby->DrawDist == 0 && !dzstrikerIsAttacking(moby) && !dzstrikerIsSpawning(pvars) && !dzstrikerIsDying(moby) && !dzstrikerIsFlinching(moby))) {
		moby->AnimSpeed = 0;
    torsoMoby->AnimSpeed = 0;
	} else {
		moby->AnimSpeed = animSpeed;
		torsoMoby->AnimSpeed = animSpeed;
	}

  // attach torso to legs hip
  MATRIX mtxHips;
  mobyGetJointMatrix(moby, DZSTRIKER_LEGS_SUBSKELETON_JOINT_HIPS, mtxHips);
  vector_copy(torsoMoby->Position, &mtxHips[12]);
  mobyUpdateTransform(torsoMoby);
  matrix_rotate_y(mtxHips, mtxHips, torsoMoby->Rotation[1] + dzstrikerVars->TorsoRotation[1]);
  matrix_rotate_x(mtxHips, mtxHips, torsoMoby->Rotation[0] + dzstrikerVars->TorsoRotation[0]);
  matrix_rotate_z(mtxHips, mtxHips, torsoMoby->Rotation[2] + dzstrikerVars->TorsoRotation[2]);
  memcpy(torsoMoby->M0_03, mtxHips, sizeof(VECTOR)*3);
  memset(dzstrikerVars->TorsoRotation, 0, sizeof(dzstrikerVars->TorsoRotation));
}

//--------------------------------------------------------------------------
void dzstrikerPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  u32 color = DZSTRIKER_LOD_COLOR | (moby->Opacity << 24);
  mobPostDrawQuad(moby, 127, color, DZSTRIKER_LEGS_SUBSKELETON_JOINT_HIPS);
}

//--------------------------------------------------------------------------
void dzstrikerMove(Moby* moby)
{
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* torsoMoby = dzstrikerVars->TorsoMoby;
  if (!torsoMoby) return;

  // prevent colliding with torso on move
  torsoMoby->CollActive = -1;
  mobMove(moby);
  torsoMoby->CollActive = 0;
}

//--------------------------------------------------------------------------
void dzstrikerOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);

  // set scale
  float scale = pvars->MobVars.Config.Scale;
  moby->Scale = 0.256339 * scale;

  // colors by mob type
	moby->GlowRGBA = DZSTRIKER_GLOW_COLOR;
	moby->PrimaryColor = DZSTRIKER_PRIMARY_COLOR;

  // set hold position to start position
  vector_copy(dzstrikerVars->HoldPosition, moby->Position);

  // targeting
  MATRIX m;
  mobyUpdateTransform(moby);
  mobyGetJointMatrix(moby, DZSTRIKER_LEGS_SUBSKELETON_JOINT_HIPS, m);
	pvars->TargetVars.targetHeight = (m[14] - moby->Position[2]);
  
#if MOB_DAMAGETYPES
  pvars->TargetVars.damageTypes = MOB_DAMAGETYPES;
#endif

  // default move step
  pvars->MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  if (pvars->MobVars.Behavior == DZSTRIKER_BEHAVIOR_FLY)
    pvars->MobVars.MoveVars.PreferredHeight = 6;
  vector_copy(pvars->MobVars.MoveVars.TargetPosition, moby->Position);
  
  // create torso
  Moby* torsoMoby = dzstrikerVars->TorsoMoby = mobySpawn(MOBY_ID_DZ_STRIKER_TORSO_RED, sizeof(DZStrikerTorsoPVar_t));
  if (!torsoMoby) {
    pvars->MobVars.Destroy = 2;
    return;
  }

  dzstrikerTorsoOnSpawn(moby, torsoMoby);
}

//--------------------------------------------------------------------------
void dzstrikerOnDestroy(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar)
    return;
    
	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = DZSTRIKER_PRIMARY_COLOR;
  
  // spawn corn
  mobBlowCorn(moby);

  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  if (dzstrikerVars->TorsoMoby && !mobyIsDestroyed(dzstrikerVars->TorsoMoby)) {
    mobBlowCorn(dzstrikerVars->TorsoMoby);
    mobyDestroy(dzstrikerVars->TorsoMoby);
    dzstrikerVars->TorsoMoby = NULL;
  }
}

//--------------------------------------------------------------------------
void dzstrikerOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->MobVars.Health - damage;

	int canFlinch = pvars->MobVars.Action != DZSTRIKER_ACTION_FLINCH 
            && pvars->MobVars.Action != DZSTRIKER_ACTION_BIG_FLINCH
            && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    dzstrikerForceLocalAction(moby, DZSTRIKER_ACTION_DIE);
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
    float powerFactor = DZSTRIKER_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
    float probability = clamp((damageRatio * DZSTRIKER_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
    probability = 2;
    powerFactor = 2;
#endif

    if (canFlinch) {
      if (e->Knockback.Force) {
        mobSetAction(moby, DZSTRIKER_ACTION_BIG_FLINCH);
      } else if (isShock) {
        mobSetAction(moby, DZSTRIKER_ACTION_FLINCH);
      } else if (randRange(0, 1) < probability) {
        if (randRange(0, 1) < powerFactor) {
          mobSetAction(moby, DZSTRIKER_ACTION_BIG_FLINCH);
        } else {
          mobSetAction(moby, DZSTRIKER_ACTION_FLINCH);
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
int dzstrikerOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void dzstrikerOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
int dzstrikerIsTargetOutOfRange(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* target = pvars->MobVars.MoveVars.Target;
  if (!target) return 0;

  // check if target is within range
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  dt[2] = 0;
  float distSqr = vector_sqrmag(dt);
  float rangedAttackRadiusSqr = pvars->MobVars.Config.RangedMaxDistanceToTarget*pvars->MobVars.Config.RangedMaxDistanceToTarget;
  return distSqr > rangedAttackRadiusSqr;
}

//--------------------------------------------------------------------------
enum DZStrikerAction dzstrikerGetPreferredAttack(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* target = pvars->MobVars.MoveVars.Target;
  int behavior = pvars->MobVars.Behavior;
  if (!target)
    return -1;

  if (pvars->MobVars.TimeTargetOutOfSightTicks > TPS)
    return -1;

  // check if target is within range
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float distSqr = vector_sqrmag(dt);
  float attackRadiusSqr = pvars->MobVars.Config.AttackRadius * pvars->MobVars.Config.AttackRadius;
  if (distSqr > attackRadiusSqr) {

    // check if in range
    if (!dzstrikerIsTargetOutOfRange(moby)) {
      dt[2] = 0;
      float theta = acosf(vector_innerproduct(dt, dzstrikerVars->TorsoMoby->M0_03));
      if (pvars->MobVars.Action != DZSTRIKER_ACTION_AIM && fabsf(theta) < (30 * MATH_DEG2RAD))
        return DZSTRIKER_ACTION_AIM;
      else if (pvars->MobVars.Action == DZSTRIKER_ACTION_AIM && fabsf(theta) < (30 * MATH_DEG2RAD))
        return DZSTRIKER_ACTION_FIRE;
    }

    return -1;
  }

  // default to swing
	return DZSTRIKER_ACTION_ATTACK;
}

//--------------------------------------------------------------------------
int dzstrikerGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;
	VECTOR t;

	// no preferred action
	if (dzstrikerIsAttacking(moby))
		return -1;

	if (dzstrikerIsSpawning(pvars))
		return -1;

  if (dzstrikerIsFlinching(moby))
    return -1;

  if (pvars->MobVars.Action == DZSTRIKER_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.IsStuck)
    return -1;

	if (pvars->MobVars.Action == DZSTRIKER_ACTION_JUMP && pvars->MobVars.MoveVars.JumpedThisAction && pvars->MobVars.MoveVars.Grounded) {
		return DZSTRIKER_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > DZSTRIKER_MAX_WALKABLE_SLOPE) {
    return DZSTRIKER_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
    return DZSTRIKER_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = mobGetNextTarget(moby);
	if (target) {
    if (dzstrikerCanAttack(pvars)) {
      int preferredAttack = dzstrikerGetPreferredAttack(moby);
      if (preferredAttack >= 0) {
        if (delayTicks) *delayTicks = pvars->MobVars.Config.ReactionTickCount;
        return preferredAttack;
      }
    }

    if (dzstrikerShouldStrafe(moby))
      return DZSTRIKER_ACTION_STRAFE;

    if (!dzstrikerIsTargetOutOfRange(moby) && pvars->MobVars.TimeTargetOutOfSightTicks < TPS)
      return DZSTRIKER_ACTION_LOOK_AT_TARGET;

    return DZSTRIKER_ACTION_WALK;
	}

  // if roaming, then we want to periodically stop or reroute
  if (dzstrikerIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->MobVars.MoveVars.TargetPosition, moby->Position);
    t[2] += minf(pvars->MobVars.MoveVars.PreferredHeight, pvars->MobVars.MoveVars.CurrentHeightLimit);
    if (!isFlying) t[2] = 0;
    float distSqr = vector_sqrmag(t);
    float radius = 1 + pvars->MobVars.Config.CollRadius; //pvars->MobVars.Config.AttackRadius;

    // idle if near target or randomly
    if (distSqr < (radius*radius) || rand(10007) == 0) {
      return DZSTRIKER_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (dzstrikerIsIdling(pvars) && pvars->MobVars.CurrentActionForTicks < TPS*3) {
    return DZSTRIKER_ACTION_IDLE;
  }
	
	return DZSTRIKER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void dzstrikerRenderPath(Moby* moby)
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
void dzstrikerTransAnim(Moby* moby, int animId, int torsoAnimId, float startOff)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* torsoMoby = dzstrikerVars->TorsoMoby;

  if (isFlying) animId = DZSTRIKER_LEGS_ANIM_FLYING;

  mobTransAnim(moby, animId, 0);
  if (torsoMoby) {
    mobTransAnimLerp(torsoMoby, torsoAnimId, 10, startOff, &dzstrikerVars->TorsoAnimationReset, &dzstrikerVars->TorsoAnimationLooped);
  }
}

//--------------------------------------------------------------------------
void dzstrikerAnimUpdate(Moby* moby)
{
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* torsoMoby = dzstrikerVars->TorsoMoby;

  mobUpdateAnim(moby);
  if (torsoMoby) {
    mobTransAnimLerp(torsoMoby, torsoMoby->AnimSeqId, 0, 0, &dzstrikerVars->TorsoAnimationReset, &dzstrikerVars->TorsoAnimationLooped);
  }
}

//--------------------------------------------------------------------------
void dzstrikerStand(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;
  float targetSpeed = pvars->MobVars.Config.Speed * MATH_DT;
  Moby* target = pvars->MobVars.MoveVars.Target;

  mobStand(moby);
  if (isFlying && target) {
    float height = minf(pvars->MobVars.MoveVars.PreferredHeight, pvars->MobVars.MoveVars.CurrentHeightLimit);
    float dy = (height - pvars->MobVars.MoveVars.DistFromGround);
    pvars->MobVars.MoveVars.Velocity[2] = dy * MATH_DT * lerpf(1, 0, 1 - powf(MATH_E, -1 * fabsf(dy) * MATH_DT));
  }
}

//--------------------------------------------------------------------------
Moby* dzstrikerFireShot(Moby* moby, Moby* target)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  int isSniper = dzstrikerIsSniper(moby);
  int jointId = isSniper ? DZSTRIKER_TORSO_SUBSKELETON_JOINT_SNIPER_CHAMBER : DZSTRIKER_TORSO_SUBSKELETON_JOINT_GUN_CHAMBER;

  VECTOR from, to={0,0,1,0}, dir, vel, offset;
  MATRIX m;
  mobyGetJointMatrix(dzstrikerVars->TorsoMoby, jointId, m);
  vector_copy(from, &m[12]);
  vector_copy(vel, &m[0]);
  
  // move shot from forward
  vector_scale(offset, &m[0], 0.15);
  vector_add(from, from, offset);

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

  float shotTrail = 1;
  float damage = pvars->MobVars.Config.Damage;
  if (isSniper) {
    shotTrail = 3;
    vector_scale(vel, vel, 2); // speed
  }

  // fire shot
  Moby* shotMoby = ((Moby* (*)(float, float, VECTOR, VECTOR, Moby*, int, int, int, int))0x0045d598)(shotTrail, damage, from, vel, dzstrikerVars->TorsoMoby, 1, 0x222124, -1, 0);
  if (shotMoby) {
    ((void (*)(Moby*, int))0x0045d758)(shotMoby, 0); // shot type
    ((void (*)(Moby*, int))0x0045d788)(shotMoby, TEAM_RED); // shot color
    ((void (*)(Moby*, int))0x0045d7A8)(shotMoby, 1); // hit flag
    ((void (*)(Moby*, int))0x0045d798)(shotMoby, 2*TPS + (int)(pvars->MobVars.Config.VisionRange)); // shot life (ticks)
    shotMoby->PParent = moby;
  }

  // spawn flare
  ((void (*)(float, float, float, Moby*, int, int, int))0x0042c178)(0.75, 0.75, 1.0, dzstrikerVars->TorsoMoby, 0, 0, jointId);
  
  // play sound
  mobyPlaySoundByClass(1, 0, dzstrikerVars->TorsoMoby, MOBY_ID_LANDSTALKER);
}

//--------------------------------------------------------------------------
int dzstrikerDoActionMove(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
  VECTOR targetPosition;
	VECTOR t;
  int behavior = pvars->MobVars.Behavior;
  int strafe = pvars->MobVars.Action == DZSTRIKER_ACTION_STRAFE;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? DZSTRIKER_TURN_RADIANS_PER_SEC : DZSTRIKER_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? DZSTRIKER_MOVE_ACCELERATION : DZSTRIKER_MOVE_AIR_ACCELERATION;

  pvars->MobVars.MoveVars.ForceUseTargetPosition = strafe;
  if (!target) {
    strafe = 0;
    vector_copy(targetPosition, moby->Position);
  } else {
    vector_copy(targetPosition, target->Position);
  }

  // if walking (target out of sight), then return to hold position  
  if (!strafe && behavior == DZSTRIKER_BEHAVIOR_HOLD_POSITION) {
    vector_copy(targetPosition, dzstrikerVars->HoldPosition);
    pvars->MobVars.MoveVars.ForceUseTargetPosition = 1;
  }

  // 
  VECTOR dt;
  vector_subtract(dt, targetPosition, moby->Position);
  float dir = (pvars->MobVars.DynamicRandom % 3) - 1;
  float strafeDir = ((pvars->MobVars.DynamicRandom + (pvars->MobVars.CurrentActionForTicks/1000)) % 2) ? 1 : -1;
  if (strafe) {
    VECTOR strafeVec, strafeFwd;
    vector_outerproduct(strafeVec, dt, moby->M2_03);
    vector_normalize(strafeVec, strafeVec);
    vector_scale(strafeVec, strafeVec, 5 * strafeDir);
    vector_add(pvars->MobVars.MoveVars.TargetPosition, moby->Position, strafeVec);
  } else {
    vector_copy(pvars->MobVars.MoveVars.TargetPosition, targetPosition);
  }

  if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby)) {
    pvars->MobVars.Dirty = 1; // new path, sync with other clients
  }
  
  if (strafe) {
    if (dir) {
      VECTOR dt2;
      vector_subtract(dt2, t, moby->Position);
      float yaw2 = atan2f(dt2[1], dt2[0]);
      float yaw = atan2f(dt[1], dt[0]);
      mobMoveTowards(moby, t, speed, 1000, acceleration, 0);
      dzstrikerVars->TorsoRotation[2] = clampAngle(yaw - yaw2);
    } else {
      dzstrikerStand(moby);
      mobTurnTowards(moby, targetPosition, turnSpeed);
    }
    return DZSTRIKER_LEGS_ANIM_RUN_FORWARD;
  } else {
    mobMoveTowards(moby, t, speed, turnSpeed, acceleration, dir);
    return DZSTRIKER_LEGS_ANIM_RUN_FORWARD;
  }
}

//--------------------------------------------------------------------------
void dzstrikerDoAction(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* torsoMoby = dzstrikerVars->TorsoMoby;
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t;
  int isSniper = dzstrikerIsSniper(moby);
  float difficulty = 1;
  float speed = pvars->MobVars.Config.Speed;
  float turnSpeed = pvars->MobVars.MoveVars.Grounded ? DZSTRIKER_TURN_RADIANS_PER_SEC : DZSTRIKER_TURN_AIR_RADIANS_PER_SEC;
  float acceleration = pvars->MobVars.MoveVars.Grounded ? DZSTRIKER_MOVE_ACCELERATION : DZSTRIKER_MOVE_AIR_ACCELERATION;
  int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded 
                      && (pvars->MobVars.LastAction == DZSTRIKER_ACTION_FLINCH || pvars->MobVars.LastAction == DZSTRIKER_ACTION_BIG_FLINCH);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

  if (!torsoMoby) return;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&dzstrikerRenderPath, moby);
#endif

  dzstrikerAnimUpdate(moby);
	switch (pvars->MobVars.Action)
	{
		case DZSTRIKER_ACTION_SPAWN:
		{
      dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, DZSTRIKER_TORSO_ANIM_IDLE_SPIN_GUN, 0);
      dzstrikerStand(moby);
			break;
		}
		case DZSTRIKER_ACTION_FLINCH:
		case DZSTRIKER_ACTION_BIG_FLINCH:
		{
      if (pvars->MobVars.Action == DZSTRIKER_ACTION_BIG_FLINCH) {
        dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_FLINCH_FALL_DOWN, DZSTRIKER_TORSO_ANIM_FLINCH_3_BAD_ANIM, 0);
      } else {
        dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_FLINCH, DZSTRIKER_TORSO_ANIM_FLINCH, 0);
      }
      
			if (pvars->MobVars.Knockback.Ticks > 0 && pvars->MobVars.Action == DZSTRIKER_ACTION_BIG_FLINCH) {
        mobGetKnockbackVelocity(moby, t);
				vector_scale(t, t, DZSTRIKER_KNOCKBACK_MULTIPLIER);
				vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
			} else if (pvars->MobVars.MoveVars.Grounded) {
        dzstrikerStand(moby);
      } else if (pvars->MobVars.CurrentActionForTicks > (1*TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter) {
        dzstrikerStand(moby);
      }
			break;
		}
		case DZSTRIKER_ACTION_IDLE:
		{
      int torsoIdleAnimId = DZSTRIKER_TORSO_ANIM_IDLE;
      if (rand(1000) == 1) torsoIdleAnimId = DZSTRIKER_TORSO_ANIM_IDLE_SPIN_GUN;
      else if (rand(1000) == 1) torsoIdleAnimId = DZSTRIKER_TORSO_ANIM_IDLE_LOOK_LEFT_RIGHT;

      dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, torsoIdleAnimId, 0);
      dzstrikerStand(moby);
			break;
		}
		case DZSTRIKER_ACTION_JUMP:
			{
        // move
        if (!isInAirFromFlinching) {
          if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
            pvars->MobVars.Dirty = 1; // new path, sync with other clients
          mobJumpTowards(moby, t);
        }

        // handle jumping
        if (pvars->MobVars.MoveVars.Grounded && !pvars->MobVars.MoveVars.JumpedThisAction) {
          dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_JUMP, DZSTRIKER_TORSO_ANIM_JUMP, 0);

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
		case DZSTRIKER_ACTION_LOOK_AT_TARGET:
    {
      dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, DZSTRIKER_TORSO_ANIM_IDLE, 0);
      dzstrikerStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      break;
    }
    case DZSTRIKER_ACTION_ROAM:
    {
      if (!isInAirFromFlinching) {
        if (pathGetTargetPos(path, t, moby, &pvars->MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->MobVars.Dirty = 1; // new path, sync with other clients
        mobMoveTowards(moby, t, speed * 0.5, turnSpeed, acceleration, 0);
      }

			// 
      if (moby->AnimSeqId == DZSTRIKER_LEGS_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        dzstrikerForceLocalAction(moby, DZSTRIKER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
        dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_WALK_FORWARD, DZSTRIKER_TORSO_ANIM_IDLE, 0);
      } else if (moby->AnimSeqId != DZSTRIKER_LEGS_ANIM_WALK_FORWARD || pvars->MobVars.AnimationLooped) {
        dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, DZSTRIKER_TORSO_ANIM_IDLE, 0);
      }
      break;
    }
    case DZSTRIKER_ACTION_WALK:
    case DZSTRIKER_ACTION_STRAFE:
		{
      int walkAnimId = DZSTRIKER_LEGS_ANIM_RUN_FORWARD;
      if (!isInAirFromFlinching) {
        walkAnimId = dzstrikerDoActionMove(moby);
      }

			// 
      if (moby->AnimSeqId == DZSTRIKER_LEGS_ANIM_JUMP && !pvars->MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->MobVars.MoveVars.QueueJumpSpeed) {
        dzstrikerForceLocalAction(moby, DZSTRIKER_ACTION_JUMP);
      } else if (mobHasVelocity(pvars)) {
        dzstrikerTransAnim(moby, walkAnimId, DZSTRIKER_TORSO_ANIM_IDLE, 0);
      } else if (moby->AnimSeqId != walkAnimId || pvars->MobVars.AnimationLooped) {
        dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, DZSTRIKER_TORSO_ANIM_IDLE, 0);
      }
			break;
		}
    case DZSTRIKER_ACTION_DIE:
    {
      mobStand(moby);
      break;
    }
		case DZSTRIKER_ACTION_ATTACK:
		{
      int legsAnimId = moby->AnimSeqId;
      int torsoAnimId = DZSTRIKER_TORSO_ANIM_MELEE_SWING;

			int swingAttackReady = torsoMoby->AnimSeqId == torsoAnimId && torsoMoby->AnimSeqT >= 3 && torsoMoby->AnimSeqT < 6;
			u32 damageFlags = 0x00081801;

      if (!isInAirFromFlinching) {
        if (target) {
          mobMoveTowards(moby, target->Position, pvars->MobVars.Config.Speed, turnSpeed, acceleration, 0);
          legsAnimId = DZSTRIKER_LEGS_ANIM_RUN_FORWARD;
        } else {
          // stand
          dzstrikerStand(moby);
        }
      }
      
			if (swingAttackReady && damageFlags) {
				dzstrikerDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
			}
      
      if (mobHasVelocity(pvars)) {
        legsAnimId = DZSTRIKER_LEGS_ANIM_RUN_FORWARD;
      } else {
        legsAnimId = DZSTRIKER_LEGS_ANIM_IDLE;
      }

      dzstrikerTransAnim(moby, legsAnimId, torsoAnimId, 0);
			break;
		}
    case DZSTRIKER_ACTION_AIM:
    {
      int torsoAnimId = isSniper ? DZSTRIKER_TORSO_ANIM_AIM_SNIPER : DZSTRIKER_TORSO_ANIM_AIM_GUN;

      dzstrikerStand(moby);
      if (!isInAirFromFlinching) {
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed);
        }
      }

      dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, torsoAnimId, 0);
      break;
    }
    case DZSTRIKER_ACTION_FIRE:
    {
      int torsoAnimId = isSniper ? DZSTRIKER_TORSO_ANIM_FIRE_SNIPER : DZSTRIKER_TORSO_ANIM_FIRE_GUN_LOOP;

      dzstrikerStand(moby);
      if (!isInAirFromFlinching) {
        if (target) {
          mobTurnTowards(moby, target->Position, turnSpeed);

          float animT = dzstrikerVars->TorsoMoby->AnimSeqT;
          if (dzstrikerVars->TorsoMoby->AnimSeqId == torsoAnimId) {
            if (animT < dzstrikerVars->LastShotAtAnimT)
              dzstrikerVars->LastShotAtAnimT = -1;

            if (animT >= 1 && dzstrikerVars->LastShotAtAnimT < 0) {
              dzstrikerFireShot(moby, target);
              dzstrikerVars->LastShotAtAnimT = animT;
            }
          }
        }
      }

      dzstrikerTransAnim(moby, DZSTRIKER_LEGS_ANIM_IDLE, torsoAnimId, 0);
      break;
    }
  }

  pvars->MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void dzstrikerDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  Moby* torsoMoby = dzstrikerVars->TorsoMoby;
  if (!torsoMoby) return;

  MATRIX mRightElbow, mGunChamber;
  VECTOR from, to;
  mobyGetJointMatrix(torsoMoby, DZSTRIKER_TORSO_SUBSKELETON_JOINT_RIGHT_ELBOW, mRightElbow);
  mobyGetJointMatrix(torsoMoby, DZSTRIKER_TORSO_SUBSKELETON_JOINT_GUN_CHAMBER, mGunChamber);
  vector_copy(from, &mRightElbow[12]);
  vector_copy(to, &mGunChamber[12]);
  mobDoSweepDamage(moby, torsoMoby, from, to, 0.25, radius, amount, damageFlags, friendlyFire, 1, 0);
}

//--------------------------------------------------------------------------
void dzstrikerForceLocalAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  if (!dzstrikerVars->TorsoMoby) return;
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
		case DZSTRIKER_ACTION_SPAWN:
		{
			// enable collision
			moby->CollActive = 0;
			break;
		}
    case DZSTRIKER_ACTION_DIE:
    {
      // can't undie
      return;
    }
    case DZSTRIKER_ACTION_JUMP:
    {
      pvars->MobVars.MoveVars.JumpedThisAction = 0;
      break;
    }
	}

	// to
	switch (action)
	{
		case DZSTRIKER_ACTION_SPAWN:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
    case DZSTRIKER_ACTION_ROAM:
    {
      if (pvars->MobVars.Behavior == DZSTRIKER_BEHAVIOR_HOLD_POSITION) {

        // hold target position
        vector_copy(pvars->MobVars.MoveVars.TargetPosition, dzstrikerVars->HoldPosition);
      } else {

        // if we're in a spawner
        // then let it determine where we roam
        if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
          spawnerOnChildGetRandomRoamTarget(moby->PParent, moby, pvars->MobVars.MoveVars.TargetPosition);
        }
      }
      break;
    }
		case DZSTRIKER_ACTION_WALK:
		{
			
			break;
		}
		case DZSTRIKER_ACTION_DIE:
		{
      pvars->MobVars.Destroy = 1;
			break;
		}
		case DZSTRIKER_ACTION_AIM:
    {
      // face legs in direction of torso
      dzstrikerVars->LastShotAtAnimT = -1;
      moby->Rotation[2] = atan2f(dzstrikerVars->TorsoMoby->M0_03[1], dzstrikerVars->TorsoMoby->M0_03[0]);
      break;
    }
		case DZSTRIKER_ACTION_FIRE:
		{
			pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
			break;
		}
		case DZSTRIKER_ACTION_ATTACK:
		{
      pvars->MobVars.AttackCooldownTicks = TPS*2;
			break;
		}
		case DZSTRIKER_ACTION_FLINCH:
		case DZSTRIKER_ACTION_BIG_FLINCH:
		{
			pvars->MobVars.FlinchCooldownTicks = DZSTRIKER_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = DZSTRIKER_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short dzstrikerGetArmor(Moby* moby)
{
	return 0;
}

//--------------------------------------------------------------------------
int dzstrikerIsAttacking(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);
  switch (pvars->MobVars.Action)
  {
    //case DZSTRIKER_ACTION_AIM:
    case DZSTRIKER_ACTION_FIRE: return pvars->MobVars.CurrentActionForTicks < TPS;
    case DZSTRIKER_ACTION_ATTACK: return !dzstrikerVars->TorsoAnimationLooped;
    default: return 0;
  }
}

//--------------------------------------------------------------------------
int dzstrikerCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == DZSTRIKER_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int dzstrikerShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == DZSTRIKER_ACTION_DIE) return 1;
  if (pvars->MobVars.Action == DZSTRIKER_ACTION_ROAM || action == DZSTRIKER_ACTION_ROAM) return 1;
  if (pvars->MobVars.Action == DZSTRIKER_ACTION_AIM || action == DZSTRIKER_ACTION_AIM) return 1;
  if (pvars->MobVars.Action == DZSTRIKER_ACTION_FIRE || action == DZSTRIKER_ACTION_FIRE) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int dzstrikerGetSeeFromPosition(Moby* moby, VECTOR out)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(moby);

  MATRIX m;
  if (!dzstrikerVars->TorsoMoby)
    return 0;

  mobyGetJointMatrix(dzstrikerVars->TorsoMoby, DZSTRIKER_TORSO_SUBSKELETON_JOINT_HEAD, m);
  vector_copy(out, &m[12]);
  return 1;
}

//--------------------------------------------------------------------------
int dzstrikerIsSpawning(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == DZSTRIKER_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int dzstrikerIsRoaming(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == DZSTRIKER_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int dzstrikerIsIdling(struct MobPVar* pvars)
{
	return pvars->MobVars.Action == DZSTRIKER_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int dzstrikerCanAttack(struct MobPVar* pvars)
{
	return pvars->MobVars.AttackCooldownTicks == 0;
}

//--------------------------------------------------------------------------
int dzstrikerIsFlinching(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return (moby->AnimSeqId == DZSTRIKER_LEGS_ANIM_FLINCH || moby->AnimSeqId == DZSTRIKER_LEGS_ANIM_FLINCH_FALL_DOWN) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int dzstrikerIsDying(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return pvars->MobVars.Action == DZSTRIKER_ACTION_DIE;
}

//--------------------------------------------------------------------------
int dzstrikerShouldStrafe(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.MoveVars.Target;
  int behavior = pvars->MobVars.Behavior;

  if (!target) return 0;
  if (pvars->MobVars.MoveVars.PreferredHeight > 0) return 0;

  // get distance to target
  VECTOR dt;
  vector_subtract(dt, target->Position, moby->Position);
  float sqrDistToTarget = vector_sqrmag(dt);
  float maxDistSqr = pvars->MobVars.Config.RangedMaxDistanceToTarget*pvars->MobVars.Config.RangedMaxDistanceToTarget;
  if (sqrDistToTarget > maxDistSqr)
    return 0;

  int strafe = 0;
  if (pvars->MobVars.TimeTargetOutOfSightTicks < 2) {
    strafe = 1;
  }

  // if stuck, return to walk state
  if (pvars->MobVars.MoveVars.IsStuck) {
    strafe = 0;
  }

  return strafe;
}

//--------------------------------------------------------------------------
int dzstrikerIsSniper(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return (pvars->MobVars.Config.Bangles & DZSTRIKER_TORSO_BANGLE_SNIPER) != 0;
}


//--------------------------------------------------------------------------
void dzstrikerTorsoUpdate(Moby* moby)
{
  // pass damage to the main mob moby (legs)
  if (moby->CollDamage >= 0 && moby->PParent) {
    moby->PParent->CollDamage = moby->CollDamage;
    moby->CollDamage = -1;
  }
}

//--------------------------------------------------------------------------
int dzstrikerTorsoOnSpawn(Moby* mobMoby, Moby* torsoMoby)
{
	struct MobPVar* mobPVars = (struct MobPVar*)mobMoby->PVar;
  DZStrikerMobVars_t* dzstrikerVars = dzstrikerGetExtraVars(mobMoby);
  DZStrikerTorsoPVar_t* torsoPVars = (DZStrikerTorsoPVar_t*)torsoMoby->PVar;
  torsoPVars->TargetVarsPtr = &torsoPVars->TargetVars;
  torsoPVars->FlashVarsPtr = &torsoPVars->FlashVars;
  torsoPVars->TargetVars.team = mobPVars->TargetVars.team;
  torsoPVars->TargetVars.targetHeight = 0;
  torsoPVars->TargetVars.hitPoints = mobPVars->MobVars.Health;

  torsoMoby->DrawDist = mobMoby->DrawDist;
  torsoMoby->UpdateDist = mobMoby->UpdateDist;
  torsoMoby->ModeBits |= MOBY_MODE_BIT_NO_POST_UPDATE | MOBY_MODE_BIT_HAS_SPECIAL_VARS;
  torsoMoby->Bangles = mobPVars->MobVars.Config.Bangles;
  torsoMoby->PParent = mobMoby;
  torsoMoby->PUpdate = &dzstrikerTorsoUpdate;
  torsoMoby->Scale = mobMoby->Scale;
  dzstrikerVars->TorsoMoby = torsoMoby;

	// initialize move vars
	mobySetAnimCache(torsoMoby, (void*)0x36f980, 0);
	torsoMoby->ModeBits &= ~MOBY_MODE_BIT_LOCK_ROTATION;
	return 0;
}
