#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include <libdl/color.h>
#include <libdl/dialog.h>
#include <libdl/utils.h>

#include "game.h"
#include "mob.h"
#include "utils.h"
#include "maputils.h"
#include "shared.h"

void reactorPreUpdate(Moby *moby);
void reactorPostUpdate(Moby *moby);
void reactorPostDraw(Moby *moby);
void reactorMove(Moby *moby);
int reactorGetExtraDataSize(int spawnParamsIdx);
void reactorOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
void reactorOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
void reactorOnDestroy(Moby *moby, int killedByPlayerId, int weaponId);
void reactorOnDamage(Moby *moby, struct MobDamageEventArgs *e);
int reactorOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void reactorOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e);
int reactorOnRespawn(Moby *moby);
void reactorOnCustomEvent(Moby *moby, GuberEvent *e);
Moby *reactorGetNextTarget(Moby *moby);
int reactorGetPreferredAction(Moby *moby, int *delayTicks);
void reactorDoAction(Moby *moby);
void reactorDoChargeDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void reactorDoSmashDamage(Moby *moby, float radius, float amount, int damageFlags);
void reactorDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
void reactorForceLocalAction(Moby *moby, int action);
short reactorGetArmor(Moby *moby);
int reactorIsAttacking(Moby *moby);
int reactorCanNonOwnerTransitionToAction(Moby *moby, int action);
int reactorShouldForceStateUpdateOnAction(Moby *moby, int action);

int reactorIsWalkingOrIdle(struct MobPVar *pvars);
int reactorIsSpawning(struct MobPVar *pvars);
int reactorCanAttack(struct MobPVar *pvars, enum ReactorAction action);
int reactorIsFlinching(Moby *moby);
void reactorFireTrailshot(Moby *moby);

void reactorOnReceivePlayDialog(Moby *moby, GuberEvent *e);
void reactorPlayDialog(Moby *moby, short dialogId);

void reactorDoSmashEffect(Moby *moby, float radius);
void trailshotSpawn(Moby *creatorMoby, VECTOR position, VECTOR velocity, u32 color, float damage, int lifeTicks);

int mapSpawnMob(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags);

struct MobVTable ReactorVTable = {
		.PreUpdate = &reactorPreUpdate,
		.PostUpdate = &reactorPostUpdate,
		.PostDraw = &reactorPostDraw,
		.Move = &reactorMove,
		.GetExtraDataSize = &reactorGetExtraDataSize,
		.OnSpawning = &reactorOnSpawning,
		.OnSpawn = &reactorOnSpawn,
		.OnDestroy = &reactorOnDestroy,
		.OnDamage = &reactorOnDamage,
		.OnLocalDamage = &reactorOnLocalDamage,
		.OnStateUpdate = &reactorOnStateUpdate,
		.OnRespawn = &reactorOnRespawn,
		.OnCustomEvent = &reactorOnCustomEvent,
		.GetNextTarget = &reactorGetNextTarget,
		.GetPreferredAction = &reactorGetPreferredAction,
		.ForceLocalAction = &reactorForceLocalAction,
		.DoAction = &reactorDoAction,
		.DoDamage = &reactorDoDamage,
		.GetArmor = &reactorGetArmor,
		.IsAttacking = &reactorIsAttacking,
		.CanNonOwnerTransitionToAction = &reactorCanNonOwnerTransitionToAction,
		.ShouldForceStateUpdateOnAction = &reactorShouldForceStateUpdateOnAction,
};

const int reactorOnSpawnDialogIds[] = {
		DIALOG_ID_REACTOR_AFTER_THIS_YOU_GONNA_FEEL_DEAD_TIRED,
		DIALOG_ID_REACTOR_WHEN_IM_DONE_WITH_YOU_YOU_GONNA_LOOK_LIKE_ROADKILL,
		DIALOG_ID_REACTOR_IM_GONNA_PUT_YOU_IN_A_WORLD_OF_HURT};

const int reactorOnDamageDialogIds[] = {
		DIALOG_ID_REACTOR_COME_ON_THATS_ALL_YOU_GOT,
		DIALOG_ID_REACTOR_AH_THAT_AINT_FUNNY,
};

const int reactorOnReachHalfDamageDialogIds[] = {
		DIALOG_ID_REACTOR_OOOH_OKAY_PLAY_TIMES_OVER_RAT_BOY_TIME_TO_DIE,
		DIALOG_ID_REACTOR_AW_DAMN_NOW_YOU_GOT_ON_MY_BAD_SIDE};

const int reactorOnDamagePlayerDialogIds[] = {
		DIALOG_ID_REACTOR_HERO_HAH_HERO_MY_TIN_BUTT,
		DIALOG_ID_REACTOR_TADAOW_HOW_YOU_LIKE_ME_NOW,
		DIALOG_ID_REACTOR_BOOYAH,
		DIALOG_ID_REACTOR_THIS_IS_MY_HOUSE,
};

Moby *reactorActiveMoby = NULL;

//--------------------------------------------------------------------------
void reactorTransAnim(Moby *moby, int animId, float startOff)
{
	mobTransAnim(moby, animId, startOff);
}

