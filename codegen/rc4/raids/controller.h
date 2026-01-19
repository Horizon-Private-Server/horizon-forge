#ifndef RAIDS_CONTROLLER_H
#define RAIDS_CONTROLLER_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define CONTROLLER_OCLASS                         (0x4003)
#define CONTROLLER_MAX_CONDITIONS                 (8)
#define CONTROLLER_MAX_TARGETS                    (8)
#define CONTROLLER_PLAYER_MASK_HOST               (1 << 10)

enum ControllerEventType {
	CONTROLLER_EVENT_SPAWN,
  CONTROLLER_EVENT_SET_STATE,
  CONTROLLER_EVENT_ITERATE,
  CONTROLLER_EVENT_INIT
};

enum ControllerState {
	CONTROLLER_STATE_DEACTIVATED,
	CONTROLLER_STATE_IDLE,
	CONTROLLER_STATE_ACTIVATED,
	CONTROLLER_STATE_COMPLETED = 127,
};

enum ControllerConditionType {
  CONTROLLER_CONDITION_TYPE_NONE,
  CONTROLLER_CONDITION_TYPE_MOBY_STATE,
  CONTROLLER_CONDITION_TYPE_CUBOID,
  CONTROLLER_CONDITION_TYPE_PLAYER_BUTTON,
  CONTROLLER_CONDITION_TYPE_DELAY,
  CONTROLLER_CONDITION_TYPE_XOR,
  CONTROLLER_CONDITION_TYPE_NPC_TARGET,
  CONTROLLER_CONDITION_TYPE_DIFFICULTY,
  CONTROLLER_CONDITION_TYPE_CHECKPOINT,
  CONTROLLER_CONDITION_TYPE_CHANCE,
  CONTROLLER_CONDITION_TYPE_HEALTH,
  CONTROLLER_CONDITION_TYPE_PLAYER_KILLS,
  CONTROLLER_CONDITION_TYPE_PLAYER_HEALTH,
  CONTROLLER_CONDITION_TYPE_COUNTER,
  CONTROLLER_CONDITION_TYPE_CHALLENGE,
  CONTROLLER_CONDITION_TYPE_PLAYER_COUNT,
};

enum ControllerCompareType {
	CONTROLLER_COMPARE_EQUAL,
	CONTROLLER_COMPARE_NOTEQUAL,
	CONTROLLER_COMPARE_CHANGED_TO,
	CONTROLLER_COMPARE_LESS,
	CONTROLLER_COMPARE_LEQUAL,
	CONTROLLER_COMPARE_GREATER,
	CONTROLLER_COMPARE_GEQUAL,
	CONTROLLER_COMPARE_INCREASED_BY,
	CONTROLLER_COMPARE_DECREASED_BY,
	CONTROLLER_COMPARE_INCREASED_BY_AT_LEAST,
	CONTROLLER_COMPARE_DECREASED_BY_AT_LEAST,
	CONTROLLER_COMPARE_INCREASED,
	CONTROLLER_COMPARE_DECREASED,
	CONTROLLER_COMPARE_CHANGED,
	CONTROLLER_COMPARE_UNCHANGED,
};

enum ControllerCuboidInteractType {
	CONTROLLER_CUBOID_INTERACT_WHEN_INSIDE,
	CONTROLLER_CUBOID_INTERACT_WHEN_OUTSIDE,
};

enum ControllerCuboidTriggerBy {
  CONTROLLER_CUBOID_TRIGGER_BY_NONE = 0,
	CONTROLLER_CUBOID_TRIGGER_BY_ANY_PLAYER = 1 << 0,
	CONTROLLER_CUBOID_TRIGGER_BY_ALL_PLAYERS = 1 << 1,
	CONTROLLER_CUBOID_TRIGGER_BY_NO_PLAYERS = 1 << 2,
	CONTROLLER_CUBOID_TRIGGER_BY_ANY_NPC = 1 << 3,
	CONTROLLER_CUBOID_TRIGGER_BY_ALL_NPCS = 1 << 4,
	CONTROLLER_CUBOID_TRIGGER_BY_NO_NPCS = 1 << 5,
	CONTROLLER_CUBOID_TRIGGER_BY_MOBY = 1 << 6,
	CONTROLLER_CUBOID_TRIGGER_BY_HOST = 1 << 7,

	CONTROLLER_CUBOID_TRIGGER_BY_CHECK_ALL_PLAYERS = CONTROLLER_CUBOID_TRIGGER_BY_ANY_PLAYER | CONTROLLER_CUBOID_TRIGGER_BY_ALL_PLAYERS | CONTROLLER_CUBOID_TRIGGER_BY_NO_PLAYERS,
	CONTROLLER_CUBOID_TRIGGER_BY_CHECK_NPC = CONTROLLER_CUBOID_TRIGGER_BY_ANY_NPC | CONTROLLER_CUBOID_TRIGGER_BY_ALL_NPCS | CONTROLLER_CUBOID_TRIGGER_BY_NO_NPCS,
};

enum ControllerCounterUpdateType {
	CONTROLLER_COUNTER_SET,
	CONTROLLER_COUNTER_ADD,
	CONTROLLER_COUNTER_SUB,
	CONTROLLER_COUNTER_MUL,
	CONTROLLER_COUNTER_DIV,
	CONTROLLER_COUNTER_MAX,
	CONTROLLER_COUNTER_MIN,
};

