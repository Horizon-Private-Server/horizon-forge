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
#include "gate.h"
#include "dummy.h"
#include "maputils.h"
#include "pathfind.h"
#include "messageid.h"

void mobForceIntoMapBounds(Moby *moby);

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

Moby *mobOtherTargets[MOB_MAX_OTHER_TARGETS];
Moby *mobOtherTargets2[MOB_MAX_OTHER_TARGETS];
int mobMoveCheckCollideWithOtherMobsRotatingIndex = 0;
extern struct MobActionConfig mobActionConfigs[][MOB_MAX_ACTIONS_PER_MOB];
extern int mobActionConfigsCount;

//--------------------------------------------------------------------------
void mobRegisterTarget(Moby *moby)
{
	int i;
	for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i)
	{
		if (!mobOtherTargets2[i])
		{
			mobOtherTargets2[i] = moby;
			return;
		}
	}
}

//--------------------------------------------------------------------------
int mobAmIOwner(Moby *moby)
{
	if (!mobyIsMob(moby))
		return 0;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return gameGetMyClientId() == pvars->MobVars.Owner;
}

//--------------------------------------------------------------------------
int mobIsFrozen(Moby *moby)
{
	if (!moby || !moby->PVar || !MapConfig.State)
		return 0;

	if (survivalIsPaused())
		return 1;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return MapConfig.State->Freeze && pvars->MobVars.Config.MobAttribute != MOB_ATTRIBUTE_FREEZE && pvars->MobVars.Health > 0;
}

//--------------------------------------------------------------------------
int mobGetBehavior(Moby *moby)
{
	if (!moby || !moby->PVar || !MapConfig.State)
		return 0;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Config.Behavior;
}

//--------------------------------------------------------------------------
struct MobActionConfig *mobGetActionConfig(Moby *moby, int action)
{
	if (!moby || !moby->PVar || !MapConfig.State)
		return NULL;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return &mobActionConfigs[pvars->MobVars.SpawnParamsIdx][action];
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
void mobTickActionCooldowns(Moby *moby, int actionCount, u32 *actionCooldowns, int *actionQueuedForTicks)
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
float mobGetActionFloat(Moby *moby, int action, int param)
{
	struct MobActionConfig *actionConfig = mobGetActionConfig(moby, action);
	if (!actionConfig)
		return 0;

	return actionConfig->Parameters[param].FloatValue;
}

//--------------------------------------------------------------------------
int mobGetActionInt(Moby *moby, int action, int param)
{
	struct MobActionConfig *actionConfig = mobGetActionConfig(moby, action);
	if (!actionConfig)
		return 0;

	return actionConfig->Parameters[param].IntValue;
}

//--------------------------------------------------------------------------
float mobGetScaleMultiplier(Moby *moby)
{
	if (!moby || !moby->PVar)
		return 1;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].Scale;
}

//--------------------------------------------------------------------------
u32 mobGetDamageFlags(Moby *moby, u32 damageFlags)
{
	if (!moby)
		return 0;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	switch (pvars->MobVars.Config.MobAttribute)
	{
	case MOB_ATTRIBUTE_FREEZE:
		damageFlags |= MOB_DAMAGE_FLAG_FREEZE;
		break;
	case MOB_ATTRIBUTE_ACID:
		damageFlags |= MOB_DAMAGE_FLAG_ACID;
		break;
	}

	return damageFlags;
}

