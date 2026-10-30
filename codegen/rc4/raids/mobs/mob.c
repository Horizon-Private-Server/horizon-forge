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
#include "mobs/mob.h"
#include "utils.h"
#include "shared.h"
#include "gate.h"
#include "spawner.h"
#include "pathfind.h"
#include "dummy.h"
#include "skills.h"

Moby* mobOtherTargets[MOB_MAX_OTHER_TARGETS];
Moby* mobOtherTargets2[MOB_MAX_OTHER_TARGETS];
extern struct MobActionConfig mobActionConfigs[][RAIDS_DIFFICULTY_COUNT][MOB_MAX_ACTIONS_PER_MOB];
extern union MobParameter mobParameters[][RAIDS_DIFFICULTY_COUNT][MOB_MAX_PARAMS_PER_MOB];

#if MOB_DZSTRIKER
#include "dzstriker.c"
#endif

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
void mobRegisterTarget(Moby* moby)
{
  int i;
  for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i) {
    if (!mobOtherTargets2[i]) {
      mobOtherTargets2[i] = moby;
      return;
    }
  }
}

//--------------------------------------------------------------------------
int mobAmIOwner(Moby* moby)
{
  if (!mobyIsMob(moby)) return 0;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	return gameGetMyClientId() == pvars->MobVars.Owner;
}

//--------------------------------------------------------------------------
void mobBlowCorn(Moby* moby)
{
  static int alt = 0;
  int lod = 2;
  if (!MapConfig.State) return;
  if (!moby || !moby->PClass) return;
  if (mobyIsDestroyed(moby)) return;
  if (PATCH_INTEROP && PATCH_INTEROP->Config)
    lod = PATCH_INTEROP->Config->levelOfDetailMobs;
  
  int cornCob = *(short*)(moby->PClass + 0x2e);
  if (!cornCob) return;

  alt = !alt;
  if (!lod) return;
  if (lod == 1 && !alt) return;

  mobyBlowCorn(
    moby
  , cornCob
  , 0
  , 3.0
  , 6.0
  , 7.0
  , 10.0
  , -1
  , -1.0
  , 0
  , TPS
  , 0
  , 0
  , 0
  , 1.0
  , 0x23
  , 0
  , 1.0
  , NULL
  , 1 // no collision physics
  );
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
int mobDoDamageTryHit(Moby* mobMoby, Moby* sourceMoby, Moby* hitMoby, VECTOR jointPosition, int isAoE, float hitRadius, int damageFlags, float amount)
{
  VECTOR mobToHitMoby, mobToJoint, jointToHitMoby;
  VECTOR hitMobyCenter = {0,0,1,0};
  MATRIX playerJointMtx;
  Player* player = guberMobyGetPlayerDamager(hitMoby);
  struct TargetVars* targetVars = mobyGetTargetVars(hitMoby);
	MobyColDamageIn in;
  float hitMobyCollRadius = 0;
  float hitHeight = 0.25;

  if (player && player->PlayerMoby) {
    mobyGetJointMatrix(player->PlayerMoby, 10, playerJointMtx);
    vector_copy(hitMobyCenter, &playerJointMtx[12]);
    hitMobyCenter[2] = clamp(jointPosition[2], playerJointMtx[14] - player->Coll.bot, playerJointMtx[14] + player->Coll.top);
    hitMobyCollRadius = player->Coll.radius;
  } else {
    if (targetVars) {
      vector_scale(hitMobyCenter, hitMoby->M2_03, targetVars->targetHeight);
      hitHeight = 1 + maxf(targetVars->targetHeight, hitHeight);
    }
    
    hitMobyCollRadius = (hitMoby->BSphere[3] / 1024.0) * 2;
    vector_add(hitMobyCenter, hitMobyCenter, hitMoby->Position);
  }

  vector_subtract(mobToHitMoby, hitMobyCenter, sourceMoby->Position);
  vector_subtract(mobToJoint, jointPosition, sourceMoby->Position);
  vector_subtract(jointToHitMoby, hitMobyCenter, jointPosition);

  if (isAoE) {
		// ensure target is in radius of AoE
    if (vector_sqrmag(jointToHitMoby) > powf(hitMobyCollRadius + hitRadius, 2))
      return 0;
  } else {
    // ignore if hit behind
    if (vector_innerproduct(mobToHitMoby, mobToJoint) < 0)
      return 0;

    // clamp within arbitrary vertical limit
    if (fabsf(jointToHitMoby[2]) > hitHeight)
      return 0;

    // ignore if past attack radius
    if (vector_innerproduct(mobToHitMoby, jointToHitMoby) > 0 && vector_sqrmag(jointToHitMoby) > powf(hitMobyCollRadius + hitRadius, 2))
      return 0;
  }

  vector_write(in.Momentum, 0);
  in.Damager = mobMoby;
  in.DamageFlags = damageFlags;
  in.DamageClass = 0;
  in.DamageStrength = 1;
  in.DamageIndex = mobMoby->OClass;
  in.Flags = 1;
  in.DamageHp = amount;

  mobyCollDamageDirect(hitMoby, &in);
  return 1;
}

//--------------------------------------------------------------------------
int mobDoSweepDamage(Moby* mobMoby, Moby* sourceMoby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE)
{
	VECTOR p, delta;
  Player** players = playerGetAll();
  int i;
  int result = 0;
  float t = 0;
  float sqrRadius = radius * radius;
  float firstPassRadius = 5 + radius;
  float firstPassSqrRadius = powf(firstPassRadius, 2);

  // get total distance to travel
  vector_subtract(delta, to, from);
  float len = vector_length(delta);

	// if no length just run once at start position
	if (len == 0)
	{
		len = 1;
		step = 1;
	}

	// prevent very small step
	if (step < 0.1)
		step = 0.1;

  for (t = 0; t < len; t += step)
  {
    vector_lerp(p, from, to, t / len);

    // if no friendly fire just check hit on players
    // otherwise check all mobys
    if (!friendlyFire) {
      for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i) {
        Moby* otherTarget = mobOtherTargets[i];
        if (!otherTarget) break;

        float otherTargetRadius = otherTarget->BSphere[3] / 1024.0;
        vector_subtract(delta, otherTarget->Position, p);
        if ((vector_length(delta)-otherTargetRadius) > firstPassRadius)
          continue;

        if (mobDoDamageTryHit(mobMoby, sourceMoby, otherTarget, p, isAoE, radius, damageFlags, amount)) {
          result |= mobMobyProcessHitFlags(mobMoby, otherTarget, amount, reactToThorns);
        }
      }

      for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
        Player* player = players[i];
        if (!player || !player->SkinMoby || playerIsDead(player))
          continue;

        vector_subtract(delta, player->PlayerPosition, p);
        if (vector_sqrmag(delta) > firstPassSqrRadius)
          continue;

        Moby* playerMoby = player->PlayerMoby;
        if (player->InVehicle && player->Vehicle) {
          playerMoby = player->Vehicle->pMoby;
        }

        if (mobDoDamageTryHit(mobMoby, sourceMoby, playerMoby, p, isAoE, radius, damageFlags, amount)) {
          result |= mobMobyProcessHitFlags(mobMoby, playerMoby, amount, reactToThorns);
        }
      }
    } else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_NONE, sourceMoby, NULL, 5 + radius) > 0) {
      Moby** hitMobies = CollMobysSphere_Fix_GetHitMobies();
      Moby* hitMoby;
      while ((hitMoby = *hitMobies++)) {
        if (hitMoby == mobMoby) continue;
        if (mobDoDamageTryHit(mobMoby, sourceMoby, hitMoby, p, isAoE, radius, damageFlags, amount)) {
          result |= mobMobyProcessHitFlags(mobMoby, hitMoby, amount, reactToThorns);
        }
      }
    }
  }

  return result;
}

