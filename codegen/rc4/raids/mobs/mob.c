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
#include "maputils.h"
#include "shared.h"
#include "gate.h"
#include "pathfind.h"

void mobForceIntoMapBounds(Moby* moby);

#if MOB_LEVIATHAN
#include "leviathan.c"
#endif

#if MOB_STALKERTURRET
#include "stalkerturret.c"
#endif

#if MOB_ZOMBIE
#include "zombie.c"
#endif

#if MOB_EXECUTIONER
#include "executioner.c"
#endif

#if MOB_TREMOR
#include "tremor.c"
#endif

#if MOB_SWARMER
#include "swarmer.c"
#endif

#if MOB_SWAMPER
#include "swamper.c"
#endif

#if MOB_REAPER
#include "reaper.c"
#endif

#if MOB_REACTOR
#include "trailshot.c"
#include "reactor.c"
#endif

#if DEBUGMOVE
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

  //DPRINTF("knockback %d %d => %f %f\n", pvars->MobVars.Knockback.Ticks, pvars->MobVars.Knockback.Power, power, slerpFactor);
}

//--------------------------------------------------------------------------
void mobReactToThorns(Moby* moby, float damage, int byPlayerId)
{
  if (byPlayerId < 0) return;
  if (!mobAmIOwner(moby)) return;

  VECTOR delta;
  struct MobDamageEventArgs args;
	Player** players = playerGetAll();
  Player* player = players[byPlayerId];
	Guber* guber = guberGetObjectByMoby(moby);

  if (!guber) return;
  if (!player || !player->SkinMoby) return;

  // take percentage of damage dealt
  //damage *= ITEM_BLESSING_THORN_DAMAGE_FACTOR;

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
  if (hitMoby == pvars->MobVars.MoveVars.Target) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET;
  if (mobyIsMob(hitMoby)) result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_MOB;
  
  // if (player && player->timers.postHitInvinc == 0 && playerHasBlessing(player->PlayerId, BLESSING_ITEM_THORNS)) {
  //   result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER_THORNS;
  //   if (reactToThorns) mobReactToThorns(moby, damage, player->PlayerId);
  // }

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
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

	// don't set if already action
	if (pvars->MobVars.Action == action)
		return;

  // we send this unreliably now
  // struct MobActionUpdateEventArgs args;
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

  pvars->MobVars.DynamicRandom = (char)rand(255);
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
  vector_scale(hitToEx, hitToEx, collRadius); // * (1 + 0.25 * pvars->MobVars.MoveVars.StuckCounter));
  vector_add(hitTo, hitTo, hitToEx);
  vector_subtract(hitFrom, hitFrom, hitToExBack);

#if DEBUGMOVE
    vector_copy(MoveCheckFrom, hitFrom);
    vector_copy(MoveCheckTo, hitTo);
#endif

  // check if we hit something
  if (CollLine_Fix(hitFrom, hitTo, collFlag, moby, NULL)) {

    vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());

    // compute wall slope
    pvars->MobVars.MoveVars.WallSlope = maxf(pvars->MobVars.MoveVars.WallSlope, getSignedSlope(horizontalDelta, hitNormal));
    pvars->MobVars.MoveVars.HitWall = 1;

    // check if we hit another mob
    Moby* hitMoby = pvars->MobVars.MoveVars.HitWallMoby = CollLine_Fix_GetHitMoby();
    if (hitMoby && mobyIsMob(hitMoby)) {
      pvars->MobVars.MoveVars.HitWall = 0;
      pvars->MobVars.MoveVars.HitWallMoby = NULL;
    }

#if DEBUGMOVE
    vector_copy(MoveCheckHit, CollLine_Fix_GetHitPosition());
#endif

    // if the hit point is before to
    // then we want to snap back before to
    vector_subtract(to, CollLine_Fix_GetHitPosition(), up);
    
    // stop if hit steep wall
    if (pvars->MobVars.MoveVars.WallSlope > (60 * MATH_DEG2RAD)) {
      vector_projectonhorizontal(hitToEx, hitToEx);
      vector_subtract(outputPos, to, hitToEx);
#if DEBUGMOVE
      DPRINTF("movecheck hit steep slope %f\n", pvars->MobVars.MoveVars.WallSlope * MATH_RAD2DEG);
#endif
      return 2;
    }

    //vector_projectonhorizontal(hitToEx, hitToEx);
    vector_reflect(reflectedDelta, hitToEx, hitNormal);
    //if (reflectedDelta[2] > delta[2])
    //  reflectedDelta[2] = delta[2];

    vector_add(outputPos, to, reflectedDelta);

#if DEBUGMOVE
    vector_copy(MoveCheckFinal, outputPos);
#endif
    return 1;
  }

  vector_copy(outputPos, to);
  return 0;
}