//--------------------------------------------------------------------------
void reactorPreUpdate(Moby *moby)
{
	int i;
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;

	// decrement tickers regardless of frozen state
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
		decTimerU16(&pvars->MobVars.LocalPlayerDamageHitInvTimer[i]);

	if (mobIsFrozen(moby))
		return;

	// update react vars
	((void (*)(Moby *))0x0051b860)(moby);

	// decrement path target pos ticker
	decTimerU8(&pvars->MobVars.MoveVars.PathTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathCheckSkipEndTicks);
	decTimerU8(&pvars->MobVars.MoveVars.PathNewTicks);
	decTimerU16(&reactorVars->AttackChargeCooldownTicks);
	decTimerU16(&reactorVars->AttackShotWithTrailCooldownTicks);
	decTimerU16(&reactorVars->AttackSmashCooldownTicks);
	decTimerU16(&reactorVars->DialogCooldownTicks);
	mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void reactorPostUpdate(Moby *moby)
{
	MATRIX mtx;
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	float scale = mobGetScaleMultiplier(moby);

	// adjust animSpeed by speed and by animation
	float animSpeed = reactorVars->AnimSpeedAdditive + 0.6 * (pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / scale;
	if (moby->AnimSeqId == REACTOR_ANIM_JUMP_UP)
	{
		animSpeed = 1 * (1 - powf(moby->AnimSeqT / 21, 1));
		if (pvars->MobVars.MoveVars.Grounded)
		{
			animSpeed = 0.6;
		}
	}
	else if (reactorIsFlinching(moby) && !pvars->MobVars.MoveVars.Grounded)
	{
		animSpeed = 0.5 * (1 - powf(moby->AnimSeqT / 20, 2));
	}
	else if (pvars->MobVars.Action == REACTOR_ACTION_ATTACK_CHARGE)
	{
		animSpeed = reactorVars->AnimSpeedAdditive + 0.9;
	}
	else if (pvars->MobVars.Action == REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL)
	{
		animSpeed = reactorVars->AnimSpeedAdditive + 1.1;
	}
	else if (pvars->MobVars.Action == REACTOR_ACTION_ATTACK_SMASH)
	{
		animSpeed = reactorVars->AnimSpeedAdditive + 0.6;
	}

	// stop animation if frozen or not visible and just walking
	// we do want to make sure the mob animated when attacking even if not visible
	if (mobIsFrozen(moby) || (moby->DrawDist == 0 && pvars->MobVars.Action == REACTOR_ACTION_WALK))
	{
		moby->AnimSpeed = 0;
	}
	else
	{
		moby->AnimSpeed = animSpeed;
	}

	// snap particle to joint
	int showPrepShotParticle = pvars->MobVars.Action == REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL && !reactorVars->HasFiredTrailshotThisLoop;
	if (reactorVars->PrepShotWithFireParticleMoby1)
	{
		mobyGetJointMatrix(moby, 3, mtx);
		vector_copy(reactorVars->PrepShotWithFireParticleMoby1->Position, &mtx[12]);
		reactorVars->PrepShotWithFireParticleMoby1->UpdateDist = showPrepShotParticle ? 0x80 : 0;
	}

	if (reactorVars->PrepShotWithFireParticleMoby2)
	{
		mobyGetJointMatrix(moby, 4, mtx);
		vector_copy(reactorVars->PrepShotWithFireParticleMoby2->Position, &mtx[12]);
		reactorVars->PrepShotWithFireParticleMoby2->UpdateDist = showPrepShotParticle ? 0x80 : 0;
	}
}

//--------------------------------------------------------------------------
void reactorPostDraw(Moby *moby)
{
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	u32 color = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].SpriteColor | (moby->Opacity << 24);
	mobPostDrawQuad(moby, 2.3, color, 1);
}

//--------------------------------------------------------------------------
void reactorAlterTarget(VECTOR out, Moby *moby, VECTOR forward, float amount)
{
	VECTOR up = {0, 0, 1, 0};

	vector_outerproduct(out, forward, up);
	vector_normalize(out, out);
	vector_scale(out, out, amount);
}

//--------------------------------------------------------------------------
void reactorMove(Moby *moby)
{
	MATRIX m;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;

	// backup current hips position before move
	mobyGetJointMatrix(moby, REACTOR_SUBSKELETON_JOINT_HIPS, m);
	vector_copy(reactorVars->LastHipsPosition, &m[12]);
	mobyGetJointMatrix(moby, REACTOR_SUBSKELETON_JOINT_RIGHT_HAND, m);
	vector_copy(reactorVars->LastRightHandPosition, &m[12]);

	mobMove(moby);
}

//--------------------------------------------------------------------------
int reactorGetExtraDataSize(int spawnParamsIdx)
{
	return sizeof(ReactorMobVars_t);
}

//--------------------------------------------------------------------------
void reactorOnSpawning(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args)
{
	// scale health by # players if boss
	if (MapConfig.State && args->MobAttribute == MOB_ATTRIBUTE_BOSS)
	{
		args->StartHealth += MapConfig.State->ActivePlayerCount * REACTOR_ADD_HEALTH_PER_PLAYER;
	}
}

//--------------------------------------------------------------------------
void reactorOnSpawn(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e)
{
	MATRIX m;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	memset(pvars->AdditionalMobVarsPtr, 0, sizeof(ReactorMobVars_t));
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	float scale = mobGetScaleMultiplier(moby);

	// set scale
	moby->Scale = 0.6 * scale;

	// colors by mob type
	moby->GlowRGBA = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].GlowColor;
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// targeting
	pvars->TargetVars.targetHeight = 1.0 + (scale * 0.5);
	pvars->MobVars.BlipType = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BlipType;

	// move step
	pvars->MobVars.MoveVars.MoveStep = 1;

	// default cooldowns
	reactorVars->AttackSmashCooldownTicks = randRangeInt(REACTOR_CHARGE_ATTACK_MIN_COOLDOWN_TICKS, REACTOR_CHARGE_ATTACK_MAX_COOLDOWN_TICKS);
	reactorVars->AttackShotWithTrailCooldownTicks = randRangeInt(REACTOR_SHOT_WITH_TRAIL_ATTACK_MIN_COOLDOWN_TICKS, REACTOR_SHOT_WITH_TRAIL_ATTACK_MAX_COOLDOWN_TICKS);
	reactorVars->AttackChargeCooldownTicks = randRangeInt(REACTOR_CHARGE_ATTACK_MIN_COOLDOWN_TICKS, REACTOR_CHARGE_ATTACK_MAX_COOLDOWN_TICKS);

	reactorVars->HealthLastDialog = pvars->MobVars.Config.Health;

	mobyGetJointMatrix(moby, REACTOR_SUBSKELETON_JOINT_HIPS, m);
	vector_copy(reactorVars->LastHipsPosition, &m[12]);

	// create particle mobys
	reactorVars->PrepShotWithFireParticleMoby1 = mobySpawn(0x1BC6, 0);
	if (reactorVars->PrepShotWithFireParticleMoby1)
	{
		reactorVars->PrepShotWithFireParticleMoby1->UpdateDist = 0xFF;
		reactorVars->PrepShotWithFireParticleMoby1->GlowRGBA = 0x80C06020;
	}

	reactorVars->PrepShotWithFireParticleMoby2 = mobySpawn(0x1BC6, 0);
	if (reactorVars->PrepShotWithFireParticleMoby2)
	{
		reactorVars->PrepShotWithFireParticleMoby2->UpdateDist = 0xFF;
		reactorVars->PrepShotWithFireParticleMoby2->GlowRGBA = 0x80C06020;
	}

	reactorActiveMoby = moby;

	// play spawn sound
	reactorPlayDialog(moby, reactorOnSpawnDialogIds[rand(COUNT_OF(reactorOnSpawnDialogIds))]);
}

//--------------------------------------------------------------------------
void reactorOnDestroy(Moby *moby, int killedByPlayerId, int weaponId)
{
	VECTOR expOffset;
	if (!moby || !moby->PVar)
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	reactorActiveMoby = NULL;

	// set colors before death so that the corn has the correct color
	moby->PrimaryColor = MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BaseColor;

	// destroy particle mobys
	if (reactorVars->PrepShotWithFireParticleMoby1)
	{
		mobyDestroy(reactorVars->PrepShotWithFireParticleMoby1);
		reactorVars->PrepShotWithFireParticleMoby1 = NULL;
	}

	if (reactorVars->PrepShotWithFireParticleMoby2)
	{
		mobyDestroy(reactorVars->PrepShotWithFireParticleMoby2);
		reactorVars->PrepShotWithFireParticleMoby2 = NULL;
	}

	// spawn explosion
	u32 expColor = 0x801E70D6;
	vector_copy(expOffset, moby->Position);
	expOffset[2] += pvars->TargetVars.targetHeight;
	u128 expPos = vector_read(expOffset);
	mobySpawnExplosion(expPos, 0, 0, 0, 0, 16, 0, 16, 0, 1, 0, 0, 0, 0,
										 0, 0, expColor, expColor, expColor, expColor, expColor, expColor, expColor, expColor,
										 0, 0, 0, 0, 0, 1, 0, 0, 0);

	mobyPlaySoundByClass(0, 0, moby, MOBY_ID_ARBITER_ROCKET0);
}

