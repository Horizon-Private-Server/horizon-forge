#ifndef _FORGE_RC4_COMMON_H_
#define _FORGE_RC4_COMMON_H_

#include <libdl/gamesettings.h>
#include <libdl/graphics.h>
#include <libdl/math3d.h>

/*
 * Fixed pointers to patch container for use by external modules.
 */
#define PATCH_INTEROP (*(PatchInterop_t **)0x000CFFC0)
#define PATCH_DZO_INTEROP_FUNCS (*(DzoInteropFunctions_t **)0x000CFFC4)
#define PATCH_POINTERS_SPECTATE (*(u8 *)(0x000CFFC0 + 8))
#define PATCH_POINTERS_PATCHMENU (*(u8 *)(0x000CFFC0 + 9))
#define PATCH_POINTERS_SCOREBOARD (*(u8 *)(0x000CFFC0 + 10))
#define PATCH_POINTERS_QUICKCHAT (*(u8 *)(0x000CFFC0 + 11))
#define DZO_MAPLOADER_WAD_BUFFER ((void *)0x02100000)

struct CustomMapDef;

typedef void (*SendCustomCommandToClientFunc_t)(int id, int size, void *data);
typedef void (*SetSpectateFunc_t)(int localPlayerIndex, int spectatePlayerOrDisable);
typedef int (*GetCustomMapDefCountFunc_t)(void);
typedef struct CustomMapDef *(*GetCustomMapDefFunc_t)(int index);
typedef int (*ReadCustomMapExtraDataFunc_t)(char *mapFilename, void *buffer, int bufferSize, int customModeId);
typedef void (*RefreshCustomMapDefsFunc_t)(void);
typedef void (*HopToCustomMapFunc_t)(struct CustomMapDef *def);

/*
 *       Function that reads the custom map extra data from the usb drive for the current game mode.
 *      Returns 1 on success, 0 on failure.
 */
typedef int (*ReadExtraData_f)(void *dst, int len);

typedef struct PatchConfig
{
	char framelimiter;
	char enableGamemodeAnnouncements;
	char enableSpectate;
	char enableSingleplayerMusic;
	char levelOfDetail;
	char enablePlayerStateSync;
	char disableAimAssist;
	char enableFpsCounter;
	char disableCircleToHackerRay;
	char disableScavengerHunt;
	char disableCameraShake;
	char minimapScale;
	char minimapBigZoom;
	char minimapSmallZoom;
	char enableFusionReticule;
	char playerFov;
	char preferredGameServer;
	char fixedCycleOrder;
	char enableSingleTapChargeboot;
	char enableInGameScoreboard;
	char enableNPSLagComp;
	char enableFastLoad;
	char levelOfDetailMobs;

#if TWEAKERS
	char characterTweakers[1 + 7 * 2];
#endif
} PatchConfig_t;

typedef struct SurvivalConfig
{
	u8 gambit;
} SurvivalConfig_t;

enum FixedCycleOrderMode
{
	FIXED_CYCLE_ORDER_OFF = 0,
	FIXED_CYCLE_ORDER_MAG_FUS_B6 = 1,
	FIXED_CYCLE_ORDER_MAG_B6_FUS = 2,
	FIXED_CYCLE_ORDER_COUNT = 3
};

enum PayloadContestMode
{
	PAYLOAD_CONTEST_OFF,
	PAYLOAD_CONTEST_SLOW,
	PAYLOAD_CONTEST_STOP
};

typedef struct UpdateGameStateRequest
{
	char TeamsEnabled;
	char PADDING;
	short Version;
	int RoundNumber;
	int TeamScores[GAME_MAX_PLAYERS];
	char ClientIds[GAME_MAX_PLAYERS];
	char Teams[GAME_MAX_PLAYERS];
} UpdateGameStateRequest_t;

typedef struct CustomGameModeStats
{
	u8 Payload[1024 * 6];
} __attribute__((aligned(16))) CustomGameModeStats_t;

typedef struct SetNameOverridesMessage
{
	int AccountIds[10];
	char Names[10][16];
} SetNameOverridesMessage_t;