//--------------------------------------------------------------------------
int mobHitIdIsBad(int hitId)
{
  hitId &= 0x0f;
  return hitId == 0x4 || hitId == 0xb || hitId == 0x0d;
}

//--------------------------------------------------------------------------
void mobMove(Moby* moby)
{
  VECTOR targetVelocity;
  VECTOR normalizedVelocity;
  VECTOR nextPos, ledgePos;
  VECTOR temp;
  VECTOR groundCheckFrom, groundCheckTo;
  int isMovingDown = 0;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isOwner = mobAmIOwner(moby);
  int nextPosHasSafeGround = 1;
  int moveStep = pvars->MobVars.MoveVars.LastMoveStep;

  u8 stuckCheckTicks = decTimerU8(&pvars->MobVars.MoveVars.StuckCheckTicks);
  u8 moveSkipTicks = decTimerU8(&pvars->MobVars.MoveVars.MoveSkipTicks);
  u8 slowTicks = decTimerU8(&pvars->MobVars.SlowTicks);
  if (pvars->MobVars.MoveVars.PathHasReachedStart) {
    decTimerU8(&pvars->MobVars.MoveVars.WasStuckTicks);
  }

#if DEBUGMOVE
  VECTOR up = {0,0,1,0};
  if (pvars->MobVars.MoveVars.Target) {
    VECTOR from, to, delta;
    vector_subtract(delta, pvars->MobVars.MoveVars.Target->Position, moby->Position);
    vector_add(from, moby->Position, up);
    vector_add(to, pvars->MobVars.MoveVars.Target->Position, up);
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
    if (!isOwner && !moby->Drawn)
      moveStep += 3;
    
#if GATE
    gateSetCollision(0);
#endif

    // move next position to last position
    vector_copy(moby->Position, pvars->MobVars.MoveVars.NextPosition);
    vector_copy(pvars->MobVars.MoveVars.LastPosition, pvars->MobVars.MoveVars.NextPosition);

    VECTOR lastVelocity;
    vector_copy(lastVelocity, pvars->MobVars.MoveVars.Velocity);

    // reset state
    pvars->MobVars.MoveVars.Grounded = 0;
    pvars->MobVars.MoveVars.WallSlope = 0;
    pvars->MobVars.MoveVars.HitWall = 0;
    pvars->MobVars.MoveVars.HitWallMoby = NULL;

#if DEBUGMOVE
    vector_write(MoveCheckHit, 0);
    vector_write(MoveCheckFrom, 0);
    vector_write(MoveCheckTo, 0);
    vector_write(MoveCheckFinal, 0);
    vector_write(MoveCheckUp, 0);
    vector_write(MoveCheckDown, 0);
    vector_write(MoveNextPos, 0);
#endif

    // add additive velocity
    vector_add(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.AddVelocity);

    // compute simulated velocity by multiplying velocity by number of ticks to simulate
    float freezeFactor = pvars->MobVars.FreezeEffectActiveTicks > 0 ? MOB_POSTFX_FREEZE_FACTOR : 1;
    vector_scale(targetVelocity, pvars->MobVars.MoveVars.Velocity, (float)moveStep * freezeFactor);

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
        pvars->MobVars.MoveVars.IsStuck = 1;
      }
    }
    
    // check ground or ceiling
    isMovingDown = targetVelocity[2] <= 0.0001;

    // check ledge
    vector_fromyaw(ledgePos, moby->Rotation[2]);
    vector_add(ledgePos, moby->Position, ledgePos);
    vector_copy(groundCheckFrom, ledgePos);
    groundCheckFrom[2] = maxf(moby->Position[2], ledgePos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
    vector_copy(groundCheckTo, ledgePos);
    groundCheckTo[2] = gameGetDeathHeight();
    if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
      if (mobHitIdIsBad(CollLine_Fix_GetHitCollisionId())) {
        nextPosHasSafeGround = 0;
      }
    } else {
      // no ground
      nextPosHasSafeGround = 0;
    }

    // check ground
    if (isMovingDown) {
      vector_copy(groundCheckFrom, moby->Position);
      groundCheckFrom[2] = maxf(moby->Position[2], nextPos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
      vector_copy(groundCheckTo, nextPos);
      groundCheckTo[2] -= 0.5;
      if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {

        // mark grounded this frame
        pvars->MobVars.MoveVars.Grounded = 1;

        // check if we've hit death barrier
        if (isOwner && mobHitIdIsBad(CollLine_Fix_GetHitCollisionId())) {
          pvars->MobVars.Respawn = 1;
        }

        // force position to above ground
        vector_copy(nextPos, CollLine_Fix_GetHitPosition());
        nextPos[2] += 0.01;

#if DEBUGMOVE
        vector_copy(MoveCheckDown, CollLine_Fix_GetHitPosition());
#endif

        // remove vertical velocity from velocity
        vector_projectonhorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
      }
    }

    // check ceiling
    if (!isMovingDown) {
      vector_copy(groundCheckFrom, nextPos);
      groundCheckFrom[2] = moby->Position[2];
      vector_copy(groundCheckTo, nextPos);
      groundCheckTo[2] += 3;
      //groundCheckTo[2] += ZOMBIE_BASE_STEP_HEIGHT;
      if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
        // force position to below ceiling
        //vector_copy(nextPos, CollLine_Fix_GetHitPosition());
        //nextPos[2] -= 0.01;

#if DEBUGMOVE
        vector_copy(MoveCheckUp, CollLine_Fix_GetHitPosition());
#endif

        vector_copy(nextPos, CollLine_Fix_GetHitPosition());
        nextPos[2] = maxf(moby->Position[2], groundCheckTo[2] - 3);

        //vector_copy(nextPos, moby->Position);

        // remove vertical velocity from velocity
        //vectorProjectOnHorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
      }
    }