//--------------------------------------------------------------------------
void reactorOnDamage(Moby *moby, struct MobDamageEventArgs *e)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	float damage = e->DamageQuarters / 4.0;
	int canFlinch = pvars->MobVars.Action != REACTOR_ACTION_FLINCH && pvars->MobVars.Action != REACTOR_ACTION_BIG_FLINCH && pvars->MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
	canFlinch = 1;
#endif

	int isShock = e->DamageFlags & 0x40;
	int isShortFreeze = e->DamageFlags & 0x40000000;

	// take more damage in crouch state
	if (pvars->MobVars.Action == REACTOR_ACTION_ATTACK_CHARGE && moby->AnimSeqId == REACTOR_ANIM_KNEE_DOWN)
	{
		e->DamageQuarters *= 2;
		damage *= 2;
	}

	// destroy
	float newHp = pvars->MobVars.Health - damage;
	if (newHp <= 0)
	{
		reactorForceLocalAction(moby, REACTOR_ACTION_DIE);
		pvars->MobVars.LastHitBy = e->SourceUID;
		pvars->MobVars.LastHitByOClass = e->SourceOClass;
	}
	else
	{

		// increase odds of playing damage dialog as amount of damage taken increases since last dialog
		// or if we reach half health stage play respective dialog
		float halfMaxHealth = pvars->MobVars.Config.MaxHealth * 0.5;
		float probability = 0.5 * ((reactorVars->HealthLastDialog - newHp) / pvars->MobVars.Config.MaxHealth);
		if (newHp <= halfMaxHealth && pvars->MobVars.Health > halfMaxHealth)
		{
			playDialog(reactorOnReachHalfDamageDialogIds[rand(COUNT_OF(reactorOnReachHalfDamageDialogIds))], 1);
			reactorVars->HealthLastDialog = newHp;
		}
		else if (randRange(0, 1) < probability)
		{
			reactorPlayDialog(moby, reactorOnDamageDialogIds[rand(COUNT_OF(reactorOnDamageDialogIds))]);
			reactorVars->HealthLastDialog = newHp;
		}
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
		float powerFactor = REACTOR_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;
		float probability = clamp((damageRatio * REACTOR_FLINCH_PROBABILITY) + powerFactor, 0, MOB_MAX_FLINCH_PROBABILITY);

#if ALWAYS_FLINCH
		probability = 2;
		powerFactor = 2;
#endif

		if (canFlinch)
		{
			if (e->Knockback.Force)
			{
				mobSetAction(moby, REACTOR_ACTION_BIG_FLINCH);
			}
			else if (isShock)
			{
				mobSetAction(moby, REACTOR_ACTION_FLINCH);
			}
			else if (randRange(0, 1) < probability)
			{
				if (randRange(0, 1) < powerFactor)
				{
					mobSetAction(moby, REACTOR_ACTION_BIG_FLINCH);
				}
				else
				{
					mobSetAction(moby, REACTOR_ACTION_FLINCH);
				}
			}
		}
	}

	// short freeze
	if (isShortFreeze && pvars->MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS)
	{
		pvars->MobVars.SlowTicks = MOB_SHORT_FREEZE_DURATION_TICKS;
		mobResetMoveStep(moby);
	}
}

//--------------------------------------------------------------------------
int reactorOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e)
{
	// we want to give each local player a cooldown on damage they can apply to reactor
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
void reactorOnStateUpdate(Moby *moby, struct MobStateUpdateEventArgs *e)
{
	mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
int reactorOnRespawn(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// don't let mode destroy and respawn
	// manually teleport reactor to random spawn point
	if (MapConfig.Functions.ModeSpawnGetRandomPointFunc)
	{
		VECTOR p;
		MapConfig.Functions.ModeSpawnGetRandomPointFunc(p, &MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx]);
		vector_copy(moby->Position, p);
		vector_copy(pvars->MobVars.MoveVars.NextPosition, p);
		vector_copy(pvars->MobVars.MoveVars.LastPosition, p);
		vector_write(pvars->MobVars.MoveVars.Velocity, 0);
		return 0;
	}

	return 1;
}

//--------------------------------------------------------------------------
void reactorOnCustomEvent(Moby *moby, GuberEvent *e)
{
	int customEventId;

	if (!moby)
		return;
	if (!e)
		return;

	guberEventRead(e, &customEventId, sizeof(customEventId));

	switch (customEventId)
	{
	case REACTOR_CUSTOM_EVENT_PLAY_DIALOG:
	{
		reactorOnReceivePlayDialog(moby, e);
		break;
	}
	}
}

//--------------------------------------------------------------------------
void reactorSendCustomEvent(Moby *moby, int customEventId, void *payload, int size)
{
	// get guber object
	Guber *guber = guberGetObjectByMoby(moby);
	if (guber)
	{

		// create event
		GuberEvent *event = guberEventCreateEvent(guber, MOB_EVENT_CUSTOM, 0, 0);
		if (event)
		{
			guberEventWrite(event, &customEventId, sizeof(customEventId));
			if (payload)
				guberEventWrite(event, payload, size);
		}
	}
}

//--------------------------------------------------------------------------
Moby *reactorGetNextTarget(Moby *moby)
{
	return mobGetNextTarget(moby, REACTOR_TARGET_KEEP_CURRENT_FACTOR);
}

//--------------------------------------------------------------------------
int reactorGetPreferredAction(Moby *moby, int *delayTicks)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	VECTOR mobyPosUp, targetPosUp;
	VECTOR up = {0, 0, 0.5, 0};

	// no preferred action
	if (reactorIsAttacking(moby))
		return -1;

	if (reactorIsSpawning(pvars))
		return -1;

	if (reactorIsFlinching(moby))
		return -1;

	if (pvars->MobVars.Action == REACTOR_ACTION_GLOAT)
	{
		return -1;
	}

	if (pvars->MobVars.Action == REACTOR_ACTION_JUMP && !pvars->MobVars.MoveVars.Grounded)
	{
		return REACTOR_ACTION_WALK;
	}

	// jump if we've hit a slope and are grounded
	if (pvars->MobVars.MoveVars.Grounded && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > REACTOR_MAX_WALKABLE_SLOPE)
	{
		return REACTOR_ACTION_JUMP;
	}

	// jump if we've hit a jump point on the path
	if (pvars->MobVars.MoveVars.QueueJumpSpeed)
	{
		return REACTOR_ACTION_JUMP;
	}

	// get next target
	Moby *target = reactorGetNextTarget(moby);
	if (target)
	{

		// prevent action changing too quickly
		if (pvars->MobVars.AttackCooldownTicks)
		{
			return REACTOR_ACTION_WALK;
		}

		float dist = mobGetDistanceToTarget(moby, target);
		float attackRadius = pvars->MobVars.Config.AttackRadius;

		// if we're on top of the target then step away
		if (dist < (REACTOR_BASE_COLL_RADIUS))
		{
			return REACTOR_ACTION_WALK;
		}

		// near then swing
		if (dist <= attackRadius && reactorCanAttack(pvars, REACTOR_ACTION_ATTACK_SWING))
		{
			if (delayTicks)
				*delayTicks = pvars->MobVars.Config.ReactionTickCount;
			return REACTOR_ACTION_ATTACK_SWING;
		}

		// wait to ground before we do the special attacks
		if (pvars->MobVars.MoveVars.Grounded)
		{

			vector_add(mobyPosUp, moby->Position, up);
			vector_add(targetPosUp, target->Position, up);
			int targetInSight = !CollLine_Fix(mobyPosUp, targetPosUp, COLLISION_FLAG_IGNORE_DYNAMIC, NULL, NULL);
			if (!targetInSight)
			{
				return REACTOR_ACTION_WALK;
			}

			// near but not for swing then charge
			if (dist <= (REACTOR_MAX_DIST_FOR_CHARGE) && rand(5) == 0 && reactorCanAttack(pvars, REACTOR_ACTION_ATTACK_CHARGE))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return REACTOR_ACTION_ATTACK_CHARGE;
			}

			// far but in sight, shoot with trail
			if (dist <= (REACTOR_SHOT_WITH_TRAIL_MAX_DIST) && rand(5) == 0 && reactorCanAttack(pvars, REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL;
			}

			// smesh
			if (reactorCanAttack(pvars, REACTOR_ACTION_ATTACK_SMASH))
			{
				if (delayTicks)
					*delayTicks = pvars->MobVars.Config.ReactionTickCount;
				return REACTOR_ACTION_ATTACK_SMASH;
			}
		}

		return REACTOR_ACTION_WALK;
	}

	return REACTOR_ACTION_IDLE;
}

