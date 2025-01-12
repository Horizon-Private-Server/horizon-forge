#ifndef RAIDS_GAME_H
#define RAIDS_GAME_H

#include <tamtypes.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "messageid.h"
#include "bank.h"

#define MAP_CONFIG_MAGIC                      (0xDEADBEEF)

#define RAIDS_HUB_MAPFILENAME                 ("raids_hub")
#define RAIDS_MAX_EXDATA_SIZE                 (2048)

#define TPS																		(60)

#define NPC_MOBY_OCLASS                       (0x4006)
#define COUNTER_MOBY_OCLASS                   (0x400F)

#define GRAVITY_MAGNITUDE                     (15 * MATH_DT)

#define MOBS_PLAY_SOUND_COOLDOWN              (10)
#define MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS (20)

#define MAX_MOBS_BASE													(10)
#define MAX_MOBS_ROUND_WEIGHT									(10)
#define MAX_MOBS_ALIVE											  (50)
#define MAX_MOBS_ALIVE_BUFFER									(10)
#define MAX_MOBS_ALIVE_REAL									  (MAX_MOBS_ALIVE - MAX_MOBS_ALIVE_BUFFER)

#define MOB_TARGET_DIST_IN_SIGHT_IGNORE_PATH 	(100)
#define MOB_MOVE_SKIP_TICKS                   (8)
#define MOB_MAX_STUCK_COUNTER_FOR_NEW_PATH    (3)

#define MOB_SHORT_FREEZE_DURATION_TICKS       (60)
#define MOB_SHORT_FREEZE_SPEED_FACTOR         (0.15)

#define MOB_SPAWN_SEMI_NEAR_PLAYER_PROBABILITY 		(1)
#define MOB_SPAWN_NEAR_PLAYER_PROBABILITY 				(0.25)
#define MOB_SPAWN_AT_PLAYER_PROBABILITY 					(0.01)
#define MOB_SPAWN_NEAR_HEALTHBOX_PROBABILITY 			(0.1)

#define MOB_SPAWN_BURST_MIN_DELAY							(1 * 60)
#define MOB_SPAWN_BURST_MAX_DELAY							(10 * 60)
#define MOB_SPAWN_BURST_MIN										(3)
#define MOB_SPAWN_BURST_MAX										(10)
#define MOB_SPAWN_BURST_MAX_INC_PER_ROUND			(1)
#define MOB_SPAWN_BURST_MIN_INC_PER_ROUND			(0)

#define MOB_AUTO_DIRTY_COOLDOWN_TICKS			    (60 * 5)

#define MOB_BASE_DAMAGE										    (10)
#define MOB_BASE_DAMAGE_SCALE                 (0.03*1)
#define MOB_BASE_SPEED											  (3)
#define MOB_BASE_SPEED_SCALE                  (0.05*1)
#define MOB_BASE_HEALTH										    (30)
#define MOB_BASE_HEALTH_SCALE                 (0.05*1)
#define MOB_JUMP_MOVE_SPEED                   (10)

#define MAX_MOB_AMMO_DROPS                    (10)
#define MOB_SPECIAL_MUTATION_PROBABILITY		  (0.005)
#define MOB_SPECIAL_MUTATION_BASE_COST			  (200)
#define MOB_SPECIAL_MUTATION_REL_COST			    (1.0)

#if PAYDAY
#define MOB_BASE_BOLTS											  (1000000)
#else
#define MOB_BASE_BOLTS											  (220)
#endif

#define MOB_POSTFX_ACID_COLOR                 (0x00008000)
#define MOB_POSTFX_FREEZE_COLOR               (0x00808000)
#define MOB_POSTFX_FACTOR                     (0.75)
#define MOB_POSTFX_ACID_DUR_TICKS             (TPS * 5)
#define MOB_POSTFX_ACID_FREQ_TICKS            ((int)(TPS * 0.5))
#define MOB_POSTFX_ACID_DMG_PERC              (0.05)
#define MOB_POSTFX_FREEZE_DUR_TICKS           (TPS * 5)
#define MOB_POSTFX_FREEZE_FACTOR              (0.75)
#define MOB_POSTFX_NAPALM_DMG_PERC            (0.05)
#define MOB_POSTFX_MINIBOMB_DMG_PERC          (0.15)

