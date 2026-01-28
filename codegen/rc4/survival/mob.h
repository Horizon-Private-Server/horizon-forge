#ifndef SURVIVAL_MOB_H
#define SURVIVAL_MOB_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "zombie.h"
#include "executioner.h"
#include "reactor.h"
#include "tremor.h"
#include "swarmer.h"
#include "reaper.h"
#include "leviathan.h"
#include "game.h"

#define MOB_MAX_OTHER_TARGETS       (32)

enum MobAttributeType
{
  MOB_ATTRIBUTE_NONE = 0,
  MOB_ATTRIBUTE_FREEZE,
  MOB_ATTRIBUTE_ACID,
  MOB_ATTRIBUTE_GHOST,
  MOB_ATTRIBUTE_EXPLODE,
  MOB_ATTRIBUTE_RUSSIAN_DOLL,
  MOB_ATTRIBUTE_RANGED_ATTACK,
  MOB_ATTRIBUTE_BOSS,
  MOB_ATTRIBUTE_COUNT
};

enum MobEvent
{
  MOB_EVENT_SPAWN,
  MOB_EVENT_DESTROY,
  MOB_EVENT_DAMAGE,
  MOB_EVENT_STATE_UPDATE,
  MOB_EVENT_TARGET_UPDATE,
  MOB_EVENT_OWNER_UPDATE,
  MOB_EVENT_CUSTOM,
};

// what kind of spawn a mob can have
enum MobSpawnType {
  SPAWN_TYPE_DEFAULT_RANDOM = 0,
  SPAWN_TYPE_SEMI_NEAR_PLAYER = 1,
  SPAWN_TYPE_NEAR_PLAYER = 2,
  SPAWN_TYPE_ON_PLAYER = 4,
  SPAWN_TYPE_NEAR_HEALTHBOX = 8,
};

// 
enum MobSpawnFlags {
  MOB_SPAWN_FLAG_NONE = 0,
  MOB_SPAWN_FLAG_FREE_AGENT = 1,
  MOB_SPAWN_FLAG_RUSSIAN_DOLL = 2
};

// 
enum MobUnreliableMsgId {
  MOB_UNRELIABLE_MSG_ID_STATE_UPDATE
};

// 
enum MobTargetingRules
{
  MOB_TARGET_BIT_NEAREST = 0x00,
  MOB_TARGET_BIT_STRONGEST = 0x01,
  MOB_TARGET_BIT_WEAKEST = 0x02,
  MOB_TARGET_BIT_ANY = 0x00,
  MOB_TARGET_BIT_PLAYER = 0x10,
  MOB_TARGET_BIT_OTHER = 0x20,

  MOB_TARGET_MASK_STRATEGY = 0x0f,
  MOB_TARGET_MASK_TARGET = 0xf0,

  // ANY
  MOB_TARGET_NEAREST_ANY = (MOB_TARGET_BIT_ANY | MOB_TARGET_BIT_NEAREST),
  MOB_TARGET_STRONGEST_ANY = (MOB_TARGET_BIT_ANY | MOB_TARGET_BIT_STRONGEST),
  MOB_TARGET_WEAKEST_ANY = (MOB_TARGET_BIT_ANY | MOB_TARGET_BIT_WEAKEST),

  // PLAYER
  MOB_TARGET_NEAREST_PLAYER = (MOB_TARGET_BIT_PLAYER | MOB_TARGET_BIT_NEAREST),
  MOB_TARGET_STRONGEST_PLAYER = (MOB_TARGET_BIT_PLAYER | MOB_TARGET_BIT_STRONGEST),
  MOB_TARGET_WEAKEST_PLAYER = (MOB_TARGET_BIT_PLAYER | MOB_TARGET_BIT_WEAKEST),

