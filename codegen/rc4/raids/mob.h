#ifndef RAIDS_MOB_H
#define RAIDS_MOB_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "zombie.h"
#include "stalkerturret.h"
//#include "executioner.h"
//#include "reactor.h"
//#include "tremor.h"
#include "swarmer.h"
#include "swamper.h"
#include "leviathan.h"
#include "dzstriker.h"
//#include "reaper.h"
#include "game.h"

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

// 
enum MobUnreliableMsgId {
  MOB_UNRELIABLE_MSG_ID_STATE_UPDATE
};

struct MobDamageEventArgs;
struct MobLocalDamageEventArgs;
struct MobSpawnEventArgs;
struct MobStateUpdateEventArgs;

typedef void (*MobGenericCallback_func)(Moby* moby);
typedef Moby* (*MobGetNextTarget_func)(Moby* moby);
typedef int (*MobGetPreferredAction_func)(Moby* moby, int * delayTicks);
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
  float AutoAggroMaxRange;
  float VisionRange;
  float RangedMaxDistanceToTarget;
  float PeripheryRangeTheta;
	u16 Bangles;
	u16 Xp;
	u16 AttackCooldownTickCount;
  u16 OutOfSightDeAggroTickCount;
	u8 ReactionTickCount;
};

struct MobSpawnedConfig {
	int Bolts;

	float Scale;
	float Damage;
	float Speed;
	float Health;

	float AttackRadius;
	float HitRadius;
  float CollRadius;

  float AutoAggroMaxRange;
  float VisionRange;
  float RangedMaxDistanceToTarget;
  float PeripheryRangeTheta;

	u16 Bangles;
	u16 Xp;
	u16 AttackCooldownTickCount;

  u16 OutOfSightDeAggroTickCount;
	u8 ReactionTickCount;
};

struct MobSpawnParams {
  MapOnMobCreate_func MobCreate;
  struct MobVTable* MobVTable;
  int RenderCost;
  float Scale;
  int OClass;
	struct MobConfig Config;
  char BlipType;
  char BlipTeam;
  char TeamPalette;
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
  VECTOR SumPositionDelta;
  VECTOR TargetPosition;
  VECTOR LastTargetPos;
  Moby* HitWallMoby;
  Moby* Target;
  float SumSpeedOver;
  float WallSlope;
  float PathEdgeAlpha;
  float LastPathEdgeAlphaForJump;
  float CollRadius;
  float PreferredHeight;
  float CurrentHeightLimit;
	u16 StuckCounter;
  char Grounded;
  char HitWall;
  char IsStuck;
  char MoveStep;
  char LastMoveStep;
  char PathGraphIdx;
  char ForceUseTargetPosition;
  char IsOwner;
  u8 WasStuckTicks;
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
	struct MobSpawnedConfig Config;
	struct Knockback Knockback;
  struct MobMoveVars MoveVars;
  int SpawnParamsIdx;
	int Action;
	int NextAction;
	int LastAction;
	float Health;
	float ClosestDistToLocal;
	float ClosestDistToPlayer;
  u32 Userdata;
	int LastAcidBy;
  float LastAcidByDamage;
	int LastHitBy;
	u16 LastHitByOClass;
	u16 NextCheckActionDelayTicks;
	u16 NextActionDelayTicks;
	u16 ActionCooldownTicks;
	u16 AttackCooldownTicks;
	u16 ScoutCooldownTicks;
	u16 FlinchCooldownTicks;
	u16 AutoDirtyCooldownTicks;
	u16 CurrentActionForTicks;
	u16 TimeLastGroundedTicks;
	u16 TimeTargetOutOfSightTicks;
	u16 AcidEffectActiveTicks;
  u16 FreezeEffectActiveTicks;
  u8 TargetOutOfSightCheckTicks;
	u8 ActionId;
	u8 LastActionId;
  u8 SlowTicks;
	char Owner;
	char AnimationLooped;
	char AnimationReset;
	char Destroy;
	char Respawn;
	char Dirty;
	char Destroyed;
	char Order;
	char Random;
	char DynamicRandom;
  char BlipType;
  char BlipTeam;
  char Behavior;
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
  int HasRemotePosDelta;
  VECTOR LastRemotePosDelta;
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
	float Position[3];
  float TargetPosition[3];
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
	u16 AttackRadiusEighths;
	u16 AttackCooldownTickCount;
  u8 SpawnParamsIdx;
	u8 Xp;
	u8 HitRadiusEighths;
  u8 CollRadiusEighths;
	u8 ReactionTickCount;
};

struct MobCreateArgs
{
  int SpawnParamsIdx;
  Moby* Parent;
  u32 Userdata;
  VECTOR Position;
  float Yaw;
  int SpawnFromUID;
  float DifficultyMult;
  struct MobConfig *Config;
  char Behavior;
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

int mobOnUnreliableMsgRemote(void* connection, void* data);
void mobReactToExplosionAt(int byPlayerId, VECTOR position, float damage, float radius);
void mobNuke(int killedByPlayerId);
int mobHandleEvent(Moby* moby, GuberEvent* event);
int mobCreate(struct MobCreateArgs* args);
void mobRegisterNpc(Moby* moby);
void mobPopulateSpawnArgsFromConfig(struct MobSpawnEventArgs* output, struct MobConfig* config, int spawnParamsIdx, int isBaseConfig, float difficultyMult);
void mobInitialize(void);
void mobTick(void);

#endif // RAIDS_MOB_H