//--------------------------------------------------------------------------
void reactorDoAction(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	Moby *target = pvars->MobVars.Target;
	VECTOR t;
	u32 damageFlags = 0x00081801;
	int walkBackwards = 0;
	float difficulty = 1;
	float turnSpeed = pvars->MobVars.MoveVars.Grounded ? REACTOR_TURN_RADIANS_PER_SEC : REACTOR_TURN_AIR_RADIANS_PER_SEC;
	float acceleration = pvars->MobVars.MoveVars.Grounded ? REACTOR_MOVE_ACCELERATION : REACTOR_MOVE_AIR_ACCELERATION;
	int isInAirFromFlinching = !pvars->MobVars.MoveVars.Grounded && (pvars->MobVars.LastAction == REACTOR_ACTION_FLINCH || pvars->MobVars.LastAction == REACTOR_ACTION_BIG_FLINCH);

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// MATRIX *joints = (MATRIX *)moby->JointCache;
	// static int asd = 3;
	// printf("a:%d id:%d f:%d j:%d: ", pvars->MobVars.Action, moby->AnimSeqId, moby->AnimFlags, asd); vector_print(&joints[asd][12]); printf("\n");

	// reset anim speed add
	reactorVars->AnimSpeedAdditive = 0;

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

	//
	switch (pvars->MobVars.Action)
	{
	case REACTOR_ACTION_SPAWN:
	{
		reactorTransAnim(moby, REACTOR_ANIM_JUMP_UP, 0);
		mobStand(moby);
		break;
	}
	case REACTOR_ACTION_FLINCH:
	case REACTOR_ACTION_BIG_FLINCH:
	{
		int nextAnimId = moby->AnimSeqId;

		switch (moby->AnimSeqId)
		{
		case REACTOR_ANIM_FLINCH_FALL_DOWN:
		{
			if (pvars->MobVars.AnimationLooped)
			{
				nextAnimId = REACTOR_ANIM_RUN;
				mobTransAnimLerp(moby, nextAnimId, 60, 0);
			}
			break;
		}
		case REACTOR_ANIM_FLINCH_SMALL:
		{
			if (pvars->MobVars.AnimationLooped || moby->AnimSeqT > 20)
			{
				reactorForceLocalAction(moby, REACTOR_ACTION_WALK);
				goto exit;
			}
			break;
		}
		case REACTOR_ANIM_RUN:
		{
			if (pvars->MobVars.AnimationLooped || moby->AnimSeqT > 20)
			{
				reactorForceLocalAction(moby, REACTOR_ACTION_WALK);
				goto exit;
			}
			break;
		}
		}

		if (!pvars->MobVars.CurrentActionForTicks)
		{
			nextAnimId = pvars->MobVars.Action == REACTOR_ACTION_BIG_FLINCH ? REACTOR_ANIM_FLINCH_FALL_DOWN : REACTOR_ANIM_FLINCH_SMALL;
		}

		reactorTransAnim(moby, nextAnimId, 0);

		if (pvars->MobVars.Knockback.Ticks > 0)
		{
			mobGetKnockbackVelocity(moby, t);
			vector_scale(t, t, REACTOR_KNOCKBACK_MULTIPLIER);
			vector_add(pvars->MobVars.MoveVars.AddVelocity, pvars->MobVars.MoveVars.AddVelocity, t);
		}
		else if (pvars->MobVars.MoveVars.Grounded)
		{
			mobStand(moby);
		}
		else if (pvars->MobVars.CurrentActionForTicks > (1 * TPS) && pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.StuckCounter)
		{
			mobStand(moby);
		}
		break;
	}
	case REACTOR_ACTION_IDLE:
	{
		reactorTransAnim(moby, REACTOR_ANIM_IDLE, 0);
		mobStand(moby);
		break;
	}
	case REACTOR_ACTION_GLOAT:
	{
		reactorTransAnim(moby, REACTOR_ANIM_TAUNT_HANDS_UP_FOR_CROWD, 0);
		if (pvars->MobVars.AnimationLooped)
		{
			reactorForceLocalAction(moby, REACTOR_ACTION_WALK);
		}

		mobStand(moby);
		break;
	}
	case REACTOR_ACTION_JUMP:
	{
		// move
		if (!isInAirFromFlinching)
		{
			if (target)
			{
				if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
					pvars->MobVars.Dirty = 1; // new path, sync with other clients
				mobJumpTowards(moby, t);
			}
			else
			{
				mobStand(moby);
			}
		}

		// handle jumping
		if (pvars->MobVars.MoveVars.Grounded)
		{
			reactorTransAnim(moby, REACTOR_ANIM_JUMP_UP, 5);

			// check if we're near last jump pos
			// if so increment StuckJumpCount
			if (pvars->MobVars.MoveVars.IsStuck)
			{
				if (pvars->MobVars.MoveVars.StuckJumpCount < 255)
					pvars->MobVars.MoveVars.StuckJumpCount++;
			}

			// use delta height between target as base of jump speed
			// with min speed
			float jumpSpeed = pvars->MobVars.MoveVars.QueueJumpSpeed;
			if (jumpSpeed <= 0 && target)
			{
				jumpSpeed = 8; // clamp(2 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->MobVars.MoveVars.WallSlope) * 2, 3, 15);
			}

			pvars->MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
			pvars->MobVars.MoveVars.Grounded = 0;
			pvars->MobVars.MoveVars.QueueJumpSpeed = 0;
		}
		break;
	}
	case REACTOR_ACTION_LOOK_AT_TARGET:
	{
		mobStand(moby);
		if (target)
		{
			mobTurnTowards(moby, target->Position, turnSpeed);
		}
		break;
	}
	case REACTOR_ACTION_WALK:
	{
		if (target)
		{

			float dir = mobGetCurrentWalkAngle(moby);

			// determine next position
			vector_copy(t, target->Position);
			vector_subtract(t, t, moby->Position);
			float dist = vector_length(t);

			if (dist < (REACTOR_TOO_CLOSE_TO_TARGET_RADIUS + pvars->MobVars.Config.CollRadius))
			{
				walkBackwards = 1;

				// mobMoveTowards(moby, backTarget, pvars->MobVars.Config.Speed, turnSpeed, acceleration, dir);
				if (dist < 0.1)
				{
					vector_fromyaw(t, moby->Rotation[2]);
				}
				else
				{
					vector_normalize(t, t);
				}
				vector_scale(t, t, 5);
				vector_subtract(t, moby->Position, t);
				mobGetVelocityToTargetSimple(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, t, pvars->MobVars.Config.Speed, acceleration);
			}
			else if (dist > (pvars->MobVars.Config.AttackRadius - pvars->MobVars.Config.HitRadius))
			{

				if (pathGetTargetPos(t, moby) && mobAmIOwner(moby))
					pvars->MobVars.Dirty = 1; // new path, sync with other clients
				mobMoveTowards(moby, t, pvars->MobVars.Config.Speed, turnSpeed, acceleration, dir);
			}
			else
			{
				mobStand(moby);
			}
		}
		else
		{
			mobStand(moby);
		}

		//
		if (moby->AnimSeqId == REACTOR_ANIM_JUMP_UP && !pvars->MobVars.MoveVars.Grounded)
		{
			// wait for jump to land
		}
		else if (pvars->MobVars.MoveVars.QueueJumpSpeed)
		{
			reactorForceLocalAction(moby, REACTOR_ACTION_JUMP);
		}
		else if (mobHasVelocity(pvars))
		{
			reactorTransAnim(moby, REACTOR_ANIM_RUN, 0);
		}
		else if (moby->AnimSeqId != REACTOR_ANIM_RUN || moby->AnimSeqT > 4)
		{
			reactorTransAnim(moby, REACTOR_ANIM_IDLE, 0);
		}
		break;
	}
	case REACTOR_ACTION_DIE:
	{
		mobTransAnimLerp(moby, REACTOR_ANIM_DIE, 5, 0);

		if (moby->AnimSeqId == REACTOR_ANIM_DIE && moby->AnimSeqT > 124)
		{
			pvars->MobVars.Destroy = 1;
		}

		mobStand(moby);
		break;
	}
	case REACTOR_ACTION_ATTACK_SWING:
	{
		int attack1AnimId = REACTOR_ANIM_SWING;
		if (pvars->MobVars.CurrentActionForTicks > 60)
		{
			reactorTransAnim(moby, REACTOR_ANIM_IDLE, 0);
			if (target)
			{
				mobTurnTowards(moby, target->Position, turnSpeed);
			}
			mobStand(moby);
			reactorForceLocalAction(moby, REACTOR_ACTION_IDLE);
			break;
		}

		reactorTransAnim(moby, attack1AnimId, 0);
		moby->AnimFlags = 0x10;

		float speedCurve = powf(clamp((20 - moby->AnimSeqT) / 2, 0, 2.5), 2);
		float speed = MOB_BASE_SPEED * ((moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 0 && moby->AnimSeqT <= 16) ? speedCurve : 0);
		int swingAttackReady = moby->AnimSeqId == attack1AnimId && moby->AnimSeqT >= 14 && moby->AnimSeqT < 18;

		if (target)
		{
			mobStand(moby);
			mobMoveTowards(moby, target->Position, speed, turnSpeed, acceleration, 0);
		}
		else
		{
			mobStand(moby);
		}

		if (swingAttackReady && damageFlags)
		{
			reactorDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
		}
		break;
	}
	case REACTOR_ACTION_ATTACK_CHARGE:
	{
		float speedMult = 0;
		int facePlayer = 1;
		int nextAnimId = moby->AnimSeqId;
		int attackCanDoChargeDamage = 0;
		int attackCanDoSwingDamage = 0;

		switch (moby->AnimSeqId)
		{
		// begins squat and foot rub animation
		// don't loop, continue into foot rub
		case REACTOR_ANIM_SQUAT_PREPARE_FOR_DASH:
		{
			if (pvars->MobVars.AnimationLooped)
			{
				if (rand(5) == 0)
					reactorPlayDialog(moby, DIALOG_ID_REACTOR_IM_GONNA_SMACK_THAT_STUPID_LOOK_OFF_YOUR_FACE);
				nextAnimId = REACTOR_ANIM_SQUAT_PREPARE_FOR_DASH_FOOT_RUB_GROUND;
			}
			break;
		}
		// repeat foot rub for the ChargeChargeupTargetCount times
		case REACTOR_ANIM_SQUAT_PREPARE_FOR_DASH_FOOT_RUB_GROUND:
		{
			if (pvars->MobVars.AnimationLooped >= reactorVars->ChargeChargeupTargetCount)
			{
				nextAnimId = REACTOR_ANIM_SQUAT_PREPARE_FOR_DASH_FOOT_RUB_GROUND2;
			}
			break;
		}
		// enters pre dash animation
		case REACTOR_ANIM_SQUAT_PREPARE_FOR_DASH_FOOT_RUB_GROUND2:
		{
			if (pvars->MobVars.AnimationLooped)
			{
				nextAnimId = REACTOR_ANIM_SWING;
			}

			break;
		}
		// hit wall animation
		case REACTOR_ANIM_STEP_BACK_KNEE_DOWN:
		{
			facePlayer = 0;
			speedMult = 0;
			if (pvars->MobVars.AnimationLooped)
			{
				nextAnimId = REACTOR_ANIM_KNEE_DOWN;
			}

			break;
		}
		// hit wall animation
		case REACTOR_ANIM_KNEE_DOWN:
		{
			facePlayer = 0;
			speedMult = 0;
			moby->CollActive = 1; // disable collision
			if (pvars->MobVars.AnimationLooped > 4)
			{
				moby->CollActive = 0;
				nextAnimId = REACTOR_ANIM_KNEE_DOWN_STEP_UP;
			}

			// create electricity
			if (pvars->MobVars.Config.MobAttribute == MOB_ATTRIBUTE_BOSS && pvars->ReactVars.effectStates == 0)
			{
				pvars->ReactVars.effectStates = 0x84;
				pvars->ReactVars.effectTimers[2] = 10;
			}
			break;
		}
		// hit wall animation
		case REACTOR_ANIM_KNEE_DOWN_STEP_UP:
		{
			facePlayer = 1;
			speedMult = 0;
			if (pvars->MobVars.AnimationLooped)
			{
				reactorForceLocalAction(moby, REACTOR_ACTION_IDLE);
			}

			break;
		}
		// dash
		case REACTOR_ANIM_SWING:
		{
			acceleration = REACTOR_CHARGE_ACCELERATION;
			if (pvars->MobVars.MoveVars.HitWall && pvars->MobVars.MoveVars.WallSlope > REACTOR_CHARGE_MAX_SLOPE && (!pvars->MobVars.MoveVars.HitWallMoby || pvars->MobVars.MoveVars.HitWallMoby->OClass == STATUE_MOBY_OCLASS))
			{
				nextAnimId = REACTOR_ANIM_STEP_BACK_KNEE_DOWN;
				playDialog(DIALOG_ID_REACTOR_OOH_IM_GONNA_NEED_TO_SIT_FOR_A_SEC, 1);
			}
			else if (moby->AnimSeqT > 6 && !reactorVars->ChargeHasPlayedSound)
			{
				reactorVars->ChargeHasPlayedSound = 1;
			}
			else if (moby->AnimSeqT > 7 && moby->AnimSeqT < 13)
			{

				speedMult = REACTOR_CHARGE_SPEED;
				reactorVars->AnimSpeedAdditive = 0.25;
				facePlayer = 0;
			}

			// damage during swing
			attackCanDoChargeDamage = speedMult > 0;
			attackCanDoSwingDamage = moby->AnimSeqT >= 13 && moby->AnimSeqT < 19;

			// exit
			if (pvars->MobVars.AnimationLooped)
			{
				reactorForceLocalAction(moby, REACTOR_ACTION_IDLE);
			}
			break;
		}
		default:
		{
			nextAnimId = REACTOR_ANIM_SQUAT_PREPARE_FOR_DASH;
			break;
		}
		}

		// transition
		reactorTransAnim(moby, nextAnimId, 0);

		if (target)
		{
			if (facePlayer)
				mobTurnTowards(moby, target->Position, turnSpeed);
			mobGetVelocityToTarget(moby, pvars->MobVars.MoveVars.Velocity, moby->Position, target->Position, speedMult, acceleration);
		}
		else
		{
			// stand
			mobStand(moby);
		}

		if (attackCanDoChargeDamage && damageFlags)
		{
			reactorDoChargeDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage * 1.5, damageFlags, 0);
		}
		if (attackCanDoSwingDamage && damageFlags)
		{
			reactorDoDamage(moby, pvars->MobVars.Config.HitRadius, pvars->MobVars.Config.Damage, damageFlags, 0);
		}
		break;
	}
	case REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL:
	{
		int nextAnimId = moby->AnimSeqId;

		switch (moby->AnimSeqId)
		{
		case REACTOR_ANIM_PREPPING_TWO_HAND_PULSE:
		{
			if (pvars->MobVars.AnimationLooped)
			{
				nextAnimId = REACTOR_ANIM_FIRE_TWO_HAND_PULSE;
			}
			break;
		}
		case REACTOR_ANIM_FIRE_TWO_HAND_PULSE:
		{
			if (moby->AnimSeqT < 15)
			{
				turnSpeed = 0;
			}
			else
			{
				turnSpeed = REACTOR_SHOT_WITH_TRAIL_TURN_RADIANS_PER_SEC * clamp((pvars->MobVars.Config.Speed / MOB_BASE_SPEED) / 3, 1, 2);
			}

			if (moby->AnimSeqT < 10 && reactorVars->HasFiredTrailshotThisLoop)
			{
				reactorVars->HasFiredTrailshotThisLoop = 0;
			}
			else if (moby->AnimSeqT >= 10 && !reactorVars->HasFiredTrailshotThisLoop)
			{
				reactorVars->HasFiredTrailshotThisLoop = 1;
				reactorFireTrailshot(moby);
			}

			// stop after n iterations
			if (pvars->MobVars.AnimationLooped > 3)
			{
				reactorForceLocalAction(moby, REACTOR_ACTION_WALK);
				goto exit;
			}
			break;
		}
		}

		if (!pvars->MobVars.CurrentActionForTicks)
		{
			if (rand(5) == 0)
				reactorPlayDialog(moby, DIALOG_ID_REACTOR_DODGE_THIS);
			nextAnimId = REACTOR_ANIM_PREPPING_TWO_HAND_PULSE;
		}

		// transition
		reactorTransAnim(moby, nextAnimId, 0);

		// stand and face target
		mobStand(moby);
		if (target)
		{
			mobTurnTowardsPredictive(moby, target, turnSpeed, 50);
		}
		break;
	}
	case REACTOR_ACTION_ATTACK_SMASH:
	{
		// transition
		reactorTransAnim(moby, REACTOR_ANIM_JUMP_SMASH, 0);

		if (moby->AnimSeqId == REACTOR_ANIM_JUMP_SMASH)
		{
			if (moby->AnimSeqT < 16 && reactorVars->HasSmashedThisLoop)
			{
				reactorVars->HasSmashedThisLoop = 0;
			}
			else if (moby->AnimSeqT > 16 && !reactorVars->HasSmashedThisLoop)
			{

				reactorVars->HasSmashedThisLoop = 1;

				reactorDoSmashEffect(moby, 10);
				reactorDoSmashDamage(moby, 20, pvars->MobVars.Config.Damage, damageFlags);
			}

			//
			if (pvars->MobVars.AnimationLooped >= reactorVars->SmashTargetCount)
			{
				reactorForceLocalAction(moby, REACTOR_ACTION_WALK);
			}
		}

		// stand and face target
		mobStand(moby);
		break;
	}
	}