#define JACKPOT_BOLTS													(50)
#define XP_ALPHAMOD_XP												(10)
#define NANOLEECH_HEALTH											(5)
#define NANOLEECH_CHANCE											(0.01)

#define LEVELUP_MAX_LEVEL                     (98)
#define LEVELUP_PLAYER_LINEAR_FACTOR          (100)
#define LEVELUP_PLAYER_INCREMENT_AMOUNT       (25)

#define PLAYER_BASE_REVIVE_TICKS					    (60 * TPS)
#define PLAYER_MIN_REVIVE_TICKS					      (10 * TPS)
#define PLAYER_REVIVE_COST_PER_REVIVE_TICKS		(10 * TPS)
#define PLAYER_TIME_TO_REVIVE_TICKS		        (5 * TPS)
#define PLAYER_REVIVE_MAX_DIST								(2.5)
#define PLAYER_REVIVE_COOLDOWN_TICKS					(120)
#define PLAYER_KNOCKBACK_BASE_POWER						(3.0)
#define PLAYER_KNOCKBACK_BASE_TICKS						(10)
#define PLAYER_COLL_RADIUS          					(0.5)

#define PLAYER_SKILLPOINT_DAMAGE_FACTOR       (0.08)
#define PLAYER_SKILLPOINT_SPEED_FACTOR        (0.03)
#define PLAYER_SKILLPOINT_HEALTH_FACTOR       (5)

#define SNACK_ITEM_MAX_COUNT                  (16)
#define DAMAGE_BUBBLE_MAX_COUNT               (16)

#define MAX_MOB_SPAWN_PARAMS                  (16)
#define MAX_MOB_COMPLEXITY_DRAWN              (7500)
#define MAX_MOB_COMPLEXITY_DRAWN_DZO          (MAX_MOB_COMPLEXITY_DRAWN * 1)
#define MOB_COMPLEXITY_SKIN_FACTOR            (500)
#define MAX_MOB_COMPLEXITY_MIN                (1000)
#define MOB_COMPLEXITY_LOD_FACTOR             (500)
#define MOB_MAX_FLINCH_PROBABILITY            (0.25)

#define GAME_DEFAULT_AMMO_DROP_CHANCE         (0.1)
#define GAME_DEFAULT_LOOT_DROP_CHANCE         (0.005)

enum GameNetMessage
{
	CUSTOM_MSG_MOB_UNRELIABLE_MSG = CUSTOM_MSG_ID_GAME_MODE_START,
  CUSTOM_MSG_BEGIN_WORLD_HOP,
  CUSTOM_MSG_WORLD_HOP_MISSING_MAP,
  CUSTOM_MSG_SET_PLAYER_EQUIPPED_INVENTORY,
  CUSTOM_MSG_SET_PLAYER_ACCOUNT,
  CUSTOM_MSG_SET_MISSION_FAILED,
  CUSTOM_MSG_USE_LIFE,
};

enum RaidsCustomMenus
{
  RAIDS_CUSTOM_MENU_NONE = 0,
  RAIDS_CUSTOM_MENU_INVENTORY,
  RAIDS_CUSTOM_MENU_LEVELSELECT,
  RAIDS_CUSTOM_MENU_STORE,
  RAIDS_CUSTOM_MENU_SKILLS,
};

enum RaidsDifficultys
{
  RAIDS_DIFFICULTY_1STAR = 0,
  RAIDS_DIFFICULTY_2STAR,
  RAIDS_DIFFICULTY_3STAR,
  RAIDS_DIFFICULTY_4STAR,
  RAIDS_DIFFICULTY_5STAR,
  RAIDS_DIFFICULTY_COUNT
};