//--------------------------------------------------------------------------
int mobDoDamage(Moby* mobMoby, Moby* sourceMoby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE)
{
	VECTOR p, delta;
	MATRIX jointMtx;
  Player** players = playerGetAll();
  int i;
  int result = 0;
  float sqrRadius = radius * radius;
  float firstPassRadius = 5 + radius;
  float firstPassSqrRadius = powf(5 + radius, 2);

  // get position of joint or source moby
  if (jointId < 0) {
    vector_copy(p, sourceMoby->Position);
  } else {
    mobyGetJointMatrix(sourceMoby, jointId, jointMtx);
    vector_copy(p, &jointMtx[12]);
  }

  // if no friendly fire just check hit on players
  // otherwise check all mobys
  if (!friendlyFire) {
    for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i) {
      Moby* otherTarget = mobOtherTargets[i];
      if (!otherTarget) break;

      float otherTargetRadius = otherTarget->BSphere[3] / 1024.0;
      vector_subtract(delta, otherTarget->Position, p);
      if ((vector_length(delta)-otherTargetRadius) > firstPassRadius)
        continue;

      if (mobDoDamageTryHit(mobMoby, sourceMoby, otherTarget, p, isAoE, radius, damageFlags, amount)) {
        result |= mobMobyProcessHitFlags(mobMoby, otherTarget, amount, reactToThorns);
      }
    }
    
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
      Player* player = players[i];
      if (!playerIsValid(player) || playerIsDead(player))
        continue;

      vector_subtract(delta, player->PlayerPosition, p);
      if (vector_sqrmag(delta) > firstPassSqrRadius)
        continue;

      Moby* playerMoby = player->PlayerMoby;
      if (player->InVehicle && player->Vehicle) {
        playerMoby = player->Vehicle->pMoby;
      }

      if (mobDoDamageTryHit(mobMoby, sourceMoby, playerMoby, p, isAoE, radius, damageFlags, amount)) {
        result |= mobMobyProcessHitFlags(mobMoby, playerMoby, amount, reactToThorns);
      }
    }
  } else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_NONE, sourceMoby, NULL, 5 + radius) > 0) {
    Moby** hitMobies = CollMobysSphere_Fix_GetHitMobies();
    Moby* hitMoby;
    while ((hitMoby = *hitMobies++)) {
      if (hitMoby == mobMoby) continue;
      if (mobDoDamageTryHit(mobMoby, sourceMoby, hitMoby, p, isAoE, radius, damageFlags, amount)) {
        result |= mobMobyProcessHitFlags(mobMoby, hitMoby, amount, reactToThorns);
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

  if (mobAmIOwner(moby)) {

    pvars->MobVars.LastAction = pvars->MobVars.Action;
    pvars->MobVars.LastActionId = pvars->MobVars.ActionId++;
    //pvars->MobVars.DynamicRandom = (char)rand(255);
      
    // mark dirty if owner and mob wants state update
    if (pvars->VTable && pvars->VTable->ShouldForceStateUpdateOnAction && pvars->VTable->ShouldForceStateUpdateOnAction(moby, action))
      pvars->MobVars.Dirty = 1;
  }

  // pass to mob handler
  if (pvars->VTable && pvars->VTable->ForceLocalAction)
    pvars->VTable->ForceLocalAction(moby, action);
}

//--------------------------------------------------------------------------
void mobTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff, char* animationReset, char* animationLooped)
{
	if (moby->AnimSeqId != animId) {
		mobyAnimTransition(moby, animId, lerpFrames, startOff);

		*animationReset = 1;
		*animationLooped = 0;
	} else {
		
		// get current t
		// if our stored start is uninitialized, then set current t as start
		float t = moby->AnimSeqT;
		float end = *(u8*)((u32)moby->AnimSeq + 0x10) - (float)(lerpFrames * 0.5);
		if (t >= end && *animationReset) {
			*animationLooped += 1;
			*animationReset = 0;
		} else if (t < end) {
			*animationReset = 1;
		}
	}
}

//--------------------------------------------------------------------------
int mobGetAnimIf(Moby* moby, int animIdFalse, int animIdTrue, int condition, int maxLoops)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // either the condition is true
  // or the animation is running and hasn't reached maxLoops
  if (condition || (maxLoops > 0 && pvars->MobVars.AnimationLooped < maxLoops && moby->AnimSeqId == animIdTrue)) {
    return animIdTrue;
  }

  return animIdFalse;
}