//--------------------------------------------------------------------------
GuberEvent *mobCreateEvent(Moby *moby, u32 eventType)
{
	GuberEvent *event = NULL;

	// create guber object
	Guber *guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
void mobSpawnCorn(Moby *moby, int bangle)
{
#if MOB_CORN
	mobyBlowCorn(
			moby, bangle, 0, 3.0, 6.0, 3.0, 6.0, -1, -1.0, -1.0, 255, 1, 0, 1, 1.0, 0x23, 3, 1.0, NULL, 0);
#endif
}

//--------------------------------------------------------------------------
void mobResetSoundTrigger(Moby *moby)
{
	moby->SoundTrigger = 0;
	moby->SoundDesired = -1;
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
void mobGetTargetCenter(Moby *target, VECTOR out)
{
	if (!target)
		return;

	switch (target->OClass)
	{
	default:
		vector_add(out, target->Position, target->M2_03);
	}
}

//--------------------------------------------------------------------------
int mobCanSeeMoby(Moby *moby, Moby *canSeeMoby)
{
	VECTOR t, t2;
	VECTOR up = {0, 0, 1, 0};
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// increment out of sight ticker
	if (canSeeMoby)
	{
		mobGetTargetCenter(canSeeMoby, t2);

		// if (!pvars->VTable->GetSeeFromPosition || !pvars->VTable->GetSeeFromPosition(moby, t)) {
		vector_scale(up, up, pvars->TargetVars.targetHeight);
		vector_add(t, moby->Position, up);
		//}

		return !CollLine_Fix(t, t2, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL) || CollLine_Fix_GetHitMoby() == canSeeMoby;
	}

	return 0;
}

//--------------------------------------------------------------------------
void mobGetKnockbackVelocity(Moby *moby, VECTOR out)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	vector_write(out, 0);
	if (pvars->MobVars.Knockback.Ticks <= 0)
		return;

	// compute
	float slerpFactor = powf(100, pvars->MobVars.Knockback.Ticks / (float)PLAYER_KNOCKBACK_BASE_TICKS) / 100;
	float power = PLAYER_KNOCKBACK_BASE_POWER * powf(MOB_KNOCKBACK_POWER_EXPONENT_BASE, pvars->MobVars.Knockback.Power) * slerpFactor;
	vector_fromyaw(out, pvars->MobVars.Knockback.Angle / 1000.0);
	out[2] = 1;
	vector_normalize(out, out);
	vector_scale(out, out, power * MATH_DT);

	// DPRINTF("knockback %d %d => %f %f\n", pvars->MobVars.Knockback.Ticks, pvars->MobVars.Knockback.Power, power, slerpFactor);
}

//--------------------------------------------------------------------------
void mobAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
float mobGetCurrentMoveSpeed(Moby *moby)
{
	VECTOR hVelocity;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	vector_projectonhorizontal(hVelocity, pvars->MobVars.MoveVars.Velocity);
	return (vector_length(hVelocity) / (pvars->MobVars.Config.Speed * MATH_DT));
}

//--------------------------------------------------------------------------
void mobReactToExplosionAt(Moby *damager, VECTOR position, float damage, float radius, int knockbackPower)
{
	if (!MapConfig.State)
		return;

	int i;
	VECTOR delta;
	struct MobDamageEventArgs args;
	float sqrRadius = radius * radius;
	u32 uid = 0;
	if (damager)
		uid = guberGetUID(damager);

	memset(&args, 0, sizeof(args));
	for (i = 0; i < MAX_MOBS_ALIVE; ++i)
	{
		Moby *m = MapConfig.State->AllMobsSorted[i];
		if (m)
		{
			struct MobPVar *pvars = (struct MobPVar *)m->PVar;
			vector_subtract(delta, m->Position, position);
			if (vector_sqrmag(delta) <= sqrRadius)
			{
				float dist = vector_length(delta);
				float angle = atan2f(delta[1] / dist, delta[0] / dist);

				// create event
				GuberEvent *guberEvent = mobCreateEvent(m, MOB_EVENT_DAMAGE);
				if (guberEvent)
				{
					args.SourceUID = uid;
					args.SourceOClass = 0;
					args.DamageQuarters = damage * 4;
					args.DamageFlags = 0;
					if (knockbackPower > 0)
					{
						args.Knockback.Angle = (short)(angle * 1000);
						args.Knockback.Ticks = pvars->MobVars.MoveVars.MoveStep + 1;
						args.Knockback.Power = knockbackPower > 255 ? 255 : knockbackPower;
						args.Knockback.Force = 1;
					}
					guberEventWrite(guberEvent, &args, sizeof(struct MobDamageEventArgs));
				}
			}
		}
	}
}

//--------------------------------------------------------------------------
int mobMobyProcessHitFlags(Moby *moby, Moby *hitMoby, float damage, int reactToThorns)
{
	int result = 0;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	Player *player = guberMobyGetPlayerDamager(hitMoby);
	if (player)
		result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER;
	if (hitMoby == pvars->MobVars.Target)
		result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET;
	if (mobyIsMob(hitMoby))
		result |= MOB_DO_DAMAGE_HIT_FLAG_HIT_MOB;

	return result;
}

//--------------------------------------------------------------------------
int mobDoDamageTryHit(Moby *moby, Moby *hitMoby, VECTOR jointPosition, int isAoE, float hitRadius, int damageFlags, float amount)
{
	VECTOR mobToHitMoby, mobToJoint, jointToHitMoby;
	VECTOR hitMobyCenter = {0, 0, 1, 0};
	MATRIX playerJointMtx;
	Player *player = guberMobyGetPlayerDamager(hitMoby);
	struct TargetVars *targetVars = mobyGetTargetVars(hitMoby);
	MobyColDamageIn in;
	float hitMobyCollRadius = 0;
	float hitHeight = 0.25;

	if (player && player->PlayerMoby)
	{
		mobyGetJointMatrix(player->PlayerMoby, 10, playerJointMtx);
		vector_copy(hitMobyCenter, &playerJointMtx[12]);
		hitMobyCenter[2] = clamp(jointPosition[2], playerJointMtx[14] - player->Coll.bot, playerJointMtx[14] + player->Coll.top);
		hitMobyCollRadius = player->Coll.radius;
	}
	else
	{
		hitMobyCollRadius = hitMoby->BSphere[3] / 1024.0;
		if (targetVars)
		{
			vector_scale(hitMobyCenter, hitMoby->M2_03, targetVars->targetHeight);
			hitHeight = 1 + maxf(targetVars->targetHeight, hitHeight);
			// hitMobyCollRadius = (u8)targetVars->targetRadiusIn8ths / 8.0;
		}

		vector_add(hitMobyCenter, hitMobyCenter, hitMoby->Position);
	}

	vector_subtract(mobToHitMoby, hitMobyCenter, moby->Position);
	vector_subtract(mobToJoint, jointPosition, moby->Position);
	vector_subtract(jointToHitMoby, hitMobyCenter, jointPosition);

	if (isAoE)
	{
		// ensure target is in radius of AoE
		if (vector_length(jointToHitMoby) > (hitMobyCollRadius + hitRadius))
			return 0;
	}
	else
	{
		// ignore if hit behind
		if (vector_innerproduct(mobToHitMoby, mobToJoint) < 0)
			return 0;

		// clamp within arbitrary vertical limit
		if (fabsf(jointToHitMoby[2]) > hitHeight)
			return 0;

		// ignore if past attack radius
		if (vector_innerproduct(mobToHitMoby, jointToHitMoby) > 0 && vector_length(jointToHitMoby) > (hitMobyCollRadius + hitRadius))
			return 0;
	}

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
int mobDoSweepDamage(Moby *moby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE)
{
	VECTOR p, delta;
	// struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int i;
	int result = 0;
	float t = 0;
	// float sqrRadius = radius * radius;
	float firstPassRadius = MOB_DAMAGE_FIRST_PASS_RADIUS_EXTRA + radius;
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

	for (t = 0; t < len; t += step)
	{
		vector_lerp(p, from, to, t / len);

		// if no friendly fire just check hit on players
		// otherwise check all mobys
		if (!friendlyFire)
		{
			for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i)
			{
				Moby *otherTarget = mobOtherTargets[i];
				if (!otherTarget)
					break;

				float otherTargetRadius = otherTarget->BSphere[3] / 1024.0;
				vector_subtract(delta, otherTarget->Position, p);
				if ((vector_length(delta) - otherTargetRadius) > firstPassRadius)
					continue;

				if (mobDoDamageTryHit(moby, otherTarget, p, isAoE, radius, damageFlags, amount))
				{
					result |= mobMobyProcessHitFlags(moby, otherTarget, amount, reactToThorns);
				}
			}

			for (i = 0; i < GAME_MAX_PLAYERS; ++i)
			{
				Player *player = playerGetFromIndex(i);
				if (!player || !player->SkinMoby || playerIsDead(player))
					continue;

				vector_subtract(delta, player->PlayerPosition, p);
				if (vector_sqrmag(delta) > firstPassSqrRadius)
					continue;

				if (mobDoDamageTryHit(moby, player->PlayerMoby, p, isAoE, radius, damageFlags, amount))
				{
					result |= mobMobyProcessHitFlags(moby, player->PlayerMoby, amount, reactToThorns);
				}
			}
		}
		else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_NONE, moby, NULL, MOB_DAMAGE_FIRST_PASS_RADIUS_EXTRA + radius) > 0)
		{
			Moby **hitMobies = CollMobysSphere_Fix_GetHitMobies();
			Moby *hitMoby;
			while ((hitMoby = *hitMobies++))
			{
				if (mobDoDamageTryHit(moby, hitMoby, p, isAoE, radius, damageFlags, amount))
				{
					result |= mobMobyProcessHitFlags(moby, hitMoby, amount, reactToThorns);
				}
			}
		}
	}

	return result;
}

