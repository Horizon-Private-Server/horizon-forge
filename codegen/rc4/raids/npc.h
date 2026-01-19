#ifndef RAIDS_NPC_H
#define RAIDS_NPC_H

#include "game.h"
#include "mob.h"

#define NPC_BASE_REACTION_TICKS						(0.25 * TPS)
#define NPC_BASE_ATTACK_COOLDOWN_TICKS		(2 * TPS)
#define NPC_BASE_EXPLODE_RADIUS						(5)
#define NPC_MELEE_HIT_RADIUS							(1.75)
#define NPC_EXPLODE_HIT_RADIUS						(5)
#define NPC_MELEE_ATTACK_RADIUS						(5)

#define NPC_TARGET_KEEP_CURRENT_FACTOR    (3)

#define NPC_MAX_WALKABLE_SLOPE            (40 * MATH_DEG2RAD)
#define NPC_BASE_STEP_HEIGHT							(2)
#define NPC_TURN_RADIANS_PER_SEC          (45 * MATH_DEG2RAD)
#define NPC_TURN_AIR_RADIANS_PER_SEC      (15 * MATH_DEG2RAD)
#define NPC_MOVE_ACCELERATION             (25)
#define NPC_MOVE_AIR_ACCELERATION         (5)

#define NPC_ANIM_ATTACK_TICKS							(30)
#define NPC_FLINCH_COOLDOWN_TICKS				  (60 * 7)
#define NPC_KNOCKBACK_MULTIPLIER				  (2.0)
#define NPC_ACTION_COOLDOWN_TICKS					(0)
#define NPC_RESPAWN_AFTER_TICKS						(60 * 30)
#define NPC_BASE_COLL_RADIUS							(0.5)
#define NPC_MAX_COLL_RADIUS								(4)
#define NPC_AMBSND_MIN_COOLDOWN_TICKS    	(60 * 2)
#define NPC_AMBSND_MAX_COOLDOWN_TICKS    	(60 * 3)
#define NPC_FLINCH_PROBABILITY            (1.0)
#define NPC_FLINCH_PROBABILITY_PWR_FACTOR (0.1)

enum NpcState
{
	NPC_STATE_OFF,
	NPC_STATE_IDLE,
	NPC_STATE_ROAM,
	NPC_STATE_WALK_TO_TARGET,
	NPC_STATE_LOOK_AT_TARGET,
  NPC_STATE_CYCLE_ANIMATIONS,
  NPC_STATE_DIE
};

enum NpcAction
{
	NPC_ACTION_IDLE,
	NPC_ACTION_JUMP,
	NPC_ACTION_WALK,
	NPC_ACTION_LOOK_AT_TARGET,
  NPC_ACTION_DIE,
  NPC_ACTION_ROAM,
  NPC_ACTION_CYCLE_ANIMATIONS
};

enum NpcAggroType {
  NPC_MOB_AGGRO_IGNORE = 0,
  NPC_MOB_AGGRO_IN_RANGE,
  NPC_MOB_AGGRO_ALWAYS,
};

enum NpcOnDeathType {
  NPC_ON_DEATH_NONE = 0,
  NPC_ON_DEATH_DESTROY,
  NPC_ON_DEATH_BLOW_CORN,
};

struct NpcDifficultyConfig
{
  float HealthMult;
  float SpeedMult;
};

struct NpcAnimDef
{
  int Id;
  float Speed;
  float Length;
};

struct NpcParameters
{
  struct NpcAnimDef IdleAnim;
  struct NpcAnimDef WalkAnim;
  struct NpcAnimDef JumpAnim;
  struct NpcAnimDef LookAtPlayerAnim;
  struct NpcAnimDef DeathAnim;
  struct NpcAnimDef Unused1Anim;
  struct NpcAnimDef Unused2Anim;
  struct NpcAnimDef Unused3Anim;

  char DefaultState;
  char Log;
  char IsOnEnemyTeam;
  char Healthbar;
  Moby* NpcMoby;
  Moby* TargetMoby;
  int AttachedCuboidIdx;
  int PathGraphIdx;
  float Health;
  float InteractRange;
  float Speed;
  float TurnSpeed;
  float Acceleration;
  float JumpSpeed;
  float CollRadius;
  float AnimSpeed;
  int Team;
  int BlipType;
  Moby* AttachedMoby;
  u16 Bangles;
  char OnDeathType;
  char DeathToAttachedMoby;

  Moby* OnHitControllerMoby;
  Moby* OnKilledControllerMoby;

  float TargetHeight;
  float TargetRadius;
  float DamageCooldownSeconds;
  float MobTargetDistance;
  float HealthbarOffset;
  char MobTargetType;
  char DamageBubbles;
  char Targetable;
  struct NpcDifficultyConfig DifficultyConfigs[RAIDS_DIFFICULTY_COUNT];

  VECTOR SpawnPosition;
  VECTOR AttachedCuboidOffset;
  VECTOR AttachedMobyOffset;
  int TicksSinceLastDamage;
  float AttachedMobyYawOffset;
};

struct NpcPVar
{
  struct NpcParameters Parameters;
  struct MobPVar Mob;
};

struct NpcPVar* npcGetPVars(Moby* moby);
void npcStart(void);
void npcInit(void);

#endif // RAIDS_NPC_H
