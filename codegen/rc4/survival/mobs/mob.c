#include <tamtypes.h>
#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/random.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/collision.h>
#include <libdl/utils.h>
#include "game.h"
#include "mob.h"
#include "utils.h"
#include "gate.h"
#include "maputils.h"
#include "shared.h"
#include "pathfind.h"
#include "messageid.h"

void mobForceIntoMapBounds(Moby* moby);

#if GATE
void gateSetCollision(int collActive);
#endif

extern int aaa;

#if MOB_ZOMBIE
#include "zombie.c"
#endif

#if MOB_EXECUTIONER
#include "executioner.c"
#endif

#if MOB_LEVIATHAN
#include "leviathan.c"
#endif

#if MOB_TREMOR
#include "tremor.c"
#endif

#if MOB_SWARMER
#include "swarmer.c"
#endif

#if MOB_REAPER
#include "reaper.c"
#endif

#if MOB_REACTOR
#include "trailshot.c"
#include "reactor.c"
#endif

#if DEBUG_MOVE
VECTOR MoveCheckHit;
VECTOR MoveCheckFrom;
VECTOR MoveCheckTo;
VECTOR MoveCheckFinal;
VECTOR MoveCheckUp;
VECTOR MoveCheckDown;
VECTOR MoveNextPos;
VECTOR MoveTargetLineOfSightHit;
#endif

int mobMoveCheckCollideWithOtherMobsRotatingIndex = 0;

//--------------------------------------------------------------------------
int mobAmIOwner(Moby* moby)
{
  if (!mobyIsMob(moby)) return 0;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return gameGetMyClientId() == pvars->MobVars.Owner;
}

//--------------------------------------------------------------------------
int mobIsFrozen(Moby* moby)
{
  if (!moby || !moby->PVar || !MapConfig.State)
    return 0;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return MapConfig.State->Freeze && pvars->MobVars.Config.MobAttribute != MOB_ATTRIBUTE_FREEZE && pvars->MobVars.Health > 0;
}

//--------------------------------------------------------------------------
int mobGetBehavior(Moby* moby)
{
  if (!moby || !moby->PVar || !MapConfig.State)
    return 0;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return pvars->MobVars.Config.Behavior;
}

//--------------------------------------------------------------------------
float mobGetScaleMultiplier(Moby* moby)
{
  if (!moby || !moby->PVar) return 1;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].Scale;
}

//--------------------------------------------------------------------------
void mobSpawnCorn(Moby* moby, int bangle)
{
#if MOB_CORN
	mobyBlowCorn(
			moby
		, bangle
		, 0
		, 3.0
		, 6.0
		, 3.0
		, 6.0
		, -1
		, -1.0
		, -1.0
		, 255
		, 1
		, 0
		, 1
		, 1.0
		, 0x23
		, 3
		, 1.0
		, NULL
		, 0
		);
#endif
}

//--------------------------------------------------------------------------
void mobResetSoundTrigger(Moby* moby)
{
  moby->SoundTrigger = 0;
  moby->SoundDesired = -1;
}

//--------------------------------------------------------------------------
void mobGetTargetCenter(Moby* target, VECTOR out)
{
  if (!target) return;

  switch (target->OClass)
  {
    default: vector_add(out, target->Position, target->M2_03);
  }
}

//--------------------------------------------------------------------------
int mobCanSeeMoby(Moby* moby, Moby* canSeeMoby)
{
	VECTOR t, t2;
  VECTOR up = {0,0,1,0};
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // increment out of sight ticker
  if (canSeeMoby) {
    mobGetTargetCenter(canSeeMoby, t2);

    //if (!pvars->VTable->GetSeeFromPosition || !pvars->VTable->GetSeeFromPosition(moby, t)) {
      vector_scale(up, up, pvars->TargetVars.targetHeight);
      vector_add(t, moby->Position, up);
    //}

    return !CollLine_Fix(t, t2, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL) || CollLine_Fix_GetHitMoby() == canSeeMoby;
  }
  
  return 0;
}

//--------------------------------------------------------------------------
void mobGetKnockbackVelocity(Moby* moby, VECTOR out)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  vector_write(out, 0);
  if (pvars->MobVars.Knockback.Ticks <= 0) return;

  // compute
  float slerpFactor = powf(100, pvars->MobVars.Knockback.Ticks / (float)PLAYER_KNOCKBACK_BASE_TICKS) / 100;
  float power = PLAYER_KNOCKBACK_BASE_POWER * powf(1.1, pvars->MobVars.Knockback.Power) * slerpFactor;
  vector_fromyaw(out, pvars->MobVars.Knockback.Angle / 1000.0);
  out[2] = 1;
  vector_normalize(out, out);
  vector_scale(out, out, power * MATH_DT);

  DPRINTF("knockback %d %d => %f %f\n", pvars->MobVars.Knockback.Ticks, pvars->MobVars.Knockback.Power, power, slerpFactor);
}