//--------------------------------------------------------------------------
void mobTransAnim(Moby* moby, int animId, float startOff)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	mobTransAnimLerp(moby, animId, 10, startOff, &pvars->MobVars.AnimationReset, &pvars->MobVars.AnimationLooped);
}

//--------------------------------------------------------------------------
void mobUpdateAnim(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	mobTransAnimLerp(moby, moby->AnimSeqId, 0, 0, &pvars->MobVars.AnimationReset, &pvars->MobVars.AnimationLooped);
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
  
  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;
  if (isFlying) {

    // remove velocity
    vector_write(pvars->MobVars.MoveVars.Velocity, 0);
  } else {

    // remove horizontal velocity
    vector_projectonvertical(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
  }
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
  VECTOR delta, horizontalDelta;
  VECTOR hitTo, hitFrom;
  VECTOR hitToEx, hitNormal, hitToExBack;
  VECTOR up = {0,0,0,0};
  if (!pvars)
    return 0;

  float collRadius = pvars->MobVars.Config.CollRadius;

  // get if we should check for collisions with all colliders
  // we have to alternate because when many mobs are in one space
  // collision checks against all of them become extremely expensive
  int collFlag = (mobMoveCheckCollideWithOtherMobsRotatingIndex == pvars->MobVars.Order) ? COLLISION_FLAG_IGNORE_NONE : COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS;

  // if we're stuck, ignore other mobs and just try and get unstuck
  if (pvars->MobVars.MoveVars.IsStuck)
    collFlag = COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS;

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
    float slope = getSignedSlope(horizontalDelta, hitNormal);
    pvars->MobVars.MoveVars.WallSlope = maxf(pvars->MobVars.MoveVars.WallSlope, slope);
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

    // get tangent to surface
    VECTOR hitTangent, hitBitangent, hitDir;
    VECTOR hitRight;
    vector_outerproduct(hitRight, horizontalDelta, up);
    vector_outerproduct(hitTangent, hitNormal, hitRight);
    vector_outerproduct(hitBitangent, hitNormal, hitTangent);
    vector_lerp(hitDir, hitTangent, hitBitangent, fabsf(2 * slope) / MATH_PI);
    vector_normalize(hitDir, hitDir);
    float hitBitangentDotDelta = signf(vector_innerproduct(hitDir, delta));

    // Snap before distant obstacles to avoid large sideways slides at high speeds.
    VECTOR fromToHit;
    vector_subtract(fromToHit, CollLine_Fix_GetHitPosition(), hitFrom);
    float distToHit = vector_length(fromToHit);
    if (distToHit > collRadius * 2.0f) {
      vector_normalize(hitToEx, delta);
      vector_scale(hitToEx, hitToEx, collRadius);
      vector_subtract(outputPos, CollLine_Fix_GetHitPosition(), hitToEx);
      // Convert the center-height collision point back to the mob's feet.
      vector_subtract(outputPos, outputPos, up);
    } else {
      vector_scale(hitDir, hitDir, hitBitangentDotDelta * vector_length(delta));
      vector_add(outputPos, from, hitDir);
    }

#if DEBUGMOVE
    vector_copy(MoveCheckFinal, outputPos);
#endif
    return 1;
  }

  vector_copy(outputPos, to);
  return 0;
}

//--------------------------------------------------------------------------
int mobCollisionIdIsLethal(int collisionId)
{
  collisionId &= 0x0f;
  return collisionId == 0x4 || collisionId == 0xb || collisionId == 0x0d;
}

//--------------------------------------------------------------------------
int mobCollisionIdIsWalkable(int collisionId)
{
  collisionId &= 0x0f;
  return collisionId == 0x03 || collisionId == 0x07 || collisionId == 0x09 || collisionId == 0x0A || collisionId == 0x0E || collisionId == 0x0F;
}

//--------------------------------------------------------------------------
float mobGetFreezeSpeedFactor(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars) return 1.0;

  // if freeze effect is active, apply freeze factor
  if (pvars->MobVars.FreezeEffectActiveTicks > 0 && pvars->MobVars.FreezeEffectStrength > 0) {
    return 0.9 - (MOB_POSTFX_FREEZE_FACTOR * pvars->MobVars.FreezeEffectStrength);
  }

  return 1.0;
}

