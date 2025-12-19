/***************************************************
 * FILENAME :		map.c
 * 
 * DESCRIPTION :
 * 		Custom map logic for Survival.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/area.h>
#include <libdl/math3d.h>
#include <libdl/random.h>
#include <libdl/collision.h>
#include <libdl/stdio.h>
#include <libdl/spawnpoint.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/patch.h>
#include <libdl/hud.h>
#include <libdl/ui.h>
#include <libdl/radar.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "messageid.h"
#include "game.h"
#include "mob.h"
#include "pathfind.h"
#include "maputils.h"
#include "upgrade.h"
#include "drop.h"
#include "pool.h"

#if SOULCOLLECTOR
#include "soulcollector.h"
#endif

#ifndef MAP_BASE_COMPLEXITY
#define MAP_BASE_COMPLEXITY (5000)
#endif

struct Guber* mapGetGuber(Moby* moby);
void mapHandleEvent(Moby* moby, GuberEvent* event);

void mobInit(void);
void mobTick(void);
void configInit(void);
void pathTick(void);

#if STACKABLES
void stackableInit(void);
void stackableTick(void);
#endif

#if BLESSINGS
void blessingsInit(void);
void blessingsTick(void);
#endif

void stackableOnMobKilled(Moby* moby, int killedByPlayerId, int killedByWeaponId);

void frameTick(void);
void mapOnMobKilled(Moby* moby, int killedByPlayerId, int killedByWeaponId);
int mapCanSpawnMobs(void);

char LocalPlayerStrBuffer[GAME_MAX_LOCALS][64];

typedef struct HudBoss_CommonData { // 0x38
	/* 0x00 */ int iShown;
	/* 0x04 */ int iState;
	/* 0x08 */ float fHP;
	/* 0x0c */ float fDisplayHP;
	/* 0x10 */ unsigned int iColor;
	/* 0x14 */ unsigned int iColor2;
	/* 0x18 */ int iIcon;
	/* 0x1c */ float fHideX;
	/* 0x20 */ float fShown;
	/* 0x24 */ char bShown;
	/* 0x25 */ char bFancy;
	/* 0x28 */ int iDelay;
	/* 0x2c */ char bUpdate;
	/* 0x30 */ float fPulse;
	/* 0x34 */ int iFillMode;
} HudBoss_CommonData_t;

// set by map
extern int mobAllowedCuboidIdx;
extern int mobSpawnPointsAreaIdx;
extern SurvivalBakedConfig_t bakedConfig;
struct SurvivalMapConfig MapConfig __attribute__((section(".config"))) = {
  .Magic = MAP_CONFIG_MAGIC,
	.State = NULL,
  .BakedConfig = &bakedConfig,
  .OnFrameTickFunc = &frameTick,
  .OnMobKilledFunc = &mapOnMobKilled,
  .CanSpawnMobsFunc = &mapCanSpawnMobs,
};

//--------------------------------------------------------------------------
int mapSpawnMob(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags)
{
  // increment # mobs to spawn
  if (MapConfig.State && spawnFromUID < 0) {
    MapConfig.State->RoundMaxMobCount += 1;
  }

  if (!gameAmIHost()) return;

  struct MobConfig* config = &MapConfig.DefaultSpawnParams[spawnParamsIdx].Config;

  // spawn
  if (MapConfig.ModeCreateMobFunc)
    return MapConfig.ModeCreateMobFunc(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);
  else
    return MapConfig.OnMobCreateFunc(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);
}

//--------------------------------------------------------------------------
void mapOnMobKilled(Moby* moby, int killedByPlayerId, int killedByWeaponId)
{
#if STACKABLES
  stackableOnMobKilled(moby, killedByPlayerId, killedByWeaponId);
#endif

#if SOULCOLLECTOR
  soulcollectorOnSoul(moby->Position, killedByPlayerId);
#endif
}

//--------------------------------------------------------------------------
int mapCanSpawnMobs(void)
{
#if DEBUG_MANUAL_SPAWNING
  return 0;
#endif

  return 1;
}

