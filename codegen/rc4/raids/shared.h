

#ifndef RAIDS_MAP_SHARED_H
#define RAIDS_MAP_SHARED_H

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

struct MobPVar;
struct MobStateUpdateEventArgs;

void mapOnMobUpdate(Moby* moby);
void mapOnMobKilled(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
void mapOnMobDestroyed(Moby* moby);
void mapOnMobSpawned(Moby* moby);
struct Guber* mapGetGuber(Moby* moby);
void mapHandleEvent(Moby* moby, GuberEvent* event);
void mapInstallMobyFunctions(MobyFunctions* mobyFunctions);
void mapStart(void);
void mapTick(void);
void mapTickEnd(void);
void mapInit(void);

int mobAmIOwner(Moby* moby);
void mobResetSoundTrigger(Moby* moby);
void mobBlowCorn(Moby* moby);
int mobDoDamage(Moby* mobMoby, Moby* sourceMoby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE);
int mobDoSweepDamage(Moby* mobMoby, Moby* sourceMoby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE);
int mobDoDamageTryHit(Moby* mobMoby, Moby* sourceMoby, Moby* hitMoby, VECTOR jointPosition, int isAoE, float sqrHitRadius, int damageFlags, float amount);
void mobSetAction(Moby* moby, int action);
void mobTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff, char* animationReset, char* animationLooped);
void mobTransAnim(Moby* moby, int animId, float startOff);
void mobUpdateAnim(Moby* moby);
int mobGetAnimIf(Moby* moby, int animIdFalse, int animIdTrue, int condition, int maxLoops);
int mobHasVelocity(struct MobPVar* pvars);
void mobGetKnockbackVelocity(Moby* moby, VECTOR out);
void mobStand(Moby* moby);
void mobResetMoveStep(Moby* moby);
int mobMoveCheck(Moby* moby, VECTOR outputPos, VECTOR from, VECTOR to);
void mobMove(Moby* moby);
float mobTurnTowards(Moby* moby, VECTOR towards, float turnSpeed);
float mobTurnTowardsPredictive(Moby* moby, Moby* target, float turnSpeed, float predictFactor);
void mobGetVelocityToTargetWithDirection(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float yaw, float speed, float acceleration);
void mobGetVelocityToTarget(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobGetVelocityToTargetSimple(Moby* moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
float mobGetCurrentMoveSpeed(Moby* moby);
float mobGetCurrentWalkAngle(Moby* moby);
void mobMoveTowards(Moby* moby, VECTOR targetPosition, float speed, float turnSpeed, float acceleration, float curveNearTargetDir);
void mobJumpTowards(Moby* moby, VECTOR targetPosition);
int mobHitWallShouldJump(Moby* moby, float maxSlope);
void mobPostDrawQuad(Moby* moby, int texId, u32 color, int jointId);
void mobOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
void mobPreUpdate(Moby* moby);
void mobGetTargetCenter(Moby* target, VECTOR out);
int mobCanSeeMoby(Moby* moby, Moby* canSeeMoby);
Moby* mobGetNextTarget(Moby* moby);
int mobIsProjectileComing(Moby* moby);

int mobCollisionIdIsLethal(int collisionId);
int mobCollisionIdIsWalkable(int collisionId);

void mobInit(void);
void mobTick(void);

#endif // RAIDS_MAP_SHARED_H