//--------------------------------------------------------------------------
void mobAlterTarget(VECTOR out, Moby* moby, VECTOR forward, float amount)
{
	VECTOR up = {0,0,1,0};
	
	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
float mobGetCurrentMoveSpeed(Moby* moby)
{
  VECTOR hVelocity;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  vector_projectonhorizontal(hVelocity, pvars->MobVars.MoveVars.Velocity);
  return (vector_length(hVelocity) / (pvars->MobVars.Config.Speed * MATH_DT));
}

//--------------------------------------------------------------------------
void mobReactToThorns(Moby* moby, float damage, int byPlayerId)
{
  if (byPlayerId < 0) return;
  if (!mobAmIOwner(moby)) return;

	int i;
  VECTOR delta;
  struct MobDamageEventArgs args;
	Player** players = playerGetAll();
  Player* player = players[byPlayerId];
	Guber* guber = guberGetObjectByMoby(moby);

  if (!guber) return;
  if (!player || !player->SkinMoby) return;

  // take percentage of damage dealt
  damage *= ITEM_BLESSING_THORN_DAMAGE_FACTOR;

  // get angle
  vector_subtract(delta, moby->Position, player->PlayerPosition);
  float dist = vector_length(delta);
  float angle = atan2f(delta[1] / dist, delta[0] / dist);
  
  // create event
	GuberEvent * guberEvent = guberEventCreateEvent(guber, MOB_EVENT_DAMAGE, 0, 0);
  if (guberEvent) {
    args.SourceUID = player->Guber.Id.UID;
    args.SourceOClass = 0;
    args.DamageQuarters = damage*4;
    args.DamageFlags = 0;
    args.Knockback.Angle = (short)(angle * 1000);
    args.Knockback.Ticks = 5;
    args.Knockback.Power = rand(4);
    args.Knockback.Force = args.Knockback.Power > 0;
    guberEventWrite(guberEvent, &args, sizeof(struct MobDamageEventArgs));
  }
}

//--------------------------------------------------------------------------
int mobMobyProcessHitFlags(Moby* moby, Moby* hitMoby, float damage, int reactToThorns)
{
  int result = 0;
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  Player* player = guberMobyGetPlayerDamager(hitMoby);
  if (player) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER;
  if (hitMoby == pvars->MobVars.Target) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET;
  if (mobyIsMob(hitMoby)) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_MOB;
  
  if (player && player->timers.postHitInvinc == 0 && playerHasBlessing(player->PlayerId, BLESSING_ITEM_THORNS)) {
    result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER_THORNS;
    if (reactToThorns) mobReactToThorns(moby, damage, player->PlayerId);
  }

  return result;
}

//--------------------------------------------------------------------------
int mobDoDamageTryHit(Moby* moby, Moby* hitMoby, VECTOR jointPosition, int isAoE, float sqrHitRadius, int damageFlags, float amount)
{
  VECTOR mobToHitMoby, mobToJoint, jointToHitMoby;
  VECTOR hitMobyCenter = {0,0,1,0};
  MATRIX playerJointMtx;
  Player* player = guberMobyGetPlayerDamager(hitMoby);
	MobyColDamageIn in;
  float hitMobyCollRadiusSqr = 0;

  if (player && player->PlayerMoby) {
    mobyGetJointMatrix(player->PlayerMoby, 10, playerJointMtx);
    vector_copy(hitMobyCenter, &playerJointMtx[12]);
    hitMobyCenter[2] = clamp(jointPosition[2], playerJointMtx[14] - player->Coll.bot, playerJointMtx[14] + player->Coll.top);
    hitMobyCollRadiusSqr = player->Coll.radiusSqd;
  } else {
    vector_add(hitMobyCenter, hitMobyCenter, hitMoby->Position);
  }

  vector_subtract(mobToHitMoby, hitMobyCenter, moby->Position);
  vector_subtract(mobToJoint, jointPosition, moby->Position);
  vector_subtract(jointToHitMoby, hitMobyCenter, jointPosition);

  // ignore if hit behind
  if (!isAoE && vector_innerproduct(mobToHitMoby, mobToJoint) < 0)
    return 0;

  // clamp within arbitrary vertical limit
  if (!isAoE && fabsf(jointToHitMoby[2]) > 0.25)
    return 0;

  // ignore if past attack radius
  if (vector_innerproduct(mobToHitMoby, jointToHitMoby) > 0 && vector_sqrmag(jointToHitMoby) > (hitMobyCollRadiusSqr + sqrHitRadius))
    return 0;

  vector_write(in.Momentum, 0);
  in.Damager = moby;
  in.DamageFlags = damageFlags;
  in.DamageClass = 0;
  in.DamageStrength = 1;
  in.DamageIndex = moby->OClass;
  in.Flags = 1;
  in.DamageHp = amount;

  mobyCollDamageDirect(hitMoby, &in);
  return 1;
}

//--------------------------------------------------------------------------
int mobDoSweepDamage(Moby* moby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE)
{
	VECTOR p, delta;
  Player** players = playerGetAll();
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int i;
  int result = 0;
  float t = 0;
  float sqrRadius = radius * radius;
  float firstPassSqrRadius = powf(5 + radius, 2);

  // get total distance to travel
  vector_subtract(delta, to, from);
  float len = vector_length(delta);

  for (t = 0; t < len; t += step)
  {
    vector_lerp(p, from, to, t / len);

    // if no friendly fire just check hit on players
    // otherwise check all mobys
    if (!friendlyFire) {
      for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
        Player* player = players[i];
        if (!player || !player->SkinMoby || playerIsDead(player))
          continue;

        vector_subtract(delta, player->PlayerPosition, p);
        if (vector_sqrmag(delta) > firstPassSqrRadius)
          continue;

        if (mobDoDamageTryHit(moby, player->PlayerMoby, p, isAoE, sqrRadius, damageFlags, amount)) {
          result |= mobMobyProcessHitFlags(moby, player->PlayerMoby, amount, reactToThorns);
        }
      }
    } else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_NONE, moby, NULL, 5 + radius) > 0) {
      Moby** hitMobies = CollMobysSphere_Fix_GetHitMobies();
      Moby* hitMoby;
      while ((hitMoby = *hitMobies++)) {
        if (mobDoDamageTryHit(moby, hitMoby, p, isAoE, sqrRadius, damageFlags, amount)) {
          result |= mobMobyProcessHitFlags(moby, hitMoby, amount, reactToThorns);
        }
      }
    }
  }

  return result;
}

