#ifndef RAIDS_MOB_STALKERTURRET_H
#define RAIDS_MOB_STALKERTURRET_H

#include "game.h"

#define STALKERTURRET_RENDER_COST                    (85)

#define STALKERTURRET_BASE_REACTION_TICKS						(0.25 * TPS)
#define STALKERTURRET_BASE_ATTACK_COOLDOWN_TICKS			(2 * TPS)
#define STALKERTURRET_BASE_EXPLODE_RADIUS						(5)
#define STALKERTURRET_MELEE_HIT_RADIUS								(0.5)
#define STALKERTURRET_EXPLODE_HIT_RADIUS							(5)
#define STALKERTURRET_MELEE_ATTACK_RADIUS						(5)

#define STALKERTURRET_TARGET_KEEP_CURRENT_FACTOR     (3)

#define STALKERTURRET_MAX_WALKABLE_SLOPE             (40 * MATH_DEG2RAD)
#define STALKERTURRET_BASE_STEP_HEIGHT								(2)
#define STALKERTURRET_TURN_RADIANS_PER_SEC           (15 * MATH_DEG2RAD)
#define STALKERTURRET_TURN_AIR_RADIANS_PER_SEC       (45 * MATH_DEG2RAD)
#define STALKERTURRET_TURN_PREDICT_FACTOR_PER_STAR   (1.0)
#define STALKERTURRET_MOVE_ACCELERATION              (25)
#define STALKERTURRET_MOVE_AIR_ACCELERATION          (5)

#define STALKERTURRET_ANIM_ATTACK_TICKS							(30)
#define STALKERTURRET_FLINCH_COOLDOWN_TICKS					(60 * 7)
#define STALKERTURRET_KNOCKBACK_MULTIPLIER				    (1.5)
#define STALKERTURRET_ACTION_COOLDOWN_TICKS					(30)
#define STALKERTURRET_RESPAWN_AFTER_TICKS						(60 * 30)
#define STALKERTURRET_BASE_COLL_RADIUS								(1.0)
#define STALKERTURRET_MAX_COLL_RADIUS								(4)
#define STALKERTURRET_AMBSND_MIN_COOLDOWN_TICKS    	(60 * 2)
#define STALKERTURRET_AMBSND_MAX_COOLDOWN_TICKS    	(60 * 3)
#define STALKERTURRET_FLINCH_PROBABILITY             (1.0)
#define STALKERTURRET_FLINCH_PROBABILITY_PWR_FACTOR  (0.1)

#define STALKERTURRET_PRIMARY_COLOR                  (0x00464443)
#define STALKERTURRET_GLOW_COLOR                     (0x80C0C0C0)
#define STALKERTURRET_LOD_COLOR                      (0x00808080)

#define STALKERTURRET_MAX_GATLING_SPEED              (10.0)
#define STALKERTURRET_GATLING_ACCELERATION           (2.0)
#define STALKERTURRET_SHOOT_AT_GATLING_SPEED         (8.0)
#define STALKERTURRET_SHOT_ALTERNATE_DELAY           (10)
#define STALKERTURRET_SHOT_LOCK_ON_WITHIN_RAD        (1 * MATH_DEG2RAD)

#define STALKERTURRET_TARGET_CACHE_COUNT             (10)

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