enum RaidsMissionStatus
{
  RAIDS_MISSION_ACTIVE = 0,
  RAIDS_MISSION_FAILED = 1,
  RAIDS_MISSION_COMPLETED = 2,
};

enum MobDamageSource
{
  MOB_DAMAGE_SOURCE_UNKNOWN = 0,
  MOB_DAMAGE_SOURCE_WRENCH,
  MOB_DAMAGE_SOURCE_DUAL_VIPERS,
  MOB_DAMAGE_SOURCE_MAGMA_CANNON,
  MOB_DAMAGE_SOURCE_ARBITER,
  MOB_DAMAGE_SOURCE_FUSION_RIFLE,
  MOB_DAMAGE_SOURCE_MINE_LAUNCHER,
  MOB_DAMAGE_SOURCE_B6_OBLITERATOR,
  MOB_DAMAGE_SOURCE_SCORPION_FLAIL,
  MOB_DAMAGE_SOURCE_HOLOSHIELD,
  MOB_DAMAGE_SOURCE_PUMA,
  MOB_DAMAGE_SOURCE_HOVERBIKE,
  MOB_DAMAGE_SOURCE_LANDSTALKER,
  MOB_DAMAGE_SOURCE_HOVERSHIP,
  MOB_DAMAGE_SOURCE_COUNT
};

struct RaidsBankMapStats;
struct MobConfig;
struct MobSpawnEventArgs;
struct MobCreateArgs;

typedef void (*PushSnack_func)(char * string, int ticksAlive, int localPlayerIdx);
typedef void (*PushDamageBubble_func)(VECTOR position, float randomRadius, float damage, int isLocal, int isCrit);
typedef long (*GetAmmoRefillCost_func)(Player* player);
typedef void (*BeginWorldHop_func)(char* mapFilename, int difficulty, int cost, int delayMs);
typedef void (*PopulateSpawnArgs_func)(struct MobSpawnEventArgs* output, struct MobConfig* config, int spawnParamsIdx, int isBaseConfig, float difficultyMult);
typedef void (*RegisterNpc_func)(Moby* moby);
typedef int (*OnGuberEvent_func)(Moby* moby, GuberEvent* event);
typedef struct Guber* (*OnGetGuber_func)(Moby* moby);
typedef int (*TryCreateMob_func)(struct MobCreateArgs* args);
typedef void (*RequestPrestigeLoot_func)(int gadgetId);
typedef void (*OnMissionComplete_func)(int cuboidIdx);
typedef void (*OnMissionFail_func)(void);

typedef void (*MapOnMobSpawned_func)(Moby* moby);
typedef int (*MapOnMobCreate_func)(struct MobCreateArgs* args);
typedef void (*MapOnMobUpdate_func)(Moby* moby);
typedef void (*MapOnMobKilled_func)(Moby* moby, int killedByPlayerId, enum MobDamageSource source);
typedef void (*MapOnMobDestroyed_func)(Moby* moby);
typedef void (*MapCreateAmmoDropAt_func)(Moby* moby);
typedef void (*FrameTick_func)(void);

struct RaidsPlayerState
{
  u32 Bolts;
  float Experience;
	int Kills;
	int Deaths;
	int AllKills[MOB_DAMAGE_SOURCE_COUNT-1][MAX_MOB_SPAWN_PARAMS];
  u16 Skills[RAIDS_SKILLS_COUNT];
};

struct RaidsPlayer
{
	float MinSqrDistFromMob;
	float MaxSqrDistFromMob;
  float LastHealth;
	struct RaidsPlayerState State;
  RaidsPlayerEquippedInventory_t Inventory;
  int TicksSinceHealthChanged;
  u16 RevivingPlayerTicks;
	u8 ActionCooldownTicks;
	u8 MessageCooldownTicks;
	char IsLocal;
	char IsDead;
	char IsDoublePoints;
	char IsDoubleXP;
  char LastEquipslots[3];
	char HealthBarStrBuf[8];
};