//--------------------------------------------------------------------------
void mobForceIntoMapBounds(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int escaped = 0;
  int i;
  for (i = 0; i < 3; ++i) {
    if (moby->Position[i] < 1 || moby->Position[i] > 1023
        || pvars->MobVars.MoveVars.LastPosition[i] < 1 || pvars->MobVars.MoveVars.LastPosition[i] > 1023
        || pvars->MobVars.MoveVars.NextPosition[i] < 1 || pvars->MobVars.MoveVars.NextPosition[i] > 1023) {
      escaped = 1;
    }
    moby->Position[i] = clamp(moby->Position[i], 1, 1023);
  }

  if (!escaped)
    return;

  // Keep interpolation endpoints inside the safe moby grid too.
  vector_copy(pvars->MobVars.MoveVars.LastPosition, moby->Position);
  vector_copy(pvars->MobVars.MoveVars.NextPosition, moby->Position);
  vector_write(pvars->MobVars.MoveVars.Velocity, 0);
  vector_write(pvars->MobVars.MoveVars.AddVelocity, 0);
  pvars->MobVars.MoveVars.MoveSkipTicks = 0;

  // Spawners and NPCs handle respawning; standalone mobs have no recovery handler.
  if (mobAmIOwner(moby)) {
    if (mobyIsNpc(moby) || (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS))
      pvars->MobVars.Respawn = 1;
    else
      pvars->MobVars.Destroy = 2;
  }
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
  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;
  int moveStep = pvars->MobVars.MoveVars.LastMoveStep;
  float currentHeightFromGround = 0;

  u8 stuckCheckTicks = decTimerU8(&pvars->MobVars.MoveVars.StuckCheckTicks);
  u8 moveSkipTicks = decTimerU8(&pvars->MobVars.MoveVars.MoveSkipTicks);
  u8 slowTicks = decTimerU8(&pvars->MobVars.SlowTicks);
  if (pvars->MobVars.MoveVars.PathHasReachedStart) {
    decTimerU8(&pvars->MobVars.MoveVars.WasStuckTicks);
  }

  // If LastMoveStep hasn't been initialized yet, default to the current move step.
  if (moveStep == 0 && pvars->MobVars.MoveVars.MoveStep > 0)
    moveStep = pvars->MobVars.MoveVars.MoveStep;

#if DEBUGMOVE
  VECTOR up = {0,0,1,0};
  if (pvars->MobVars.MoveVars.Target) {
    VECTOR from, to, delta;
    vector_subtract(delta, pvars->MobVars.MoveVars.Target->Position, moby->Position);
    vector_add(from, moby->Position, up);
    vector_add(to, pvars->MobVars.MoveVars.Target->Position, up);
    if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL)) {
      vector_copy(MoveTargetLineOfSightHit, CollLine_Fix_GetHitPosition());
    } else {
      vector_copy(MoveTargetLineOfSightHit, to);
    }
  }
#endif

  if (moveSkipTicks == 0) {

    // reset move step
    moveStep = pvars->MobVars.MoveVars.MoveStep;
    int rotatingDt = WRAP_DISTANCE(mobMoveCheckCollideWithOtherMobsRotatingIndex - pvars->MobVars.Order, MAX_MOBS_ALIVE) < 15;
    if (!isOwner || !rotatingDt)
      moveStep = MOB_MOVE_SKIP_TICKS_LOWPRIORITY;
    
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
    float freezeFactor = mobGetFreezeSpeedFactor(moby);
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
        //pvars->MobVars.MoveVars.IsStuck = 1;
      }
    }
    
    // check ground or ceiling
    isMovingDown = targetVelocity[2] <= 0.0001;

    if (isFlying) {
      
      // check height
      vector_projectonhorizontal(ledgePos, lastVelocity);
      vector_normalize(ledgePos, ledgePos);
      vector_add(ledgePos, moby->Position, ledgePos);
      vector_copy(groundCheckFrom, ledgePos);
      groundCheckFrom[2] = maxf(moby->Position[2], ledgePos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
      vector_copy(groundCheckTo, ledgePos);
      groundCheckTo[2] = gameGetDeathHeight();
      if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL)) {
        currentHeightFromGround = pvars->MobVars.MoveVars.DistFromGround = vector_distance(moby->Position, CollLine_Fix_GetHitPosition());
      } else {
        // no ground
        currentHeightFromGround = pvars->MobVars.MoveVars.DistFromGround = 0;
        nextPosHasSafeGround = 0;
      }

      pvars->MobVars.MoveVars.Grounded = 1;

    } else {

      // check ledge
      if (1) {
        vector_projectonhorizontal(ledgePos, lastVelocity);
        vector_normalize(ledgePos, ledgePos);
        //vector_fromyaw(ledgePos, moby->Rotation[2]);
        vector_add(ledgePos, moby->Position, ledgePos);
        vector_copy(groundCheckFrom, ledgePos);
        groundCheckFrom[2] = maxf(moby->Position[2], ledgePos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
        vector_copy(groundCheckTo, ledgePos);
        groundCheckTo[2] = gameGetDeathHeight();
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL)) {
          currentHeightFromGround = pvars->MobVars.MoveVars.DistFromGround = vector_distance(moby->Position, CollLine_Fix_GetHitPosition());
          if (!mobCollisionIdIsWalkable(CollLine_Fix_GetHitCollisionId()) || mobCollisionIdIsLethal(CollLine_Fix_GetHitCollisionId())) {
            nextPosHasSafeGround = 0;
          }
        } else {
          // no ground
          nextPosHasSafeGround = 0;
          currentHeightFromGround = pvars->MobVars.MoveVars.DistFromGround = 0;
        }
      }

      // check ground
      if (isMovingDown) {
        vector_copy(groundCheckFrom, moby->Position);
        groundCheckFrom[2] = maxf(moby->Position[2], nextPos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
        vector_copy(groundCheckTo, nextPos);
        groundCheckTo[2] -= 0.5;
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL)) {

          // mark grounded this frame
          pvars->MobVars.MoveVars.Grounded = 1;
          pvars->MobVars.MoveVars.DistFromGround = 0;

          // check if we've hit death barrier
          if (isOwner && mobCollisionIdIsLethal(CollLine_Fix_GetHitCollisionId())) {
            pvars->MobVars.Destroy = 2;
          } else if (!mobCollisionIdIsWalkable(CollLine_Fix_GetHitCollisionId())) {
            nextPosHasSafeGround = 1; // disable ledge if we're already on 
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
      else if (0) {
        vector_copy(groundCheckFrom, nextPos);
        groundCheckFrom[2] = moby->Position[2];
        vector_copy(groundCheckTo, nextPos);
        groundCheckTo[2] += 3;
        //groundCheckTo[2] += ZOMBIE_BASE_STEP_HEIGHT;
        if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL)) {
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

    }

#if DEBUGMOVE
    vector_copy(MoveNextPos, nextPos);
#endif

    // detect ledge
    if (!nextPosHasSafeGround) {
      //pvars->MobVars.MoveVars.IsStuck = 1;
      nextPos[0] = moby->Position[0];
      nextPos[1] = moby->Position[1];
      //nextPos[2] = moby->Position[2];
    }

    // set position
    vector_copy(pvars->MobVars.MoveVars.NextPosition, nextPos);

    // add gravity to velocity with clamp on downwards speed
    if (!isFlying || currentHeightFromGround > pvars->MobVars.MoveVars.CurrentHeightLimit) {
      pvars->MobVars.MoveVars.Velocity[2] -= GRAVITY_MAGNITUDE * MATH_DT * (float)moveStep;
      if (pvars->MobVars.MoveVars.Velocity[2] < -10 * MATH_DT)
        pvars->MobVars.MoveVars.Velocity[2] = -10 * MATH_DT;
    }

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

  u8* edge = pathGetCurrentEdge(path, moby, &pvars->MobVars.MoveVars);
  if (edge) {
    pathGetNodePosition(path, edge[1], 0, mobyToTargetDelta);
    vector_subtract(mobyToTargetDelta, mobyToTargetDelta, moby->Position);
  } else {
    vector_copy(mobyToTargetDelta, pvars->MobVars.MoveVars.Velocity);
  }

  VECTOR hVelocity;
  vector_projectonhorizontal(mobyToTargetDelta, mobyToTargetDelta);
  vector_projectonhorizontal(hVelocity, pvars->MobVars.MoveVars.Velocity);
  if (vector_innerproduct(hVelocity, mobyToTargetDelta) > 0.5 && pathShouldJump(path, moby, &pvars->MobVars.MoveVars)) {
    pvars->MobVars.MoveVars.QueueJumpSpeed = pathGetJumpSpeed(path, moby, &pvars->MobVars.MoveVars);
  }

  mobForceIntoMapBounds(moby);
}