exit:;
	pvars->MobVars.CurrentActionForTicks++;
}

//--------------------------------------------------------------------------
void reactorDoSmashDamage(Moby *moby, float radius, float amount, int damageFlags)
{
	VECTOR dt;
	int i;
	int hitPlayerFull = 0;
	float sqrRadius = radius * radius;
	MobyColDamageIn in;
	// struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	// ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;

	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
	{
		Player *player = playerGetFromIndex(i);
		if (!player || !player->SkinMoby || playerIsDead(player))
			continue;

		vector_subtract(dt, player->PlayerPosition, moby->Position);
		if (vector_sqrmag(dt) < sqrRadius)
		{

			// deal more damage if player is on ground
			if (player->Ground.onGood)
			{
				hitPlayerFull = 1;
				in.DamageHp = amount;
			}
			else
			{
				in.DamageHp = 5;
			}

			vector_write(in.Momentum, 0);
			in.Damager = moby;
			in.DamageFlags = damageFlags;
			in.DamageClass = 0;
			in.DamageStrength = 1;
			in.DamageIndex = moby->OClass;
			in.Flags = 1;
			mobyCollDamageDirect(player->PlayerMoby, &in);
		}
	}

	if (hitPlayerFull && rand(5) == 0)
	{
		reactorPlayDialog(moby, reactorOnDamagePlayerDialogIds[rand(COUNT_OF(reactorOnDamagePlayerDialogIds))]);
	}
}

