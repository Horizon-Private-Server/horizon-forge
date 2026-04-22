#ifndef SURVIVAL_MOB_H
#define SURVIVAL_MOB_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "mobs/zombie.h"
#include "mobs/executioner.h"
#include "mobs/reactor.h"
#include "mobs/tremor.h"
#include "mobs/swarmer.h"
#include "mobs/swamper.h"
#include "mobs/reaper.h"
#include "mobs/leviathan.h"
#include "game.h"

#define MOB_MAX_OTHER_TARGETS (32)

// Damage flag constants used when mobs deal damage
#define MOB_DAMAGE_FLAG_BASE (0x00081801)
#define MOB_DAMAGE_FLAG_EXPLODE_BASE (0x00008801)
#define MOB_DAMAGE_FLAG_FREEZE (0x00800000)
#define MOB_DAMAGE_FLAG_ACID (0x00000080)
#define MOB_DAMAGE_FLAG_SHOCK (0x40)
#define MOB_DAMAGE_FLAG_SHORT_FREEZE (0x40000000)

// Armor health thresholds (fraction of max health)
#define MOB_ARMOR_THRESHOLD_LOW (0.3)
#define MOB_ARMOR_THRESHOLD_MID (0.5)
#define MOB_ARMOR_THRESHOLD_HIGH (0.7)

// Physics / movement constants
#define MOB_TERMINAL_VELOCITY (-10)
#define MOB_STUCK_CHECK_INTERVAL_TICKS (60)
#define MOB_STUCK_SPEED_THRESHOLD_FACTOR (0.25)
#define MOB_STUCK_EXPANSION_FACTOR (0.25)
#define MOB_CEILING_CHECK_HEIGHT (3)
#define MOB_GROUND_SNAP_EPSILON (0.01)
#define MOB_WALK_ANGLE_NEAR_TARGET_DIST (20)
#define MOB_KNOCKBACK_POWER_EXPONENT_BASE (1.1)
#define MOB_INCOMING_PROJECTILE_RADIUS (7)
#define MOB_HAS_VELOCITY_THRESHOLD (0.0001)
#define MOB_DAMAGE_FIRST_PASS_RADIUS_EXTRA (5)

