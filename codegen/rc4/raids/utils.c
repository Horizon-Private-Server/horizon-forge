/***************************************************
 * FILENAME :		utils.c
 * 
 * DESCRIPTION :
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
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/hud.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/collision.h>
#include <libdl/ui.h>
#include <libdl/random.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "window.h"
#include "mobs/mob.h"
#include "game.h"
#include "bank.h"
#include "utils.h"

extern struct RaidsMapConfig MapConfig;

/* 
 * reusable menu sound def
 */
SoundDef MenuSoundDef =
{
	0.0,	// MinRange
	20.0,	// MaxRange
	100,		// MinVolume
	2000,		// MaxVolume
	0,			// MinPitch
	0,			// MaxPitch
	0,			// Loop
	0x10,		// Flags
	19,		  // Index
	3			  // Bank
};

//--------------------------------------------------------------------------
void playEquipRejectSound(Player* player)
{	
  MenuSoundDef.Index = 27;
	soundPlay(&MenuSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
void playEquipSound(Player* player)
{	
  MenuSoundDef.Index = 19;
	soundPlay(&MenuSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
void playUpgradeSound(Player* player)
{	
  MenuSoundDef.Index = 58;
	soundPlay(&MenuSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
void playPaidSound(Player* player)
{
  MenuSoundDef.Index = 32;
  soundPlay(&MenuSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
enum MobDamageSource getDamageSourceFromOClass(short oclass)
{
  switch (oclass)
  {
    case MOBY_ID_WRENCH: return MOB_DAMAGE_SOURCE_WRENCH;
    case MOBY_ID_DUAL_VIPER_SHOT: return MOB_DAMAGE_SOURCE_DUAL_VIPERS;
    case MOBY_ID_MAGMA_CANNON: return MOB_DAMAGE_SOURCE_MAGMA_CANNON;
    case MOBY_ID_ARBITER_ROCKET0: return MOB_DAMAGE_SOURCE_ARBITER;
    case MOBY_ID_FUSION_SHOT: return MOB_DAMAGE_SOURCE_FUSION_RIFLE;
    case MOBY_ID_MINE_LAUNCHER_MINE: return MOB_DAMAGE_SOURCE_MINE_LAUNCHER;
    case MOBY_ID_B6_BOMB_EXPLOSION: return MOB_DAMAGE_SOURCE_B6_OBLITERATOR;
    case MOBY_ID_FLAIL: return MOB_DAMAGE_SOURCE_SCORPION_FLAIL;
    case MOBY_ID_HOLOSHIELD_LAUNCHER:
    case MOBY_ID_HOLOSHIELD_SHOT: return MOB_DAMAGE_SOURCE_HOLOSHIELD;
    default: return MOB_DAMAGE_SOURCE_UNKNOWN;
  }
}

//--------------------------------------------------------------------------
enum WEAPON_IDS getWeaponIdFromDamageSource(enum MobDamageSource source)
{
  switch (source)
  {
    case MOB_DAMAGE_SOURCE_WRENCH: return WEAPON_ID_WRENCH;
    case MOB_DAMAGE_SOURCE_DUAL_VIPERS: return WEAPON_ID_VIPERS;
    case MOB_DAMAGE_SOURCE_MAGMA_CANNON: return WEAPON_ID_MAGMA_CANNON;
    case MOB_DAMAGE_SOURCE_ARBITER: return WEAPON_ID_ARBITER;
    case MOB_DAMAGE_SOURCE_FUSION_RIFLE: return WEAPON_ID_FUSION_RIFLE;
    case MOB_DAMAGE_SOURCE_MINE_LAUNCHER: return WEAPON_ID_MINE_LAUNCHER;
    case MOB_DAMAGE_SOURCE_B6_OBLITERATOR: return WEAPON_ID_B6;
    case MOB_DAMAGE_SOURCE_SCORPION_FLAIL: return WEAPON_ID_FLAIL;
    case MOB_DAMAGE_SOURCE_HOLOSHIELD: return WEAPON_ID_OMNI_SHIELD;
    default: return WEAPON_ID_EMPTY;
  }
}

//--------------------------------------------------------------------------
int mobyIsMob(Moby* moby)
{
  if (!moby) return 0;
  if (mobyIsNpc(moby)) return 1;

  // Thrown zombie heads share the model OClass, but have projectile PVars.
  if (!moby->Guber) return 0;

  int i;
  for (i = 0; i < MapConfig.MobSpawnParamsCount; ++i) {
    if (MapConfig.MobSpawnParams[i].OClass == moby->OClass)
      return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int mobyIsNpc(Moby* moby)
{
  return moby && moby->OClass == NPC_MOBY_OCLASS;
}

//--------------------------------------------------------------------------
int hasPendingWorldHop(void)
{
  return MapConfig.State && MapConfig.State->PendingWorldHopMapDef && MapConfig.State->PendingWorldHopAtTime > 0;
}

//--------------------------------------------------------------------------
int missionIsFailed(void)
{
  return MapConfig.State && MapConfig.State->MissionStatus == RAIDS_MISSION_FAILED;
}

//--------------------------------------------------------------------------
int missionIsComplete(void)
{
  return MapConfig.State && MapConfig.State->MissionStatus == RAIDS_MISSION_COMPLETED;
}

//--------------------------------------------------------------------------
int missionIsActive(void)
{
  return !isOnHubWorld() && !hasPendingWorldHop() && MapConfig.State && MapConfig.State->MissionStatus == RAIDS_MISSION_ACTIVE;
}

//--------------------------------------------------------------------------
int missionIsBossRaid(void)
{
  return MapConfig.State && MapConfig.State->MissionType == RAIDS_MISSION_RAID;
}

//--------------------------------------------------------------------------
int missionIsOpenWorld(void)
{
  return MapConfig.State && MapConfig.State->MissionType == RAIDS_MISSION_OPEN_WORLD;
}

//--------------------------------------------------------------------------
int isOnHubWorld(void)
{
  return MapConfig.State && MapConfig.State->OnHubWorld;
}

//--------------------------------------------------------------------------
int shouldCloseMenu(void)
{
  return !isInGame() || gameHasEnded() || hasPendingWorldHop()
      || gameIsAnyStartMenuOpen() || PATCH_POINTERS_PATCHMENU != 0;
}

//--------------------------------------------------------------------------
int shouldCloseHubMenu(void)
{
  return !isOnHubWorld() || shouldCloseMenu();
}

//--------------------------------------------------------------------------
int bankTryChargeLocalAccount(Player* player, u32 cost)
{
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return 0;
  if (bank->Account.Bolts < cost) return 0;

  // charge
  bank->Account.Bolts -= cost;
  playPaidSound(player);

  // send new bolts to server
  bankSendAccountToServer();
  return 1;
}

//--------------------------------------------------------------------------
double getLevelFromXpQuadratic(double xp, double a, double b)
{
    return (-b + sqrt(b * b + 4 * a * xp)) / (2 * a);
}

//--------------------------------------------------------------------------
double getXpFromLevelQuadratic(double level, double a, double b)
{
    return a * level * level + b * level;
}

//--------------------------------------------------------------------------
int getLevelFromXp(u64 xp)
{
  if (xp < 0) return 0;

  int level = 0;
  while (getXpForLevel(level+1) <= xp && level < LEVELUP_MAX_PLAYER_LEVEL)
    ++level;

  return level;
}

//--------------------------------------------------------------------------
u64 getXpForLevel(int level)
{
  return (u64)(10 * (double)powf(level, 3) + 250*level);
}

//--------------------------------------------------------------------------
int getProficiencyFromXp(double xp)
{
  static int init = 0;
  static double xpCache[LEVELUP_MAX_PROF_LEVEL + 1];
  if (!init) {
    init = 1;
    int i;
    for (i = 0; i <= LEVELUP_MAX_PROF_LEVEL; ++i) {
      xpCache[i] = getXpForProficiency(i);
    }
  }
  
  if (xp < 0) return 0;

  // binary search xpCache for the proficiency level
  int low = 0, high = LEVELUP_MAX_PROF_LEVEL;
  while (low < high) {
    int mid = (low + high) / 2;
    if (xpCache[mid] < xp) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  
  // low is now the first index where xpCache[low] >= xp
  if (low > LEVELUP_MAX_PROF_LEVEL) return LEVELUP_MAX_PROF_LEVEL;
  if (xpCache[low] > xp) {
    // if xpCache[low] is greater than xp, we need to return the previous proficiency level
    if (low == 0) return 0; // no proficiency levels below 0
    return low - 1;
  }
  // otherwise, we can return the proficiency level
  return low;
}

//--------------------------------------------------------------------------
double getXpForProficiency(int proficiency)
{
  if (proficiency > LEVELUP_MAX_PROF_LEVEL) proficiency = LEVELUP_MAX_PROF_LEVEL;
  if (proficiency <= 0) return 0;

  // 10x^3 + 100x + 250
  return 10*pow(proficiency, 3) + 100*proficiency + 250;
  //return getXpFromLevelQuadratic(proficiency, 250, 500);
}

//--------------------------------------------------------------------------
int getAmmoRefillCost(Player* player)
{
  if (!player || !player->GadgetBox) return -1;
  if (!MapConfig.Functions.ModeGetAmmoRefillCostFunc) return -1;

  return MapConfig.Functions.ModeGetAmmoRefillCostFunc(player);
}