//--------------------------------------------------------------------------
void mapReturnPlayersToMap(void)
{
  int i;
  VECTOR p,r,o;

  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!player || !player->SkinMoby) continue;

    // if we're under the map, teleport back up
    if (player->PlayerPosition[2] < (gameGetDeathHeight() + 1)) {
      
      // move back to player start
      playerGetSpawnpoint(player, p, r, 0);
      vector_fromyaw(o, (player->PlayerId / (float)GAME_MAX_PLAYERS) * MATH_TAU - MATH_PI);
      vector_scale(o, o, 2.5);
      vector_add(p, p, o);
      playerSetPosRot(player, p, r);
      playerSetHealth(player, maxf(0, player->Health - player->MaxHealth*0.5));
    }
  }
}

//--------------------------------------------------------------------------
void mobForceIntoMapBounds(Moby* moby)
{
  if (!moby)
    return;
    
  int i;
	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // if no cuboid defined, restrict mobs to moby grid range
  if (mobAllowedCuboidIdx < 0) {
    VECTOR min = { 0, 0, 0, 0 };
    VECTOR max = { 1024, 1024, 1024, 0 };
    for (i = 0; i < 3; ++i) {
      if (moby->Position[i] < min[i]) {
        moby->Position[i] = min[i];
        pvars->MobVars.Respawn = 1;
        break;
      }
      else if (moby->Position[i] > max[i]) {
        moby->Position[i] = max[i];
        pvars->MobVars.Respawn = 1;
        break;
      }
    }
    return;
  }
  
  SpawnPoint* cuboid = spawnPointGet(mobAllowedCuboidIdx);
  VECTOR rel;
  if (spawnPointIsPointInside(cuboid, moby->Position, rel)) return;

  // clamp position & respawn
  rel[0] = clamp(rel[0], -1, 1);
  rel[1] = clamp(rel[1], -1, 1);
  rel[2] = clamp(rel[2], -1, 1);
  vector_apply(moby->Position, rel, cuboid->M0);
  pvars->MobVars.Respawn = 1;
}

//--------------------------------------------------------------------------
int mapPathCanBeSkippedForTarget(Moby* moby)
{
  return 1;
}

//--------------------------------------------------------------------------
int createMob(int spawnParamsIdx, VECTOR position, float yaw, int spawnFromUID, int spawnFlags, struct MobConfig *config)
{
  if (spawnParamsIdx < 0 || spawnParamsIdx >= MapConfig.DefaultSpawnParamsCount) {
    DPRINTF("unhandled create spawnParamsIdx %d\\n", spawnParamsIdx);
    return 0;
  }

  //struct MobSpawnParams* spawnParams = &MapConfig.DefaultSpawnParams[spawnParamsIdx];
  //if (spawnParams->MobCreate)
  //  return spawnParams->MobCreate(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);

  return mobCreate(spawnParamsIdx, position, yaw, spawnFromUID, spawnFlags, config);
}

//--------------------------------------------------------------------------
void addBlip(Moby* moby, int type, int team, int life)
{
  if (!moby) return;

  // add blip
  int blipId = radarGetBlipIndex(moby);
  if (blipId >= 0)
  {
    RadarBlip * blip = radarGetBlips() + blipId;
    blip->X = moby->Position[0];
    blip->Y = moby->Position[1];
    blip->Life = life;
    blip->Type = type;
    blip->Team = team;
  }
}