typedef struct PayloadConfig
{
	u8 contestMode;
} PayloadConfig_t;

typedef struct TrainingConfig
{
	u8 type;
	u8 variant;
	u8 aggression;
	u8 opt3;
} TrainingConfig_t;

typedef struct HNSConfig
{
	u8 hideStageTime;
} HNSConfig_t;

typedef struct ForgeCgmConfig
{
	u8 params[4];
} ForgeCgmConfig_t;

typedef struct PatchGameConfig
{
	char customModeId;
	// char prWeatherId;
	char grInputRestriction;
	char grNoPacks;
	char grV2s;
	char grNoSpawnImmunity;
	// char prMirrorWorld;
	char grNoHealthBoxes;
	char grVampire;
	char grHalfTime;
	char grOvertime;
	char grBetterHills;
	char grBetterFlags;
	char grHealthBars;
	char grNoNames;
	char grNoInvTimer;
	char grNoPickups;
	char grFusionShotsAlwaysHit;
	char grNoSniperHelpers;
	char grCqPersistentCapture;
	char grCqDisableTurrets;
	char grCqDisableUpgrades;
	char grNewPlayerSync;
	char grLagjump;
	char grNoFusionADS;
	char grRespawnOverride;
	char grFogOfWarRadar;
	char grRadarShortDistance;
	char grInstantDeath;
	// char prPlayerSize;
	char prRotatingWeapons;
	char prHeadbutt;
	char prHeadbuttFriendlyFire;
	char prChargebootForever;
	char drFreecam;
	char drNoRank;
	char drLevelReload;
	PayloadConfig_t payloadConfig;
	TrainingConfig_t trainingConfig;
	HNSConfig_t hnsConfig;
	SurvivalConfig_t survivalConfig;
	ForgeCgmConfig_t forgeCgmConfig;
} PatchGameConfig_t;

typedef struct PatchStateContainer
{
	PatchConfig_t *Config;
	PatchGameConfig_t *GameConfig;
	int UpdateGameState;
	UpdateGameStateRequest_t GameStateUpdate;
	int UpdateCustomGameStats;
	CustomGameModeStats_t *CustomGameStats;
	GameSettings GameSettingsAtStart;
	int CustomGameStatsSize;
	int ClientsReadyMask;
	int AllClientsReady;
	int VoteToEndPassed;
	int HalfTimeState;
	int OverTimeState;
	SetNameOverridesMessage_t LobbyNameOverrides;
	int SelectedCustomMapId;
	int SelectedCustomMapChanged;
	ReadExtraData_f ReadExtraDataFunc;
} PatchStateContainer_t;

typedef struct PatchInterop
{
	PatchConfig_t *Config;
	PatchGameConfig_t *GameConfig;
	char Client;
	char Month;
	SetSpectateFunc_t SetSpectate;
	char *MapLoaderFilename;
	GetCustomMapDefCountFunc_t GetCustomMapDefCount;
	GetCustomMapDefFunc_t GetCustomMapDef;
	ReadCustomMapExtraDataFunc_t ReadCustomMapExtraData;
	RefreshCustomMapDefsFunc_t RefreshCustomMapDefs;
	HopToCustomMapFunc_t HopToCustomMap;
	PatchStateContainer_t *PatchStateContainer;
	int *ClientLatency;
} PatchInterop_t;

typedef struct DzoInteropFunctions
{
	SendCustomCommandToClientFunc_t SendCustomCommandToClient;
} DzoInteropFunctions_t;

typedef struct CustomMapDef
{
	int Version;
	int CustomModeExtraDataMask;
	short ShrubMinRenderDistance;
	short Subsort;
	char BaseMapId;
	char ForcedCustomModeId;
	char Name[32];
	char Filename[48];
} CustomMapDef_t;

