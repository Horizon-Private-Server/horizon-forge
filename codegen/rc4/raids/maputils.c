/***************************************************
 * FILENAME :		maputils.c
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
#include "mob.h"
#include "game.h"
#include "maputils.h"

extern char LocalPlayerStrBuffer[2][64];
extern struct RaidsMapConfig MapConfig;

/* 
 * paid sound def
 */
SoundDef PaidSoundDef =
{
	0.0,	// MinRange
	20.0,	// MaxRange
	100,		// MinVolume
	2000,		// MaxVolume
	0,			// MinPitch
	0,			// MaxPitch
	0,			// Loop
	0x10,		// Flags
	32,		  // Index
	3			  // Bank
};

//--------------------------------------------------------------------------
Moby * spawnExplosion(VECTOR position, float size, u32 color)
{
	// SpawnMoby_5025
  Moby* moby = mobySpawnExplosion(
    vector_read(position), 0, 0, 0, 0, 16, 0, 16, 0, 1, 0, 0, 0, 0,
    0, 0, color, color, color, color, color, color, color, color,
    0, 0, 0, 0, 0, size / 2.5, 0, 0, 0
  );
  
  mobyPlaySoundByClass(0, 0, moby, MOBY_ID_ARBITER_ROCKET0);

	return moby;
}

//--------------------------------------------------------------------------
void damageRadius(Moby* moby, VECTOR position, u32 damageFlags, float damage, float damageRadius)
{
	MobyColDamageIn in;

  vector_write(in.Momentum, 0);
  in.Damager = moby;
  in.DamageFlags = damageFlags;
  in.DamageClass = 0;
  in.DamageStrength = 1;
  in.DamageIndex = moby->OClass;
  in.Flags = 1;
  in.DamageHp = damage;

  CollMobysSphere_Fix(position, COLLISION_FLAG_IGNORE_STATIC, moby, &in, damageRadius);
}