//--------------------------------------------------------------------------
float mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed)
{
  VECTOR delta;
  
  if (!moby || !moby->PVar || fabsf(turnSpeed) < 0.001)
    return 0;

  vector_subtract(delta, towards, moby->Position);
  float targetYaw = atan2f(delta[1], delta[0]);
  float yawDelta = clampAngle(targetYaw - moby->Rotation[2]);

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float radians = turnSpeed * pvars->MobVars.Config.Speed * MATH_DT;
  moby->Rotation[2] = clampAngle(moby->Rotation[2] + clamp(yawDelta, -radians, radians));

  return clampAngle(moby->Rotation[2] - targetYaw);
}

//--------------------------------------------------------------------------
float mobTurnTowardsPredictive(Moby* moby, Moby* target, float turnSpeed, float predictFactor)
{
  VECTOR pos;
  
  if (!moby || !moby->PVar || fabsf(turnSpeed) < 0.001)
    return 0;

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

  return mobTurnTowards(moby, pos, turnSpeed);
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

  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;
  if (pathUseTargetMoby(pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars), moby, &pvars->MobVars.MoveVars)) {
    vector_copy(targetPosition, pvars->MobVars.MoveVars.Target->Position);
  } else {
    vector_copy(targetPosition, pvars->MobVars.MoveVars.TargetPosition);
  }

  Moby* target = pvars->MobVars.MoveVars.Target;
  if (target) {
    targetRadius = mobGetTargetRadius(target);
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

  if (isFlying) {
    float height = minf(pvars->MobVars.MoveVars.PreferredHeight, pvars->MobVars.MoveVars.CurrentHeightLimit);
    float dy = (height - pvars->MobVars.MoveVars.DistFromGround);
    velocity[2] = dy * MATH_DT * lerpf(1, 0, 1 - powf(MATH_E, -1 * fabsf(dy) * MATH_DT));
  }

  // stop when at target
  if (targetSpeed > 0) {
    vector_subtract(fromToTarget, targetPosition, from);
    vector_projectonhorizontal(fromToTarget, fromToTarget);
    float distToTarget = vector_length(fromToTarget);
    float min = pvars->MobVars.Config.CollRadius + targetRadius;

    // Clamp horizontal velocity before it can cross the target's stopping distance.
    vector_projectonhorizontal(hVelocity, velocity);
    float speedThisFrame = vector_length(hVelocity);
    if (speedThisFrame > 0 && distToTarget > min && speedThisFrame > (distToTarget - min)) {
      float scale = (distToTarget - min) / speedThisFrame;
      vector_scale(hVelocity, hVelocity, scale);
      vector_projectonvertical(velocity, velocity);
      vector_add(velocity, velocity, hVelocity);
      return;
    }

    vector_add(next, from, velocity);
    vector_subtract(nextToTarget, targetPosition, next);
    vector_projectonhorizontal(nextToTarget, nextToTarget);
    float distNextToTarget = vector_length(nextToTarget);
    
    // if too close to target, stop
    if (distNextToTarget < min) {
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
float mobGetCurrentMoveSpeed(Moby* moby)
{
  VECTOR hVelocity;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  vector_projectonhorizontal(hVelocity, pvars->MobVars.MoveVars.Velocity);
  return (vector_length(hVelocity) / (pvars->MobVars.Config.Speed * MATH_DT)) * mobGetFreezeSpeedFactor(moby);
}

//--------------------------------------------------------------------------
float mobGetCurrentWalkAngle(Moby* moby)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	Moby* target = pvars->MobVars.MoveVars.Target;
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
float mobGetLungeSpeed(Moby* moby, int animId, float startFrame, float endFrame, float multiplier)
{
  if (!moby || !moby->PVar || moby->AnimSeqId != animId || endFrame <= startFrame
      || moby->AnimSeqT < startFrame || moby->AnimSeqT >= endFrame || multiplier <= 0)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float progress = clamp((moby->AnimSeqT - startFrame) / (endFrame - startFrame), 0, 1);
  // Standard burst tapers from 5.0625x movement speed toward 1x over the lunge window.
  float curve = lerpf(2.25, 1, progress);
  return pvars->MobVars.Config.Speed * multiplier * curve * curve;
}

//--------------------------------------------------------------------------
void mobMoveTowards(Moby* moby, VECTOR targetPosition, float speed, float turnSpeed, float acceleration, float curveNearTargetDir)
{
  VECTOR t, t2;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int isFlying = pvars->MobVars.MoveVars.PreferredHeight > 0;

  if (!isFlying) {
    vector_subtract(t, targetPosition, moby->Position);
    float dist = vector_length(t);
    if (dist < 10.0 && fabsf(curveNearTargetDir) > 0.001) {
      mobAlterTarget(t2, moby, t, clamp(dist, 0, 10) * 0.3 * curveNearTargetDir);
      vector_add(t, t, t2);
    }
    //vector_scale(t, t, 1 / dist);
    vector_add(t, moby->Position, t);
  } else {
    vector_copy(t, targetPosition);
  }

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
      && (!pvars->MobVars.MoveVars.HitWallMoby || pvars->MobVars.MoveVars.HitWallMoby != pvars->MobVars.MoveVars.Target)
      ;
}

//--------------------------------------------------------------------------
void mobPostDrawQuad(Moby* moby, float scale, u32 color, int jointId)
{
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
	if (!pvars)
		return;

	struct QuadDef quad;
	float size = scale * mobGetScaleMultiplier(moby);
	MATRIX m2;
	VECTOR pTL = {0,size,size,1};
	VECTOR pTR = {0,-size,size,1};
	VECTOR pBL = {0,size,-size,1};
	VECTOR pBR = {0,-size,-size,1};
	VECTOR offset = {0,0,size};

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
	quad.Tex0 = gfxGetFrameTex(mobGetSpawnParams(moby)->SpriteTexId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera* camera = cameraGetGameCamera(0);
	if (!camera)
		return;
	
	// face the camera with the bottom of the sprite at the mob's position
  memcpy(m2, camera->uMtx, sizeof(VECTOR) * 3);
  vector_add(&m2[12], moby->Position, offset);

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

    if (!pvars->VTable->GetSeeFromPosition || !pvars->VTable->GetSeeFromPosition(moby, t)) {
      vector_scale(up, up, pvars->TargetVars.targetHeight);
      vector_add(t, moby->Position, up);
    }

    return !CollLine_Fix(t, t2, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, moby, NULL) || CollLine_Fix_GetHitMoby() == canSeeMoby;
  }
  
  return 0;
}

//--------------------------------------------------------------------------
Moby* mobGetNextTargetFiltered(Moby* moby, int playersOnly, MobCanAcquireTarget_func canAcquireTarget)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  Player** players = playerGetAll();
  int i;
  VECTOR delta;
  VECTOR forward;
  Moby* currentTarget = pvars->MobVars.MoveVars.Target;
  Moby* bestTargetMoby = NULL;
  int bestHasAggroZone = 0;
  float closestTargetDist = 100000;

  // check other targets first unless the caller only targets players
  if (!playersOnly) {
    for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i) {
      Moby* otherTarget = mobOtherTargets[i];
      if (!otherTarget) break;
      if (otherTarget->OClass != DUMMY_OCLASS) break;

      struct DummyPVar* otherPVars = (struct DummyPVar*)otherTarget->PVar;
      if ((!currentTarget || currentTarget == otherTarget) && otherPVars->Config.MobTargetType == DUMMY_MOB_AGGRO_ALWAYS) {
        return otherTarget;
      }

      vector_subtract(delta, otherTarget->Position, moby->Position);
      float dist = vector_length(delta);
      float maxDist = otherPVars->Config.MobTargetDistance*otherPVars->Config.MobTargetDistance;
      if (otherPVars->Config.MobTargetType == DUMMY_MOB_AGGRO_IN_RANGE && dist < maxDist) {
        bestTargetMoby = otherTarget;
        closestTargetDist = dist;
      }
    }
  }

  vector_fromyaw(forward, moby->Rotation[2]);
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* p = players[i];
    if (p && p->SkinMoby && !playerIsDead(p) && p->Health > 0 && p->SkinMoby->Opacity >= 0x80) {
      vector_subtract(delta, p->PlayerPosition, moby->Position);
      float dist = vector_length(delta);
      Moby* pTargetMoby = playerGetTargetMoby(p);
      int isCurrentTarget = pTargetMoby == currentTarget;
      
      // determine angle from mob forward to player
      float theta = acosf(vector_innerproduct(forward, delta));
      int inAggroZone = moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS && spawnerOnChildIsTargetInAggroZone(moby->PParent, moby, pvars->MobVars.Userdata, pTargetMoby);

      // skip if not in sight, unless already targeted
      if (!isCurrentTarget && !inAggroZone) {
        float detection = skillsGetDetectionFactor(p);
        if (dist > pvars->MobVars.Config.AutoAggroMaxRange * detection && (dist > pvars->MobVars.Config.VisionRange * detection || fabsf(theta) > pvars->MobVars.Config.PeripheryRangeTheta)) continue;
        if (canAcquireTarget && !canAcquireTarget(moby, pTargetMoby)) continue;
      }

      // favor existing target
      if (isCurrentTarget)
        dist *= (1.0 / 3.0); // factor of 3 favor current target
      
      // pick closest target
      if ((bestHasAggroZone == inAggroZone && dist < closestTargetDist) || (!bestHasAggroZone && inAggroZone)) {
        bestTargetMoby = pTargetMoby;
        bestHasAggroZone = inAggroZone;
        closestTargetDist = dist;
      }
    }
  }

  if (bestTargetMoby)
    return bestTargetMoby;

  return NULL;
}

