/***************************************************
 * FILENAME :		map.c
 * 
 * DESCRIPTION :
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <libdl/moby.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/ui.h>
#include <libdl/utils.h>
#include <libdl/spawnpoint.h>
#include "spawner.h"
#include "mover.h"
#include "gate.h"
#include "dummy.h"
#include "controller.h"
#include "hackerorb.h"
#include "checkpoint.h"
#include "mob.h"
#include "game.h"
#include "badges.h"
#include "shared.h"
#include "maputils.h"

extern struct RaidsMapConfig MapConfig;

u8 mobPlaySoundCooldownTicks[MAX_MOB_SPAWN_PARAMS][MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS] = {};

#if RAIDS

//--------------------------------------------------------------------------
void mapOnMobUpdate(Moby* moby)
{
  if (!moby || !moby->PVar) return;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobUpdate(moby->PParent, moby, pvars->MobVars.Userdata);
  }
}

//--------------------------------------------------------------------------
void mapOnMobDamaged(Moby* moby, struct MobDamageEventArgs* args)
{
  if (!moby || !moby->PVar) return;
}

//--------------------------------------------------------------------------
void mapOnMobKilled(Moby* moby, int killedByPlayerId, enum MobDamageSource source)
{
  if (!moby || !moby->PVar) return;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobKilled(moby->PParent, moby, pvars->MobVars.Userdata, killedByPlayerId, source);
  }

  if (killedByPlayerId >= 0) {
    Player* killedByPlayer = playerGetAll()[killedByPlayerId];
    int weaponId = getWeaponIdFromDamageSource(source);
    int modStrength = bankGetEquippedWeaponModRarity(killedByPlayerId, weaponId, RAIDS_WEAPON_MOD_WILL_O_WISP) + 1;
    if (killedByPlayer && modStrength > 0) {
      u32 damageFlags = mobAmIOwner(moby) ? 0x00081801 : 0;
      float radius = modStrength * BADGES_EXPLODINGENEMIES_RADIUS_MULT;
      float damage = pvars->MobVars.LastHitByDamage * modStrength * BADGES_EXPLODINGENEMIES_DAMAGE_MULT;
      spawnExplosionDamage(moby->Position, radius, 0x800000C0, killedByPlayer->PlayerMoby, damage, damageFlags);
    }
  }
}

//--------------------------------------------------------------------------
void mapOnMobDestroyed(Moby* moby)
{
  if (!moby || !moby->PVar) return;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobDestroyed(moby->PParent, moby, pvars->MobVars.Userdata);
  }
}

//--------------------------------------------------------------------------
void mapOnMobSpawned(Moby* moby)
{
  if (!moby || !moby->PVar) return;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobSpawned(moby->PParent, moby, pvars->MobVars.Userdata);
  }
}

#endif

//--------------------------------------------------------------------------
int mapWhoKilledMeHook(Player* player, Moby* moby, int b)
{
  if (!moby)
    return 0;

  // only allow mobs or special mobys
  if (mobyIsMob(moby) || moby->Bolts == -1) {
    return ((int (*)(Player*, Moby*, int))0x005dff08)(player, moby, b);
  }

	return 0;
}

//--------------------------------------------------------------------------
void mapPlayerOnPushedIntoWall(Player* player)
{
  if (!player || !player->SkinMoby || !player->PlayerMoby) return;
  
  // push mobs away
  //mobReactToExplosionAt(player->PlayerId, player->PlayerPosition, 1, 8);

  // move player out of clipped wall
  // using lastGoodPos doesn't always return us to before the clip
  if (player->IsLocal) {
    playerSetPosRot(player, player->Ground.lastGoodPos, player->PlayerRotation);
  }
}

//--------------------------------------------------------------------------
// TODO: Move this into the code segment, overwriting GuiMain_GetGadgetVersionName at (0x00541850)
char * mapCustomGetGadgetVersionName(int localPlayerIndex, int weaponId, int showWeaponLevel, int capitalize, int minLevel)
{
	Player* p = playerGetFromSlot(localPlayerIndex);
	char* buf = (char*)(0x2F9D78 + localPlayerIndex*0x40);
  struct GadgetDef* gadgetDef = weaponGetDef(weaponId, 0);
	int level = 0;
  int msgId = capitalize ? gadgetDef->uppercaseTag : gadgetDef->quickSelectTag;
	if (p && p->GadgetBox) {
		level = p->GadgetBox->Gadgets[weaponId].Level;
	}

	if (level >= 9)
    msgId = capitalize ? gadgetDef->upgUCTag : gadgetDef->upgQSTag;

  RaidsInventoryItem_t* item = bankGetLocalEquippedWeapon(weaponId);
  if (!item) {
    return uiMsgString(msgId);
  }

	char* str = uiMsgString(msgId);
  snprintf(buf, 0x40, "%s P%d", str, item->WeaponData.Proficiency+1);
  return buf;
}

//--------------------------------------------------------------------------
void mapCustomMineMobyUpdate(Moby* moby)
{
	// handle auto destructing v10 child mines after N seconds
	// and auto destroy v10 child mines if they came from a remote client
	u32 pvar = (u32)moby->PVar;
	if (pvar) {
		int mLayer = *(int*)(pvar + 0x200);
		Player * p = playerGetAll()[*(short*)(pvar + 0xC0)];
		int createdLocally = p && p->IsLocal;
		int gameTime = gameGetTime();
		int timeCreated = *(int*)(&moby->Rotation[3]);

		// set initial time created
		if (timeCreated == 0) {
			timeCreated = gameTime;
			*(int*)(&moby->Rotation[3]) = timeCreated;
		}

		// don't spawn child mines if mine was created by remote client
		if (!createdLocally) {
			*(int*)(pvar + 0x200) = 2;
		} else if (p && mLayer > 0 && (gameTime - timeCreated) > (5 * TIME_SECOND)) {
			*(int*)(&moby->Rotation[3]) = gameTime;
			((void (*)(Moby*, Player*, int))0x003C90C0)(moby, p, 0);
			return;
		}
	}
	
	// call base
	((void (*)(Moby*))0x003C6C28)(moby);
}

//--------------------------------------------------------------------------
void mapOnV10MagDamageMoby(Moby* target, MobyColDamageIn* in)
{
  if (in->Damager) {
    VECTOR dt;
    vector_subtract(dt, target->Position, in->Damager->Position);
    float dist = vector_length(dt);
    float min = 0.2;
    Player* damager = guberMobyGetPlayerDamager(in->Damager);
    if (damager) min += 0.05 * playerGetWeaponAlphaModCount(damager->GadgetBox, WEAPON_ID_MAGMA_CANNON, ALPHA_MOD_AREA);

    float falloff = minf(1, maxf(min, minf(1, 1 - (dist / 32))));
    in->DamageHp *= falloff;
  }

  mobyCollDamageDirect(target, in);
}

//--------------------------------------------------------------------------
void mapOnV10VipersHitSurface(Moby* moby)
{
  ((void (*)(Moby*))0x003C05D8)(moby);

  Player* player = guberMobyGetPlayerDamager(moby);
  if (player && player->GadgetBox) {
    RaidsInventoryItem_t* item = bankGetLocalEquippedWeapon(WEAPON_ID_VIPERS);
    if (item && bankGetRarityFromQuality(item->Quality) == RAIDS_ITEM_RARITY_MYTHIC) {
      ((void (*)(float radius, float damage, VECTOR p, u32 damageFlags, Moby* moby, Moby* hitMoby))0x003c3a48)(item->WeaponData.AlphaModCounts[ALPHA_MOD_AREA-1] * 0.5, bankGetWeaponDamage(item) * 0.25, moby->Position, 0x801, moby, NULL);
    }
  }
}

//--------------------------------------------------------------------------
int mapOnMobyPlayDesiredSound(int sound, int a1, Moby* moby)
{
  // catch mobs
  if (mobyIsMob(moby) && moby->PVar) {
    int midx = ((struct MobPVar*)moby->PVar)->MobVars.SpawnParamsIdx;
    if (midx >= 0) {
      int sidx = sound % MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS;
      if (mobPlaySoundCooldownTicks[midx][sidx]) return -1;
      mobPlaySoundCooldownTicks[midx][sidx] = MOBS_PLAY_SOUND_COOLDOWN;
    }
  }

  // pass to mobyPlaySound
  return mobyPlaySound(sound, a1, moby);
}

//--------------------------------------------------------------------------
void mapGetResurrectPoint(Player* player, VECTOR outPos, VECTOR outRot, int firstRes)
{
  // pass to base if we don't have a player start
  playerGetSpawnpoint(player, outPos, outRot, firstRes);

  playerSetHealth(player, player->MaxHealth);
}

//--------------------------------------------------------------------------
void mapOnHealthboxHeal(Player* player, float amount)
{
  if (amount > 100) amount = 100;

  playerIncHealth(player, amount);
}

//--------------------------------------------------------------------------
Moby* mapOnSetPlayerTargetMoby(void)
{
  Moby* targetMoby = CollLine_Fix_GetHitMoby();
  if (!targetMoby) return NULL; // this should never happen

  // intercept moby target
  // grab guber moby instead
  // most cases this will return the same moby
  // special cases (like 2 moby mobs) will return parent moby
  Guber* guber = guberGetObjectByMoby(targetMoby);
  if (guber) {
    Moby* guberMoby = guber->VTable->GetMoby(guber);
    if (guberMoby)
      return guberMoby;
  }

  return targetMoby;
}

//--------------------------------------------------------------------------
Moby* mapOnGuberEventCreateMoby(int oclass, int pvarSize)
{
  if (mobyGetNumSpawnableMobys() < 50) {
    Moby* m = mobyFindNextByOClass(mobyListGetStart(), 0x13A1);
    if (!m) return NULL;
    if (m) {
      mobyDestroy(m);
      m->CollCnt = 0; // lets the moby be reused instantly
    }
  }

  return mobySpawn(oclass, pvarSize);
}

//--------------------------------------------------------------------------
void mapApplyFixes(void)
{
  HOOK_JAL(0x0061c3ec, &mapOnGuberEventCreateMoby);
}

//--------------------------------------------------------------------------
void mapApplyZoning(void)
{
  Player* player = playerGetFromSlot(0);
  if (!player || !MapConfig.State) return;

  int i;
  for (i = 0; i < mapDifficultyZonesCount; ++i) {
    int cuboidIdx = mapDifficultyZones[i].CuboidIdx;
    if (cuboidIdx < 0) continue;

    SpawnPoint* cuboid = spawnPointGet(cuboidIdx);
    if (!cuboid) continue;

    if (spawnPointIsPointInside(cuboid, player->PlayerPosition, NULL)) {
      MapConfig.State->DifficultyStars = mapDifficultyZones[i].Difficulty;
    }
  }
  
  MapConfig.State->Difficulty = 1;
  MapConfig.State->LivesLeft = 2;
}

//--------------------------------------------------------------------------
void mapStart(void)
{
  // tick down mob sound cooldown
  int i,j;
  for (i = 0; i < MAX_MOB_SPAWN_PARAMS; ++i) {
    for (j = 0; j < MOBS_PLAY_SOUND_COOLDOWN_MAX_SOUNDIDS; ++j) {
      if (mobPlaySoundCooldownTicks[i][j] > 0) {
        --mobPlaySoundCooldownTicks[i][j];
      }
    }
  }
}

//--------------------------------------------------------------------------
void mapTick(void)
{
  mapApplyFixes();
}

//--------------------------------------------------------------------------
void mapTickEnd(void)
{
}

//--------------------------------------------------------------------------
void mapInit(void)
{
  int i;

  // spawn area mod explosion on each ricochet of the v10 vipers
  HOOK_JAL(0x003C283C, &mapOnV10VipersHitSurface);

  // disable holoshields from disappearing
  //*(u16*)0x00401478 = 2;

  // disable ammo drop despawn
  //POKE_U32(0x004FE814, 0);

  // disable jump pad effect
  POKE_U32(0x0042608C, 0);

  // disable team based holoshield toggling
  // enables players to shoot through eachothers shields
  POKE_U32(0x005A3830, 0x2402FFFF);
  POKE_U32(0x005A3834, 0x14500018);

  // force holoshield hit testing
  *(u32*)0x00401194 = 0;
  *(u32*)0x003FFDE8 = 0x1000000D;
  POKE_U32(0x003FFD98, 0x120000DD); // fix holo crash when owner leaves

	// Disables end game draw dialog
	*(u32*)0x0061fe84 = 0;

	// sets start of colored weapon icons to v10
	*(u32*)0x005420E0 = 0x2A020009;
	*(u32*)0x005420E4 = 0x10400005;

	// Removes MP check on HudAmmo weapon icon color (so v99 is pinkish)
	*(u32*)0x00542114 = 0;

	// Enable sniper to shoot through multiple enemies
	*(u32*)0x003FC2A8 = 0;

	// Disable sniper shot corn
	//*(u32*)0x003FC410 = 0;
	*(u32*)0x003FC5A8 = 0;

	// Fix v10 arb overlapping shots
	*(u32*)0x003F2E70 = 0x24020000;

  // fix bolt crank moby (1A27) resetting itself on capture
  POKE_U32(0x003D74C0, 0);
  POKE_U16(0x003D7844, 0xBF80);
  POKE_U32(0x003D7848, 0);
  POKE_U16(0x003D7850, 0x3F80);
  POKE_U32(0x003D7854, 0);

  // hook when MobyPlayDesiredSound plays a sound for a moby
  // lets us reduce the # of mob sounds
  HOOK_JAL(0x004f790c, &mapOnMobyPlayDesiredSound);
  HOOK_J(0x004fa800, &mapOnMobyPlayDesiredSound);

  // fix emp
  //POKE_U32(0x0042075C, 0x2C42010B);
  //POKE_U32(0x00420610, 0x24050001);
  //HOOK_JAL(0x00420634, &onEmpHitMoby);
  //HOOK_JAL(0x004209bc, &onEmpExplode);
  //HOOK_JAL(0x0041fb8c, &onEmpFired);

  // hook when v10 mag shot hits
  HOOK_JAL(0x0044B374, &mapOnV10MagDamageMoby);

	// Change mine update function to ours
  u32 mineUpdateFunc = 0x003c6c28;
  u32* updateFuncs = (u32*)0x00249980;
  for (i = 0; i < 113; ++i) {
    if (*updateFuncs == mineUpdateFunc) { *updateFuncs = (u32)&mapCustomMineMobyUpdate; }
    updateFuncs++;
  }

	// Change bangelize weapons call to ours
	//*(u32*)0x005DD890 = 0x0C000000 | ((u32)&customBangelizeWeapons >> 2);

	// Enable weapon version and v10 name variant in places that display weapon name
  HOOK_J_OP(0x00541850, &mapCustomGetGadgetVersionName, 0);

	// patch who killed me to prevent damaging others
  HOOK_JAL(0x005E07C8, &mapWhoKilledMeHook);
  HOOK_JAL(0x005E11B0, &mapWhoKilledMeHook);

  // patch mobs pushing you into walls and killing you
  POKE_U32(0x005e4188, 0);
  POKE_U32(0x005e419c, 0);
  HOOK_JAL(0x005e41bc, &mapPlayerOnPushedIntoWall);

  // disable ammo drop pickup text "Got %s ammo"
  POKE_U32(0x003AC400, 0);

  // disable MP dialog messages
  //POKE_U32(0x00620E70, 0);
  //POKE_U32(0x00620e88, 0);

	// set default ammo for flail to 8
	//*(u8*)0x0039A3B4 = 8;

	// disable targeting players
	*(u32*)0x005F8A80 = 0x10A20002;
	*(u32*)0x005F8A84 = 0x0000102D;
	*(u32*)0x005F8A88 = 0x24440001;
  
  // hook spawn
  HOOK_JAL(0x00610724, &mapGetResurrectPoint);
  HOOK_JAL(0x005e2d44, &mapGetResurrectPoint);

  // hook healthbox heal
  HOOK_JAL(0x004130F4, &mapOnHealthboxHeal);

  // hook set player target moby
  HOOK_JAL(0x005f7dcc, &mapOnSetPlayerTargetMoby);

  // set health
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player) || playerIsDead(player)) continue;
    
    playerSetHealth(player, player->MaxHealth);
  }
}