#if DEBUGMOVE
    vector_copy(MoveNextPos, nextPos);
#endif

    // detect ledge
    if (!nextPosHasSafeGround) {
      //vector_projectonvertical(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
      //vector_scale(pvars->MobVars.MoveVars.Velocity, lastVelocity, 0.9);
      //vector_copy(nextPos, pvars->MobVars.MoveVars.LastPosition);
      nextPos[0] = moby->Position[0];
      nextPos[1] = moby->Position[1];
      //DPRINTF("%f\n", nextPos[2] - moby->Position[2]);
    }

    // set position
    vector_copy(pvars->MobVars.MoveVars.NextPosition, nextPos);

    // add gravity to velocity with clamp on downwards speed
    pvars->MobVars.MoveVars.Velocity[2] -= GRAVITY_MAGNITUDE * MATH_DT * (float)moveStep;
    if (pvars->MobVars.MoveVars.Velocity[2] < -10 * MATH_DT)
      pvars->MobVars.MoveVars.Velocity[2] = -10 * MATH_DT;

    // check if stuck by seeing if the sum horizontal delta position over the last second
    // is less than 1 in magnitude
    if (!stuckCheckTicks) {
      pvars->MobVars.MoveVars.StuckCheckTicks = 60;
      pvars->MobVars.MoveVars.IsStuck = /* pvars->MobVars.MoveVars.HitWall && */ vector_length(pvars->MobVars.MoveVars.SumPositionDelta) < (pvars->MobVars.MoveVars.SumSpeedOver * 0.25);
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
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars);
  if (pathGetTargetPos(path, targetPos, moby, &pvars->MobVars.MoveVars) && isOwner)
    pvars->MobVars.Dirty = 1; // new path, sync with clients
  vector_subtract(mobyToTargetDelta, targetPos, moby->Position);
  if (vector_innerproduct(pvars->MobVars.MoveVars.Velocity, mobyToTargetDelta) > 0.5 && pathShouldJump(path, moby, &pvars->MobVars.MoveVars)) {
    pvars->MobVars.MoveVars.QueueJumpSpeed = pathGetJumpSpeed(path, moby, &pvars->MobVars.MoveVars);
  }

  mobForceIntoMapBounds(moby);
}