struct RaidsMobStats
{
	int MobsDrawnCurrent;
	int MobsDrawnLast;
	int MobsDrawGameTime;
  int TotalSpawning;
  int TotalAlive;
  int TotalSpawned;
  int NumSpawnedThisRound[MAX_MOB_SPAWN_PARAMS];
  u8 NumAlive[MAX_MOB_SPAWN_PARAMS];
};

struct RaidsState
{
	int InitializedTime;
  int MapBaseComplexity;
  struct RaidsMobStats MobStats;
	struct RaidsPlayer PlayerStates[GAME_MAX_PLAYERS];
  int ClientsReady;
	int MenuOpen;
  int OnHubWorld;
	struct RaidsPlayer* LocalPlayerState;
	int GameOver;
  int MissionStatus;
  int MissionStartTime;
  int MissionCompleteTime;
  int LivesLeft;
	int WinningTeam;
	int ActivePlayerCount;
	int AlivePlayerCount;
  int TicksWithNoLivingPlayers;
	int IsHost;
	float Difficulty;
  float AmmoDropChance;
  float AmmoRefillCostMultiplier;
  int DifficultyStars;
  int DesiredMusicTrack;
  int PendingWorldHopAtTime;
  int PendingWorldHopDifficultyStars;
  CustomMapDef_t* PendingWorldHopMapDef;
  CustomMapDef_t* CurrentMapDef;
  struct RaidsBankMapStats CurrentMapStats;
	char NumTeams;
  char DesiredMusicTrackSkipTransition;
  char DesiredMusicTrackForce;
  char DesiredMusicTrackLoop;
};

struct RaidsMapConfig
{
  u32 Magic;
  int ClientsReady;
  struct RaidsState* State;
  struct MobSpawnParams* MobSpawnParams;
  int MobSpawnParamsCount;
  int* TrackWhitelist;
  int TrackWhitelistCount;
  int TrackWhitelistEnabled;
  struct BankVTable* BankVTable;

  // mode
  PushSnack_func PushSnackFunc;
  PushDamageBubble_func PushDamageBubbleFunc;
  GetAmmoRefillCost_func GetAmmoRefillCostFunc;
  BeginWorldHop_func BeginWorldHopFunc;
  PopulateSpawnArgs_func PopulateSpawnArgsFunc;
  RegisterNpc_func RegisterNpcFunc;
  OnGuberEvent_func OnGuberEventFunc;
  OnGetGuber_func OnGetGuberFunc;
  TryCreateMob_func TryCreateMobFunc;
  RequestPrestigeLoot_func RequestPrestigeLootFunc;
  OnMissionComplete_func OnMissionCompleteFunc;
  OnMissionFail_func OnMissionFailFunc;

  // map
  MapOnMobCreate_func OnMobCreateFunc;
  MapOnMobSpawned_func OnMobSpawnedFunc;
  MapOnMobUpdate_func OnMobUpdateFunc;
  MapOnMobKilled_func OnMobKilledFunc;
  MapOnMobDestroyed_func OnMobDestroyedFunc;
  MapCreateAmmoDropAt_func CreateAmmoDropAtFunc;
  FrameTick_func OnFrameTickFunc;
};

struct RaidsCustomMapExtraData
{
  int RaidsVersion;
  int MinPlayerLevel;
  int CollectiblesCount;
  int ChallengesCount;
  int Cost[5];
  char Author[32];
  char Description[256];
  char* Challenges[0]; // array of char* tuples, for challenge name & description
};

struct RaidsGameData
{
	u32 Version;
	u64 Points[GAME_MAX_PLAYERS];
	int Kills[GAME_MAX_PLAYERS];
	int Deaths[GAME_MAX_PLAYERS];
};

struct RaidsSnackItem
{
  int TicksAlive;
  char DisplayForLocalPlayerIdx;
  char Str[64];
};

struct Guber* getGuber(Moby* moby);
int handleEvent(Moby* moby, GuberEvent* event);

#endif // RAIDS_GAME_H