//--------------------------------------------------------------------------
int mobDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE)
{
	VECTOR p, delta;
	MATRIX jointMtx;
  Player** players = playerGetAll();
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int i;
  int result = 0;
  float sqrRadius = radius * radius;
  float firstPassSqrRadius = powf(5 + radius, 2);

  // get position of right spike joint
  mobyGetJointMatrix(moby, jointId, jointMtx);
  vector_copy(p, &jointMtx[12]);

  // if no friendly fire just check hit on players
  // otherwise check all mobys
  if (!friendlyFire) {
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      Player* player = players[i];
      if (!player || !playerIsConnected(player) || playerIsDead(player))
        continue;

      vector_subtract(delta, player->PlayerPosition, p);
      if (vector_sqrmag(delta) > firstPassSqrRadius)
        continue;

      if (mobDoDamageTryHit(moby, player->PlayerMoby, p, isAoE, sqrRadius, damageFlags, amount)) {
        result |= mobMobyProcessHitFlags(moby, player->PlayerMoby, amount, reactToThorns);
      }
    }
  } else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_NONE, moby, NULL, 5 + radius) > 0) {
    Moby** hitMobies = CollMobysSphere_Fix_GetHitMobies();
    Moby* hitMoby;
    while ((hitMoby = *hitMobies++)) {
      if (mobDoDamageTryHit(moby, hitMoby, p, isAoE, sqrRadius, damageFlags, amount)) {
        result |= mobMobyProcessHitFlags(moby, hitMoby, amount, reactToThorns);
      }
    }
  }

  return result;
}

//--------------------------------------------------------------------------
void mobSetAction(Moby* moby, int action)
{
	struct MobActionUpdateEventArgs args;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

	// don't set if already action
	if (pvars->MobVars.Action == action)
		return;

  // we send this unreliably now
	// GuberEvent* event = mobCreateEvent(moby, MOB_EVENT_STATE_UPDATE);
	// if (event) {
	// 	args.Action = action;
	// 	args.ActionId = ++pvars->MobVars.ActionId;
  //   args.Random = (char)rand(255);
	// 	guberEventWrite(event, &args, sizeof(struct MobActionUpdateEventArgs));
	// }

  // mark dirty if owner and mob wants state update
  if (mobAmIOwner(moby) && pvars->VTable && pvars->VTable->ShouldForceStateUpdateOnAction && pvars->VTable->ShouldForceStateUpdateOnAction(moby, action))
    pvars->MobVars.Dirty = 1;
  
  pvars->MobVars.LastActionId = pvars->MobVars.ActionId++;
  pvars->MobVars.LastAction = pvars->MobVars.Action;
  
  // pass to mob handler
  if (pvars->VTable && pvars->VTable->ForceLocalAction)
    pvars->VTable->ForceLocalAction(moby, action);

  //pvars->MobVars.DynamicRandom = (char)rand(255);
}

//--------------------------------------------------------------------------
void mobTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	if (moby->AnimSeqId != animId) {
		mobyAnimTransition(moby, animId, lerpFrames, startOff);

		pvars->MobVars.AnimationReset = 1;
		pvars->MobVars.AnimationLooped = 0;
	} else {
		
		// get current t
		// if our stored start is uninitialized, then set current t as start
		float t = moby->AnimSeqT;
		float end = *(u8*)((u32)moby->AnimSeq + 0x10) - (float)(lerpFrames * 0.5);
		if (t >= end && pvars->MobVars.AnimationReset) {
			pvars->MobVars.AnimationLooped++;
			pvars->MobVars.AnimationReset = 0;
		} else if (t < end) {
			pvars->MobVars.AnimationReset = 1;
		}
	}
}

//--------------------------------------------------------------------------
void mobTransAnim(Moby* moby, int animId, float startOff)
{
	mobTransAnimLerp(moby, animId, 10, startOff);
}