//--------------------------------------------------------------------------
void randomizeWeaponPickups(void)
{
  int i,j;
  GameOptions* gameOptions = gameGetOptions();
  char wepCounts[9];
  char wepEnabled[17];
  int pickupCount = 0;
  int pickupOptionCount = 0;
  memset(wepEnabled, 0, sizeof(wepEnabled));
  memset(wepCounts, 0, sizeof(wepCounts));

  if (gameOptions->WeaponFlags.DualVipers) { wepEnabled[2] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.MagmaCannon) { wepEnabled[3] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.Arbiter) { wepEnabled[4] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.FusionRifle) { wepEnabled[5] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.MineLauncher) { wepEnabled[6] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.B6) { wepEnabled[7] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.Holoshield) { wepEnabled[16] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.Flail) { wepEnabled[12] = 1; pickupOptionCount++; }
  if (gameOptions->WeaponFlags.Chargeboots && gameOptions->GameFlags.MultiplayerGameFlags.SpawnWithChargeboots == 0) { wepEnabled[13] = 1; pickupOptionCount++; }

  if (pickupOptionCount > 0) {
    Moby* moby = mobyListGetStart();
    Moby* mEnd = mobyListGetEnd();

    while (moby < mEnd) {
      if (moby->OClass == MOBY_ID_WEAPON_PICKUP && moby->PVar) {
        
        int target = pickupCount / pickupOptionCount;
        int gadgetId = 1;
        if (target < 3) {
          do { j = rand(pickupOptionCount); } while (wepCounts[j] != target);

          ++wepCounts[j];

          i = -1;
          do
          {
            ++i;
            if (wepEnabled[i])
              --j;
          } while (j >= 0);

          gadgetId = i;
        }

        // set pickup
        ((void (*)(Moby*, int))0x0043A370)(moby, gadgetId);

        ++pickupCount;
      }

      ++moby;
    }
  }
}

//--------------------------------------------------------------------------
void updateBossMeter(void)
{
  static float lastHealth = -1;
  HudBoss_CommonData_t* hudBossData = (HudBoss_CommonData_t*)0x00310400;
  int i;
  Moby* bossMoby = NULL;

  if (MapConfig.State)
    bossMoby = MapConfig.State->BossMoby;

  // show/hide boss meter
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    PlayerHUDFlags* hud = hudGetPlayerFlags(i);
    if (hud) {
      hud->Flags.Meter = bossMoby ? 1 : 0;
    }
  }

  if (!bossMoby) return;
  struct MobPVar* pvars = (struct MobPVar*)bossMoby->PVar;
  float health = pvars->MobVars.Health / pvars->MobVars.Config.MaxHealth;

  // update meter and icon
  hudBossData->fHP = health;

  // refresh on health change
  if (lastHealth != health) {
    lastHealth = health;
    hudBossData->bUpdate = 1;
  }

  // set boss image to reactor sprite
  u32 id = hudPanelGetElement((void*)0x222b18, 6);
  struct HUDWidgetRectangleObject* bossImgRectObject = (struct HUDWidgetRectangleObject*)hudCanvasGetObject(hudGetCanvas(4), id);
  if (bossImgRectObject) ((void (*)(u32, u32))0x005ca3e8)(bossImgRectObject, MapConfig.DefaultSpawnParams[pvars->MobVars.SpawnParamsIdx].BossTexUid /*0x75AF + 3*/);
}