//--------------------------------------------------------------------------
void mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed)
{
  VECTOR delta;
  
  if (!moby || !moby->PVar)
    return;

  vector_subtract(delta, towards, moby->Position);
  float targetYaw = atan2f(delta[1], delta[0]);
  float yawDelta = clampAngle(targetYaw - moby->Rotation[2]);

  float radians = turnSpeed * MATH_DT;
  moby->Rotation[2] = clampAngle(moby->Rotation[2] + clamp(yawDelta, -radians, radians));
}

//--------------------------------------------------------------------------
void mobTurnTowardsPredictive(Moby* moby, Moby* target, float turnSpeed, float predictFactor)
{
  VECTOR pos;
  
  if (!moby || !moby->PVar)
    return;

  // if target is player, use their velocity to predict their future position
  Player* player = guberMobyGetPlayerDamager(target);
  if (player) {
    vector_scale(pos, player->Velocity, predictFactor);
    vector_add(pos, pos, player->PlayerPosition);
  } else if (mobyIsMob(target)) {
	  struct MobPVar* pvars = (struct MobPVar*)target->PVar;
    vector_scale(pos, pvars->MobVars.MoveVars.Velocity, predictFactor);
    vector_add(pos, pos, target->Position);
  } else {
    vector_copy(pos, target->Position);
  }

  mobTurnTowards(moby, pos, turnSpeed);
}