//--------------------------------------------------------------------------
int mobIsProjectileComing(Moby* moby)
{
  VECTOR t;
  Moby* m = NULL;
  Moby** mobys = (Moby**)0x0026BDA0;

  while ((m = *mobys++)) {
    if (!mobyIsDestroyed(m)) {
      switch (m->OClass)
      {
        case MOBY_ID_B6_BALL0:
        case MOBY_ID_ARBITER_ROCKET0:
        case MOBY_ID_DUAL_VIPER_SHOT:
        case MOBY_ID_MINE_LAUNCHER_MINE:
        {
          // projectile is within 5 units
          vector_subtract(t, moby->Position, m->Position);
          if (vector_sqrmag(t) < (7*7)) return 1;
          break;
        }
      }
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
int mobHasVelocity(struct MobPVar* pvars)
{
	VECTOR t;
	vector_projectonhorizontal(t, pvars->MobVars.MoveVars.Velocity);
	return vector_sqrmag(t) >= 0.0001;
}

//--------------------------------------------------------------------------
void mobStand(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

  // remove horizontal velocity
  vector_projectonvertical(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
}

//--------------------------------------------------------------------------
void mobResetMoveStep(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  vector_copy(pvars->MobVars.MoveVars.NextPosition, moby->Position);
  pvars->MobVars.MoveVars.MoveSkipTicks = 0;
}

//--------------------------------------------------------------------------
int mobMoveCheck(Moby* moby, VECTOR outputPos, VECTOR from, VECTOR to)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  VECTOR delta, horizontalDelta, reflectedDelta;
  VECTOR hitTo, hitFrom;
  VECTOR hitToEx, hitNormal, hitToExBack;
  VECTOR up = {0,0,0,0};
  float collRadius = pvars->MobVars.Config.CollRadius;
  if (!pvars)
    return 0;

  // get if we should check for collisions with all colliders
  // we have to alternate because when many mobs are in one space
  // collision checks against all of them become extremely expensive
  int collFlag = (mobMoveCheckCollideWithOtherMobsRotatingIndex == pvars->MobVars.Order) ? COLLISION_FLAG_IGNORE_NONE : COLLISION_FLAG_IGNORE_DYNAMIC;

  // if we're stuck, ignore other mobs and just try and get unstuck
  if (pvars->MobVars.MoveVars.IsStuck)
    collFlag = COLLISION_FLAG_IGNORE_DYNAMIC;

  // move up by collradius
  up[2] = collRadius; // 0.5;

  // offset hit scan to center of mob
  //vector_add(hitFrom, from, up);
  //vector_add(hitTo, to, up);
  
  // get horizontal delta between to and from
  vector_subtract(delta, to, from);
  vector_projectonhorizontal(horizontalDelta, delta);

  // offset hit scan to center of mob
  vector_add(hitFrom, from, up);
  vector_add(hitTo, hitFrom, horizontalDelta);
  
  vector_normalize(horizontalDelta, horizontalDelta);

  // move to further out to factor in the radius of the mob
  vector_normalize(hitToEx, delta);
  vector_scale(hitToExBack, hitToEx, collRadius);
  vector_scale(hitToEx, hitToEx, collRadius * (1 + 0.25 * pvars->MobVars.MoveVars.StuckCounter));
  vector_add(hitTo, hitTo, hitToEx);
  vector_subtract(hitFrom, hitFrom, hitToExBack);

#if DEBUG_MOVE
    vector_copy(MoveCheckFrom, hitFrom);
    vector_copy(MoveCheckTo, hitTo);
#endif

  // check if we hit something
  if (CollLine_Fix(hitFrom, hitTo, collFlag, moby, NULL)) {

    vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());

    // compute wall slope
    float slope = getSignedSlope(horizontalDelta, hitNormal);
    pvars->MobVars.MoveVars.WallSlope = maxf(pvars->MobVars.MoveVars.WallSlope, slope);
    pvars->MobVars.MoveVars.HitWall = 1;

    // check if we hit another mob
    Moby* hitMoby = pvars->MobVars.MoveVars.HitWallMoby = CollLine_Fix_GetHitMoby();
    if (hitMoby && mobyIsMob(hitMoby)) {
      pvars->MobVars.MoveVars.HitWall = 0;
      pvars->MobVars.MoveVars.HitWallMoby = NULL;
    }

#if DEBUG_MOVE
    vector_copy(MoveCheckHit, CollLine_Fix_GetHitPosition());
#endif

    // if the hit point is before to
    // then we want to snap back before to
    //vector_subtract(hitTo, CollLine_Fix_GetHitPosition(), up);
    //vector_subtract(hitTo, hitTo, hitToEx);
    
    // stop if hit steep wall
    if (pvars->MobVars.MoveVars.WallSlope > (60 * MATH_DEG2RAD)) {
      //vector_projectonhorizontal(hitToEx, hitToEx);
      //vector_subtract(outputPos, to, hitToEx);
      DPRINTF("movecheck hit steep slope %f\n", pvars->MobVars.MoveVars.WallSlope * MATH_RAD2DEG);
      //return 2;
    }

    // get tangent to surface
    VECTOR hitTangent, hitBitangent, hitDir;
    VECTOR hitRight;
    vector_outerproduct(hitRight, horizontalDelta, up);
    vector_outerproduct(hitTangent, hitNormal, hitRight);
    vector_outerproduct(hitBitangent, hitNormal, hitTangent);

    vector_lerp(hitDir, hitTangent, hitBitangent, fabsf(2 * slope) / MATH_PI);
    vector_normalize(hitDir, hitDir);

    float hitBitangentDotDelta = signf(vector_innerproduct(hitDir, delta));
    // float velToSurfaceTangentAngle = acosf(fabsf(hitBitangentDotDelta));
    // if (velToSurfaceTangentAngle < (60 * MATH_DEG2RAD)) {
    //   pvars->MobVars.MoveVars.HitWall = 0;
    // }

    vector_scale(hitDir, hitDir, hitBitangentDotDelta * vector_length(delta));
    vector_add(outputPos, from, hitDir);

    //vector_projectonhorizontal(hitToEx, hitToEx);
    //vector_reflect(reflectedDelta, hitToEx, hitNormal);
    //if (reflectedDelta[2] > delta[2])
    //  reflectedDelta[2] = delta[2];

    //vector_add(outputPos, to, reflectedDelta);

#if DEBUG_MOVE
    vector_copy(MoveCheckFinal, outputPos);
#endif
    return 1;
  }

  vector_copy(outputPos, to);
  return 0;
}

//--------------------------------------------------------------------------
void mobMove(Moby* moby)
{
  VECTOR targetVelocity;
  VECTOR normalizedVelocity;
  VECTOR nextPos;
  VECTOR temp;
  VECTOR groundCheckFrom, groundCheckTo;
  VECTOR up = {0,0,1,0};
  int isMovingDown = 0;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isOwner = mobAmIOwner(moby);
  int moveStep = pvars->MobVars.MoveVars.LastMoveStep;

  u8 stuckCheckTicks = decTimerU8(&pvars->MobVars.MoveVars.StuckCheckTicks);
  u8 ungroundedTicks = decTimerU8(&pvars->MobVars.MoveVars.UngroundedTicks);
  u8 moveSkipTicks = decTimerU8(&pvars->MobVars.MoveVars.MoveSkipTicks);
  u8 slowTicks = decTimerU8(&pvars->MobVars.SlowTicks);

#if DEBUG_MOVE
  if (pvars->MobVars.Target) {
    VECTOR from, to, delta;
    vector_subtract(delta, pvars->MobVars.Target->Position, moby->Position);
    vector_add(from, moby->Position, up);
    vector_add(to, pvars->MobVars.Target->Position, up);
    if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
      vector_copy(MoveTargetLineOfSightHit, CollLine_Fix_GetHitPosition());
    } else {
      vector_copy(MoveTargetLineOfSightHit, to);
    }
  }
#endif

  if (moveSkipTicks == 0) {

    // reset move step
    moveStep = pvars->MobVars.MoveVars.MoveStep;
    int rotatingDt = fabsf((mobMoveCheckCollideWithOtherMobsRotatingIndex - pvars->MobVars.Order) % MAX_MOBS_ALIVE) < 15;
    if (!isOwner || !rotatingDt)
      moveStep = MOB_MOVE_SKIP_TICKS_LOWPRIORITY;

#if GATE
    gateSetCollision(0);
#endif

    // move next position to last position
    vector_copy(moby->Position, pvars->MobVars.MoveVars.NextPosition);
    vector_copy(pvars->MobVars.MoveVars.LastPosition, pvars->MobVars.MoveVars.NextPosition);

    // reset state
    pvars->MobVars.MoveVars.Grounded = 0;
    pvars->MobVars.MoveVars.WallSlope = 0;
    pvars->MobVars.MoveVars.HitWall = 0;
    pvars->MobVars.MoveVars.HitWallMoby = NULL;

#if DEBUG_MOVE
    vector_write(MoveCheckHit, 0);
    vector_write(MoveCheckFrom, 0);
    vector_write(MoveCheckTo, 0);
    vector_write(MoveCheckFinal, 0);
    vector_write(MoveCheckUp, 0);
    vector_write(MoveCheckDown, 0);
    vector_write(MoveNextPos, 0);
#endif

    if (1)
    {
      // add additive velocity
      vector_add(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.AddVelocity);

      // compute simulated velocity by multiplying velocity by number of ticks to simulate
      vector_scale(targetVelocity, pvars->MobVars.MoveVars.Velocity, (float)moveStep);

      // slow speed in short freeze
      if (slowTicks > 0) {
        vector_scale(targetVelocity, targetVelocity, MOB_SHORT_FREEZE_SPEED_FACTOR);
      }

      // get horizontal normalized velocity
      vector_normalize(normalizedVelocity, targetVelocity);
      normalizedVelocity[2] = 0;

      // compute next position
      vector_add(nextPos, moby->Position, targetVelocity);

      // move physics check twice to prevent clipping walls
      if (mobMoveCheck(moby, nextPos, moby->Position, nextPos) == 1) {
        if (mobMoveCheck(moby, nextPos, moby->Position, nextPos)) {
          //vector_copy(nextPos, moby->Position); // don't move
          //pvars->MobVars.MoveVars.IsStuck = 1;
        }
      }

      // check ground or ceiling
      isMovingDown = targetVelocity[2] <= 0.0001;
      if (isMovingDown) {
        vector_copy(groundCheckFrom, nextPos);
        groundCheckFrom[2] = maxf(moby->Position[2], nextPos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
        vector_copy(groundCheckTo, nextPos);
        groundCheckTo[2] -= 0.5;
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
          // mark grounded this frame
          pvars->MobVars.MoveVars.Grounded = 1;

          // check if we've hit death barrier
          if (isOwner) {
            int hitId = CollLine_Fix_GetHitCollisionId() & 0x0F;
            if (hitId == 0x4 || hitId == 0xb || hitId == 0x0d) {
              pvars->MobVars.Respawn = 1;
            }
          }

          // force position to above ground
          vector_copy(nextPos, CollLine_Fix_GetHitPosition());
          nextPos[2] += 0.01;

#if DEBUG_MOVE
          vector_copy(MoveCheckDown, CollLine_Fix_GetHitPosition());
#endif

          // remove vertical velocity from velocity
          vector_projectonhorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
        }
      } else {
        vector_copy(groundCheckFrom, nextPos);
        groundCheckFrom[2] = moby->Position[2];
        vector_copy(groundCheckTo, nextPos);
        groundCheckTo[2] += 3;
        //groundCheckTo[2] += ZOMBIE_BASE_STEP_HEIGHT;
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
          // force position to below ceiling
          //vector_copy(nextPos, CollLine_Fix_GetHitPosition());
          //nextPos[2] -= 0.01;

#if DEBUG_MOVE
          vector_copy(MoveCheckUp, CollLine_Fix_GetHitPosition());
#endif

          vector_copy(nextPos, CollLine_Fix_GetHitPosition());
          nextPos[2] = maxf(moby->Position[2], groundCheckTo[2] - 3);

          //vector_copy(nextPos, moby->Position);

          // remove vertical velocity from velocity
          //vectorProjectOnHorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
        }
      }

#if DEBUG_MOVE
      vector_copy(MoveNextPos, nextPos);
#endif

      // set position
      vector_copy(pvars->MobVars.MoveVars.NextPosition, nextPos);
    }

    // add gravity to velocity with clamp on downwards speed
    pvars->MobVars.MoveVars.Velocity[2] -= GRAVITY_MAGNITUDE * MATH_DT * (float)moveStep;
    if (pvars->MobVars.MoveVars.Velocity[2] < -10 * MATH_DT)
      pvars->MobVars.MoveVars.Velocity[2] = -10 * MATH_DT;

    // check if stuck by seeing if the sum horizontal delta position over the last second
    // is less than 1 in magnitude
    if (!stuckCheckTicks) {
      pvars->MobVars.MoveVars.StuckCheckTicks = 60;
      pvars->MobVars.MoveVars.IsStuck = /* pvars->MobVars.MoveVars.HitWall && */ vector_length(pvars->MobVars.MoveVars.SumPositionDelta) < (pvars->MobVars.MoveVars.SumSpeedOver * 0.25);
      //pvars->MobVars.MoveVars.IsStuck = 0;
      if (!pvars->MobVars.MoveVars.IsStuck) {
        pvars->MobVars.MoveVars.StuckJumpCount = 0;
        pvars->MobVars.MoveVars.StuckCounter = 0;
      } else {
        pvars->MobVars.MoveVars.StuckCounter++;
      }

      // reset counters
      vector_write(pvars->MobVars.MoveVars.SumPositionDelta, 0);
      pvars->MobVars.MoveVars.SumSpeedOver = 0;
    }

    // add horizontal delta position to sum
    vector_subtract(temp, pvars->MobVars.MoveVars.NextPosition, pvars->MobVars.MoveVars.LastPosition);
    vector_projectonhorizontal(temp, temp);
    vector_add(pvars->MobVars.MoveVars.SumPositionDelta, pvars->MobVars.MoveVars.SumPositionDelta, temp);
    vector_projectonhorizontal(temp, pvars->MobVars.MoveVars.Velocity);
    pvars->MobVars.MoveVars.SumSpeedOver += vector_length(temp) * moveStep;

    vector_write(pvars->MobVars.MoveVars.AddVelocity, 0);
    pvars->MobVars.MoveVars.MoveSkipTicks = moveStep;
    pvars->MobVars.MoveVars.LastMoveStep = moveStep;
  } else {
    
    float t = clamp(1 - (pvars->MobVars.MoveVars.MoveSkipTicks / (float)moveStep), 0, 1);
    vector_lerp(moby->Position, pvars->MobVars.MoveVars.LastPosition, pvars->MobVars.MoveVars.NextPosition, t);

  }

  // tell mob we want to jump
  // but check that we're moving towards the next node/target first before jumping
  VECTOR targetPos, mobyToTargetDelta;
  pathGetTargetPos(targetPos, moby);
  vector_subtract(mobyToTargetDelta, targetPos, moby->Position);
  if (vector_innerproduct(pvars->MobVars.MoveVars.Velocity, mobyToTargetDelta) > 0.5 && pathShouldJump(moby)) {
    pvars->MobVars.MoveVars.QueueJumpSpeed = pathGetJumpSpeed(moby);
  }

  mobForceIntoMapBounds(moby);
}

