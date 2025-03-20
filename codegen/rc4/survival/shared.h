

#ifndef SURVIVAL_MAP_SHARED_H
#define SURVIVAL_MAP_SHARED_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "game.h"

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
void mobResetSoundTrigger(Moby* moby);
void mobSpawnCorn(Moby* moby, int bangle);
int mobDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE);
int mobDoSweepDamage(Moby* moby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE);
int mobDoDamageTryHit(Moby* moby, Moby* hitMoby, VECTOR jointPosition, int isAoE, float sqrHitRadius, int damageFlags, float amount);
void mobSetAction(Moby* moby, int action);
void mobTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff);
void mobTransAnim(Moby* moby, int animId, float startOff);
int mobHasVelocity(struct MobPVar* pvars);
void mobGetKnockbackVelocity(Moby* moby, VECTOR out);
void mobStand(Moby* moby);
int mobResetMoveStep(Moby* moby);
int mobMoveCheck(Moby* moby, VECTOR outputPos, VECTOR from, VECTOR to);
void mobMove(Moby* moby);
void mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed);
void mobTurnTowardsPredictive(Moby* moby, Moby* target, float turnSpeed, float predictFactor);
void mobGetVelocityToTarget(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobGetVelocityToTargetSimple(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobPostDrawQuad(Moby* moby, int texId, u32 color, int jointId);
void mobOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
void mobPreUpdate(Moby* moby);
int mobIsProjectileComing(Moby* moby);


#endif // SURVIVAL_MAP_SHARED_H