//--------------------------------------------------------------------------
void mobGetVelocityToTarget(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
  VECTOR targetVelocity;
  VECTOR hVelocity;
  VECTOR fromToTarget;
  VECTOR next, nextToTarget;
  VECTOR temp;
  float targetSpeed = speed * MATH_DT;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

  // target velocity from rotation
  vector_fromyaw(targetVelocity, moby->Rotation[2]);

  // acclerate velocity towards target velocity
  //vector_normalize(targetVelocity, targetVelocity);
  vector_scale(targetVelocity, targetVelocity, targetSpeed);
  vector_subtract(temp, targetVelocity, velocity);
  vector_projectonhorizontal(temp, temp);
  vector_scale(temp, temp, acceleration * MATH_DT);
  vector_add(velocity, velocity, temp);
  
  // stop when at target
  if (pvars->MobVars.MoveVars.Target && targetSpeed > 0) {
    vector_subtract(fromToTarget, pvars->MobVars.MoveVars.Target->Position, from);
    vector_add(next, from, velocity);
    vector_subtract(nextToTarget, pvars->MobVars.MoveVars.Target->Position, next);
    float distNextToTarget = vector_length(nextToTarget);
    
    float min = pvars->MobVars.Config.CollRadius + PLAYER_COLL_RADIUS;
    float max = min + PLAYER_COLL_RADIUS; //(pvars->MobVars.Config.AttackRadius + PLAYER_COLL_RADIUS) + (targetSpeed * 0.2);

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
void mobGetVelocityToTargetSimple(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
  VECTOR targetVelocity;
  VECTOR temp;
  float targetSpeed = speed * MATH_DT;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars)
    return;

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
void mobAlterTarget(VECTOR out, Moby* moby, VECTOR forward, float amount)
{
	VECTOR up = {0,0,1,0};
	
	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
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
  vector_scale(t, t, 1 / dist);
  vector_add(t, moby->Position, t);

  if (pvars->MobVars.MoveVars.IsStuck) {
    mobTurnTowards(moby, targetPosition, turnSpeed);
  } else {
    mobTurnTowards(moby, t, turnSpeed);
  }
  mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, speed, acceleration);
}

//--------------------------------------------------------------------------
void mobPostDrawQuad(Moby* moby, int texId, u32 color, int jointId)
{
	struct QuadDef quad;
	float size = moby->Scale * 2;
	MATRIX m2;
	VECTOR pTL = {0,size,size,1};
	VECTOR pTR = {0,-size,size,1};
	VECTOR pBL = {0,size,-size,1};
	VECTOR pBR = {0,-size,-size,1};
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
	quad.Tex0 = gfxGetFrameTex(texId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera* camera = cameraGetGameCamera(0);
	if (!camera)
		return;
	
	// set world matrix by joint
	mobyGetJointMatrix(moby, jointId, m2);

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 1);
}

#if DEBUG

//--------------------------------------------------------------------------
void mobPostDrawDebug(Moby* moby)
{
#if PRINT_JOINTS
  int i = 0;
  char buf[32];
  int animJointCount = 0;
  MATRIX jointMtx;

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

#if DEBUGMOVE
  draw3DMarker(MoveCheckHit, 1, 0x80FFFFFF, "-");
  draw3DMarker(MoveCheckFrom, 1, 0x80FFFFFF, "a");
  draw3DMarker(MoveCheckTo, 1, 0x80FFFFFF, "b");
  draw3DMarker(MoveCheckFinal, 1, 0x80FFFFFF, "+");
  draw3DMarker(MoveCheckUp, 1, 0x80FFFFFF, "^");
  draw3DMarker(MoveCheckDown, 1, 0x80FFFFFF, "v");
  draw3DMarker(MoveNextPos, 1, 0x80FFFFFF, "o");
  draw3DMarker(MoveTargetLineOfSightHit, 1, 0x80FFFFFF, "x");
#endif
}
#endif

//--------------------------------------------------------------------------
int mobCanSeeMoby(Moby* moby, Moby* canSeeMoby)
{
	VECTOR t, t2;
  VECTOR up = {0,0,1,0};
  
  // increment out of sight ticker
  if (canSeeMoby) {
    vector_add(t, moby->Position, up);
    vector_add(t2, canSeeMoby->Position, up);
    
    return !CollLine_Fix(t, t2, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL) || CollLine_Fix_GetHitMoby() == canSeeMoby;
  }
  
  return 0;
}

//--------------------------------------------------------------------------
void mobUpdateTargetOutOfSight(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.MoveVars.Target;
	VECTOR t, t2;
  VECTOR up = {0,0,1,0};
  
  // increment out of sight ticker
  if (target) {
    if (pvars->MobVars.TargetOutOfSightCheckTicks == 0) {
      vector_add(t, moby->Position, up);
      vector_add(t2, target->Position, up);
      if (CollLine_Fix(t, t2, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL) && CollLine_Fix_GetHitMoby() != target) {
        pvars->MobVars.TimeTargetOutOfSightTicks++;
      } else {
        pvars->MobVars.TimeTargetOutOfSightTicks = 0;
      }

      pvars->MobVars.TargetOutOfSightCheckTicks = 15;
    } else {
      pvars->MobVars.TimeTargetOutOfSightTicks += (pvars->MobVars.TimeTargetOutOfSightTicks > 0) ? 1 : 0;
    }
  } else {
    pvars->MobVars.TimeTargetOutOfSightTicks = 0;
  }
}

//--------------------------------------------------------------------------
void mobPreUpdate(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  decTimerU8(&pvars->MobVars.TargetOutOfSightCheckTicks);

  // update react vars
  ((void (*)(Moby*))0x0051b860)(moby);

  mobUpdateTargetOutOfSight(moby);
  
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
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  pathSetPath(pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars), moby, &pvars->MobVars.MoveVars, e->PathStartNodeIdx, e->PathEndNodeIdx, e->PathCurrentEdgeIdx, e->PathHasReachedStart, e->PathHasReachedEnd);
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