//--------------------------------------------------------------------------
int mobDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE)
{
	VECTOR p, delta;
	MATRIX jointMtx;
	// struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int i;
	int result = 0;
	// float sqrRadius = radius * radius;
	float firstPassRadius = MOB_DAMAGE_FIRST_PASS_RADIUS_EXTRA + radius;
	float firstPassSqrRadius = powf(MOB_DAMAGE_FIRST_PASS_RADIUS_EXTRA + radius, 2);

	// get position of joint or moby position
	if (jointId < 0)
	{
		vector_copy(p, moby->Position);
	}
	else
	{
		mobyGetJointMatrix(moby, jointId, jointMtx);
		vector_copy(p, &jointMtx[12]);
	}

	// if no friendly fire just check hit on players
	// otherwise check all mobys
	if (!friendlyFire)
	{
		for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i)
		{
			Moby *otherTarget = mobOtherTargets[i];
			if (!otherTarget)
				break;

			float otherTargetRadius = otherTarget->BSphere[3] / 1024.0;
			vector_subtract(delta, otherTarget->Position, p);
			if ((vector_length(delta) - otherTargetRadius) > firstPassRadius)
				continue;

			if (mobDoDamageTryHit(moby, otherTarget, p, isAoE, radius, damageFlags, amount))
			{
				result |= mobMobyProcessHitFlags(moby, otherTarget, amount, reactToThorns);
			}
		}

		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			Player *player = playerGetFromIndex(i);
			if (!playerIsValid(player) || playerIsDead(player))
				continue;

			vector_subtract(delta, player->PlayerPosition, p);
			if (vector_sqrmag(delta) > firstPassSqrRadius)
				continue;

			if (mobDoDamageTryHit(moby, player->PlayerMoby, p, isAoE, radius, damageFlags, amount))
			{
				result |= mobMobyProcessHitFlags(moby, player->PlayerMoby, amount, reactToThorns);
			}
		}
	}
	else if (CollMobysSphere_Fix(p, COLLISION_FLAG_IGNORE_NONE, moby, NULL, MOB_DAMAGE_FIRST_PASS_RADIUS_EXTRA + radius) > 0)
	{
		Moby **hitMobies = CollMobysSphere_Fix_GetHitMobies();
		Moby *hitMoby;
		while ((hitMoby = *hitMobies++))
		{
			if (mobDoDamageTryHit(moby, hitMoby, p, isAoE, radius, damageFlags, amount))
			{
				result |= mobMobyProcessHitFlags(moby, hitMoby, amount, reactToThorns);
			}
		}
	}

	return result;
}

//--------------------------------------------------------------------------
void mobSetState(Moby *moby, int state)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// don't set if already state
	if (pvars->MobVars.State == state)
		return;

	// mark dirty if owner and mob wants state update
	if (mobAmIOwner(moby) && pvars->VTable && pvars->VTable->ShouldForceStateUpdateOnState && pvars->VTable->ShouldForceStateUpdateOnState(moby, state))
		pvars->MobVars.Dirty = 1;

	pvars->MobVars.LastStateId = pvars->MobVars.StateId++;
	pvars->MobVars.LastState = pvars->MobVars.State;

	// pass to mob handler
	if (pvars->VTable && pvars->VTable->ForceLocalState)
		pvars->VTable->ForceLocalState(moby, state);

	// pvars->MobVars.DynamicRandom = (char)rand(255);
}

//--------------------------------------------------------------------------
void mobTransAnimLerp(Moby *moby, int animId, int lerpFrames, float startOff)
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
void mobTransAnim(Moby *moby, int animId, float startOff)
{
	mobTransAnimLerp(moby, animId, 10, startOff);
}