//--------------------------------------------------------------------------
float mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed)
{
  VECTOR delta;
  
  if (!moby || !moby->PVar)
    return 0;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  //float turnSpeed = pvars->MobVars.MoveVars.Grounded ? ZOMBIE_TURN_RADIANS_PER_SEC : ZOMBIE_TURN_AIR_RADIANS_PER_SEC;
  float radians = turnSpeed * pvars->MobVars.Config.Speed * MATH_DT;

  vector_subtract(delta, towards, moby->Position);
  float targetYaw = atan2f(delta[1], delta[0]);
  float yawDelta = clampAngle(targetYaw - moby->Rotation[2]);

  moby->Rotation[2] = clampAngle(moby->Rotation[2] + clamp(yawDelta, -radians, radians));

  return clampAngle(moby->Rotation[2] - targetYaw);
}

//--------------------------------------------------------------------------
float mobTurnTowardsPredictive(Moby* moby, Moby* target, float turnSpeed, float predictFactor)
{
  VECTOR pos;
  
  if (!moby || !moby->PVar)
    return 0;

  // if target is player, use their velocity to predict their future position
  Player* player = guberMobyGetPlayerDamager(target);
  if (player) {
    vector_scale(pos, player->Velocity, predictFactor);
    vector_add(pos, pos, player->PlayerPosition);
  } else {
    vector_copy(pos, target->Position);
  }

  return mobTurnTowards(moby, pos, turnSpeed);
}

