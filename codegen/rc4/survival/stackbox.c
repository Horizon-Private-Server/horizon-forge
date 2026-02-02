/***************************************************
 * FILENAME :		stackbox.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the stack box.
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
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/math3d.h>
#include <libdl/radar.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "stackbox.h"
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "maputils.h"
#include "common.h"

char* STACKABLE_ITEM_NAMES[] = {
  [STACKABLE_ITEM_EXTRA_JUMP] "Extra Jump Perk",
  [STACKABLE_ITEM_EXTRA_SHOT] "Double Shot Perk",
  [STACKABLE_ITEM_HOVERBOOTS] "Hoverboots Perk",
  [STACKABLE_ITEM_LOW_HEALTH_DMG_BUF] "Berserker Perk",
  [STACKABLE_ITEM_ALPHA_MOD_AMMO] "Ammo Mod Perk",
  [STACKABLE_ITEM_ALPHA_MOD_SPEED] "Speed Mod Perk",
  [STACKABLE_ITEM_ALPHA_MOD_AREA] "Area Mod Perk",
  [STACKABLE_ITEM_ALPHA_MOD_IMPACT] "Impact Mod Perk",
  [STACKABLE_ITEM_VAMPIRE] "Vampire Perk",
  [STACKABLE_ITEM_EXPLODING_ENEMIES] "Will o' the Wisp",
};

char* STACKABLE_ITEM_DESC[] = {
  [STACKABLE_ITEM_EXTRA_JUMP] "Gives +1 extra jump",
  [STACKABLE_ITEM_EXTRA_SHOT] "Shoots +1 shot each time you fire",
  [STACKABLE_ITEM_HOVERBOOTS] "Gives hoverboots, +10%% movement speed, removes chargeboots",
  [STACKABLE_ITEM_LOW_HEALTH_DMG_BUF] "Grants +75%% bonus damage for amount of health missing",
  [STACKABLE_ITEM_ALPHA_MOD_AMMO] "Gives +2 extra ammo mod for all weapons",
  [STACKABLE_ITEM_ALPHA_MOD_SPEED] "Gives +2 extra speed mod for all weapons",
  [STACKABLE_ITEM_ALPHA_MOD_AREA] "Gives +2 extra area mod for all weapons",
  [STACKABLE_ITEM_ALPHA_MOD_IMPACT] "Gives +2 extra impact mod for all weapons",
  [STACKABLE_ITEM_VAMPIRE] "Gives +3 health after each kill, disables health pickups",
  [STACKABLE_ITEM_EXPLODING_ENEMIES] "Deals +10 explosive damage after each kill",
};

int STACKABLE_ITEM_TEX_IDS[] = {
  [STACKABLE_ITEM_EXTRA_JUMP] 111,
  [STACKABLE_ITEM_EXTRA_SHOT] 108,
  [STACKABLE_ITEM_HOVERBOOTS] 60,
  [STACKABLE_ITEM_LOW_HEALTH_DMG_BUF] 109,
  [STACKABLE_ITEM_ALPHA_MOD_AMMO] 41 - 3,
  [STACKABLE_ITEM_ALPHA_MOD_SPEED] 55 - 3,
  [STACKABLE_ITEM_ALPHA_MOD_AREA] 42 - 3,
  [STACKABLE_ITEM_ALPHA_MOD_IMPACT] 47 - 3,
  [STACKABLE_ITEM_VAMPIRE] 110,
  [STACKABLE_ITEM_EXPLODING_ENEMIES] 131,
};

u32 STACKABLE_ITEM_COLORS[] = {
  [STACKABLE_ITEM_EXTRA_JUMP] 0x80FFFFFF,
  [STACKABLE_ITEM_EXTRA_SHOT] 0x80FFFFFF,
  [STACKABLE_ITEM_HOVERBOOTS] 0x80FFFFFF,
  [STACKABLE_ITEM_LOW_HEALTH_DMG_BUF] 0x80FFFFFF,
  [STACKABLE_ITEM_ALPHA_MOD_AMMO] 0x80FFFFFF,
  [STACKABLE_ITEM_ALPHA_MOD_SPEED] 0x80FFFFFF,
  [STACKABLE_ITEM_ALPHA_MOD_AREA] 0x80FFFFFF,
  [STACKABLE_ITEM_ALPHA_MOD_IMPACT] 0x80FFFFFF,
  [STACKABLE_ITEM_VAMPIRE] 0x80FFFFFF,
  [STACKABLE_ITEM_EXPLODING_ENEMIES] 0x80FFFFFF,
};

int STACKABLE_ITEM_MAX[] = {
  [STACKABLE_ITEM_EXTRA_JUMP] 0,
  [STACKABLE_ITEM_EXTRA_SHOT] 0,
  [STACKABLE_ITEM_HOVERBOOTS] 0,
  [STACKABLE_ITEM_LOW_HEALTH_DMG_BUF] 0,
  [STACKABLE_ITEM_ALPHA_MOD_AMMO] 0,
  [STACKABLE_ITEM_ALPHA_MOD_SPEED] 10,
  [STACKABLE_ITEM_ALPHA_MOD_AREA] 5,
  [STACKABLE_ITEM_ALPHA_MOD_IMPACT] 5,
  [STACKABLE_ITEM_VAMPIRE] 0,
  [STACKABLE_ITEM_EXPLODING_ENEMIES] 0
};

extern int StackboxItems[];
extern const int StackboxItemsCount;

//--------------------------------------------------------------------------
int sboxCanBuy(int playerId, enum StackableItemId item)
{
  if (!MapConfig.State) return 0;

  int count = playerGetStackableCount(playerId, (int)item);
  return STACKABLE_ITEM_MAX[(int)item] <= 0 || count < STACKABLE_ITEM_MAX[(int)item];
}

//--------------------------------------------------------------------------
int sboxGetStackableCost(int playerId, enum StackableItemId item)
{
  if (!MapConfig.State) return 0;


  return bakedConfig.StackboxBaseCost + playerGetStackableCount(playerId, (int)item) * bakedConfig.StackboxCostPerPerk;
}

//--------------------------------------------------------------------------
int sboxGetRandomItem(Moby* moby)
{
  if (!moby) return StackboxItems[rand(StackboxItemsCount)];

  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;

  // randomly pick an item
  // avoid picking the current item
  int item = pvars->Item;
  while (item == pvars->Item)
    item = StackboxItems[rand(StackboxItemsCount)];

  return item;
}

//--------------------------------------------------------------------------
void sboxActivate(Moby* moby, int state, enum StackableItemId item, int activatedTime)
{
  int i;

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, STACK_BOX_EVENT_ACTIVATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
    guberEventWrite(guberEvent, &item, 4);
    guberEventWrite(guberEvent, &activatedTime, 4);
  }
}

//--------------------------------------------------------------------------
void sboxGivePlayer(Moby* moby, int playerId, enum StackableItemId item, int cost)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, STACK_BOX_EVENT_GIVE_PLAYER);
  if (guberEvent) {
    guberEventWrite(guberEvent, &playerId, 4);
    guberEventWrite(guberEvent, &item, 4);
    guberEventWrite(guberEvent, &cost, 4);
  }
}

//--------------------------------------------------------------------------
void sboxPlayerBuy(Moby* moby, int playerId, enum StackableItemId item)
{
  int cost = sboxGetStackableCost(playerId, item);

  // check if valid
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (moby->State != STACK_BOX_STATE_ACTIVE) return;
  if (pvars->Item != item) return;
  if (!MapConfig.State) return;
  if (MapConfig.State->PlayerStates[playerId].State.Bolts < cost) return;
  if ((gameGetTime() - pvars->ActivatedTime) < (TIME_SECOND * 0.2)) return;
  if (!sboxCanBuy(playerId, item)) return;

  // send to host
  if (!gameAmIHost()) {
    GuberEvent * guberEvent = guberCreateEvent(moby, STACK_BOX_EVENT_PLAYER_BUY);
    if (guberEvent) {
      guberEventWrite(guberEvent, &playerId, 4);
      guberEventWrite(guberEvent, &item, 4);
    }

    return;
  }

  // give player
  sboxGivePlayer(moby, playerId, item, cost);

  // set new item
  sboxActivate(moby, moby->State, sboxGetRandomItem(moby), gameGetTime());
}

//--------------------------------------------------------------------------
void sboxPlayerReroll(Moby* moby, int playerId, enum StackableItemId item)
{
  int cost = STACK_BOX_REROLL_COST;

  // check if valid
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (moby->State != STACK_BOX_STATE_ACTIVE) return;
  if (pvars->Item != item) return;
  if (!MapConfig.State) return;
  if (MapConfig.State->PlayerStates[playerId].State.Bolts < cost) return;
  if ((gameGetTime() - pvars->ActivatedTime) < TIME_SECOND) return;

  // send to host
  if (!gameAmIHost()) {
    GuberEvent * guberEvent = guberCreateEvent(moby, STACK_BOX_EVENT_PLAYER_REROLL);
    if (guberEvent) {
      guberEventWrite(guberEvent, &playerId, 4);
      guberEventWrite(guberEvent, &item, 4);
    }

    return;
  }

  // deduct cost from player bolts
  sboxGivePlayer(moby, playerId, -1, cost);

  // set new item
  sboxActivate(moby, moby->State, sboxGetRandomItem(moby), gameGetTime());
}

//--------------------------------------------------------------------------
void sboxDraw(Moby* moby)
{
  MATRIX m2, mRot;
	VECTOR t;
  VECTOR offset = {0,0,3,0};
	VECTOR pTL = {0.5,0,0.5,1};
	VECTOR pTR = {-0.5,0,0.5,1};
	VECTOR pBL = {0.5,0,-0.5,1};
	VECTOR pBR = {-0.5,0,-0.5,1};
  struct QuadDef quad;

  if (!moby || !moby->PVar || moby->State == STACK_BOX_STATE_DISABLED)
    return;

  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  int item = pvars->Item;
  u32 color = STACKABLE_ITEM_COLORS[item];
  int texId = STACKABLE_ITEM_TEX_IDS[item];

	// set draw args
	matrix_unit(m2);

	// init
  gfxResetQuad(&quad);

	// color of each corner?
	vector_copy(quad.VertexPositions[0], pTL);
	vector_copy(quad.VertexPositions[1], pTR);
	vector_copy(quad.VertexPositions[2], pBL);
	vector_copy(quad.VertexPositions[3], pBR);
	quad.VertexColors[0] = quad.VertexColors[1] = quad.VertexColors[2] = quad.VertexColors[3] = color;
  quad.VertexUVs[0] = (struct UV){0,0};
  quad.VertexUVs[1] = (struct UV){1,0};
  quad.VertexUVs[2] = (struct UV){0,1};
  quad.VertexUVs[3] = (struct UV){1,1};
	quad.Clamp = 0x0000000100000001;
	quad.Tex0 = gfxGetFrameTex(texId);
	quad.Tex1 = 0xFF9000000260;
	quad.Alpha = 0x8000000044;

	GameCamera* camera = cameraGetGameCamera(0);
	if (!camera)
		return;

	// get forward vector
	vector_subtract(t, camera->pos, moby->Position);
	t[2] = 0;
	vector_normalize(&m2[4], t);

	// up vector
	m2[8 + 2] = 1;

	// right vector
	vector_outerproduct(&m2[0], &m2[4], &m2[8]);

	// position
  matrix_unit(mRot);
  memcpy(mRot, moby->M0_03, sizeof(VECTOR)*3);
  vector_apply(offset, offset, mRot);
  vector_scale(offset, offset, 1);
	vector_add(&m2[12], moby->Position, offset);

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 0);
}

//--------------------------------------------------------------------------
void sboxUpdate(Moby* moby)
{
  Player** players = playerGetAll();
  int i;
  char buf[64];
  if (!moby || !moby->PVar)
    return;
    
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  int timeSinceActivated = gameGetTime() - pvars->ActivatedTime;

  // set state
#if !DEBUGSBOX
  if (MapConfig.State && MapConfig.State->RoundCompleteTime) {
    if (moby->State != STACK_BOX_STATE_ACTIVE) {
      if (pvars->BaseMoby) pvars->BaseMoby->ModeBits = MOBY_MODE_BIT_HIDE_BACKFACES | MOBY_MODE_BIT_HAS_GLOW;
      mobySetState(moby, STACK_BOX_STATE_ACTIVE, -1);
      if (gameAmIHost()) sboxActivate(moby, STACK_BOX_STATE_ACTIVE, sboxGetRandomItem(moby), gameGetTime());
    }
  } else {
    if (moby->State != STACK_BOX_STATE_DISABLED) {
      mobySetState(moby, STACK_BOX_STATE_DISABLED, -1);
      if (gameAmIHost()) sboxActivate(moby, STACK_BOX_STATE_DISABLED, sboxGetRandomItem(moby), gameGetTime());
    }
  }
#endif

	// show/hide
  if (moby->State == STACK_BOX_STATE_DISABLED) {
    moby->ModeBits |= MOBY_MODE_BIT_DISABLED;
    moby->CollActive = -1;
    if (pvars->BaseMoby) {
      pvars->BaseMoby->CollActive = -1;
      pvars->BaseMoby->ModeBits |= MOBY_MODE_BIT_DISABLED;
    }
    return;
  } else {
    moby->ModeBits &= ~MOBY_MODE_BIT_DISABLED;
    moby->CollActive = 0;
    if (pvars->BaseMoby) {
      pvars->BaseMoby->CollActive = 0;
      pvars->BaseMoby->ModeBits &= ~MOBY_MODE_BIT_DISABLED;
    }
  }

  // post draw  
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&sboxDraw, moby);

  // map icon
  int blipIdx = radarGetBlipIndex(moby);
  if (blipIdx >= 0) {
    RadarBlip * blip = radarGetBlips() + blipIdx;
    blip->X = moby->Position[0];
    blip->Y = moby->Position[1];
    blip->Life = 0x1F;
    blip->Type = 5;
    blip->Team = TEAM_ORANGE;
  }

  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!player || !player->pNetPlayer || !playerIsConnected(player)) continue;

    // prompt for 
    if (!sboxCanBuy(i, pvars->Item)) {
      
      tryPlayerInteract(moby, player, "Maxed Out", STACKABLE_ITEM_DESC[pvars->Item], 0, 0, PLAYER_STACK_BOX_COOLDOWN_TICKS, STACK_BOX_MAX_DIST*STACK_BOX_MAX_DIST, PAD_CIRCLE);
    } else {

      int cost = sboxGetStackableCost(player->PlayerId, pvars->Item);
      snprintf(buf, sizeof(buf), "\x11 %s \x01\x0E%'d\x08", STACKABLE_ITEM_NAMES[pvars->Item], cost);
      if (tryPlayerInteract(moby, player, buf, STACKABLE_ITEM_DESC[pvars->Item], 0, 0, PLAYER_STACK_BOX_COOLDOWN_TICKS, STACK_BOX_MAX_DIST*STACK_BOX_MAX_DIST, PAD_CIRCLE)) {
        sboxPlayerBuy(moby, player->PlayerId, pvars->Item);
      }
    }
  }
}

//--------------------------------------------------------------------------
int sboxHandleEvent_Spawned(Moby* moby, GuberEvent* event)
{
  DPRINTF("sbox spawned: %08X\n", (u32)moby);
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
	// read event
  int item;
	guberEventRead(event, moby->Position, 12);
	guberEventRead(event, moby->Rotation, 12);
	guberEventRead(event, &item, 4);

	// set update
	moby->PUpdate = &sboxUpdate;

  // set item
  pvars->Item = item;

  // add base
  Moby* baseMoby = pvars->BaseMoby = mobySpawn(8348, 0);
  if (baseMoby) {
    vector_copy(baseMoby->Position, moby->Position);
    baseMoby->Scale = 0.14;
    baseMoby->PUpdate = NULL;
    baseMoby->ModeBits = MOBY_MODE_BIT_HIDE_BACKFACES | MOBY_MODE_BIT_HAS_GLOW;
    baseMoby->Opacity = 0x80;
    baseMoby->UpdateDist = 0xFF;
    baseMoby->DrawDist = 0x00FF;
  }

  // lower glass a bit
  moby->Position[2] -= 1;

  // set default state
	mobySetState(moby, STACK_BOX_STATE_ACTIVE, -1);
  return 0;
}

//--------------------------------------------------------------------------
int sboxHandleEvent_Activate(Moby* moby, GuberEvent* event)
{
  int state, item, activatedTime;
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  guberEventRead(event, &state, 4);
  guberEventRead(event, &item, 4);
  guberEventRead(event, &activatedTime, 4);
  
  // update
  mobySetState(moby, state, -1);
  pvars->Item = item;
  pvars->ItemTexId = STACKABLE_ITEM_TEX_IDS[item];
  pvars->ActivatedTime = activatedTime;
  return 0;
}

//--------------------------------------------------------------------------
int sboxHandleEvent_GivePlayer(Moby* moby, GuberEvent* event)
{
  int playerId, item, cost, i;
  Player** players = playerGetAll();
  
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  // read
  guberEventRead(event, &playerId, 4);
  guberEventRead(event, &item, 4);
  guberEventRead(event, &cost, 4);
  
  if (!MapConfig.State) return 0;

  // add
  MapConfig.State->PlayerStates[playerId].State.Bolts -= cost;

  // item can be negative if box wants to only charge player bolts
  if (item >= 0) {
    MapConfig.State->PlayerStates[playerId].State.ItemStackable[item] += 1;

    Player* player = playerGetAll()[playerId];
    playPaidSound(player);
    if (player && playerIsLocal(player)) {
      char buf[64];
      snprintf(buf, sizeof(buf), "Got %s", STACKABLE_ITEM_NAMES[item]);
      pushSnack(player->LocalPlayerIndex, buf, 120);
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
int sboxHandleEvent_PlayerBuy(Moby* moby, GuberEvent* event)
{
  int playerId, item, i;
  Player** players = playerGetAll();
  
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  // read
  guberEventRead(event, &playerId, 4);
  guberEventRead(event, &item, 4);

  // only handle if host
  if (gameAmIHost()) {
    sboxPlayerBuy(moby, playerId, item);
  }
  return 0;
}

//--------------------------------------------------------------------------
int sboxHandleEvent_PlayerReroll(Moby* moby, GuberEvent* event)
{
  int playerId, item, i;
  Player** players = playerGetAll();
  
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  // read
  guberEventRead(event, &playerId, 4);
  guberEventRead(event, &item, 4);

  // only handle if host
  if (gameAmIHost()) {
    sboxPlayerReroll(moby, playerId, item);
  }
  return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby* sboxGetGuber(Moby* moby)
{
	if (moby->OClass == STACK_BOX_OCLASS && moby->PVar)
		return moby->GuberMoby;
	
	return 0;
}

//--------------------------------------------------------------------------
int sboxHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == STACK_BOX_OCLASS && moby->PVar) {
		u32 sboxEvent = event->NetEvent.EventID;

		switch (sboxEvent)
		{
			//case STACK_BOX_EVENT_SPAWN: return sboxHandleEvent_Spawned(moby, event);
			case STACK_BOX_EVENT_ACTIVATE: return sboxHandleEvent_Activate(moby, event);
			case STACK_BOX_EVENT_GIVE_PLAYER: return sboxHandleEvent_GivePlayer(moby, event);
			case STACK_BOX_EVENT_PLAYER_BUY: return sboxHandleEvent_PlayerBuy(moby, event);
			case STACK_BOX_EVENT_PLAYER_REROLL: return sboxHandleEvent_PlayerReroll(moby, event);
			default:
			{
				DPRINTF("unhandle sbox event %d\n", sboxEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void sboxOnGuberCreated(Moby* moby)
{
  struct StackBoxPVar* pvars = (struct StackBoxPVar*)moby->PVar;

  pvars->Item = sboxGetRandomItem(NULL);
	moby->PUpdate = &sboxUpdate;
  moby->UpdateDist = -1;
  moby->DrawDist = 255;
  moby->ModeBits = MOBY_MODE_BIT_DISABLE_Z_WRITE | MOBY_MODE_BIT_LOCK_ROTATION | MOBY_MODE_BIT_DRAW_SHADOW;

  // add base
  Moby* baseMoby = pvars->BaseMoby = mobySpawn(8348, 0);
  if (baseMoby) {
    vector_copy(baseMoby->Position, moby->Position);
    baseMoby->Scale = 0.14 * (moby->Scale / 0.25);
    baseMoby->PUpdate = NULL;
    baseMoby->ModeBits = MOBY_MODE_BIT_HIDE_BACKFACES | MOBY_MODE_BIT_HAS_GLOW;
    baseMoby->Opacity = moby->Opacity;
    baseMoby->DrawDist = moby->DrawDist;
    mobyUpdateTransform(baseMoby);
  }

  // lower glass a bit
  moby->Position[2] -= 1;

  // set default state
	mobySetState(moby, STACK_BOX_STATE_ACTIVE, -1);
}

//--------------------------------------------------------------------------
int sboxCreate(VECTOR position, VECTOR rotation)
{
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(STACK_BOX_OCLASS, sizeof(struct StackBoxPVar), &guberEvent, NULL);
	if (guberEvent)
	{
    int item = sboxGetRandomItem(NULL);
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, rotation, 12);
		guberEventWrite(guberEvent, &item, 4);
	}
	else
	{
		DPRINTF("failed to guberevent sbox\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void sboxFrameTick(void)
{
  char buffer[64];
  int i;

  if (!MapConfig.State) return;
  
  // draw upgrade counts
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* p = playerGetFromSlot(i);
    if (!p || !p->PlayerMoby) continue;
    struct SurvivalPlayer* playerData = &MapConfig.State->PlayerStates[p->PlayerId];

    if (gameIsStartMenuOpen(i) && !playerData->IsInWeaponsMenu) {
      int j;
      for (j = 0; j < STACKABLE_ITEM_COUNT; ++j) {
        float x = 400;
        float y = 54 + (j * 32);

        transformToSplitscreenPixelCoordinates(i, &x, &y);

        // draw count
        int count = playerGetStackableCount(p->PlayerId, j);
        snprintf(buffer, 32, "%d", count);
        gfxScreenSpaceText(x+2, y+8+2, 1, 1, 0x40000000, buffer, -1, 0);
        x = gfxScreenSpaceText(x,   y+8,   1, 1, 0x80C0C0C0, buffer, -1, 0);
        
        // draw icon
        gfxSetupGifPaging(0);
        u64 texId = gfxGetFrameTex(STACKABLE_ITEM_TEX_IDS[j]);
        gfxDrawSprite(x+2,   y+8,   16, 16, 0, 0, 32, 32, 0x80C0C0C0, texId);
        x += 16 + 4;
        gfxDoGifPaging();

        // draw factor
        switch (j)
        {
          case STACKABLE_ITEM_ALPHA_MOD_AMMO:
          case STACKABLE_ITEM_ALPHA_MOD_SPEED:
          case STACKABLE_ITEM_ALPHA_MOD_IMPACT:
          case STACKABLE_ITEM_ALPHA_MOD_AREA:
            snprintf(buffer, 32, "+%d", count * ITEM_STACKABLE_ALPHA_MOD_AMT); break;
          case STACKABLE_ITEM_EXTRA_JUMP: snprintf(buffer, 32, "+%d", count); break;
          case STACKABLE_ITEM_EXTRA_SHOT: snprintf(buffer, 32, "+%d", count); break;
          case STACKABLE_ITEM_LOW_HEALTH_DMG_BUF: snprintf(buffer, 32, "+%.f%%", count * ITEM_STACKABLE_LOW_HEALTH_DMG_BUF_FAC * 100); break;
          case STACKABLE_ITEM_HOVERBOOTS: snprintf(buffer, 32, "+%.0f%%", ITEM_STACKABLE_HOVERBOOTS_SPEED_BUF * count * 100); break;
          case STACKABLE_ITEM_VAMPIRE: snprintf(buffer, 32, "+%d", ITEM_STACKABLE_VAMPIRE_HEALTH_AMT * count); break;
          case STACKABLE_ITEM_EXPLODING_ENEMIES: snprintf(buffer, 32, "+%d", ITEM_STACKABLE_EXPLODINGENEMIES_DAMAGE * count); break;
        }
        gfxScreenSpaceText(x+6, y+8+2, 1, 1, 0x40000000, buffer, -1, 0);
        gfxScreenSpaceText(x+4,   y+8,   1, 1, 0x8000C0C0, buffer, -1, 0);
      }
    }
  }
}

//--------------------------------------------------------------------------
void sboxInit(void)
{
  Moby* temp = mobySpawn(STACK_BOX_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  u32 mobyFunctionsPtr = (u32)mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("SBOX oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);

  // create gubers for stackboxes
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, STACK_BOX_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DPRINTF("created sbox guber %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        sboxOnGuberCreated(moby);
      }
    }

		++moby;
	}
}