enum MobAttributeType
{
	MOB_ATTRIBUTE_NONE = 0,
	MOB_ATTRIBUTE_FREEZE = 1,
	MOB_ATTRIBUTE_ACID = 2,
	MOB_ATTRIBUTE_GHOST = 3,
	MOB_ATTRIBUTE_BOSS = 6,
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
enum MobSpawnType
{
	SPAWN_TYPE_DEFAULT_RANDOM = 0,
	SPAWN_TYPE_SEMI_NEAR_PLAYER = 1,
	SPAWN_TYPE_NEAR_PLAYER = 2,
	SPAWN_TYPE_ON_PLAYER = 4,
	SPAWN_TYPE_NEAR_HEALTHBOX = 8,
};

//
enum MobSpawnFlags
{
	MOB_SPAWN_FLAG_NONE = 0,
	MOB_SPAWN_FLAG_FREE_AGENT = 1
};

//
enum MobUnreliableMsgId
{
	MOB_UNRELIABLE_MSG_ID_STATE_UPDATE
};

//
enum MOB_DO_DAMAGE_HIT_FLAGS
{
	MOB_DO_DAMAGE_HIT_FLAG_NONE = 0,
	MOB_DO_DAMAGE_HIT_FLAG_HIT_TARGET = 1,
	MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER = 2,
	MOB_DO_DAMAGE_HIT_FLAG_HIT_MOB = 4,
	MOB_DO_DAMAGE_HIT_FLAG_HIT_PLAYER_THORNS = 8,
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
struct MobFullStateUpdateEventArgs;

typedef void (*MobGenericCallback_func)(Moby *moby);
typedef Moby *(*MobGetNextTarget_func)(Moby *moby);
typedef int (*MobGetPreferredState_func)(Moby *moby, int *delayTicks);
typedef int (*MobGetExtraDataSize_func)(int spawnParamsIdx);
typedef void (*MobOnSpawning_func)(int spawnParamsIdx, VECTOR position, float *yaw, int *spawnFromUID, int *spawnFlags, char *random, struct MobSpawnEventArgs *args);
typedef void (*MobOnSpawn_func)(Moby *moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs *e);
typedef void (*MobOnDestroy_func)(Moby *moby, int killedByPlayerId, int weaponId);
typedef void (*MobOnDamage_func)(Moby *moby, struct MobDamageEventArgs *e);
typedef int (*MobOnLocalDamage_func)(Moby *moby, struct MobLocalDamageEventArgs *e);
typedef void (*MobOnFullStateUpdate_func)(Moby *moby, struct MobFullStateUpdateEventArgs *e);
typedef int (*MobOnRespawn_func)(Moby *moby);
typedef void (*MobOnCustomEvent_func)(Moby *moby, GuberEvent *event);
typedef void (*MobForceLocalState_func)(Moby *moby, int state);
typedef void (*MobDoDamage_func)(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire);
typedef short (*MobGetArmor_func)(Moby *moby);
typedef int (*MobIsAttacking_func)(Moby *moby);
typedef int (*MobCanNonOwnerTransitionToState_func)(Moby *moby, int state);
typedef int (*MobShouldForceStateUpdateOnState_func)(Moby *moby, int state);

struct MobVTable
{
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
	MobOnFullStateUpdate_func OnFullStateUpdate;
	MobOnRespawn_func OnRespawn;
	MobOnCustomEvent_func OnCustomEvent;
	MobGetNextTarget_func GetNextTarget;
	MobGetPreferredState_func GetPreferredState;
	MobForceLocalState_func ForceLocalState;
	MobGenericCallback_func DoState;
	MobDoDamage_func DoDamage;
	MobGetArmor_func GetArmor;
	MobIsAttacking_func IsAttacking;
	MobCanNonOwnerTransitionToState_func CanNonOwnerTransitionToState;
	MobShouldForceStateUpdateOnState_func ShouldForceStateUpdateOnState;
};

struct MobConfig
{
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
	u16 AttackCooldownTickCount;
	u8 ReactionTickCount;
	char MobAttribute;
	char Behavior;
	char SharedXp;
};

struct MobSpawnParams
{
	struct MobVTable *MobVTable;
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

struct Knockback
{
	short Angle;
	u8 Power;
	u8 Ticks;
	char Force;
};

struct MobMoveVars
{
	VECTOR LastPosition;
	VECTOR NextPosition;
	VECTOR Velocity;
	VECTOR AddVelocity;
	VECTOR LastJumpPosition;
	VECTOR SumPositionDelta;
	VECTOR LastTargetPos;
	Moby *HitWallMoby;
	float SumSpeedOver;
	float WallSlope;
	float PathEdgeAlpha;
	float LastPathEdgeAlphaForJump;
	u16 StuckCounter;
	char Grounded;
	char HitWall;
	char IsStuck;
	u8 MoveStep;
	u8 LastMoveStep;
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

struct MobVars
{
	struct MobConfig Config;
	struct Knockback Knockback;
	struct MobMoveVars MoveVars;
	int SpawnParamsIdx;
	int SpawnFlags;
	VECTOR TargetPosition;
	int State;
	int NextState;
	int LastState;
	float Health;
	float ClosestDist;
	float LastSpeed;
	Moby *Target;
	int LastHitBy;
	u16 LastHitByOClass;
	u16 NextCheckStateDelayTicks;
	u16 NextStateDelayTicks;
	u16 StateCooldownTicks;
	u16 AttackCooldownTicks;
	u16 ScoutCooldownTicks;
	u16 FlinchCooldownTicks;
	u16 AutoDirtyCooldownTicks;
	u16 ForcedBlipCooldownTicks;
	u16 TimeBombTicks;
	u16 MovingTicks;
	u16 CurrentStateForTicks;
	u16 TimeLastGroundedTicks;
	u16 LocalPlayerDamageHitInvTimer[GAME_MAX_LOCALS];
	u8 StateId;
	u8 LastStateId;
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
struct ReactVars
{
	/*   0 */ int flags;
	/*   4 */ int lastReactFrame;
	/*   8 */ Moby *pInfectionMoby;
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

struct MobPVar
{
	struct TargetVars *TargetVarsPtr;
	char _pad0[0x0C];
	struct ReactVars *ReactVarsPtr;
	char _pad1[0x08];
	struct MoveVars_V2 *MoveVarsPtr;
	char _pad2[0x14];
	struct FlashVars *FlashVarsPtr;
	char _pad3[0x14];
	void *AdditionalMobVarsPtr;