//--------------------------------------------------------------------------
Moby* mobGetNextTarget(Moby* moby)
{
  return mobGetNextTargetFiltered(moby, 0, NULL);
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

#if GATE
      gateSetCollision(0);
#endif

      if (mobCanSeeMoby(moby, target)) {
        pvars->MobVars.TimeTargetOutOfSightTicks = 0;
      } else {
        pvars->MobVars.TimeTargetOutOfSightTicks++;
      }

#if GATE
      gateSetCollision(1);
#endif

      pvars->MobVars.TargetOutOfSightCheckTicks = TPS;
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
  pvars->MobVars.MoveVars.IsOwner = mobAmIOwner(moby);

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
  MapConfig.Functions.OnMobSpawnedFunc = &mobOnSpawned;
}

//--------------------------------------------------------------------------
void mobTick(void)
{
  if (MapConfig.State && MapConfig.State->MobStats.TotalAlive > 0)
    mobMoveCheckCollideWithOtherMobsRotatingIndex = (mobMoveCheckCollideWithOtherMobsRotatingIndex + 1) % MapConfig.State->MobStats.TotalAlive;
  else
    mobMoveCheckCollideWithOtherMobsRotatingIndex = (mobMoveCheckCollideWithOtherMobsRotatingIndex + 1) % MAX_MOBS_ALIVE;

  // clear targets
  memcpy(mobOtherTargets, mobOtherTargets2, sizeof(mobOtherTargets));
  memset(mobOtherTargets2, 0, sizeof(mobOtherTargets2));
}