//--------------------------------------------------------------------------
float mobGetCurrentWalkAngle(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.Target;
  float dir = 0;

  // if we have a target
  // we're near the target
  // and we're walking towards the target
  // walk at an angle
  if (target && vector_sqrdistance(target->Position, moby->Position) < (20*20) && vector_sqrdistance(pvars->MobVars.MoveVars.LastTargetPos, target->Position) < 1) {
    dir = ((u8)pvars->MobVars.DynamicRandom % 3) - 1;
  }

  //DPRINTF("td:%f wd:%f dir:%d=>%f\n", vector_sqrdistance(target->Position, moby->Position), vector_sqrdistance(target->Position, pvars->MobVars.MoveVars.LastTargetPos), (u8)pvars->MobVars.DynamicRandom, dir);
  return dir;
}

//--------------------------------------------------------------------------
void mobGetVelocityToTargetWithDirection(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float yaw, float speed, float acceleration)
{
  VECTOR targetVelocity;
  VECTOR hVelocity;
  VECTOR fromToTarget;
  VECTOR next, nextToTarget;
  VECTOR temp;
  VECTOR targetPosition;
  float targetSpeed = speed * MATH_DT;
  float targetRadius = PLAYER_COLL_RADIUS;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

  if (pathUseTargetMoby(moby)) {
    vector_copy(targetPosition, pvars->MobVars.Target->Position);
  } else {
    vector_copy(targetPosition, pvars->MobVars.TargetPosition);
  }

  Moby* target = pvars->MobVars.Target;
  if (target) {
    targetRadius = (target->BSphere[3] / 1024) * 0.5;
  }

  // target velocity from rotation
  vector_fromyaw(targetVelocity, yaw);

  // acclerate velocity towards target velocity
  //vector_normalize(targetVelocity, targetVelocity);
  vector_scale(targetVelocity, targetVelocity, targetSpeed);
  vector_subtract(temp, targetVelocity, velocity);
  vector_projectonhorizontal(temp, temp);
  vector_scale(temp, temp, acceleration * MATH_DT);
  vector_add(velocity, velocity, temp);

  // stop when at target
  if (targetSpeed > 0) {
    vector_subtract(fromToTarget, targetPosition, from);
    vector_add(next, from, velocity);
    vector_subtract(nextToTarget, targetPosition, next);
    vector_projectonhorizontal(nextToTarget, nextToTarget);
    float distNextToTarget = vector_length(nextToTarget);
    
    float min = pvars->MobVars.Config.CollRadius + targetRadius;
    float max = min + targetRadius; //(pvars->MobVars.Config.AttackRadius + PLAYER_COLL_RADIUS) + (targetSpeed * 0.2);

    // if too close to target, stop
    if (max > min && distNextToTarget < max && distNextToTarget > min) {
      float amt = (distNextToTarget - min) / (max - min);
      //vector_normalize(velocity, velocity);
      vector_projectonhorizontal(hVelocity, velocity);
      vector_scale(hVelocity, hVelocity, sqrtf(maxf(amt, 0)));
      vector_projectonvertical(velocity, velocity);
      vector_add(velocity, velocity, hVelocity);
      return;
    } else if (distNextToTarget < min) {
      vector_projectonvertical(velocity, velocity);
    }
  }

  if (targetSpeed <= 0) {
    vector_projectonvertical(velocity, velocity);
    return;
  }
}