//--------------------------------------------------------------------------
void frameTick(void)
{
#if STACKABLES
  sboxFrameTick();
#endif

#if SOULCOLLECTOR
  soulcollectorFrameUpdate();
#endif

  //char buf[32];
  //snprintf(buf, sizeof(buf), "%d", mobyGetNumSpawnableMobys());
  //gfxHelperDrawText(5, SCREEN_HEIGHT - 5, 0, 0, 1, 0x80FFFFFF, buf, -1, TEXT_ALIGN_BOTTOMLEFT, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
int mapConsiderMobSpawnPoint(struct MobSpawnParams* mobSpawnParams, VECTOR position, float yaw, Player* targetPlayer)
{
  if (!targetPlayer || !targetPlayer->PlayerMoby) return 1;

  // check if we have a path to
  // if not, don't spawn here
  if (!pathHasRouteFromTo(pathGetClosestNodeIdx(position), pathTargetCacheGetClosestNodeIdx(targetPlayer->PlayerMoby))) {
    return 0;
  }

  return 1;
}

//--------------------------------------------------------------------------
int mapGetSpawnPoints(int** outSpawnPointIndices)
{
  Area_t area;
  if (mobSpawnPointsAreaIdx < 0 || !areaGetArea(mobSpawnPointsAreaIdx, &area)) {
    *outSpawnPointIndices = NULL;
    return 0;
  }

  *outSpawnPointIndices = area.Cuboids;
  return area.CuboidCount;
}

//--------------------------------------------------------------------------
int mapBlockPlayerUseTeleporter(Moby* moby, Player* player)
{
	// pointer to player is in $s1
	asm volatile (
    ".set noreorder;\n"
		"move %0, $s1"
		: : "r" (player)
	);

  return 0;
}

//--------------------------------------------------------------------------
void survivalDebugManualSpawn(void)
{
  static int manSpawnMobId = 0;
  if (MapConfig.DefaultSpawnParamsCount <= 0) return; // no mobs
  if (!localPlayerHasInput()) return;

  Player* localPlayer = playerGetFromSlot(0);
  if (!playerIsValid(localPlayer)) return;
  
  // check for destroy all mobs
  if (MapConfig.ModeMobNukeFunc && padGetButtonDown(0, PAD_UP) > 0) {
    MapConfig.ModeMobNukeFunc(-1);
  }

  // nav mobs
  int dir = 0;
  if (padGetButtonDown(0, PAD_LEFT) > 0) {
    dir = -1;
  } else if (padGetButtonDown(0, PAD_RIGHT) > 0) {
    dir = 1;
  }

  if (dir) {
    manSpawnMobId = (manSpawnMobId + dir + MapConfig.DefaultSpawnParamsCount) % MapConfig.DefaultSpawnParamsCount;
    DPRINTF("selected mob: %s (%d)\n", MapConfig.DefaultSpawnParams[manSpawnMobId].Name, manSpawnMobId);
  }

  // check for spawn pad button
  if (padGetButtonDown(0, PAD_DOWN) <= 0) return;

  // build spawn position
  VECTOR t;
  VECTOR offset = {1,1,1,0};
  vector_scale(t, offset, MapConfig.DefaultSpawnParams[manSpawnMobId].Config.CollRadius*2);
  vector_add(t, t, localPlayer->PlayerPosition);

  int r = mapSpawnMob(manSpawnMobId, t, 0, -1, 0);
  DPRINTF("manual spawn mob %s (idx %d) returned %d\n", MapConfig.DefaultSpawnParams[manSpawnMobId].Name, manSpawnMobId, r);
}

//--------------------------------------------------------------------------
void survivalDebugInfiniteHealth(void)
{
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;

    player->Health = 125;
  }
}

//--------------------------------------------------------------------------
void survivalDebugInfiniteAmmo(void)
{
  GameOptions* go = gameGetOptions();
  go->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 1;
}

//--------------------------------------------------------------------------
void survivalDebugPayday(void)
{
  int i;
  static int init = 0;
  if (!MapConfig.State) return;
  
  // give max alpha mods
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;

    GadgetBox* gbox = player->GadgetBox;
    if (!gbox) continue;
  
    gbox->ModBasic[0] = 64;
    gbox->ModBasic[1] = 64;
    gbox->ModBasic[2] = 64;
    gbox->ModBasic[3] = 64;
    gbox->ModBasic[4] = 64;
    gbox->ModBasic[5] = 64;
    gbox->ModBasic[6] = 64;
    gbox->ModBasic[7] = 64;
  }

  if (init) return;

  // set bolts/tokens once
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    MapConfig.State->PlayerStates[i].State.Bolts = 100000000;
    MapConfig.State->PlayerStates[i].State.CurrentTokens = 10000;
  }

  init = 1;
}

//--------------------------------------------------------------------------
void survivalDebugMoonjump(void)
{
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;
    if (padGetButton(i, PAD_CROSS) <= 0) continue;

    player->Velocity[2] = 0.125;
  }
}