enum CHARACTER_TWEAKER_ID
{
	CHARACTER_TWEAKER_HEAD_SCALE,
	CHARACTER_TWEAKER_UPPER_TORSO_SCALE,
	CHARACTER_TWEAKER_LEFT_ARM_SCALE,
	CHARACTER_TWEAKER_RIGHT_ARM_SCALE,
	CHARACTER_TWEAKER_LEFT_LEG_SCALE,
	CHARACTER_TWEAKER_RIGHT_LEG_SCALE,
	CHARACTER_TWEAKER_TOGGLE,
	CHARACTER_TWEAKER_LOWER_TORSO_SCALE,

	CHARACTER_TWEAKER_HEAD_POS,
	CHARACTER_TWEAKER_UPPER_TORSO_POS,
	CHARACTER_TWEAKER_LOWER_TORSO_POS,
	CHARACTER_TWEAKER_LEFT_ARM_POS,
	CHARACTER_TWEAKER_RIGHT_ARM_POS,
	CHARACTER_TWEAKER_LEFT_LEG_POS,
	CHARACTER_TWEAKER_RIGHT_POS,

	CHARACTER_TWEAKER_COUNT
};

enum CUSTOM_MODE_ID
{
	CUSTOM_MODE_NONE = 0,
	CUSTOM_MODE_1000_KILLS,
	CUSTOM_MODE_GUN_GAME,
	CUSTOM_MODE_INFECTED,
	// CUSTOM_MODE_INFINITE_CLIMBER,
	CUSTOM_MODE_PAYLOAD,
	CUSTOM_MODE_SEARCH_AND_DESTROY,
	CUSTOM_MODE_SURVIVAL,
	CUSTOM_MODE_TEAM_DEFENDER,
	CUSTOM_MODE_TRAINING,
	// CUSTOM_MODE_BENCHMARK,
	CUSTOM_MODE_HNS,
	CUSTOM_MODE_GRIDIRON,
	CUSTOM_MODE_TAG,
	CUSTOM_MODE_RAIDS,

#if DEV
	CUSTOM_MODE_ANIM_EXTRACTOR,
#endif

	// always at the end to indicate how many items there are
	CUSTOM_MODE_COUNT
};

enum TRAINING_TYPE
{
	TRAINING_TYPE_FUSION,
	TRAINING_TYPE_CYCLE,
	TRAINING_TYPE_RUSH,
	TRAINING_TYPE_MAX
};

enum TRAINING_AGGRESSION
{
	TRAINING_AGGRESSION_AGGRO,
	TRAINING_AGGRESSION_AGGRO_NO_DAMAGE,
	TRAINING_AGGRESSION_PASSIVE,
	TRAINING_AGGRESSION_IDLE,
	TRAINING_AGGRESSION_MAX
};

enum CLIENT_TYPE
{
	CLIENT_TYPE_NORMAL = 0,
	CLIENT_TYPE_DZO = 1,
	CLIENT_TYPE_PCSX2 = 2
};

//------------------------------------------------------------------------------
//------------------------------------ DRAW ------------------------------------
//------------------------------------------------------------------------------

enum COMMON_DZO_DRAW_TYPE
{
	COMMON_DZO_DRAW_NONE = 0,
	COMMON_DZO_DRAW_NORMAL,
	COMMON_DZO_DRAW_STRETCHED,
	COMMON_DZO_DRAW_ONLY,
};

/*
 * NAME :    helperAlign
 *
 * DESCRIPTION :
 *       Transforms the point by the given alignment.
 *
 * NOTES :
 *
 * ARGS :
 *
 * RETURN :
 *
 * AUTHOR :      Daniel "Dnawrkshp" Gerendasy
 */
void helperAlign(float *pX, float *pY, float w, float h, enum TextAlign alignment);

/*
 * NAME :    helperRealign
 *
 * DESCRIPTION :
 *       Transforms the point from one alignment to another.
 *
 * NOTES :
 *
 * ARGS :
 *
 * RETURN :
 *
 * AUTHOR :      Daniel "Dnawrkshp" Gerendasy
 */
void helperRealign(float *pX, float *pY, float w, float h, enum TextAlign fromAlignment, enum TextAlign toAlignment);