	struct TargetVars TargetVars;
	struct ReactVars ReactVars;
	struct FlashVars FlashVars;
	struct MobVars MobVars;
	struct MobVTable *VTable;
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
	Moby *Damager;
	Player *PlayerDamager;
};

struct MobStateUpdateEventArgs
{
	int State;
	u8 StateId;
	char Random;
};

struct MobFullStateUpdateEventArgs
{
	VECTOR Position;
	int TargetUID;
	int State;
	u8 PathStartNodeIdx;
	u8 PathEndNodeIdx;
	u8 PathCurrentEdgeIdx;
	char PathHasReachedStart;
	char PathHasReachedEnd;
	u8 StateId;
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
	u16 AttackCooldownTickCount;
	char MobType;
	char MobAttribute;
	char Behavior;
	u8 SpawnParamsIdx;
	u8 AttackRadiusEighths;
	u8 HitRadiusEighths;
	u8 CollRadiusEighths;
	u8 ReactionTickCount;
};

struct MobUnreliableBaseMsgArgs
{
	int MsgId;
	u32 MobUID;
};

struct MobUnreliableMsgStateUpdateArgs
{
	struct MobUnreliableBaseMsgArgs Base;
	struct MobFullStateUpdateEventArgs StateUpdate;
};

GuberEvent *mobCreateEvent(Moby *moby, u32 eventType);
float mobGetTargetRadius(Moby *target);
float mobGetDistanceToTarget(Moby *moby, Moby *target);
void mobRegisterTarget(Moby *moby);
int mobOnUnreliableMsgRemote(void *connection, void *data);
void mobReactToExplosionAt(Moby *damager, VECTOR position, float damage, float radius, int knockbackPower);
void mobNuke(int killedByPlayerId);
int mobHandleEvent(Moby *moby, GuberEvent *event);
int mobCreate(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config);
void mobInitialize(void);
void mobTick(void);

int mobAmIOwner(Moby *moby);
int mobIsFrozen(Moby *moby);
int mobGetBehavior(Moby *moby);
void mobResetSoundTrigger(Moby *moby);
void mobSpawnCorn(Moby *moby, int bangle);
int mobDoDamage(Moby *moby, float radius, float amount, int damageFlags, int friendlyFire, int jointId, int reactToThorns, int isAoE);
int mobDoSweepDamage(Moby *moby, VECTOR from, VECTOR to, float step, float radius, float amount, int damageFlags, int friendlyFire, int reactToThorns, int isAoE);
int mobDoDamageTryHit(Moby *moby, Moby *hitMoby, VECTOR jointPosition, int isAoE, float hitRadius, int damageFlags, float amount);
void mobSetState(Moby *moby, int state);
void mobTransAnimLerp(Moby *moby, int animId, int lerpFrames, float startOff);
void mobTransAnim(Moby *moby, int animId, float startOff);
int mobHasVelocity(struct MobPVar *pvars);
float mobGetCurrentMoveSpeed(Moby *moby);
void mobGetKnockbackVelocity(Moby *moby, VECTOR out);
void mobGetTargetCenter(Moby *target, VECTOR out);
int mobCanSeeMoby(Moby *moby, Moby *canSeeMoby);
void mobStand(Moby *moby);
void mobResetMoveStep(Moby *moby);
int mobMoveCheck(Moby *moby, VECTOR outputPos, VECTOR from, VECTOR to);
void mobMove(Moby *moby);
void mobMoveTowards(Moby *moby, VECTOR targetPosition, float speed, float turnSpeed, float acceleration, float curveNearTargetDir);
void mobJumpTowards(Moby *moby, VECTOR targetPosition);
int mobHitWallShouldJump(Moby *moby, float maxSlope);
float mobTurnTowards(Moby *moby, VECTOR towards, float turnSpeed);
float mobTurnTowardsPredictive(Moby *moby, Moby *target, float turnSpeed, float predictFactor);
float mobTurnTowardsPredictiveWithSpeed(Moby *moby, Moby *target, float turnSpeed, float speed);
void mobGetVelocityToTargetWithDirection(Moby *moby, VECTOR velocity, VECTOR from, VECTOR to, float yaw, float speed, float acceleration);
void mobGetVelocityToTarget(Moby *moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobGetVelocityToTargetSimple(Moby *moby, VECTOR velocity, VECTOR from, VECTOR to, float speed, float acceleration);
void mobPostDrawQuad(Moby *moby, float scale, u32 color, int jointId);
void mobOnFullStateUpdate(Moby *moby, struct MobFullStateUpdateEventArgs *e);
void mobPreUpdate(Moby *moby);
int mobIsProjectileComing(Moby *moby);
float mobGetCurrentWalkAngle(Moby *moby);
float mobGetScaleMultiplier(Moby *moby);
Moby *mobGetNextTarget(Moby *moby, float keepCurrentTargetFactor);
void mobDefaultPreUpdate(Moby *moby);
int mobDefaultOnLocalDamage(Moby *moby, struct MobLocalDamageEventArgs *e);
void mobHandleFlinch(Moby *moby, struct MobDamageEventArgs *e, int canFlinch, int isShock, float probability, float powerFactor, int flinchState, int bigFlinchState);
u32 mobGetDamageFlags(Moby *moby, u32 damageFlags);

#endif // SURVIVAL_MOB_H