//--------------------------------------------------------------------------
void survivalInit(void)
{
  static int initialized = 0;
  if (initialized)
    return;

  MapConfig.Magic = MAP_CONFIG_MAGIC;
  MapConfig.ConsiderMobSpawnPointFunc = mapConsiderMobSpawnPoint;
  MapConfig.GetSpawnPointsFunc = mapGetSpawnPoints;
  MapConfig.OnMobCreateFunc = &createMob;

  mapApplyFixes();
  poolInit();
  mboxInit();
  mobInit();
  configInit();
  upgradeInit();
  dropInit();
  bboxInit();
  demonbellInit();
#if STACKABLES
  sboxInit();
  stackableInit();
#endif
#if GAMBITS
  gambitsInit();
#endif
#if BLESSINGS
  blessingsInit();
#endif
#if RANDOMIZE_WEAPONS_AT_START
  randomizeWeaponPickups();
#endif

  // disable jump pad effect
  POKE_U32(0x0042608C, 0);

  // have moby 0x1BC6 use glowColor for particle color
  // for reactor fire effect
  POKE_U32(0x004171D8, 0x8FA80070);
  POKE_U32(0x00417200, 0x8D080060);

  // enable teleporter for everyone
  HOOK_JAL(0x003dfd18, &mapBlockPlayerUseTeleporter);
  POKE_U32(0x003dfd1c, 0x0240202D);
  //POKE_U32(0x003DFD20, 0x10000006);
  POKE_U32(0x003DFD6C, 0x00000000);

  DPRINTF("path %08X end %08X\n", (u32)&MOB_PATHFINDING_PATHS, (u32)&MOB_PATHFINDING_PATHS + (MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH * MOB_PATHFINDING_NODES_COUNT * MOB_PATHFINDING_NODES_COUNT));

  initialized = 1;
}

//--------------------------------------------------------------------------
int survivalTick(void)
{
  //
  if (MapConfig.ClientsReady || !netGetDmeServerConnection())
  {
    mboxSpawn();
  }

  poolTick();
  mobTick();
  pathTick();
  upgradeTick();
  dropTick();
  demonbellTick();
#if STACKABLES
  stackableTick();
#endif
#if GAMBITS
  gambitsTick();
#endif
#if BLESSINGS
  blessingsTick();
#endif
  mapReturnPlayersToMap();
  updateBossMeter();

  if (MapConfig.State) {
    MapConfig.State->MapBaseComplexity = MAP_BASE_COMPLEXITY;
  }
  
  // 
  if (MapConfig.State) {
    addBlip(MapConfig.State->BigAl, 14, TEAM_YELLOW, 31);

    Moby* vendorMoby = MapConfig.State->Vendor;
    while (vendorMoby && vendorMoby->OClass == MOBY_ID_WEAPON_VENDOR) {
      addBlip(vendorMoby, 4, TEAM_GREEN, 31);
      ++vendorMoby;
    }

    // track round completions
#if GAMBITS
    static int lastRoundCompleted = 0;
    if (MapConfig.State->RoundEndTime && lastRoundCompleted != MapConfig.State->RoundNumber) {
      lastRoundCompleted = MapConfig.State->RoundNumber;
      gambitsOnRoundComplete(lastRoundCompleted);
    }
#endif
      
    // enable prestige if round % 25
#if SHOW_PRESTIGE_EVERY_25
    Moby* prestigeMachineMoby = MapConfig.State->PrestigeMachine;
    if (prestigeMachineMoby) {
      int enabled = MapConfig.State->RoundEndTime && ((MapConfig.State->RoundNumber + 0) % 25) == 0;
        
      if (enabled) {
        if (prestigeMachineMoby->CollActive < 0) {
          pushSnack(0, "Prestige Machine Activated", 120);
        }
        prestigeMachineMoby->DrawDist = 64;
        prestigeMachineMoby->CollActive = 0;
        prestigeMachineMoby->ModeBits &= ~MOBY_MODE_BIT_DISABLED;
      } else {
        prestigeMachineMoby->DrawDist = 0;
        prestigeMachineMoby->CollActive = -1;
        prestigeMachineMoby->ModeBits |= MOBY_MODE_BIT_DISABLED;
      }
    }
#endif
  }

#if DEBUG_MANUAL_SPAWNING
  survivalDebugManualSpawn();
#endif

#if DEBUG_INFINITE_HEALTH
  survivalDebugInfiniteHealth();
#endif

#if DEBUG_INFINITE_AMMO
  survivalDebugInfiniteAmmo();
#endif

#if DEBUG_MOONJUMP
  survivalDebugMoonjump();
#endif

#if DEBUG_PAYDAY
  survivalDebugPayday();
#endif

  dlPostUpdate();
	return 0;
}
