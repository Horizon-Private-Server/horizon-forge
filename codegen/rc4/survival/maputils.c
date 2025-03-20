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
#include "messageid.h"
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
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "game.h"
#include "game.h"

extern char LocalPlayerStrBuffer[2][64];
extern struct SurvivalMapConfig MapConfig;


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
  float damageRadiusSqr = damageRadius * damageRadius;
  Moby* hitMoby;

  vector_write(in.Momentum, 0);
  in.Damager = moby;
  in.DamageFlags = damageFlags;
  in.DamageClass = 0;
  in.DamageStrength = 1;
  in.DamageIndex = moby->OClass;
  in.Flags = 1;
  in.DamageHp = damage;

  CollMobysSphere_Fix(position, 1, moby, &in, damageRadius);
}

//--------------------------------------------------------------------------
void playPaidSound(Player* player)
{
  if (!player) return;
  soundPlay(&PaidSoundDef, 0, player->PlayerMoby, 0, 0x400);
}

//--------------------------------------------------------------------------
int tryPlayerInteract(Moby* moby, Player* player, char* message, char* lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns)
{
  static int shown[GAME_MAX_LOCALS] = {0,0};
  VECTOR delta;
  if (!player || !player->PlayerMoby || !player->IsLocal || !isInGame())
    return 0;

  struct SurvivalPlayer* playerData = NULL;
  int localPlayerIndex = player->LocalPlayerIndex;
  int pIndex = player->PlayerId;

  if (MapConfig.State) {
    playerData = &MapConfig.State->PlayerStates[pIndex];
  }
  
  vector_subtract(delta, player->PlayerPosition, moby->Position);
  if (vector_sqrmag(delta) < sqrDistance) {

    // draw help popup
    snprintf(LocalPlayerStrBuffer[localPlayerIndex], sizeof(LocalPlayerStrBuffer[localPlayerIndex]), message);
    uiShowPopup(player->LocalPlayerIndex, LocalPlayerStrBuffer[localPlayerIndex]);
    shown[localPlayerIndex] = gameGetTime();

    // handle pad input
    if (padGetAnyButtonDown(localPlayerIndex, btns) > 0 && (!playerData || (playerData->State.Bolts >= boltCost && playerData->State.CurrentTokens >= tokenCost))) {
      if (playerData) {
        //playerData->State.Bolts -= boltCost;
        //playerData->State.CurrentTokens -= tokenCost;
        playerData->ActionCooldownTicks = actionCooldown;
        playerData->MessageCooldownTicks = 5;
      }

      return 1;
    }
    
    // draw lower message
    if (lowerMessage) {
      char* a = uiMsgString(0x2415);
      strncpy(a, lowerMessage, 0x40);
      uiShowLowerPopup(0, 0x2415);
    }
  } else if (shown[localPlayerIndex] && gameGetTime() > shown[localPlayerIndex]) {
    hudHidePopup();
    shown[localPlayerIndex] = 0;
  }

  return 0;
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

  return moby->OClass == ZOMBIE_MOBY_OCLASS
    || moby->OClass == EXECUTIONER_MOBY_OCLASS
    || moby->OClass == TREMOR_MOBY_OCLASS
    || moby->OClass == SWARMER_MOBY_OCLASS
    || moby->OClass == REACTOR_MOBY_OCLASS
    || moby->OClass == REAPER_MOBY_OCLASS
    ;
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
int playerHasBlessing(int playerId, int blessing)
{
  if (!MapConfig.State) return 0;

  int i;
  for (i = 0; i < PLAYER_MAX_BLESSINGS; ++i) {
    if (MapConfig.State->PlayerStates[playerId].State.ItemBlessings[i] == blessing) return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int playerGetStackableCount(int playerId, int stackable)
{
  if (!MapConfig.State) return 0;

  return MapConfig.State->PlayerStates[playerId].State.ItemStackable[stackable];
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
int bakedSpawnGetFirst(int bakedSpawnType, VECTOR outPos, VECTOR outRot) {
  if (!MapConfig.BakedConfig) return 0;

  int i;
  for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {
    if (MapConfig.BakedConfig->BakedSpawnPoints[i].Type == bakedSpawnType) {

      memcpy(outPos, MapConfig.BakedConfig->BakedSpawnPoints[i].Position, 12);
      memcpy(outRot, MapConfig.BakedConfig->BakedSpawnPoints[i].Rotation, 12);
      return 1;
    }
  }

  return 0;
}