/*
 * NAME :    gfxHelperDrawBox
 *
 * DESCRIPTION :
 *       Draws a box on the screen.
 *
 * NOTES :
 *
 * ARGS :
 *      x:              Screen X position (0-SCREEN_WIDTH).
 *      y:              Screen Y position (0-SCREEN_HEIGHT).
 *      w:              Width (0-SCREEN_WIDTH).
 *      h:              Height (0-SCREEN_HEIGHT).
 *      color:          Color of box.
 *      alignment:
 *      dzoDrawType:    DZO draw type.
 *
 * RETURN :
 *
 * AUTHOR :      Daniel "Dnawrkshp" Gerendasy
 */
void gfxHelperDrawBox(float x, float y, float offsetX, float offsetY, float w, float h, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType);
void gfxHelperDrawBox_WS(VECTOR worldPosition, float w, float h, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType);

/*
 * NAME :    gfxHelperDrawText
 *
 * DESCRIPTION :
 *       Draws text on the screen.
 *
 * NOTES :
 *
 * ARGS :
 *      x:              Screen X position (0-SCREEN_WIDTH).
 *      y:              Screen Y position (0-SCREEN_HEIGHT).
 *      scale:          Text scale.
 *      color:          color of box.
 *      str:            String to draw. Max 64 characters for DZO.
 *      length:         Number of characters to draw. -1 to draw full length of string.
 *      alignment:      Text alignment.
 *      dzoDraw:        Whether to draw on the DZO client.
 *
 * RETURN :
 *
 * AUTHOR :      Daniel "Dnawrkshp" Gerendasy
 */
void gfxHelperDrawText(float x, float y, float offsetX, float offsetY, float scale, u32 color, char *str, int length, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType);
void gfxHelperDrawText_WS(VECTOR worldPosition, float scale, u32 color, char *str, int length, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType);

/*
 * NAME :    gfxHelperDrawTextWindow
 *
 * DESCRIPTION :
 *       Draws text on the screen constrained to a window.
 *
 * NOTES :
 *
 * ARGS :
 *      x:              Screen X position (0-SCREEN_WIDTH).
 *      y:              Screen Y position (0-SCREEN_HEIGHT).
 *      scale:          Text scale.
 *      color:          color of box.
 *      str:            String to draw. Max 64 characters for DZO.
 *      length:         Number of characters to draw. -1 to draw full length of string.
 *      alignment:      Text alignment.
 *      dzoDraw:        Whether to draw on the DZO client.
 *
 * RETURN :
 *      Total height of wrapped text.
 *
 * AUTHOR :      Daniel "Dnawrkshp" Gerendasy
 */
float gfxHelperDrawTextWindow(float x, float y, float offsetX, float offsetY, float width, float height, float textOffsetX, float textOffsetY, float scale, u32 color, char *str, int length, enum TextAlign alignment, enum FontWindowFlags flags, enum COMMON_DZO_DRAW_TYPE dzoDrawType);

/*
 * NAME :    gfxHelperDrawSprite
 *
 * DESCRIPTION :
 *       Draws a sprite on the screen.
 *
 * NOTES :
 *
 * ARGS :
 *      x:              Screen X position (0-SCREEN_WIDTH).
 *      y:              Screen Y position (0-SCREEN_HEIGHT).
 *      w:              Width (0-SCREEN_WIDTH).
 *      h:              Height (0-SCREEN_HEIGHT).
 *      texWidth:       Pixel width of the given texture.
 *      texHeight:      Pixel height of the given texture.
 *      color:          Color of box.
 *      alignment:      Image alignment.
 *      dzoDrawType:    DZO draw type.
 *
 * RETURN :
 *
 * AUTHOR :      Daniel "Dnawrkshp" Gerendasy
 */
void gfxHelperDrawSprite(float x, float y, float offsetX, float offsetY, float w, float h, int texWidth, int texHeight, int texId, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType);
void gfxHelperDrawSprite_WS(VECTOR worldPosition, float w, float h, int texWidth, int texHeight, int texId, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType);

#endif // _FORGE_RC4_COMMON_H_