//--------------------------------------------------------------------------
void mobGetVelocityToTarget(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
  mobGetVelocityToTargetWithDirection(moby, velocity, from, to, moby->Rotation[2], speed, acceleration);
}

//--------------------------------------------------------------------------
void mobGetVelocityToTargetSimple(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
  VECTOR targetVelocity;
  VECTOR fromToTarget;
  VECTOR next, nextToTarget;
  VECTOR temp;
  float targetSpeed = speed * MATH_DT;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

  float collRadius = pvars->MobVars.Config.CollRadius + 0.5;
  
  // target velocity from rotation
  vector_subtract(targetVelocity, to, from);
  vector_normalize(targetVelocity, targetVelocity);

  // acclerate velocity towards target velocity
  vector_scale(targetVelocity, targetVelocity, targetSpeed);
  vector_subtract(temp, targetVelocity, velocity);
  vector_projectonhorizontal(temp, temp);
  vector_scale(temp, temp, acceleration * MATH_DT);
  vector_add(velocity, velocity, temp);
  
  if (targetSpeed <= 0) {
    vector_projectonvertical(velocity, velocity);
    return;
  }
}

//--------------------------------------------------------------------------
Moby* mobGetNextTarget(Moby* moby, float keepCurrentTargetFactor)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Player ** players = playerGetAll();
	int i;
	VECTOR delta;
	Moby * currentTarget = pvars->MobVars.Target;
	Player * closestPlayer = NULL;
	float closestPlayerDist = 100000;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
		Player* p = players[i];
		if (p && p->SkinMoby && !playerIsDead(p) && p->Health > 0 && p->SkinMoby->Opacity >= 0x80) {
			vector_subtract(delta, p->PlayerPosition, moby->Position);
			float dist = vector_length(delta);

			if (dist < 300) {
        Moby* pTargetMoby = playerGetTargetMoby(p);

        // don't target players that are in jump pad state
        // unless we're already targeting them
        if (p->PlayerState == PLAYER_STATE_MOON_JUMP && pTargetMoby != currentTarget)
          continue;

				// favor existing target
				if (pTargetMoby == currentTarget)
					dist *= (1.0 / keepCurrentTargetFactor);
				
				// pick closest target
				if (dist < closestPlayerDist) {

          // confirm we can walk to target
          if (pathHasRouteToTarget(moby, pTargetMoby)) {
            closestPlayer = p;
            closestPlayerDist = dist;
          }
				}
			}
		}
	}

	if (closestPlayer) {
    return playerGetTargetMoby(closestPlayer);
  }

	return NULL;
}

//--------------------------------------------------------------------------
void mobMoveTowards(Moby* moby, VECTOR targetPosition, float speed, float turnSpeed, float acceleration, float curveNearTargetDir)
{
  VECTOR t, t2;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  vector_subtract(t, targetPosition, moby->Position);
  float dist = vector_length(t);
  if (dist < 10.0) {
    mobAlterTarget(t2, moby, t, clamp(dist, 0, 10) * 0.3 * curveNearTargetDir);
    vector_add(t, t, t2);
  }
  //vector_scale(t, t, 1 / dist);
  vector_add(t, moby->Position, t);

  float deltaYaw = 0;
  if (pvars->MobVars.MoveVars.IsStuck) {
    deltaYaw = mobTurnTowards(moby, targetPosition, turnSpeed);
  } else {
    deltaYaw = mobTurnTowards(moby, t, turnSpeed);
  }

  // DYAW > 0DEG
  float yawLerpT = fabsf(deltaYaw) / MATH_PI;
  if (yawLerpT > 0) {
    float v = (yawLerpT+0.0) * 1.0;
    //v = powf(v, 1 / speed);
    v = 1 - powf(clamp(v, 0, 1), 2);
    //printf("v:%f speed:%f\n", v, speed);
    speed *= v;
  }
  
  mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, speed, acceleration);
}

//--------------------------------------------------------------------------
void mobJumpTowards(Moby* moby, VECTOR targetPosition)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float speedCurve = lerpf(0, 1, clamp(pvars->MobVars.CurrentActionForTicks / (float)TPS, 0, 1));
  if (pvars->MobVars.MoveVars.Velocity[2] < 0) {
    mobMoveTowards(moby, targetPosition, MOB_JUMP_MOVE_SPEED*0.5, 45 * MATH_DEG2RAD, 10, 0);
  } else {
    mobMoveTowards(moby, targetPosition, MOB_JUMP_MOVE_SPEED, 180 * MATH_DEG2RAD, 10 * speedCurve, 0);
  }
}

//--------------------------------------------------------------------------
int mobHitWallShouldJump(Moby* moby, float maxSlope)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // must be grounded
  // must hit wall steeper than max slope
  // ignore case where we hit the target (since we want to reach the target)
  return pvars->MobVars.MoveVars.Grounded
      && pvars->MobVars.MoveVars.HitWall
      && pvars->MobVars.MoveVars.WallSlope > maxSlope
      && (!pvars->MobVars.MoveVars.HitWallMoby || pvars->MobVars.MoveVars.HitWallMoby != pvars->MobVars.Target)
      ;
}