  // OTHER
  MOB_TARGET_NEAREST_OTHER = (MOB_TARGET_BIT_OTHER | MOB_TARGET_BIT_NEAREST),
  MOB_TARGET_STRONGEST_OTHER = (MOB_TARGET_BIT_OTHER | MOB_TARGET_BIT_STRONGEST),
  MOB_TARGET_WEAKEST_OTHER = (MOB_TARGET_BIT_OTHER | MOB_TARGET_BIT_WEAKEST),
};

struct MobDamageEventArgs;
struct MobLocalDamageEventArgs;
struct MobSpawnEventArgs;
struct MobStateUpdateEventArgs;

typedef void (*MobGenericCallback_func)(Moby* moby);
typedef Moby* (*MobGetNextTarget_func)(Moby* moby);
typedef int (*MobGetPreferredAction_func)(Moby* moby, int * delayTicks);
typedef int (*MobGetExtraDataSize_func)(int spawnParamsIdx);
typedef void (*MobOnSpawning_func)(int spawnParamsIdx, VECTOR position, float* yaw, int* spawnFromUID, int* spawnFlags, char* random, struct MobSpawnEventArgs *args);
typedef void (*MobOnSpawn_func)(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
typedef void (*MobOnDestroy_func)(Moby* moby, int killedByPlayerId, int weaponId);
typedef void (*MobOnDamage_func)(Moby* moby, struct MobDamageEventArgs* e);
typedef int (*MobOnLocalDamage_func)(Moby* moby, struct MobLocalDamageEventArgs* e);
typedef void (*MobOnStateUpdate_func)(Moby* moby, struct MobStateUpdateEventArgs* e);
typedef int (*MobOnRespawn_func)(Moby* moby);
typedef void (*MobOnCustomEvent_func)(Moby* moby, GuberEvent* event);
typedef void (*MobForceLocalAction_func)(Moby* moby, int action);
typedef void (*MobDoDamage_func)(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
typedef short (*MobGetArmor_func)(Moby* moby);
typedef int (*MobIsAttacking_func)(Moby* moby);
typedef int (*MobCanNonOwnerTransitionToAction_func)(Moby* moby, int action);
typedef int (*MobShouldForceStateUpdateOnAction_func)(Moby* moby, int action);

struct MobVTable {
  MobGenericCallback_func PreUpdate;
  MobGenericCallback_func PostUpdate;
  MobGenericCallback_func PostDraw;
  MobGenericCallback_func Move;
  MobGetExtraDataSize_func GetExtraDataSize;
  MobOnSpawning_func OnSpawning;
  MobOnSpawn_func OnSpawn;
  MobOnDestroy_func OnDestroy;
  MobOnDamage_func OnDamage;
  MobOnLocalDamage_func OnLocalDamage;
  MobOnStateUpdate_func OnStateUpdate;
  MobOnRespawn_func OnRespawn;
  MobOnCustomEvent_func OnCustomEvent;
  MobGetNextTarget_func GetNextTarget;
  MobGetPreferredAction_func GetPreferredAction;
  MobForceLocalAction_func ForceLocalAction;
  MobGenericCallback_func DoAction;
  MobDoDamage_func DoDamage;
  MobGetArmor_func GetArmor;
  MobIsAttacking_func IsAttacking;
  MobCanNonOwnerTransitionToAction_func CanNonOwnerTransitionToAction;
  MobShouldForceStateUpdateOnAction_func ShouldForceStateUpdateOnAction;
};

struct MobConfig {
  int Bolts;
  float Damage;
  float MaxDamage;
  float DamageScale;
  float Speed;
  float MaxSpeed;
  float SpeedScale;
  float Health;
  float MaxHealth;
  float HealthScale;
  float AttackRadius;
  float HitRadius;
  float CollRadius;
  u16 Bangles;
  u16 Xp;
  u16 DamageCooldownTickCount;
  u8 ReactionTickCount;
  u8 AttackCooldownTickCount;
  char MobAttribute;
  char Behavior;
  char SharedXp;
};

struct MobSpawnParams {
  struct MobVTable* MobVTable;
  int RenderCost;
  float Scale;
  int OClass;
  int MaxSpawnedAtOnce;
  int MaxSpawnedPerRound;
  int MinRound;
  int CooldownTicks;
  float CooldownOffsetPerRoundFactor; // 0 is unchanged, -1 is -1 tick per round, +1 is +1 tick per round
  float Probability;
  float RangedAttackDistance;
  u32 BaseColor;
  u32 GlowColor;
  u32 SpriteColor;
  int SpriteTexId;
  int BossTexUid;
  enum MobSpawnType SpawnType;
  enum MobStatId StatId;
  char Name[32];
  struct MobConfig Config;
  char SpecialRoundOnly;
  char BlipType;
};

struct Knockback {
  short Angle;
  u8 Power;
  u8 Ticks;
  char Force;
};

struct MobMoveVars {
  VECTOR LastPosition;
  VECTOR NextPosition;
  VECTOR Velocity;
  VECTOR AddVelocity;
  VECTOR LastJumpPosition;
  VECTOR SumPositionDelta;
  VECTOR LastTargetPos;
  Moby* HitWallMoby;
  float SumSpeedOver;
  float WallSlope;
  float PathEdgeAlpha;
  float LastPathEdgeAlphaForJump;
  u16 StuckCounter;
  char Grounded;
  char HitWall;
  char IsStuck;
  char MoveStep;
  char LastMoveStep;
  char ForceUseTargetPosition;
  u8 UngroundedTicks;
  u8 StuckCheckTicks;
  u8 StuckJumpCount;
  u8 MoveSkipTicks;
  u8 QueueJumpSpeed;

  u8 PathEdgeCount;
  u8 PathEdgeCurrent;
  u8 PathHasReachedStart;
  u8 PathHasReachedEnd;
  u8 PathStartEndNodes[2];
  u8 PathTicks;
  u8 PathNewTicks;
  u8 PathCheckNearAndSeeTargetTicks;
  u8 PathCheckSkipEndTicks;
  u8 CurrentPath[20];
};

struct MobVars {
  struct MobConfig Config;
  struct Knockback Knockback;
  struct MobMoveVars MoveVars;
  int SpawnParamsIdx;
  int SpawnFlags;
  VECTOR TargetPosition;
  int Action;
  int NextAction;
  int LastAction;
  float Health;
  float ClosestDist;
  float LastSpeed;
  Moby * Target;
  int LastHitBy;
  u16 LastHitByOClass;
  u16 NextCheckActionDelayTicks;
  u16 NextActionDelayTicks;
  u16 ActionCooldownTicks;
  u16 AttackCooldownTicks;
  u16 ScoutCooldownTicks;
  u16 FlinchCooldownTicks;
  u16 AutoDirtyCooldownTicks;
  u16 ForcedBlipCooldownTicks;
  u16 TimeBombTicks;
  u16 MovingTicks;
  u16 CurrentActionForTicks;
  u16 TimeLastGroundedTicks;
  u16 LocalPlayerDamageHitInvTimer[GAME_MAX_LOCALS];
  u8 ActionId;
  u8 LastActionId;
  u8 SlowTicks;
  char Owner;
  char IsTraversing;
  char AnimationLooped;
  char AnimationReset;
  char OpacityFlickerDirection;
  char Destroy;
  char Respawn;
  char Dirty;
  char Destroyed;
  char Order;
  char Random;
  char DynamicRandom;
  char BlipType;
  char NoTargetCounter;
  char TargetingRule;
};

// warning: multiple differing types with the same name, only one recovered
struct ReactVars {
  /*   0 */ int flags;
  /*   4 */ int lastReactFrame;
  /*   8 */ Moby* pInfectionMoby;
  /*   c */ float acidDamage;
  /*  10 */ char eternalDeathCount;
  /*  11 */ char doHotSpotChecks;
  /*  12 */ char unchainable;
  /*  13 */ char isShielded;
  /*  14 */ char state;
  /*  15 */ char deathState;
  /*  16 */ char deathEffectState;
  /*  17 */ char deathStateType;
  /*  18 */ signed char knockbackRes;
  /*  19 */ char padC;
  /*  1a */ char padD;
  /*  1b */ char padE;
  /*  1c */ char padF;
  /*  1d */ char padG;
  /*  1e */ char padH;
  /*  1f */ char padI;
  /*  20 */ float minorReactPercentage;
  /*  24 */ float majorReactPercentage;
  /*  28 */ float deathHeight;
  /*  2c */ float bounceDamp;
  /*  30 */ int deadlyHotSpots;
  /*  34 */ float curUpGravity;
  /*  38 */ float curDownGravity;
  /*  3c */ float shieldDamageReduction;
  /*  40 */ float damageReductionSameOClass;
  /*  44 */ int deathCorn;
  /*  48 */ int deathType;
  /*  4c */ short int deathSound;
  /*  4e */ short int deathSound2;
  /*  50 */ float peakFrame;
  /*  54 */ float landFrame;
  /*  58 */ float drag;
  /*  5c */ short unsigned int effectStates;
  /*  5e */ short unsigned int effectPrimMask;
  /*  60 */ short unsigned int effectTimers[16];
};

struct MobPVar {
  struct TargetVars * TargetVarsPtr;
  char _pad0[0x0C];
  struct ReactVars * ReactVarsPtr;
  char _pad1[0x08];
  struct MoveVars_V2 * MoveVarsPtr;
  char _pad2[0x14];
   struct FlashVars * FlashVarsPtr;
  char _pad3[0x14];
   void * AdditionalMobVarsPtr;

  struct TargetVars TargetVars;
  struct ReactVars ReactVars;
  struct FlashVars FlashVars;
  struct MobVars MobVars;
  struct MobVTable* VTable;
  int TicksSinceLastStateUpdate;
};

struct MobDamageEventArgs
{
  struct Knockback Knockback;
  int SourceUID;
  u32 DamageFlags;
  u32 DamageQuarters;
  u16 SourceOClass;
};

struct MobLocalDamageEventArgs
{
  float Damage;
  u32 DamageFlags;
  Moby* Damager;
  Player* PlayerDamager;
};

struct MobActionUpdateEventArgs
{
  int Action;
  u8 ActionId;
  char Random;
};

struct MobStateUpdateEventArgs
{
  VECTOR Position;
  int TargetUID;
  int Action;
  u8 PathStartNodeIdx;
  u8 PathEndNodeIdx;
  u8 PathCurrentEdgeIdx;
  char PathHasReachedStart;
  char PathHasReachedEnd;
  u8 ActionId;
  char Random;
};

struct MobSpawnEventArgs
{
  int Bolts;
  int StartHealth;
  u16 Bangles;
  u16 SpeedEighths;
  u16 Damage;
  u16 Xp;
  u16 DamageCooldownTickCount;
  char MobType;
  char MobAttribute;
  char Behavior;
  u8 SpawnParamsIdx;
  u8 AttackRadiusEighths;
  u8 HitRadiusEighths;
  u8 CollRadiusEighths;
  u8 ReactionTickCount;
  u8 AttackCooldownTickCount;
};

struct MobUnreliableBaseMsgArgs
{
  int MsgId;
  u32 MobUID;
};

struct MobUnreliableMsgStateUpdateArgs
{
  struct MobUnreliableBaseMsgArgs Base;
  struct MobStateUpdateEventArgs StateUpdate;
};

float mobGetTargetRadius(Moby* target);
float mobGetDistanceToTarget(Moby* moby, Moby* target);
int mobOnUnreliableMsgRemote(void* connection, void* data);
void mobReactToExplosionAt(Moby* damager, VECTOR position, float damage, float radius, int bKnockback);
void mobNuke(int killedByPlayerId);
int mobHandleEvent(Moby* moby, GuberEvent* event);
int mobCreate(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config);
void mobInitialize(void);
void mobTick(void);

#endif // SURVIVAL_MOB_H