//--------------------------------------------------------------------------
int mobIsProjectileComing(Moby *moby)
{
	VECTOR t;
	Moby *m = NULL;
	Moby **mobys = (Moby **)0x0026BDA0;

	while ((m = *mobys++))
	{
		if (!mobyIsDestroyed(m))
		{
			switch (m->OClass)
			{
			case MOBY_ID_B6_BALL0:
			case MOBY_ID_ARBITER_ROCKET0:
			case MOBY_ID_DUAL_VIPER_SHOT:
			case MOBY_ID_MINE_LAUNCHER_MINE:
			{
				// projectile is within 5 units
				vector_subtract(t, moby->Position, m->Position);
				if (vector_sqrmag(t) < (MOB_INCOMING_PROJECTILE_RADIUS * MOB_INCOMING_PROJECTILE_RADIUS))
					return 1;
				break;
			}
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int mobHasVelocity(struct MobPVar *pvars)
{
	VECTOR t;
	vector_projectonhorizontal(t, pvars->MobVars.MoveVars.Velocity);
	return vector_sqrmag(t) >= MOB_HAS_VELOCITY_THRESHOLD;
}

//--------------------------------------------------------------------------
// Shared PreUpdate logic common to all mobs:
//   - decrements local-player hit-inv timers (always, even while frozen)
//   - returns early if frozen
//   - decrements path tickers
//   - calls mobPreUpdate
void mobDefaultPreUpdate(Moby *moby)
{
	int i;
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// decrement tickers regardless of frozen state
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
		decTimerU16(&pvars->MobVars.LocalPlayerDamageHitInvTimer[i]);

	if (mobIsFrozen(moby))
		return;

	// decrement path target pos tickers
	decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);

	mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
// Shared OnLocalDamage logic common to all mobs:
// Enforces a per-local-player hit invincibility cooldown.
// Returns 1 to accept the damage, 0 to reject it.
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
// Shared flinch decision logic used in OnDamage handlers.
// Applies knockback, then decides whether to trigger a flinch or big-flinch
// state based on knockback force, shock flag, and a probability roll.
// canFlinch    : whether the mob is currently eligible to flinch
// isShock      : non-zero if the hit has the shock damage flag
// probability  : total probability [0,1] of triggering a flinch
// powerFactor  : probability [0,1] of escalating to big flinch
// flinchState : state id for the normal flinch
// bigFlinchState : state id for the big flinch
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
			mobSetState(moby, bigFlinchState);
		}
		else if (isShock)
		{
			mobSetState(moby, flinchState);
		}
		else if (randRange(0, 1) < probability)
		{
			if (randRange(0, 1) < powerFactor)
			{
				mobSetState(moby, bigFlinchState);
			}
			else
			{
				mobSetState(moby, flinchState);
			}
		}
	}
}

//--------------------------------------------------------------------------
void mobStand(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (!pvars)
		return;

	// remove horizontal velocity
	vector_projectonvertical(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
}

//--------------------------------------------------------------------------
void mobResetMoveStep(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	vector_copy(pvars->MobVars.MoveVars.NextPosition, moby->Position);
	pvars->MobVars.MoveVars.MoveSkipTicks = 0;
}

//--------------------------------------------------------------------------
int mobMoveCheck(Moby *moby, VECTOR outputPos, VECTOR from, VECTOR to)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	VECTOR delta, horizontalDelta;
	VECTOR hitTo, hitFrom;
	VECTOR hitToEx, hitNormal, hitToExBack;
	VECTOR up = {0, 0, 0, 0};
	if (!pvars)
		return 0;

	// get configured collision radius
	float collRadius = pvars->MobVars.Config.CollRadius;

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
	// vector_add(hitFrom, from, up);
	// vector_add(hitTo, to, up);

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
	vector_scale(hitToEx, hitToEx, collRadius * (1 + MOB_STUCK_EXPANSION_FACTOR * pvars->MobVars.MoveVars.StuckCounter));
	vector_add(hitTo, hitTo, hitToEx);
	vector_subtract(hitFrom, hitFrom, hitToExBack);

#if DEBUG_MOVE
	vector_copy(MoveCheckFrom, hitFrom);
	vector_copy(MoveCheckTo, hitTo);
#endif

	// check if we hit something
	if (CollLine_Fix(hitFrom, hitTo, collFlag, moby, NULL))
	{

		vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());

		// compute wall slope
		float slope = getSignedSlope(horizontalDelta, hitNormal);
		pvars->MobVars.MoveVars.WallSlope = maxf(pvars->MobVars.MoveVars.WallSlope, slope);
		pvars->MobVars.MoveVars.HitWall = 1;

		// check if we hit another mob
		Moby *hitMoby = pvars->MobVars.MoveVars.HitWallMoby = CollLine_Fix_GetHitMoby();
		if (hitMoby && mobyIsMob(hitMoby))
		{
			pvars->MobVars.MoveVars.HitWall = 0;
			pvars->MobVars.MoveVars.HitWallMoby = NULL;
		}

#if DEBUG_MOVE
		vector_copy(MoveCheckHit, CollLine_Fix_GetHitPosition());
#endif

		// if the hit point is before to
		// then we want to snap back before to
		// vector_subtract(hitTo, CollLine_Fix_GetHitPosition(), up);
		// vector_subtract(hitTo, hitTo, hitToEx);

		// stop if hit steep wall
		if (pvars->MobVars.MoveVars.WallSlope > (60 * MATH_DEG2RAD))
		{
			// vector_projectonhorizontal(hitToEx, hitToEx);
			// vector_subtract(outputPos, to, hitToEx);
#if DEBUG_MOVE
			DPRINTF("movecheck hit steep slope %f\n", pvars->MobVars.MoveVars.WallSlope * MATH_RAD2DEG);
#endif
			// return 2;
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

		// Distance threshold check: if we traveled a significant distance to reach this wall,
		// snap to the wall instead of sliding along it. This prevents large timesteps from
		// causing sideways movement when hitting distant obstacles (like the player).
		VECTOR fromToHit;
		vector_subtract(fromToHit, CollLine_Fix_GetHitPosition(), hitFrom);
		float distToHit = vector_length(fromToHit);
		float threshold = collRadius * 2.0f;

		if (distToHit > threshold)
		{
			// Far collision: snap to wall surface, don't slide
			vector_normalize(hitToEx, delta);
			vector_scale(hitToEx, hitToEx, collRadius);
			vector_subtract(outputPos, CollLine_Fix_GetHitPosition(), hitToEx);
		}
		else
		{
			// Close collision: use wall-slide as normal
			vector_scale(hitDir, hitDir, hitBitangentDotDelta * vector_length(delta));
			vector_add(outputPos, from, hitDir);
		}

		// VECTOR reflectedDelta;
		// vector_projectonhorizontal(hitToEx, hitToEx);
		// vector_reflect(reflectedDelta, hitToEx, hitNormal);
		// if (reflectedDelta[2] > delta[2])
		//   reflectedDelta[2] = delta[2];

		// vector_add(outputPos, to, reflectedDelta);

#if DEBUG_MOVE
		vector_copy(MoveCheckFinal, outputPos);
#endif
		return 1;
	}

	vector_copy(outputPos, to);
	return 0;
}

//--------------------------------------------------------------------------
void mobMove(Moby *moby)
{
	VECTOR targetVelocity;
	VECTOR normalizedVelocity;
	VECTOR nextPos;
	VECTOR temp;
	VECTOR groundCheckFrom, groundCheckTo;
	int isMovingDown = 0;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int isOwner = mobAmIOwner(moby);
	int moveStep = pvars->MobVars.MoveVars.LastMoveStep;

	u8 stuckCheckTicks = decTimerU8(&pvars->MobVars.MoveVars.StuckCheckTicks);
	decTimerU8(&pvars->MobVars.MoveVars.UngroundedTicks);
	u8 moveSkipTicks = decTimerU8(&pvars->MobVars.MoveVars.MoveSkipTicks);
	u8 slowTicks = decTimerU8(&pvars->MobVars.SlowTicks);

	// if LastMoveStep hasn't been initialized yet default to current move step
	if (moveStep == 0 && pvars->MobVars.MoveVars.MoveStep > 0)
		moveStep = pvars->MobVars.MoveVars.MoveStep;

#if DEBUG_MOVE
	VECTOR up = {0, 0, 1, 0};
	if (pvars->MobVars.Target)
	{
		VECTOR from, to, delta;
		vector_subtract(delta, pvars->MobVars.Target->Position, moby->Position);
		vector_add(from, moby->Position, up);
		vector_add(to, pvars->MobVars.Target->Position, up);
		if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
		{
			vector_copy(MoveTargetLineOfSightHit, CollLine_Fix_GetHitPosition());
		}
		else
		{
			vector_copy(MoveTargetLineOfSightHit, to);
		}
	}
#endif

	if (moveSkipTicks == 0)
	{

		// reset move step
		moveStep = pvars->MobVars.MoveVars.MoveStep;
		int rotatingDt = WRAP_DISTANCE(mobMoveCheckCollideWithOtherMobsRotatingIndex - pvars->MobVars.Order, MAX_MOBS_ALIVE) < 15;
		if (!isOwner || !rotatingDt)
			moveStep = MOB_MOVE_SKIP_TICKS_LOWPRIORITY;

#if GATE
		gateSetCollision(0);
#endif

		// move next position to last position
		// if first tick, NextPosition is initialized to spawn position in mode
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
			if (slowTicks > 0)
			{
				vector_scale(targetVelocity, targetVelocity, MOB_SHORT_FREEZE_SPEED_FACTOR);
			}

			// get horizontal normalized velocity
			vector_normalize(normalizedVelocity, targetVelocity);
			normalizedVelocity[2] = 0;

			// compute next position
			vector_add(nextPos, moby->Position, targetVelocity);

			// move physics check twice to prevent clipping walls
			if (mobMoveCheck(moby, nextPos, moby->Position, nextPos) == 1)
			{
				if (mobMoveCheck(moby, nextPos, moby->Position, nextPos))
				{
					// vector_copy(nextPos, moby->Position); // don't move
					// pvars->MobVars.MoveVars.IsStuck = 1;
				}
			}

			// check ground or ceiling
			isMovingDown = targetVelocity[2] <= 0.0001;
			if (isMovingDown)
			{
				vector_copy(groundCheckFrom, nextPos);
				groundCheckFrom[2] = maxf(moby->Position[2], nextPos[2]) + ZOMBIE_BASE_STEP_HEIGHT;
				vector_copy(groundCheckTo, nextPos);
				groundCheckTo[2] -= 0.5;
				if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
				{
					// mark grounded this frame
					pvars->MobVars.MoveVars.Grounded = 1;

					// check if we've hit death barrier
					if (isOwner)
					{
						int hitId = CollLine_Fix_GetHitCollisionId() & 0x0F;
						if (hitId == 0x4 || hitId == 0xb || hitId == 0x0d)
						{
							pvars->MobVars.Respawn = 1;
						}
					}

					// force position to above ground
					vector_copy(nextPos, CollLine_Fix_GetHitPosition());
					nextPos[2] += MOB_GROUND_SNAP_EPSILON;

#if DEBUG_MOVE
					vector_copy(MoveCheckDown, CollLine_Fix_GetHitPosition());
#endif

					// remove vertical velocity from velocity
					vector_projectonhorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
				}
			}
			else
			{
				vector_copy(groundCheckFrom, nextPos);
				groundCheckFrom[2] = moby->Position[2];
				vector_copy(groundCheckTo, nextPos);
				groundCheckTo[2] += MOB_CEILING_CHECK_HEIGHT;
				// groundCheckTo[2] += ZOMBIE_BASE_STEP_HEIGHT;
				if (CollLine_Fix(groundCheckFrom, groundCheckTo, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
				{
					// force position to below ceiling
					// vector_copy(nextPos, CollLine_Fix_GetHitPosition());
					// nextPos[2] -= 0.01;

#if DEBUG_MOVE
					vector_copy(MoveCheckUp, CollLine_Fix_GetHitPosition());
#endif

					vector_copy(nextPos, CollLine_Fix_GetHitPosition());
					nextPos[2] = maxf(moby->Position[2], groundCheckTo[2] - 3);

					// vector_copy(nextPos, moby->Position);

					// remove vertical velocity from velocity
					// vectorProjectOnHorizontal(pvars->MobVars.MoveVars.Velocity, pvars->MobVars.MoveVars.Velocity);
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
		if (pvars->MobVars.MoveVars.Velocity[2] < MOB_TERMINAL_VELOCITY * MATH_DT)
			pvars->MobVars.MoveVars.Velocity[2] = MOB_TERMINAL_VELOCITY * MATH_DT;

		// check if stuck by seeing if the sum horizontal delta position over the last second
		// is less than 1 in magnitude
		if (!stuckCheckTicks)
		{
			pvars->MobVars.MoveVars.StuckCheckTicks = MOB_STUCK_CHECK_INTERVAL_TICKS;
			pvars->MobVars.MoveVars.IsStuck = /* pvars->MobVars.MoveVars.HitWall && */ vector_length(pvars->MobVars.MoveVars.SumPositionDelta) < (pvars->MobVars.MoveVars.SumSpeedOver * MOB_STUCK_SPEED_THRESHOLD_FACTOR);
			// pvars->MobVars.MoveVars.IsStuck = 0;
			if (!pvars->MobVars.MoveVars.IsStuck)
			{
				pvars->MobVars.MoveVars.StuckJumpCount = 0;
				pvars->MobVars.MoveVars.StuckCounter = 0;
			}
			else
			{
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
	}
	else
	{

		float t = clamp(1 - (pvars->MobVars.MoveVars.MoveSkipTicks / (float)moveStep), 0, 1);
		vector_lerp(moby->Position, pvars->MobVars.MoveVars.LastPosition, pvars->MobVars.MoveVars.NextPosition, t);
	}

	// tell mob we want to jump
	// but check that we're moving towards the next node/target first before jumping
	VECTOR targetPos, mobyToTargetDelta;
	pathGetTargetPos(targetPos, moby);
	vector_subtract(mobyToTargetDelta, targetPos, moby->Position);
	if (vector_innerproduct(pvars->MobVars.MoveVars.Velocity, mobyToTargetDelta) > 0.5 && pathShouldJump(moby))
	{
		pvars->MobVars.MoveVars.QueueJumpSpeed = pathGetJumpSpeed(moby);
	}

	mobForceIntoMapBounds(moby);
}

//--------------------------------------------------------------------------
float mobTurnTowards(Moby *moby, VECTOR towards, float turnSpeed)
{
	VECTOR delta;

	if (!moby || !moby->PVar || fabsf(turnSpeed) < 0.001)
		return 0;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float radians = turnSpeed * pvars->MobVars.Config.Speed * MATH_DT;

	vector_subtract(delta, towards, moby->Position);
	float targetYaw = atan2f(delta[1], delta[0]);
	float yawDelta = clampAngle(targetYaw - moby->Rotation[2]);

	moby->Rotation[2] = clampAngle(moby->Rotation[2] + clamp(yawDelta, -radians, radians));

	return clampAngle(moby->Rotation[2] - targetYaw);
}

//--------------------------------------------------------------------------
float mobTurnTowardsPredictive(Moby *moby, Moby *target, float turnSpeed, float predictFactor)
{
	VECTOR pos;

	if (!moby || !moby->PVar || fabsf(turnSpeed) < 0.001)
		return 0;

	// if target is player, use their velocity to predict their future position
	Player *player = guberMobyGetPlayerDamager(target);
	if (player)
	{
		vector_scale(pos, player->Velocity, predictFactor);
		vector_add(pos, pos, player->PlayerPosition);
	}
	else
	{
		vector_copy(pos, target->Position);
	}

	return mobTurnTowards(moby, pos, turnSpeed);
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
float mobGetCurrentWalkAngle(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	Moby *target = pvars->MobVars.Target;
	float dir = 0;

	// if we have a target
	// we're near the target
	// and we're walking towards the target
	// walk at an angle
	if (target && vector_sqrdistance(target->Position, moby->Position) < (MOB_WALK_ANGLE_NEAR_TARGET_DIST * MOB_WALK_ANGLE_NEAR_TARGET_DIST) && vector_sqrdistance(pvars->MobVars.MoveVars.LastTargetPos, target->Position) < 1)
	{
		dir = ((u8)pvars->MobVars.DynamicRandom % 3) - 1;
	}

	// DPRINTF("td:%f wd:%f dir:%d=>%f\n", vector_sqrdistance(target->Position, moby->Position), vector_sqrdistance(target->Position, pvars->MobVars.MoveVars.LastTargetPos), (u8)pvars->MobVars.DynamicRandom, dir);
	return dir;
}

//--------------------------------------------------------------------------
void mobGetVelocityToTargetWithDirection(Moby *moby, VECTOR velocity, VECTOR from, VECTOR to, float yaw, float speed, float acceleration)
{
	VECTOR targetVelocity;
	VECTOR hVelocity;
	VECTOR fromToTarget;
	VECTOR next, nextToTarget;
	VECTOR temp;
	VECTOR targetPosition;
	float targetSpeed = speed * MATH_DT;
	float targetRadius = PLAYER_COLL_RADIUS;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (!pvars)
		return;

	if (pathUseTargetMoby(moby))
	{
		vector_copy(targetPosition, pvars->MobVars.Target->Position);
	}
	else
	{
		vector_copy(targetPosition, pvars->MobVars.TargetPosition);
	}

	Moby *target = pvars->MobVars.Target;
	if (target)
	{
		targetRadius = (target->BSphere[3] / 1024) * 1;
	}

	// target velocity from rotation
	vector_fromyaw(targetVelocity, yaw);

	// acclerate velocity towards target velocity
	// vector_normalize(targetVelocity, targetVelocity);
	vector_scale(targetVelocity, targetVelocity, targetSpeed);
	vector_subtract(temp, targetVelocity, velocity);
	vector_projectonhorizontal(temp, temp);
	vector_scale(temp, temp, acceleration * MATH_DT);
	vector_add(velocity, velocity, temp);

	// stop when at target
	if (targetSpeed > 0)
	{
		vector_subtract(fromToTarget, targetPosition, from);
		vector_projectonhorizontal(fromToTarget, fromToTarget);
		float distToTarget = vector_length(fromToTarget);

		float min = pvars->MobVars.Config.CollRadius + targetRadius;

		// Clamp velocity to prevent overshoot regardless of magnitude
		// If current velocity would cause the mob to overshoot past the stop-band,
		// scale it down so the next position lands exactly at min distance
		vector_projectonhorizontal(hVelocity, velocity);
		float speedThisFrame = vector_length(hVelocity);

		if (speedThisFrame > 0 && distToTarget > min && speedThisFrame > (distToTarget - min))
		{
			// Would overshoot — scale velocity down
			float scale = (distToTarget - min) / speedThisFrame;
			vector_scale(hVelocity, hVelocity, scale);
			vector_projectonvertical(velocity, velocity);
			vector_add(velocity, velocity, hVelocity);
			return;
		}

		// Original stop-band logic for fine-grained approach control
		vector_add(next, from, velocity);
		vector_subtract(nextToTarget, targetPosition, next);
		vector_projectonhorizontal(nextToTarget, nextToTarget);
		float distNextToTarget = vector_length(nextToTarget);

		float max = min + targetRadius; //(pvars->MobVars.Config.AttackRadius + PLAYER_COLL_RADIUS) + (targetSpeed * 0.2);

		// if too close to target, stop
		if (max > min && distNextToTarget < max && distNextToTarget > min)
		{
			float amt = (distNextToTarget - min) / (max - min);
			// vector_normalize(velocity, velocity);
			vector_projectonhorizontal(hVelocity, velocity);
			vector_scale(hVelocity, hVelocity, sqrtf(maxf(amt, 0)));
			vector_projectonvertical(velocity, velocity);
			vector_add(velocity, velocity, hVelocity);
			return;
		}
		else if (distNextToTarget < min)
		{
			vector_projectonvertical(velocity, velocity);
		}
	}

	if (targetSpeed <= 0)
	{
		vector_projectonvertical(velocity, velocity);
		return;
	}
}

//--------------------------------------------------------------------------
void mobGetVelocityToTarget(Moby *moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
	mobGetVelocityToTargetWithDirection(moby, velocity, from, to, moby->Rotation[2], speed, acceleration);
}

//--------------------------------------------------------------------------
void mobGetVelocityToTargetSimple(Moby *moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration)
{
	VECTOR targetVelocity;
	VECTOR temp;
	float targetSpeed = speed * MATH_DT;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (!pvars)
		return;

	// float collRadius = pvars->MobVars.Config.CollRadius + 0.5;

	// target velocity from rotation
	vector_subtract(targetVelocity, to, from);
	vector_normalize(targetVelocity, targetVelocity);

	// acclerate velocity towards target velocity
	vector_scale(targetVelocity, targetVelocity, targetSpeed);
	vector_subtract(temp, targetVelocity, velocity);
	vector_projectonhorizontal(temp, temp);
	vector_scale(temp, temp, acceleration * MATH_DT);
	vector_add(velocity, velocity, temp);

	if (targetSpeed <= 0)
	{
		vector_projectonvertical(velocity, velocity);
		return;
	}
}

//--------------------------------------------------------------------------
Moby *mobGetNextTarget(Moby *moby, float keepCurrentTargetFactor)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	int i;
	VECTOR delta;
	Moby *currentTarget = pvars->MobVars.Target;
	Moby *bestTargetMoby = NULL;
	float closestTargetDist = 100000;
	int targetAny = (pvars->MobVars.TargetingRule & MOB_TARGET_MASK_TARGET) == MOB_TARGET_BIT_ANY;
	int targetOther = (pvars->MobVars.TargetingRule & MOB_TARGET_MASK_TARGET) == MOB_TARGET_BIT_OTHER || targetAny;
	int targetPlayer = (pvars->MobVars.TargetingRule & MOB_TARGET_MASK_TARGET) == MOB_TARGET_BIT_PLAYER || targetAny;

	// check nearest other targets
	if (targetOther)
	{
		for (i = 0; i < MOB_MAX_OTHER_TARGETS; ++i)
		{
			Moby *otherTarget = mobOtherTargets[i];
			if (!otherTarget)
				break;
			if (otherTarget->OClass != DUMMY_OCLASS)
				break;

			struct DummyPVar *otherPVars = (struct DummyPVar *)otherTarget->PVar;
			if ((!currentTarget || currentTarget == otherTarget) && otherPVars->Config.MobTargetType == DUMMY_MOB_AGGRO_ALWAYS)
			{
				return otherTarget;
			}

			vector_subtract(delta, otherTarget->Position, moby->Position);
			float dist = vector_length(delta);
			float maxDist = otherPVars->Config.MobTargetDistance * otherPVars->Config.MobTargetDistance;
			if (otherPVars->Config.MobTargetType == DUMMY_MOB_AGGRO_IN_RANGE && dist < maxDist)
			{
				bestTargetMoby = otherTarget;
				closestTargetDist = dist;
			}
		}
	}

	// check nearest players
	if (targetPlayer)
	{
		for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		{
			Player *player = playerGetFromIndex(i);
			if (player && player->SkinMoby && !playerIsDead(player) && player->Health > 0 && player->SkinMoby->Opacity >= 0x80)
			{
				vector_subtract(delta, player->PlayerPosition, moby->Position);
				float dist = vector_length(delta);

				if (dist < 300)
				{
					Moby *pTargetMoby = playerGetTargetMoby(player);

					// don't target players that are in jump pad state
					// unless we're already targeting them
					if (player->PlayerState == PLAYER_STATE_MOON_JUMP && pTargetMoby != currentTarget)
						continue;

					// favor existing target
					if (pTargetMoby == currentTarget)
						dist *= (1.0 / keepCurrentTargetFactor);

					// pick closest target
					if (dist < closestTargetDist)
					{

						// confirm we can walk to target
						if (pathHasRouteToTarget(moby, pTargetMoby))
						{
							bestTargetMoby = pTargetMoby;
							closestTargetDist = dist;
						}
					}
				}
			}
		}
	}

	return bestTargetMoby;
}

//--------------------------------------------------------------------------
void mobMoveTowards(Moby *moby, VECTOR targetPosition, float speed, float turnSpeed, float acceleration, float curveNearTargetDir)
{
	VECTOR t, t2;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	vector_subtract(t, targetPosition, moby->Position);
	float dist = vector_length(t);
	if (dist < 10.0 && fabsf(curveNearTargetDir) > 0.001)
	{
		mobAlterTarget(t2, moby, t, clamp(dist, 0, 10) * 0.3 * curveNearTargetDir);
		vector_add(t, t, t2);
	}

	// vector_scale(t, t, 1 / dist);
	vector_add(t, moby->Position, t);

	float deltaYaw = 0;
	if (pvars->MobVars.MoveVars.IsStuck)
	{
		deltaYaw = mobTurnTowards(moby, targetPosition, turnSpeed);
	}
	else
	{
		deltaYaw = mobTurnTowards(moby, t, turnSpeed);
	}

	// DYAW > 0DEG
	float yawLerpT = fabsf(deltaYaw) / MATH_PI;
	if (yawLerpT > 0)
	{
		float v = (yawLerpT + 0.0) * 1.0;
		// v = powf(v, 1 / speed);
		v = 1 - powf(clamp(v, 0, 1), 2);
		// printf("v:%f speed:%f\n", v, speed);
		speed *= v;
	}

	mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, speed, acceleration);
}

//--------------------------------------------------------------------------
void mobJumpTowards(Moby *moby, VECTOR targetPosition)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float speedCurve = lerpf(0, 1, clamp(pvars->MobVars.CurrentStateForTicks / (float)TPS, 0, 1));
	if (pvars->MobVars.MoveVars.Velocity[2] < 0)
	{
		mobMoveTowards(moby, targetPosition, MOB_JUMP_MOVE_SPEED * 0.5, 45 * MATH_DEG2RAD, 10, 0);
	}
	else
	{
		mobMoveTowards(moby, targetPosition, MOB_JUMP_MOVE_SPEED, 180 * MATH_DEG2RAD, 10 * speedCurve, 0);
	}
}

//--------------------------------------------------------------------------
int mobHitWallShouldJump(Moby *moby, float maxSlope)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// must be grounded
	// must hit wall steeper than max slope
	// ignore case where we hit the target (since we want to reach the target)
	return pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > maxSlope && (!pvars->MobVars.MoveVars.HitWallMoby || pvars->MobVars.MoveVars.HitWallMoby != pvars->MobVars.Target);
}

//--------------------------------------------------------------------------
void mobPostDrawQuad(Moby *moby, float scale, u32 color, int jointId)
{
	struct QuadDef quad;
	float size = scale * mobGetScaleMultiplier(moby);
	MATRIX m2;
	VECTOR pTL = {0, size, size, 1};
	VECTOR pTR = {0, -size, size, 1};
	VECTOR pBL = {0, size, -size, 1};
	VECTOR pBR = {0, -size, -size, 1};
	VECTOR offset = {0, 0, size};
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	if (!pvars)
		return;

	// u32 color = MobLODColors[pvars->MobVars.Config.MobType] | (moby->Opacity << 24);

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
	quad.VertexUVs[0] = (struct UV){0, 0};
	quad.VertexUVs[1] = (struct UV){1, 0};
	quad.VertexUVs[2] = (struct UV){0, 1};
	quad.VertexUVs[3] = (struct UV){1, 1};
	quad.Clamp = 0x0000000100000001;
	quad.Tex0 = gfxGetFrameTex(MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteTexId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera *camera = cameraGetGameCamera(0);
	if (!camera)
		return;

	// set world matrix by joint
	// mobyGetJointMatrix(moby, jointId, m2);

	// memcpy(m2, moby->M0_03, sizeof(VECTOR) * 3);
	memcpy(m2, camera->uMtx, sizeof(VECTOR) * 3);
	vector_add(&m2[12], moby->Position, offset);

	// draw
	gfxDrawQuad((void *)0x00222590, &quad, m2, 1);
}

#if DEBUG

//--------------------------------------------------------------------------
void mobPostDrawDebug(Moby *moby)
{
#if PRINT_JOINTS
	MATRIX jointMtx;
	int i = 0;
	char buf[32];
	int animJointCount = 0;

	// get anim joint count
	void *pclass = moby->PClass;
	if (pclass)
	{
		animJointCount = **(u32 **)((u32)pclass + 0x1C);
	}

	for (i = 0; i < animJointCount; ++i)
	{
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
void mobPreUpdate(Moby *moby)
{
#if DEBUG
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&mobPostDrawDebug, moby);
#endif
}

//--------------------------------------------------------------------------
void mobOnSpawned(Moby *moby)
{
}

//--------------------------------------------------------------------------
void mobOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e)
{
	// update pathfinding state
	pathSetPath(moby, e->PathStartNodeIdx, e->PathEndNodeIdx, e->PathCurrentEdgeIdx, e->PathHasReachedStart, e->PathHasReachedEnd);
}

//--------------------------------------------------------------------------
int mobCreate(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config)
{
	struct MobSpawnEventArgs args;
	struct MobSpawnParams *spawnParams = &MapConfig.DefaultSpawnParams[spawnParamsIdx];
	int extraDataSize = 0;

	// get extra data size
	if (spawnParams->MobVTable->GetExtraDataSize)
	{
		extraDataSize = spawnParams->MobVTable->GetExtraDataSize(spawnParamsIdx);
	}

	// create guber object
	GuberEvent *guberEvent = 0;
	guberMobyCreateSpawned(spawnParams->OClass, sizeof(struct MobPVar) + extraDataSize, &guberEvent, NULL);
	if (guberEvent)
	{
		if (MapConfig.Functions.ModePopulateSpawnArgsFunc)
		{
			MapConfig.Functions.ModePopulateSpawnArgsFunc(&args, config, spawnParamsIdx, spawnFromUID == -1, spawnFlags);
		}

		u8 random = (u8)rand(100);
		position[2] += 1; // spawn slightly above point

		// give mob change to make last minute changes
		if (spawnParams->MobVTable->OnSpawning)
		{
			spawnParams->MobVTable->OnSpawning(spawnParamsIdx, position, &yaw, &spawnFromUID, &spawnFlags, &random, &args);
		}

		// pack position to 16bits per axis
		// we assume position is always between 0 and 1024 on each axis (moby grid limits)
		u16 pos16[3] = {
				(u16)(position[0] * 64),
				(u16)(position[1] * 64),
				(u16)(position[2] * 64)};
		guberEventWrite(guberEvent, pos16, 2 * 3);
		// guberEventWrite(guberEvent, position, 12);
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
	MapConfig.Functions.OnMobSpawnedFunc = &mobOnSpawned;
}

//--------------------------------------------------------------------------
void mobTick(void)
{
	if (MapConfig.State && MapConfig.State->MobStats.TotalAlive > 0)
		mobMoveCheckCollideWithOtherMobsRotatingIndex = (mobMoveCheckCollideWithOtherMobsRotatingIndex + 1) % MapConfig.State->MobStats.TotalAlive;
	else
		mobMoveCheckCollideWithOtherMobsRotatingIndex = (mobMoveCheckCollideWithOtherMobsRotatingIndex + 1) % MAX_MOBS_ALIVE;

	// flip targets
	memcpy(mobOtherTargets, mobOtherTargets2, sizeof(mobOtherTargets));
	memset(mobOtherTargets2, 0, sizeof(mobOtherTargets2));
}