//--------------------------------------------------------------------------
void reactorDoChargeDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	MATRIX m;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;

	// get current hips matrix
	mobyGetJointMatrix(moby, REACTOR_SUBSKELETON_JOINT_HIPS, m);

	if (mobDoSweepDamage(moby, reactorVars->LastHipsPosition, &m[12], 1.0, radius * 2.0, amount, damageFlags, friendlyFire, 0, 0) & MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER)
	{
		if (rand(5) == 0)
		{
			reactorPlayDialog(moby, reactorOnDamagePlayerDialogIds[rand(COUNT_OF(reactorOnDamagePlayerDialogIds))]);
		}
	}
}

//--------------------------------------------------------------------------
void reactorDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire)
{
	MATRIX m;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;

	// get current hips matrix
	mobyGetJointMatrix(moby, REACTOR_SUBSKELETON_JOINT_RIGHT_HAND, m);

	if (mobDoSweepDamage(moby, reactorVars->LastRightHandPosition, &m[12], 1.0, radius, amount, damageFlags, friendlyFire, 1, 0) & MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER)
	{
		if (rand(5) == 0)
		{
			reactorPlayDialog(moby, reactorOnDamagePlayerDialogIds[rand(COUNT_OF(reactorOnDamagePlayerDialogIds))]);
		}
	}
}

