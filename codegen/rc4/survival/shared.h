#ifndef SURVIVAL_MAP_SHARED_H
#define SURVIVAL_MAP_SHARED_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "game.h"
#include "mob.h"

enum MOB_DO_DAMAGE_HIT_FLAGS
{
  MOB_DO_DAMAGE_HIT_FLAG_NONE = 0,
  MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET = 1,
  MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER = 2,
  MOB_DO_DAMAGE_HIT_FLAG_HIT_MOB = 4,
  MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER_THORNS = 8,
};

int mobAmIOwner(Moby* moby);
int mobIsFrozen(Moby* moby);
int mobGetBehavior(Moby* moby);
void mobResetSoundTrigger(Moby* moby);
void mobSpawnCorn(Moby* moby, int bangle);
int mobDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE);
int mobDoSweepDamage(Moby* moby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE);
int mobDoDamageTryHit(Moby* moby, Moby* hitMoby, VECTOR jointPosition, int isAoE, float sqrHitRadius, int damageFlags, float amount);
void mobSetAction(Moby* moby, int action);
void mobTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff);
void mobTransAnim(Moby* moby, int animId, float startOff);
int mobHasVelocity(struct MobPVar* pvars);
float mobGetCurrentMoveSpeed(Moby* moby);
void mobGetKnockbackVelocity(Moby* moby, VECTOR out);
void mobGetTargetCenter(Moby* target, VECTOR out);
int mobCanSeeMoby(Moby* moby, Moby* canSeeMoby);
void mobStand(Moby* moby);
void mobResetMoveStep(Moby* moby);
int mobMoveCheck(Moby* moby, VECTOR outputPos, VECTOR from, VECTOR to);
void mobMove(Moby* moby);
void mobMoveTowards(Moby* moby, VECTOR targetPosition, float speed, float turnSpeed, float acceleration, float curveNearTargetDir);
void mobJumpTowards(Moby* moby, VECTOR targetPosition);
int mobHitWallShouldJump(Moby* moby, float maxSlope);
float mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed);
float mobTurnTowardsPredictive(Moby* moby, Moby* target, float turnSpeed, float predictFactor);
void mobGetVelocityToTargetWithDirection(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float yaw, float speed, float acceleration);
void mobGetVelocityToTarget(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobGetVelocityToTargetSimple(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobPostDrawQuad(Moby* moby, int texId, u32 color, int jointId);
void mobOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
void mobPreUpdate(Moby* moby);
int mobIsProjectileComing(Moby* moby);
float mobGetCurrentWalkAngle(Moby* moby);
float mobGetScaleMultiplier(Moby* moby);
Moby* mobGetNextTarget(Moby* moby, float keepCurrentTargetFactor);

#endif // SURVIVAL_MAP_SHARED_H