//--------------------------------------------------------------------------
int mobGetActionCooldownTicks(Moby *moby, int action)
{
	struct MobActionConfig *actionConfig = mobGetActionConfig(moby, action);
	if (!actionConfig)
		return 0;

	return randRangeInt(actionConfig->MinCooldownTicks, actionConfig->MaxCooldownTicks);
}

//--------------------------------------------------------------------------
void mobTickActionCooldowns(Moby *moby, int actionCount, u32 *actionCooldowns, u32 *actionQueuedForTicks)
{
	int i;
	for (i = 0; i < actionCount; ++i)
	{
		struct MobActionConfig *actionConfig = mobGetActionConfig(moby, i);

		// increment and check if any queues have reached their end
		if (actionQueuedForTicks[i] > 0)
		{
			if (++actionQueuedForTicks[i] > actionConfig->QueuedForTicks && actionConfig->QueuedForTicks > 0)
			{
				// reset
				actionCooldowns[i] = mobGetActionCooldownTicks(moby, i);
				actionQueuedForTicks[i] = 0;
			}

			// action is queued so no need to decrement cooldown
			continue;
		}

		// decrement and check if cooldown is 0
		if (decTimerU32(&actionCooldowns[i]) == 0)
		{
			// cooldown hit 0
			// check if action is already queued
			if (actionQueuedForTicks[i] == 0)
			{
				// action is not queued
				// run probability check
				float roll = randRange(0, 1);
				int success = roll < actionConfig->Probability;
				if (success)
				{
					// queue action
					actionQueuedForTicks[i] = 1;
				}
				else
				{
					// reset action cooldown
					actionCooldowns[i] = mobGetActionCooldownTicks(moby, i);
				}
			}
		}
	}
}

//--------------------------------------------------------------------------
float mobGetTargetRadius(Moby *target)
{
	if (!target)
		return 0;

	Player *player = guberMobyGetPlayerDamager(target);
	if (player)
		return player->Coll.radius;

	return target->BSphere[3] / 1024.0;
}

//--------------------------------------------------------------------------
float mobGetDistanceToTarget(Moby *moby, Moby *target)
{
	// struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	VECTOR t;

	if (!target)
		return 0;

	vector_copy(t, target->Position);
	vector_subtract(t, t, moby->Position);
	float dist = vector_length(t) - mobGetTargetRadius(target);

	return maxf(0, dist);
}