//--------------------------------------------------------------------------
void reactorForceLocalAction(Moby *moby, int action)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	float difficulty = 1;

	if (MapConfig.State)
		difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->MobVars.Action)
	{
	case REACTOR_ACTION_SPAWN:
	{
		// enable collision
		moby->CollActive = 0;
		break;
	}
	case REACTOR_ACTION_ATTACK_SWING:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		break;
	}
	case REACTOR_ACTION_ATTACK_CHARGE:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		reactorVars->AttackChargeCooldownTicks = randRangeInt(REACTOR_CHARGE_ATTACK_MIN_COOLDOWN_TICKS, REACTOR_CHARGE_ATTACK_MAX_COOLDOWN_TICKS);
		reactorVars->ChargeHasPlayedSound = 0;
		break;
	}
	case REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		reactorVars->AttackShotWithTrailCooldownTicks = randRangeInt(REACTOR_SHOT_WITH_TRAIL_ATTACK_MIN_COOLDOWN_TICKS, REACTOR_SHOT_WITH_TRAIL_ATTACK_MAX_COOLDOWN_TICKS);
		break;
	}
	case REACTOR_ACTION_ATTACK_SMASH:
	{
		pvars->MobVars.AttackCooldownTicks = pvars->MobVars.Config.AttackCooldownTickCount;
		reactorVars->AttackSmashCooldownTicks = randRangeInt(REACTOR_SMASH_ATTACK_MIN_COOLDOWN_TICKS, REACTOR_SMASH_ATTACK_MAX_COOLDOWN_TICKS);
		break;
	}
	case REACTOR_ACTION_DIE:
	{
		// can't undie
		return;
	}
	}

	// to
	switch (action)
	{
	case REACTOR_ACTION_SPAWN:
	{
		// disable collision
		moby->CollActive = 1;
		break;
	}
	case REACTOR_ACTION_WALK:
	{

		break;
	}
	case REACTOR_ACTION_DIE:
	{

		break;
	}
	case REACTOR_ACTION_ATTACK_SWING:
	{
		break;
	}
	case REACTOR_ACTION_ATTACK_CHARGE:
	{
		reactorVars->ChargeChargeupTargetCount = REACTOR_CHARGE_ATTACK_MIN_STALL_LOOPS + ((u8)pvars->MobVars.DynamicRandom % (REACTOR_CHARGE_ATTACK_MAX_STALL_LOOPS - REACTOR_CHARGE_ATTACK_MIN_STALL_LOOPS));
		break;
	}
	case REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL:
	{
		reactorVars->HasFiredTrailshotThisLoop = 0;
		break;
	}
	case REACTOR_ACTION_ATTACK_SMASH:
	{
		reactorVars->SmashTargetCount = REACTOR_SMASH_ATTACK_MIN_COUNT + ((u8)pvars->MobVars.DynamicRandom % (REACTOR_SMASH_ATTACK_MAX_COUNT - REACTOR_SMASH_ATTACK_MIN_COUNT));
		break;
	}
	case REACTOR_ACTION_FLINCH:
	case REACTOR_ACTION_BIG_FLINCH:
	{
		pvars->MobVars.FlinchCooldownTicks = REACTOR_FLINCH_COOLDOWN_TICKS;
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
	pvars->MobVars.ActionCooldownTicks = REACTOR_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short reactorGetArmor(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	float t = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;
	int bangles = pvars->MobVars.Config.Bangles;

	if (t < 0.5)
		return 0x0000; // remove should plates

	return bangles;
}

//--------------------------------------------------------------------------
int reactorCanNonOwnerTransitionToAction(Moby *moby, int action)
{
	// only let owner choose actions for reactor
	// since for this boss it is critical it syncs well
	return 0;
}

//--------------------------------------------------------------------------
int reactorShouldForceStateUpdateOnAction(Moby *moby, int action)
{
	// always send state update whenever action changes
	// uses a lot more network bandwidth but reduces desyncing
	return 1;
}

//--------------------------------------------------------------------------
int reactorIsWalkingOrIdle(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == REACTOR_ACTION_WALK || pvars->MobVars.Action == REACTOR_ACTION_IDLE;
}

//--------------------------------------------------------------------------
int reactorIsAttacking(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return (pvars->MobVars.Action == REACTOR_ACTION_ATTACK_SWING && !pvars->MobVars.AnimationLooped) || pvars->MobVars.Action == REACTOR_ACTION_ATTACK_CHARGE || pvars->MobVars.Action == REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL || pvars->MobVars.Action == REACTOR_ACTION_ATTACK_SMASH;
}

//--------------------------------------------------------------------------
int reactorIsSpawning(struct MobPVar *pvars)
{
	return pvars->MobVars.Action == REACTOR_ACTION_SPAWN && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
int reactorCanAttack(struct MobPVar *pvars, enum ReactorAction action)
{
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	if (pvars->MobVars.AttackCooldownTicks != 0)
		return 0;

	switch (action)
	{
	case REACTOR_ACTION_ATTACK_SWING:
	{
		return 1;
	}
	case REACTOR_ACTION_ATTACK_CHARGE:
	{
		// charge doesn't activate until 80% health
		return pvars->MobVars.Health <= (pvars->MobVars.Config.MaxHealth * 0.80) && reactorVars->AttackChargeCooldownTicks == 0;
	}
	case REACTOR_ACTION_ATTACK_SHOT_WITH_TRAIL:
	{
		// trailshot doesn't activate until 60% health
		return pvars->MobVars.Health <= (pvars->MobVars.Config.MaxHealth * 0.60) && reactorVars->AttackShotWithTrailCooldownTicks == 0;
	}
	case REACTOR_ACTION_ATTACK_SMASH:
	{
		// must have less than 10 alive
		if (MapConfig.State && MapConfig.State->MobStats.TotalAlive > 10)
			return 0;

		// smash doesn't activate until 40% health
		return pvars->MobVars.Health <= (pvars->MobVars.Config.MaxHealth * 0.40) && reactorVars->AttackSmashCooldownTicks == 0;
	}
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
int reactorIsFlinching(Moby *moby)
{
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	return pvars->MobVars.Action == REACTOR_ACTION_BIG_FLINCH || pvars->MobVars.Action == REACTOR_ACTION_FLINCH;
	// return (moby->AnimSeqId == REACTOR_ANIM_FLINCH_SMALL || moby->AnimSeqId == REACTOR_ANIM_FLINCH_FALL_DOWN) && !pvars->MobVars.AnimationLooped;
}

//--------------------------------------------------------------------------
void reactorOnReceivePlayDialog(Moby *moby, GuberEvent *e)
{
	short dialogId = 0;
	guberEventRead(e, &dialogId, sizeof(dialogId));
	playDialog(dialogId, 1);
}

//--------------------------------------------------------------------------
void reactorPlayDialog(Moby *moby, short dialogId)
{
	if (!mobAmIOwner(moby))
		return;

	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
	ReactorMobVars_t *reactorVars = (ReactorMobVars_t *)pvars->AdditionalMobVarsPtr;
	if (!reactorVars->DialogCooldownTicks)
	{
		reactorSendCustomEvent(moby, REACTOR_CUSTOM_EVENT_PLAY_DIALOG, &dialogId, sizeof(dialogId));
		reactorVars->DialogCooldownTicks = randRangeInt(REACTOR_DIALOG_COOLDOWN_MIN_TPS, REACTOR_DIALOG_COOLDOWN_MAX_TPS);
	}
}

//--------------------------------------------------------------------------
void reactorSpawnMinion(Moby *moby, float radius)
{
	VECTOR position, from, to;
	VECTOR hitOffset = {0, 0, 2, 0};
	int attempts = 0;
	struct MobSpawnParams *spawnParams = &MapConfig.DefaultSpawnParams[reactorMinionSpawnParamIdx];

	if (reactorMinionSpawnParamIdx < 0 || reactorMinionSpawnParamIdx >= MAX_MOB_SPAWN_PARAMS)
		return;

	// don't go over max mobys alive
	if (MapConfig.State)
	{
		if (MapConfig.State->MobStats.TotalAlive >= MAX_MOBS_ALIVE)
			return;
		if (MapConfig.State->MobStats.NumAlive[reactorMinionSpawnParamIdx] >= spawnParams->MaxSpawnedAtOnce)
			return;
	}

	// default to spawn on reactor
	vector_copy(position, moby->Position);

	// attempt to find a spot near reactor that they can spawn on
	while (attempts < 4)
	{
		float yaw = randRadian();
		float r = randRange(1, radius);
		vector_fromyaw(from, yaw);
		vector_scale(from, from, r);
		vector_add(from, from, moby->Position);

		vector_subtract(to, from, hitOffset);
		vector_add(from, from, hitOffset);

		if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
		{
			vector_copy(position, CollLine_Fix_GetHitPosition());
			break;
		}

		++attempts;
	}

	// spawn
	mapSpawnMob(reactorMinionSpawnParamIdx, position, moby->Rotation[2], -1, 0);
}

//--------------------------------------------------------------------------
void reactorDoSmashEffect(Moby *moby, float radius)
{
	int i;

	// spawn explosion
	u32 expColor = 0xFF00254B;
	u32 expDarkColor = 0x8000254B;
	u32 expNoColor = 0;
	VECTOR expPos = {0, 0, 0.5, 0};
	vector_add(expPos, expPos, moby->Position);
	mobySpawnExplosion(vector_read(expPos),
										 1, 0, 0, 0, 100, 7, 4, 0, 0, 0, 0, 1, 0, 0, NULL,
										 expNoColor, expNoColor, expNoColor, expNoColor, expNoColor, expNoColor, expColor, expNoColor, expDarkColor,
										 0, 0, 0, 0, 5, 0, 0, 0);

	// spawn mobs
	if (mobAmIOwner(moby))
	{
		int count = randRangeInt(REACTOR_SMASH_ATTACK_MIN_MINION_SPAWN_COUNT, REACTOR_SMASH_ATTACK_MAX_MINION_SPAWN_COUNT);
		for (i = 0; i < count; ++i)
		{
			reactorSpawnMinion(moby, radius);
		}
	}
}

//--------------------------------------------------------------------------
void reactorFireTrailshot(Moby *moby)
{
	VECTOR pos;
	VECTOR vel;
	VECTOR forward;
	VECTOR delta;
	VECTOR up = {0, 0, 1, 0};
	MATRIX mtx;
	struct MobPVar *pvars = (struct MobPVar *)moby->PVar;

	// get pos
	vector_copy(forward, moby->M0_03);
	mobyGetJointMatrix(moby, 2, mtx);
	vector_scale(forward, forward, 0.2);
	vector_add(pos, &mtx[12], forward);

	// if we have a target, vertically orient towards them
	if (pvars->MobVars.Target)
	{
		vector_subtract(delta, pvars->MobVars.Target->Position, pos);
		delta[2] += 0.5; // to center of player
		float pitch = -asinf(vector_innerproduct(delta, up));

		matrix_unit(mtx);
		matrix_rotate_y(mtx, mtx, pitch);
		matrix_rotate_z(mtx, mtx, moby->Rotation[2]);
		vector_copy(forward, &mtx[0]);
	}

	// get vel
	vector_scale(vel, forward, REACTOR_SHOT_WITH_TRAIL_SHOT_SPEED);

	// spawn
	trailshotSpawn(moby, pos, vel, 0x80C06020, pvars->MobVars.Config.Damage, TPS * 1.5);
}