//--------------------------------------------------------------------------
void mobPostDrawQuad(Moby* moby, float scale, u32 color, int jointId)
{
	struct QuadDef quad;
	float size = scale * mobGetScaleMultiplier(moby);
	MATRIX m2;
	VECTOR pTL = {0,size,size,1};
	VECTOR pTR = {0,-size,size,1};
	VECTOR pBL = {0,size,-size,1};
	VECTOR pBR = {0,-size,-size,1};
  VECTOR offset = {0,0,size};
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	if (!pvars)
		return;

	//u32 color = MobLODColors[pvars->MobVars.Config.MobType] | (moby->Opacity << 24);

	// set draw args
	matrix_unit(m2);

	// init
  gfxResetQuad(&quad);

	// color of each corner?
	vector_copy(quad.VertexPositions[0], pTL);
	vector_copy(quad.VertexPositions[1], pTR);
	vector_copy(quad.VertexPositions[2], pBL);
	vector_copy(quad.VertexPositions[3], pBR);
	quad.VertexColors[0] = quad.VertexColors[1] = quad.VertexColors[2] = quad.VertexColors[3] = color;
  quad.VertexUVs[0] = (struct UV){0,0};
  quad.VertexUVs[1] = (struct UV){1,0};
  quad.VertexUVs[2] = (struct UV){0,1};
  quad.VertexUVs[3] = (struct UV){1,1};
	quad.Clamp = 0x0000000100000001;
	quad.Tex0 = gfxGetFrameTex(MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteTexId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera* camera = cameraGetGameCamera(0);
	if (!camera)
		return;
	
	// set world matrix by joint
	//mobyGetJointMatrix(moby, jointId, m2);

  //memcpy(m2, moby->M0_03, sizeof(VECTOR) * 3);
  memcpy(m2, camera->uMtx, sizeof(VECTOR) * 3);
  vector_add(&m2[12], moby->Position, offset);

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 1);
}

#if DEBUG

//--------------------------------------------------------------------------
void mobPostDrawDebug(Moby* moby)
{
  MATRIX jointMtx;
#if PRINT_JOINTS
  int i = 0;
  char buf[32];
  int animJointCount = 0;

  // get anim joint count
  void* pclass = moby->PClass;
  if (pclass) {
    animJointCount = **(u32**)((u32)pclass + 0x1C);
  }

  for (i = 0; i < animJointCount; ++i) {
    snprintf(buf, sizeof(buf), "%d", i);
    mobyGetJointMatrix(moby, i, jointMtx);
    draw3DMarker(&jointMtx[12], 0.5, 0x80FFFFFF, buf);
  }
#endif

#if DEBUG_MOVE
  draw3DMarker(MoveCheckHit, 1, 0x80FF00FF, "-");
  draw3DMarker(MoveCheckFrom, 1, 0x80FFFFFF, "a");
  draw3DMarker(MoveCheckTo, 1, 0x80FFFFFF, "b");
  draw3DMarker(MoveCheckFinal, 1, 0x80FF00FF, "+");
  draw3DMarker(MoveCheckUp, 1, 0x8000FFFF, "^");
  draw3DMarker(MoveCheckDown, 1, 0x8000FFFF, "v");
  draw3DMarker(MoveNextPos, 1, 0x80FFFFFF, "o");
  draw3DMarker(MoveTargetLineOfSightHit, 1, 0x80FFFFFF, "x");
#endif
}
#endif

//--------------------------------------------------------------------------
void mobPreUpdate(Moby* moby)
{
#if DEBUG
	gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&mobPostDrawDebug, moby);
#endif
}

//--------------------------------------------------------------------------
void mobOnSpawned(Moby* moby)
{

}

//--------------------------------------------------------------------------
void mobOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  // update pathfinding state
  pathSetPath(moby, e->PathStartNodeIdx, e->PathEndNodeIdx, e->PathCurrentEdgeIdx, e->PathHasReachedStart, e->PathHasReachedEnd);
}

//--------------------------------------------------------------------------
int mobCreate(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config)
{
	struct MobSpawnEventArgs args;
  struct MobSpawnParams* spawnParams = &MapConfig.DefaultSpawnParams[spawnParamsIdx];
  int extraDataSize = 0;

  // get extra data size
  if (spawnParams->MobVTable->GetExtraDataSize) {
    extraDataSize = spawnParams->MobVTable->GetExtraDataSize(spawnParamsIdx);
  }

	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + extraDataSize, &guberEvent, NULL);
	if (guberEvent)
	{
    if (MapConfig.PopulateSpawnArgsFunc) {
      MapConfig.PopulateSpawnArgsFunc(&args, config, spawnParamsIdx, spawnFromUID == -1, spawnFlags);
    }

		u8 random = (u8)rand(100);
    position[2] += 1; // spawn slightly above point

    // give mob change to make last minute changes
    if (spawnParams->MobVTable->OnSpawning) {
      spawnParams->MobVTable->OnSpawning(spawnParamsIdx, position, &yaw, &spawnFromUID, &spawnFlags, &random, &args);
    }
    
    // pack position to 16bits per axis
    // we assume position is always between 0 and 1024 on each axis (moby grid limits)
    u16 pos16[3] = {
      (u16)(position[0] * 64),
      (u16)(position[1] * 64),
      (u16)(position[2] * 64)
    };
		guberEventWrite(guberEvent, pos16, 2*3);
		//guberEventWrite(guberEvent, position, 12);
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
void mobInit(void)
{
  MapConfig.OnMobSpawnedFunc = &mobOnSpawned;
}

//--------------------------------------------------------------------------
void mobTick(void)
{
  if (MapConfig.State && MapConfig.State->MobStats.TotalAlive > 0)
    mobMoveCheckCollideWithOtherMobsRotatingIndex = (mobMoveCheckCollideWithOtherMobsRotatingIndex + 1) % MapConfig.State->MobStats.TotalAlive;
  else
    mobMoveCheckCollideWithOtherMobsRotatingIndex = (mobMoveCheckCollideWithOtherMobsRotatingIndex + 1) % MAX_MOBS_ALIVE;
}