//--------------------------------------------------------------------------
void mobDefaultPreUpdate(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// Runtime owns the per-local-player hit timers, including NPC updates.

	// decrement path target pos tickers
	decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);

	mobPreUpdate(moby);

  struct MobSpawnParams* params = mobGetSpawnParams(moby);
  if (pvars->MobVars.AcidEffectActiveTicks) {
    moby->PrimaryColor = colorLerp(params->BaseColor, MOB_POSTFX_ACID_COLOR, MOB_POSTFX_FACTOR);
  } else if (pvars->MobVars.FreezeEffectActiveTicks) {
    moby->PrimaryColor = colorLerp(params->BaseColor, MOB_POSTFX_FREEZE_COLOR, MOB_POSTFX_FACTOR);
  } else {
    moby->PrimaryColor = params->BaseColor;
  }
  moby->GlowRGBA = params->GlowColor;
}

//--------------------------------------------------------------------------
int mobDefaultOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	if (!e->PlayerDamager)
		return 1;
	if (!e->PlayerDamager->IsLocal)
		return 1;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// only accept local damage when timer is 0
	int timer = pvars->MobVars.LocalPlayerDamageHitInvTimer[e->PlayerDamager->LocalPlayerIndex];
	if (timer == 0)
	{
		pvars->MobVars.LocalPlayerDamageHitInvTimer[e->PlayerDamager->LocalPlayerIndex] = pvars->MobVars.Config.DamageCooldownTickCount;
		return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
void mobHandleFlinch(Moby *moby, struct MobDamageEventArgs *e, int canFlinch, int isShock, float probability, float powerFactor, int flinchState, int bigFlinchState)
{
	// knockback
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
		memcpy(&pvars->MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
	}

	if (!mobAmIOwner(moby))
		return;

#if ALWAYS_FLINCH
	probability = 2;
	powerFactor = 2;
#endif

	if (canFlinch)
	{
		if (e->Knockback.Force)
		{
			mobSetAction(moby, bigFlinchState);
		}
		else if (isShock)
		{
			mobSetAction(moby, flinchState);
		}
		else if (randRange(0, 1) < probability)
		{
			if (randRange(0, 1) < powerFactor)
			{
				mobSetAction(moby, bigFlinchState);
			}
			else
			{
				mobSetAction(moby, flinchState);
			}
		}
	}
}

//--------------------------------------------------------------------------
void mobTransAnimDefaultLerp(Moby *moby, int animId, int lerpFrames, float startOff)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (moby->AnimSeqId != animId)
	{
		mobyAnimTransition(moby, animId, lerpFrames, startOff);

		pvars->MobVars.AnimationReset = 1;
		pvars->MobVars.AnimationLooped = 0;
	}
	else
	{

		// get current t
		// if our stored start is uninitialized, then set current t as start
		float t = moby->AnimSeqT;
		float end = *(u8 *)((u32)moby->AnimSeq + 0x10) - (float)(lerpFrames * 0.5);
		if (t >= end && pvars->MobVars.AnimationReset)
		{
			pvars->MobVars.AnimationLooped++;
			pvars->MobVars.AnimationReset = 0;
		}
		else if (t < end)
		{
			pvars->MobVars.AnimationReset = 1;
		}
	}
}

//--------------------------------------------------------------------------
float mobTurnTowardsPredictiveWithSpeed(Moby *moby, Moby *target, float turnSpeed, float speed)
{
	if (!moby || !target || fabsf(turnSpeed) < 0.001)
		return 0;

	// Calculate distance between moby and target
	float distance = mobGetDistanceToTarget(moby, target);

	// Calculate predictFactor from distance and speed
	// predictFactor = distance / speed (in game units per frame)
	float predictFactor = speed > 0 ? distance / speed : 0;

	// Call the existing predictive turn function with calculated predictFactor
	return mobTurnTowardsPredictive(moby, target, turnSpeed, predictFactor);
}

//--------------------------------------------------------------------------
struct MobSpawnParams* mobGetSpawnParams(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return &MapConfig.MobSpawnParams[pvars->MobVars.SpawnParamsIdx];
}

//--------------------------------------------------------------------------
int mobIsBoss(Moby* moby)
{
  return mobGetSpawnParams(moby)->IsBoss;
}

//--------------------------------------------------------------------------
int mobIsFrozen(Moby* moby)
{
  return 0;
}

//--------------------------------------------------------------------------
int mobGetBehavior(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return pvars->MobVars.Behavior;
}

//--------------------------------------------------------------------------
float mobGetScaleMultiplier(Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return pvars->MobVars.Config.Scale;
}

//--------------------------------------------------------------------------
u32 mobGetDamageFlags(Moby* moby, u32 flags)
{
  return flags;
}

//--------------------------------------------------------------------------
void mobSpawnCorn(Moby* moby, int bangle)
{
  mobBlowCorn(moby);
}

//--------------------------------------------------------------------------
struct MobActionConfig* mobGetActionConfig(Moby* moby, int action)
{
  if (!moby || !moby->PVar || !MapConfig.State)
    return NULL;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return &mobActionConfigs[pvars->MobVars.SpawnParamsIdx][pvars->MobVars.DifficultyStars][action];
}

//--------------------------------------------------------------------------
float mobGetFloat(Moby* moby, int parameter)
{
  if (!moby || !moby->PVar || !MapConfig.State)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return mobParameters[pvars->MobVars.SpawnParamsIdx][pvars->MobVars.DifficultyStars][parameter].FloatValue;
}

//--------------------------------------------------------------------------
int mobGetInt(Moby* moby, int parameter)
{
  if (!moby || !moby->PVar || !MapConfig.State)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return mobParameters[pvars->MobVars.SpawnParamsIdx][pvars->MobVars.DifficultyStars][parameter].IntValue;
}

//--------------------------------------------------------------------------
int mobGetPathTargetPos(VECTOR output, Moby* moby)
{
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  return pathGetTargetPos(pathGetMobyPathGraph(moby, &pvars->MobVars.MoveVars), output, moby, &pvars->MobVars.MoveVars);
}
