#ifndef RAIDS_MOB_STALKERTURRET_H
#define RAIDS_MOB_STALKERTURRET_H

#include "game.h"

#define STALKERTURRET_RENDER_COST                    (85)

#define STALKERTURRET_TARGET_KEEP_CURRENT_FACTOR     (3)

#define STALKERTURRET_TURN_RADIANS_PER_SEC           (15 * MATH_DEG2RAD)
#define STALKERTURRET_TURN_PREDICT_FACTOR_PER_STAR   (1.0)

#define STALKERTURRET_ACTION_COOLDOWN_TICKS					(30)

#define STALKERTURRET_TARGET_CACHE_COUNT             (10)

enum StalkerTurretAttackActions
{
  STALKERTURRET_ATTACK_ACTION_FIRE,
  STALKERTURRET_ATTACK_ACTION_COUNT,
};

enum StalkerTurretParameters
{
  /* STALKERTURRET_ATTACK_ACTION_FIRE */
  STALKERTURRET_PARAM_FIRE_PROJECTILE_DAMAGE = 0,
  STALKERTURRET_PARAM_FIRE_PROJECTILE_SPEED = 1,
  STALKERTURRET_PARAM_FIRE_GATLING_MAX_SPEED = 2,
  STALKERTURRET_PARAM_FIRE_GATLING_ACCELERATION = 3,
  STALKERTURRET_PARAM_FIRE_GATLING_FIRE_SPEED = 4,
  STALKERTURRET_PARAM_FIRE_SHOT_DELAY = 5,
  STALKERTURRET_PARAM_FIRE_LOCK_ANGLE = 6,
  STALKERTURRET_PARAM_FIRE_MAX_DISTANCE = 7,
};

enum StalkerTurretAnimId
{
	STALKERTURRET_ANIM_0
};

// enum StalkerTurretBangles {
  
// };

enum StalkerTurretAction
{
	STALKERTURRET_ACTION_SPAWN,
	STALKERTURRET_ACTION_IDLE,
	STALKERTURRET_ACTION_LOOK_AT_TARGET,
  STALKERTURRET_ACTION_DIE,
	STALKERTURRET_ACTION_ATTACK,
	STALKERTURRET_ACTION_ROAM,
	STALKERTURRET_ACTION_RESET_ROTATION,
};

enum StalkerTurretSubskeletonJoints
{
  STALKERTURRET_SUBSKELETON_JOINT_0 = 0,
};

struct StalkerTurretTargetCache {
  Moby* Moby;
  char CanSee;
  u8 TicksSinceLastCheck;
};

typedef struct StalkerTurretMobVars {
  u32 ActionCooldownTicks[STALKERTURRET_ATTACK_ACTION_COUNT];
  u32 ActionQueuedForTicks[STALKERTURRET_ATTACK_ACTION_COUNT];
  u32 ShotCooldownTicks;

  Moby* TurretMoby;
  Moby* BaseMoby;
  float GatlingRotation;
  float GatlingSpeed;
  struct StalkerTurretTargetCache TargetCache[STALKERTURRET_TARGET_CACHE_COUNT];
  int TargetCacheThisFrame;
  char GatlingActive;
  char GatlingDelay1;
  char GatlingDelay2;
  char Team;
} StalkerTurretMobVars_t;

extern struct MobVTable StalkerTurretVTable;

int stalkerturretCreate(struct MobCreateArgs* args);

#endif // RAIDS_MOB_STALKERTURRET_H