//--------------------------------------------------------------------------
void playPaidSound(Player* player)
{
  if (!player) return;
  soundPlay(&PaidSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
GuberEvent* guberCreateEvent(Moby* moby, u32 eventType)
{
	GuberEvent * event = NULL;

	// create guber object
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
struct PartInstance * spawnParticle(VECTOR position, u32 color, char opacity, int idx)
{
	u32 a3 = *(u32*)0x002218E8;
	u32 t0 = *(u32*)0x002218E4;
	float f12 = *(float*)0x002218DC;
	float f1 = *(float*)0x002218E0;

	return ((struct PartInstance* (*)(VECTOR, u32, char, u32, u32, int, int, int, float))0x00533308)(position, color, opacity, a3, t0, -1, 0, 0, f12 + (f1 * idx));
}

//--------------------------------------------------------------------------
void destroyParticle(struct PartInstance* particle)
{
	((void (*)(struct PartInstance*))0x005284d8)(particle);
}

//--------------------------------------------------------------------------
float getSignedRelativeSlope(VECTOR forward, VECTOR normal)
{
  VECTOR up = {0,0,1,0}, right;
  VECTOR projectedNormal;

  vector_outerproduct(right, forward, up);
  vector_projectonplane(projectedNormal, normal, right);
  vector_outerproduct(up, forward, projectedNormal);
  return atan2f(vector_length(up), vector_innerproduct(forward, projectedNormal)) - MATH_PI/2;
}

//--------------------------------------------------------------------------
float getSignedSlope(VECTOR forward, VECTOR normal)
{
  VECTOR up, hForward;

  vector_projectonhorizontal(hForward, forward);
  vector_normalize(hForward, hForward);
  vector_outerproduct(up, hForward, normal);
  return atan2f(vector_length(up), vector_innerproduct(hForward, normal)) - MATH_PI/2;
}

//--------------------------------------------------------------------------
u8 decTimerU8(u8* timeValue)
{
	int value = *timeValue;
	if (value == 0)
		return 0;

	*timeValue = --value;
	return value;
}

//--------------------------------------------------------------------------
u16 decTimerU16(u16* timeValue)
{
	int value = *timeValue;
	if (value == 0)
		return 0;

	*timeValue = --value;
	return value;
}

//--------------------------------------------------------------------------
u32 decTimerU32(u32* timeValue)
{
	long value = *timeValue;
	if (value == 0)
		return 0;

	*timeValue = --value;
	return value;
}

//--------------------------------------------------------------------------
void pushSnack(int localPlayerIdx, char* string, int ticksAlive)
{
  if (MapConfig.PushSnackFunc)
    MapConfig.PushSnackFunc(string, ticksAlive, localPlayerIdx);
  else
    uiShowPopup(localPlayerIdx, string);
}

//--------------------------------------------------------------------------
void uiShowLowerPopup(int localPlayerIdx, int msgStringId)
{
	((void (*)(int, int, int))0x0054ea30)(localPlayerIdx, msgStringId, 0);
}

//--------------------------------------------------------------------------
int isInDrawDist(Moby* moby)
{
  int i;
  VECTOR delta;
  if (!moby)
    return 0;

  int drawDistSqr = moby->DrawDist*moby->DrawDist;

  for (i = 0; i < 2; ++i) {
    GameCamera* camera = cameraGetGameCamera(i);
    if (!camera)
      continue;
    
    // check if in range of camera
    vector_subtract(delta, camera->pos, moby->Position);
    if (vector_sqrmag(delta) < drawDistSqr)
      return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int mobyIsMob(Moby* moby)
{
  if (!moby) return 0;

  int i;
  for (i = 0; i < MapConfig.MobSpawnParamsCount; ++i) {
    if (MapConfig.MobSpawnParams[i].OClass == moby->OClass)
      return 1;
  }

  return moby->OClass == NPC_MOBY_OCLASS;
}

//--------------------------------------------------------------------------
int mobyIsNpc(Moby* moby)
{
  return moby && moby->OClass == NPC_MOBY_OCLASS;
}

//--------------------------------------------------------------------------
Moby* mobyGetFromIdxOrNull(int mobyIdx)
{
  if (mobyIdx < 0) return NULL;

  Moby* moby = mobyListGetStart() + mobyIdx;
  if (mobyIsDestroyed(moby)) return NULL;

  return moby;
}

//--------------------------------------------------------------------------
Player* mobyGetPlayer(Moby* moby)
{
  if (!moby) return 0;
  
  Player** players = playerGetAll();
  int i;

  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player) continue;

    if (player->PlayerMoby == moby) return player;
    if (player->SkinMoby == moby) return player;
  }

  return NULL;
}

//--------------------------------------------------------------------------
Moby* playerGetTargetMoby(Player* player)
{
  if (!player) return NULL;
  return player->SkinMoby;
}

//--------------------------------------------------------------------------
void draw3DMarker(VECTOR position, float scale, u32 color, char* str)
{
  int x,y;
  if (gfxWorldSpaceToScreenSpace(position, &x, &y)) {
    gfxScreenSpaceText(x, y, scale, scale, color, str, -1, 4);
  }
}

//--------------------------------------------------------------------------
void playDialog(short dialogId, int force)
{
  const int flag = 1;

  // reset play count
  if (force) {
    POKE_U16(0x001f1400 + (flag * 12), 0);
  }

  ((int (*)(short, short))0x004e3da8)(dialogId, flag);
}

//--------------------------------------------------------------------------
void transformToSplitscreenPixelCoordinates(int localPlayerIndex, float *x, float *y)
{
  int localCount = playerGetNumLocals();

  //
  switch (localCount)
  {
    case 0: // 1 player
    case 1: return;
    case 2: // 2 players
    {
      // vertical split
      *y *= 0.5;
      if (localPlayerIndex == 1)
        *y += 0.5 * SCREEN_HEIGHT;

      break;
    }
    case 3: // 3 players
    {
      // player 1 on top
      // player 2/3 horizontal split on bottom
      *y *= 0.5;
      if (localPlayerIndex > 0) {
        *x *= 0.5;
        *y += 0.5 * SCREEN_HEIGHT;
        if (localPlayerIndex == 2)
          *x += 0.5 * SCREEN_WIDTH;
      }
      break;
    }
    case 4: // 4 players
    {
      // player 1/2 horizontal split on top
      // player 2/3 horizontal split on bottom
      *x *= 0.5;
      *y *= 0.5;
      if ((localPlayerIndex % 2) == 1)
        *x += 0.5 * SCREEN_WIDTH;
      if ((localPlayerIndex / 2) == 1)
        *y += 0.5 * SCREEN_HEIGHT;

      break;
    }
  }
}

//--------------------------------------------------------------------------
int selectRandomIndex(int count, void* userdata, CanSelectIndex_func canSelectIndexFunc)
{
  if (!canSelectIndexFunc) return -1;

  int idx = -1;
  int ticker = rand(count) + 1;
  int iterations = 0, hasAny = 0;
  while (ticker > 0) {
    
    // looped all spawn cuboids and none exist
    if (iterations > count && !hasAny) return -1;
    
    idx = iterations++ % count;
    if (canSelectIndexFunc(userdata, idx)) {
      hasAny = 1;
      --ticker;
    }
  }

  return idx;
}

//--------------------------------------------------------------------------
int hasPendingWorldHop(void)
{
  return MapConfig.State && MapConfig.State->PendingWorldHopMapDef && MapConfig.State->PendingWorldHopAtTime > 0;
}

//--------------------------------------------------------------------------
int missionIsComplete(void)
{
  return MapConfig.State && MapConfig.State->MissionComplete;
}

//--------------------------------------------------------------------------
int isOnHubWorld(void)
{
  return MapConfig.State && MapConfig.State->OnHubWorld;
}

//--------------------------------------------------------------------------
int missionIsActive(void)
{
  return !hasPendingWorldHop() && !missionIsComplete();
}

//--------------------------------------------------------------------------
int bankTryChargeLocalAccount(u32 cost)
{
  if (!MapConfig.GetBankFunc) return 0;

  RaidsPlayerBank_t* bank = MapConfig.GetBankFunc();
  if (!bank) return 0;

  if (bank->Account.Bolts < cost) return 0;

  // charge
  bank->Account.Bolts -= cost;

  // send new bolts to server
  if (MapConfig.SendBankAccountToServerFunc)
    MapConfig.SendBankAccountToServerFunc();

  return 1;
}

//--------------------------------------------------------------------------
int getAmmoRefillCost(Player* player)
{

}

//--------------------------------------------------------------------------
void replenishAmmo(Player* player)
{
  // if any weapon ran out of ammo, return back to max
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!player || !player->GadgetBox) continue;

    int j;
    for (j = WEAPON_SLOT_VIPERS; j < WEAPON_SLOT_COUNT; ++j) {
      int gadgetId = weaponSlotToId(j);
      if (player->GadgetBox->Gadgets[gadgetId].Level >= 0 && player->GadgetBox->Gadgets[gadgetId].Ammo <= 0) {
        player->GadgetBox->Gadgets[gadgetId].Ammo = playerGetWeaponMaxAmmo(player->GadgetBox, gadgetId);
      }
    }
  }
}

//--------------------------------------------------------------------------
void respawnAllPlayers(void)
{
  // respawn all players
  Player** players = playerGetAll();
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->PlayerMoby || !player->pNetPlayer) continue;
    
    // respawn player
    playerGetSpawnpoint(player, player->PlayerPosition, player->PlayerRotation, 1);
    vector_copy(player->PlayerMoby->Position, player->PlayerPosition);
    if (!player->IsLocal) {
      memset((void*)((u32)player->pNetPlayer + 0x38), 0, 0xAD0 - 0x38);
      player->pNetPlayer->lastActiveSeqNum = -1;
    }
  }
}

//--------------------------------------------------------------------------
void * mobyGetClassPtr(int oClass)
{
  int mClass = *(u8*)(0x0024a110 + oClass);
  return *(u32*)(0x002495c0 + mClass*4);
}