enum ControllerTargetUpdateType {
  CONTROLLER_TARGET_UPDATE_TYPE_NONE,
  CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE,
  CONTROLLER_TARGET_UPDATE_TYPE_MOBY_ANIMATION,
  CONTROLLER_TARGET_UPDATE_TYPE_MOBY_ENABLED,
  CONTROLLER_TARGET_UPDATE_TYPE_MOVE_CUBOID,
  CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE_ADDITIVE,
  CONTROLLER_TARGET_UPDATE_TYPE_NPC_CONTROLLER_TARGET,
  CONTROLLER_TARGET_UPDATE_TYPE_NPC_CONTROLLER_TARGET_TO_TRIGGERED,
  CONTROLLER_TARGET_UPDATE_TYPE_GIVE_PLAYER_AMMO,
  CONTROLLER_TARGET_UPDATE_TYPE_GIVE_PLAYER_HEALTH,
  CONTROLLER_TARGET_UPDATE_TYPE_RESPAWN,
  CONTROLLER_TARGET_UPDATE_TYPE_COMPLETE_MISSION,
  CONTROLLER_TARGET_UPDATE_TYPE_MOBY_SET_CHECKPOINT,
  CONTROLLER_TARGET_UPDATE_TYPE_SET_AMMO_DROP_PROBABILITY,
  CONTROLLER_TARGET_UPDATE_TYPE_SET_REFILL_AMMO_COST_MULTIPLIER,
  CONTROLLER_TARGET_UPDATE_TYPE_SET_MUSIC_TRACK,
  CONTROLLER_TARGET_UPDATE_TYPE_FAIL_MISSION,
  CONTROLLER_TARGET_UPDATE_TYPE_UPDATE_CHALLENGE,
  CONTROLLER_TARGET_UPDATE_TYPE_UPDATE_COUNTER,
};

struct ControllerRuntimeState
{
  char TriggersActivated;
  int Iterations;
  int RemoteIterationTime;
  int DelayStartTime[CONTROLLER_MAX_CONDITIONS];
  float LastValue[CONTROLLER_MAX_CONDITIONS];
  float CounterValue[CONTROLLER_MAX_CONDITIONS];
  Moby* TriggeredByMoby;
};

struct ControllerCondition
{
  char ConditionType;
  short MobyUID;
  Moby* Moby;

  union {
    
    // trigger if moby state
    struct {
      short CompareType;
      short State;
    } MobyState;
    
    // trigger if cuboid
    struct {
      int CuboidIdx;
      short InteractType;
      char TriggerBy;
      char TriggerByAll;
    } Cuboid;
    
    // trigger if player button
    struct {
      short PlayerMask;
      short PadMask;
    } PlayerButtons;
    
    // trigger if delay
    struct {
      int Milliseconds;
    } Delay;

    // trigger if npc target
    struct {
      int CuboidIdx;
      short InteractType;
    } NPCTarget;

    // trigger if difficulty
    struct {
      int Mask;
    } Difficulty;

    // trigger if checkpoint
    struct {
      char IsActive;
    } Checkpoint;

    // trigger if chance
    struct {
      float Probability;
    } Chance;

    // trigger if moby health
    struct {
      float Value;
      char CompareType;
      char Normalized;
    } Health;

    // trigger if weapon kills
    struct {
      short PlayerMask;
      short WeaponMask;
      short MobMask;
      short Value;
      char CompareType;
      char MatchAllPlayers;
      char MatchAllWeapons;
      char Aggregate;
    } PlayerKills;

    // trigger if player health
    struct {
      short PlayerMask;
      char CompareType;
      char MatchAllPlayers;
      float Value;
      char Normalized;
    } PlayerHealth;

    // trigger if counter
    struct {
      float Value;
      char CompareType;
    } Counter;

    // trigger if Challenge
    struct {
      int ChallengeIdx;
      char Value;
    } Challenge;

    // trigger if player button
    struct {
      short CountMask;
      char Filter;
    } PlayerCount;
    
  };
};

struct ControllerTarget
{
  char TargetUpdateType;
  char DifficultyMask;
  union {
    // mobys
    struct {
      Moby* Moby;
      union {
        struct {
          int State;
        };
        struct {
          int AnimId;
        };
        struct {
          int Enabled;
        };
        struct {
          Moby* NPCTargetMoby;
        };
      };
    } Moby;

    // cuboids
    struct {
      int DestIdx;
      int SrcIdx;
    } Cuboid;
    
    // give player
    struct {
      int PlayerMask;
      char IsPercent;
      char LivingOnly;
      short Amount;
    } GivePlayer;
    
    // respawn player
    struct {
      int PlayerMask;
      char TriggeredOnly;
      char DeadOnly;
    } RespawnPlayer;

    // respawn player
    struct {
      short TrackId;
      char SkipTransition;
      char Force;
      char Loop;
    } Music;

    // values
    struct {
      union {
        float FloatValue;
        int IntValue;
        char CharValue;
        short ShortValue;
      };
    } Value;

    // challenge
    struct {
      int ChallengeIdx;
    } Challenge;

    // counter moby
    struct {
      Moby* Moby;
      float UpdateValue;
      char UpdateType;
      char CounterValueIdx;
    } Counter;
  };
};

struct ControllerPVar
{
  char Init;
  char PADDING[2];
  char CountNoTrigger; // if true, will count failed trigger ticks towards total # of repeats
  char DefaultState;
  char Log;
  char TriggerIfAllTrue;
  char NoSync;
  struct ControllerTarget Targets[CONTROLLER_MAX_TARGETS];
  short Repeat;
  struct ControllerCondition Conditions[CONTROLLER_MAX_CONDITIONS];
  struct ControllerRuntimeState State;
};

int controllerAmIOwner(Moby* moby);
void controllerSetTriggerMoby(Moby* moby, Moby* triggerMoby);
void controllerBroadcastNewState(Moby* moby, enum ControllerState state);
struct Guber* controllerGetGuber(Moby* moby);
int controllerHandleEvent(Moby* moby, GuberEvent* event);
void controllerStart(void);
void controllerInit(void);

#endif // RAIDS_CONTROLLER_H
