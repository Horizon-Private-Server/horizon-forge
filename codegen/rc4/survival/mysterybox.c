/***************************************************
 * FILENAME :		mysterybox.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the mystery box.
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
#include "mysterybox.h"
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "utils.h"
#include "maputils.h"

#if POWER
void powerOnMysteryBoxActivatePower(void);
#endif

#if DEBUGMBOX
int MysteryBoxItemCounts[MYSTERY_BOX_ITEM_COUNT];
int MysteryBoxTotalRolls = 0;
#endif

char* ITEM_NAMES[] = {
  [MYSTERY_BOX_ITEM_RESET_GATE] "",
  [MYSTERY_BOX_ITEM_TEDDY_BEAR] "",
  [MYSTERY_BOX_ITEM_RANDOMIZE_WEAPON_PICKUPS] "",
  [MYSTERY_BOX_ITEM_UPGRADE_WEAPON] "Upgrade Weapon",
  [MYSTERY_BOX_ITEM_INFINITE_AMMO] "Infinite Ammo",
  [MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK] "Invisibility Cloak",
  [MYSTERY_BOX_ITEM_ACTIVATE_POWER] "Turn on Power",
  [MYSTERY_BOX_ITEM_REVIVE_TOTEM] "Self Revive",
  [MYSTERY_BOX_ITEM_DREAD_TOKEN] "Dread Token",
  [MYSTERY_BOX_ITEM_WEAPON_MOD] "",
  [MYSTERY_BOX_ITEM_QUAD] "Quad",
  [MYSTERY_BOX_ITEM_SHIELD] "Shield",
  [MYSTERY_BOX_ITEM_EMP_HEALTH_GUN] "Health Tornado",
};

int ITEM_TEX_IDS[] = {
  [MYSTERY_BOX_ITEM_RESET_GATE] 14 - 3,
  [MYSTERY_BOX_ITEM_TEDDY_BEAR] 132 - 3,
  [MYSTERY_BOX_ITEM_RANDOMIZE_WEAPON_PICKUPS] 16 - 3,
  [MYSTERY_BOX_ITEM_UPGRADE_WEAPON] 37 - 3,
  [MYSTERY_BOX_ITEM_INFINITE_AMMO] 93 - 3,
  [MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK] 140 - 3,
  [MYSTERY_BOX_ITEM_ACTIVATE_POWER] 54 - 3,
  [MYSTERY_BOX_ITEM_REVIVE_TOTEM] 80 - 3,
  [MYSTERY_BOX_ITEM_DREAD_TOKEN] 35 - 3,
  [MYSTERY_BOX_ITEM_WEAPON_MOD] 46 - 3,
  [MYSTERY_BOX_ITEM_QUAD] 56 - 3,
  [MYSTERY_BOX_ITEM_SHIELD] 96 - 3,
  [MYSTERY_BOX_ITEM_EMP_HEALTH_GUN] 15 - 3,
};

u32 ITEM_COLORS[] = {
  [MYSTERY_BOX_ITEM_RESET_GATE] 0x8000FFFF,
  [MYSTERY_BOX_ITEM_TEDDY_BEAR] 0x80808080,
  [MYSTERY_BOX_ITEM_RANDOMIZE_WEAPON_PICKUPS] 0x80FFFFFF,
  [MYSTERY_BOX_ITEM_UPGRADE_WEAPON] 0x80FFFFFF,
  [MYSTERY_BOX_ITEM_INFINITE_AMMO] 0x8000FFFF,
  [MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK] 0x80FFFFFF,
  [MYSTERY_BOX_ITEM_ACTIVATE_POWER] 0x80FFFF00,
  [MYSTERY_BOX_ITEM_REVIVE_TOTEM] 0x80FFFFFF,
  [MYSTERY_BOX_ITEM_DREAD_TOKEN] 0x80FFFFFF,
  [MYSTERY_BOX_ITEM_WEAPON_MOD] 0x80FFFFFF,
  [MYSTERY_BOX_ITEM_QUAD] 0x80808080,
  [MYSTERY_BOX_ITEM_SHIELD] 0x80808080,
  [MYSTERY_BOX_ITEM_EMP_HEALTH_GUN] 0x80FFFFFF,
};

int ALPHA_MOD_TEX_IDS[] = {
  [ALPHA_MOD_SPEED] 55 - 3,
  [ALPHA_MOD_AMMO] 41 - 3,
  [ALPHA_MOD_AIMING] 40 - 3,
  [ALPHA_MOD_IMPACT] 47 - 3,
  [ALPHA_MOD_AREA] 42 - 3,
  [ALPHA_MOD_JACKPOT] 49 - 3,
  [ALPHA_MOD_XP] 44 - 3,
};

const char ENABLED_ALPHA_MODS[] = {
  ALPHA_MOD_SPEED,
  ALPHA_MOD_AMMO,
  ALPHA_MOD_IMPACT,
  ALPHA_MOD_AREA,
  ALPHA_MOD_JACKPOT,
  ALPHA_MOD_XP
};

const int ENABLED_ALPHA_MODS_COUNT = COUNT_OF(ENABLED_ALPHA_MODS);

const char * ALPHA_MODS[] = {
	"",
	"Speed Mod",
	"Ammo Mod",
	"Aiming Mod",
	"Impact Mod",
	"Area Mod",
	"Xp Mod",
	"Jackpot Mod",
	"Nanoleech Mod"
};

SoundDef RespawnSoundDef = {
  .MinRange = 0.0,
  .MaxRange = 2000.0,
  .MinVolume = 600,
  .MaxVolume = 600,
  .MinPitch = 0,
  .MaxPitch = 0,
  .Loop = 0,
  .Flags = 0x10,
  .Index = 54,
  .BankIndex = 3
};

extern struct MysteryBoxItemWeight MysteryBoxItemProbabilities[];
extern struct MysteryBoxItemWeight MysteryBoxItemProbabilitiesLucky[];
extern const int MysteryBoxItemProbabilitiesCount;
extern const int MysteryBoxItemProbabilitiesLuckyCount;

char MysteryBoxRespawnImmediately = 0;

//--------------------------------------------------------------------------
void mboxPlayOpenSound(Moby* moby)
{
  mobyPlaySoundByClass(0, 0, moby, MOBY_ID_NODE_BASE);
}

//--------------------------------------------------------------------------
void mboxPlayRespawnSound(Moby* moby)
{
  soundPlay(&RespawnSoundDef, 0, moby, 0, 0x400);
}

//--------------------------------------------------------------------------
int mboxGetCost(Moby* moby, int playerId)
{
  if (!moby || !moby->PVar)
    return 0;
    
  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  return pvars->BoltCostMultiplier * (MYSTERY_BOX_COST + (MYSTERY_BOX_COST_PER_VOX * pvars->NumVoxPerPlayer[playerId]));
}

//--------------------------------------------------------------------------
int mboxGetAlphaMod(Moby* moby)
{
  if (!moby || !moby->PVar)
    return ALPHA_MOD_EMPTY;

  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  
  return ENABLED_ALPHA_MODS[pvars->Random % ENABLED_ALPHA_MODS_COUNT];
}

//--------------------------------------------------------------------------
float mboxRand(void)
{
  return randRange(0, 1);
}

//--------------------------------------------------------------------------
void mboxRandomizeWeaponPickups(void)
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
#if LOG_STATS2
				DPRINTF("setting pickup at %08X to %d\n", (u32)moby, gadgetId);
#endif
				((void (*)(Moby*, int))0x0043A370)(moby, gadgetId);

				++pickupCount;
			}

			++moby;
		}
	}
}

//--------------------------------------------------------------------------
void mboxActivateQuad(void)
{
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->SkinMoby)
      continue;

    // increase duration by player pickup cooldown upgrade
    short duration = ITEM_QUAD_DURATION_TPS;
    if (MapConfig.State) {
      duration += MapConfig.State->PlayerStates[player->PlayerId].State.Upgrades[UPGRADE_PICKUPS] * TPS * 1.00 * 2;
    }

    player->timers.damageMuliplierTimer = duration;
    player->DamageMultiplier = 4;
  }
}

//--------------------------------------------------------------------------
void mboxActivateShield(void)
{
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->SkinMoby)
      continue;

    // increase duration by player pickup cooldown upgrade
    short duration = ITEM_SHIELD_DURATION_TPS;
    if (MapConfig.State) {
      duration += MapConfig.State->PlayerStates[player->PlayerId].State.Upgrades[UPGRADE_PICKUPS] * TPS * 1.25 * 2;
    }

    player->timers.armorLevelTimer = duration;
    player->ArmorLevel = 3;
  }
}

//--------------------------------------------------------------------------
void mboxActivate(Moby* moby, int activatedByPlayerId)
{
  int i;
  int item = MYSTERY_BOX_ITEM_WEAPON_MOD;

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, MYSTERY_BOX_EVENT_ACTIVATE);
  if (guberEvent) {

    // check if user has luck
    // pick random by weight
    if (playerHasBlessing(activatedByPlayerId, BLESSING_ITEM_LUCK)) {
      for (i = 0; i < (MysteryBoxItemProbabilitiesLuckyCount-1); ++i)
      {
        float r = mboxRand();
        if (r < MysteryBoxItemProbabilitiesLucky[i].Probability) {
          DPRINTF("lucky hit %d (%s) %f<%f\n", MysteryBoxItemProbabilitiesLucky[i].Item, ITEM_NAMES[MysteryBoxItemProbabilitiesLucky[i].Item], r, MysteryBoxItemProbabilitiesLucky[i].Probability);
          break;
        }
      }

      item = MysteryBoxItemProbabilitiesLucky[i].Item;
    } else {
      for (i = 0; i < (MysteryBoxItemProbabilitiesCount-1); ++i)
      {
        float r = mboxRand();
        if (r < MysteryBoxItemProbabilities[i].Probability) {
          DPRINTF("hit %d (%s) %f<%f\n", MysteryBoxItemProbabilities[i].Item, ITEM_NAMES[MysteryBoxItemProbabilities[i].Item], r, MysteryBoxItemProbabilitiesLucky[i].Probability);
          break;
        }
      }

      item = MysteryBoxItemProbabilities[i].Item;
    }

    int random = rand(100);

    guberEventWrite(guberEvent, &activatedByPlayerId, 4);
    guberEventWrite(guberEvent, &item, 4);
    guberEventWrite(guberEvent, &random, 4);
  }
}

//--------------------------------------------------------------------------
void mboxGetRandomRespawn(int random, VECTOR outPos, VECTOR outRot)
{
  int i;

  // find next spawn point
  if (MapConfig.BakedConfig) {
    int foundSpot = 1;
    int r = 1 + (random % BAKED_SPAWNPOINT_COUNT);
    while (foundSpot) {
      foundSpot = 0;
      for (i = 0; i < BAKED_SPAWNPOINT_COUNT; ++i) {
        if (MapConfig.BakedConfig->BakedSpawnPoints[i].Type == BAKED_SPAWNPOINT_MYSTERY_BOX) {
          --r;
          foundSpot = 1;
          if (!r) {
            memcpy(outPos, MapConfig.BakedConfig->BakedSpawnPoints[i].Position, 12);
            memcpy(outRot, MapConfig.BakedConfig->BakedSpawnPoints[i].Rotation, 12);
            foundSpot = 0;
            break;
          }
        }
      }
    }
  }
}

//--------------------------------------------------------------------------
void mboxSetRandomRespawn(Moby* moby, int random)
{
  if (!moby || !moby->PVar)
    return;

  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;

  // find next spawn point
  mboxGetRandomRespawn(random, pvars->SpawnpointPosition, pvars->SpawnpointRotation);

  if (MapConfig.State)
    pvars->RoundHidden = MapConfig.State->RoundNumber;

  mobySetState(moby, MYSTERY_BOX_STATE_HIDDEN, -1);
}

//--------------------------------------------------------------------------
void mboxGivePlayer(Moby* moby, int playerId, enum MysteryBoxItem item, int random)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, MYSTERY_BOX_EVENT_GIVE_PLAYER);
  if (guberEvent) {

    guberEventWrite(guberEvent, &playerId, 4);
    guberEventWrite(guberEvent, &item, 4);
    guberEventWrite(guberEvent, &random, 4);

    //DPRINTF("mbox give player %d item %d (%d)\n", playerId, item, random);
    
  }
}

//--------------------------------------------------------------------------
void mboxOpenDoor(Moby* moby, float openFactor)
{
  MATRIX m1;
  MATRIX* jointCache;
  if (!moby)
    return;

  jointCache = (MATRIX*)moby->JointCache;
  if (!jointCache)
    return;

  // door 1
  matrix_unit(m1);
  matrix_rotate_x(m1, m1, openFactor * 90 * MATH_DEG2RAD);
  matrix_multiply(jointCache[1], m1, jointCache[1]);

  // door 2
  matrix_unit(m1);
  matrix_rotate_x(m1, m1, openFactor * 90 * MATH_DEG2RAD);
  matrix_multiply(jointCache[3], m1, jointCache[3]);
}

//--------------------------------------------------------------------------
void mboxDraw(Moby* moby)
{
  if (!moby || !moby->PVar || moby->State == MYSTERY_BOX_STATE_IDLE)
    return;

  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  struct QuadDef quad;
	MATRIX m2, mRot;
	VECTOR t;
  VECTOR offset = {1,0,1.25,0};
	VECTOR pTL = {0.25,0,0.25,1};
	VECTOR pTR = {-0.25,0,0.25,1};
	VECTOR pBL = {0.25,0,-0.25,1};
	VECTOR pBR = {-0.25,0,-0.25,1};

  int item = pvars->CycleItem = pvars->Item;
  if (moby->State == MYSTERY_BOX_STATE_CYCLING_ITEMS) {
    if (MapConfig.State && playerHasBlessing(pvars->ActivatedByPlayerId, BLESSING_ITEM_LUCK)) {
      pvars->CycleItem = item = MysteryBoxItemProbabilitiesLucky[rand(MysteryBoxItemProbabilitiesLuckyCount)].Item;
    } else {
      pvars->CycleItem = item = MysteryBoxItemProbabilities[rand(MysteryBoxItemProbabilitiesCount)].Item;
    }
  }

  u32 color = ITEM_COLORS[item];
  int texId = ITEM_TEX_IDS[item];
  if (moby->State != MYSTERY_BOX_STATE_CYCLING_ITEMS && pvars->Item == MYSTERY_BOX_ITEM_WEAPON_MOD) {
    texId = ALPHA_MOD_TEX_IDS[mboxGetAlphaMod(moby)];
  }

  // save
  pvars->ItemTexId = texId;

  // determine how far out to draw the sprite
  float openFactor = clamp((gameGetTime() - pvars->ActivatedTime) / (1.0 * TIME_SECOND), 0, 1);
  if (moby->State == MYSTERY_BOX_STATE_CLOSING) {
    openFactor = 1 - clamp((gameGetTime() - pvars->StateChangedAtTime) / (0.1 * TIME_SECOND), 0, 1);
  }

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
  vector_scale(offset, offset, openFactor);
	vector_add(&m2[12], moby->Position, offset);

	// draw
	gfxDrawQuad((void*)0x00222590, &quad, m2, 0);
}

//--------------------------------------------------------------------------
void mboxUpdate(Moby* moby)
{
  Player** players = playerGetAll();
  int i;
  char buf[48];
  if (!moby || !moby->PVar)
    return;
    
  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  Player* activatedByPlayer = players[pvars->ActivatedByPlayerId];
  int timeSinceActivated = gameGetTime() - pvars->ActivatedTime;
  int timeSinceStateChanged = gameGetTime() - pvars->StateChangedAtTime;
  int ticksSinceLastStateChange = pvars->TicksSinceLastStateChanged++;

  if (pvars->ActivatedByPlayerId < 0)
    activatedByPlayer = NULL;

	// post draw
  if (moby->State != MYSTERY_BOX_STATE_HIDDEN) {
    gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&mboxDraw, moby);

    int blipIdx = radarGetBlipIndex(moby);
    if (blipIdx >= 0) {
      RadarBlip * blip = radarGetBlips() + blipIdx;
      blip->X = moby->Position[0];
      blip->Y = moby->Position[1];
      blip->Life = 0x1F;
      blip->Type = 4;
      blip->Team = TEAM_YELLOW;
    }
  }

  // handle state
  switch (moby->State)
  {
    case MYSTERY_BOX_STATE_CLOSING:
    {
      float t = 1 - clamp(powf(timeSinceStateChanged / (0.1 * TIME_SECOND), 2), 0, 1);
      mboxOpenDoor(moby, t);

      // transition to next state after
      if (t <= 0) {
        mobySetState(moby, MYSTERY_BOX_STATE_IDLE, -1);
        pvars->StateChangedAtTime = gameGetTime();
        pvars->TicksSinceLastStateChanged = 0;
      }
      break;
    }
    case MYSTERY_BOX_STATE_BEFORE_CLOSING:
    {
      mboxOpenDoor(moby, 1);

      // handle items that are forced onto player
      switch (pvars->Item)
      {
        case MYSTERY_BOX_ITEM_RESET_GATE:
        case MYSTERY_BOX_ITEM_TEDDY_BEAR:
        case MYSTERY_BOX_ITEM_RANDOMIZE_WEAPON_PICKUPS:
        {
          if ((activatedByPlayer && activatedByPlayer->IsLocal) || (gameAmIHost() && !activatedByPlayer)) {
            mboxGivePlayer(moby, pvars->ActivatedByPlayerId, pvars->Item, pvars->Random);
          }
          break;
        }
        default: break;
      }

      // transition to next state after
      mobySetState(moby, MYSTERY_BOX_STATE_CLOSING, -1);
      pvars->StateChangedAtTime = gameGetTime();
      pvars->TicksSinceLastStateChanged = 0;
      break;
    }
    case MYSTERY_BOX_STATE_DISPLAYING_ITEM:
    {
      mboxOpenDoor(moby, 1);

      // play vox dialog on appear
      if (!ticksSinceLastStateChange && pvars->Item == MYSTERY_BOX_ITEM_TEDDY_BEAR) {
        playDialog(DIALOG_ID_VOX_JACKPOT, 1);
      }

      // let player who activated interact with item
      // if the item is interactable
      if (pvars->ActivatedByPlayerId >= 0) {
        int showInteract = 0;
        int random = pvars->Random;

        switch (pvars->Item)
        {
          case MYSTERY_BOX_ITEM_UPGRADE_WEAPON:
          {
            snprintf(buf, sizeof(buf), "\x11 %s", ITEM_NAMES[pvars->Item]);
            random = players[pvars->ActivatedByPlayerId]->WeaponHeldId;
            showInteract = 1;
            break;
          }
          //case MYSTERY_BOX_ITEM_SUPER_JUMP:
          case MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK:
          case MYSTERY_BOX_ITEM_REVIVE_TOTEM:
          case MYSTERY_BOX_ITEM_ACTIVATE_POWER:
          case MYSTERY_BOX_ITEM_INFINITE_AMMO:
          case MYSTERY_BOX_ITEM_QUAD:
          case MYSTERY_BOX_ITEM_SHIELD:
          case MYSTERY_BOX_ITEM_EMP_HEALTH_GUN:
          case MYSTERY_BOX_ITEM_DREAD_TOKEN:
          {
            snprintf(buf, sizeof(buf), "\x11 %s", ITEM_NAMES[pvars->Item]);
            showInteract = 1;
            break;
          }
          case MYSTERY_BOX_ITEM_WEAPON_MOD:
          {
            random = mboxGetAlphaMod(moby);
            snprintf(buf, sizeof(buf), "\x11 %s", ALPHA_MODS[random]);
            showInteract = 1;
            break;
          }
          default:
          {
            break;
          }
        }

        if (showInteract && tryPlayerInteract(moby, players[pvars->ActivatedByPlayerId], buf, NULL, 0, 0, PLAYER_MYSTERY_BOX_COOLDOWN_TICKS, 9, PAD_CIRCLE)) {
          mboxGivePlayer(moby, pvars->ActivatedByPlayerId, pvars->Item, random);
        }
      }

      // transition to next state after
#if DEBUGMBOX1
      mobySetState(moby, MYSTERY_BOX_STATE_BEFORE_CLOSING, -1);
      pvars->StateChangedAtTime = gameGetTime();
      pvars->TicksSinceLastStateChanged = 0;
#else
      if (timeSinceStateChanged > (TIME_SECOND * 5)) {
        mobySetState(moby, MYSTERY_BOX_STATE_BEFORE_CLOSING, -1);
        pvars->StateChangedAtTime = gameGetTime();
        pvars->TicksSinceLastStateChanged = 0;
      }
#endif
      break;
    }
    case MYSTERY_BOX_STATE_CYCLING_ITEMS:
    {
      mboxOpenDoor(moby, 1);

      // transition to next state after
#if DEBUGMBOX
      mobySetState(moby, MYSTERY_BOX_STATE_DISPLAYING_ITEM, -1);
      pvars->StateChangedAtTime = gameGetTime();
      pvars->TicksSinceLastStateChanged = 0;
#else
      if (timeSinceStateChanged > MYSTERY_BOX_CYCLE_ITEMS_DURATION) {
        mobySetState(moby, MYSTERY_BOX_STATE_DISPLAYING_ITEM, -1);
        pvars->StateChangedAtTime = gameGetTime();
        pvars->TicksSinceLastStateChanged = 0;
      }
#endif
      break;
    }
    case MYSTERY_BOX_STATE_OPENING:
    {
      float t = 1 - clamp(powf(1 - (timeSinceActivated / (0.1 * TIME_SECOND)), 3), 0, 1);
      mboxOpenDoor(moby, t);

      // transition to next state after
      if (t >= 1) {
        mobySetState(moby, MYSTERY_BOX_STATE_CYCLING_ITEMS, -1);
        pvars->StateChangedAtTime = gameGetTime();
        pvars->TicksSinceLastStateChanged = 0;
      }
      break;
    }
    case MYSTERY_BOX_STATE_IDLE:
    {
      moby->DrawDist = 64;
      moby->CollActive = 0;

      // find local players to activate
      for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
        int cost = mboxGetCost(moby, i);
        snprintf(buf, sizeof(buf), "\x11 Open [\x0E%'d\x08]", cost);

        if (tryPlayerInteract(moby, players[i], buf, NULL, cost, 0, PLAYER_MYSTERY_BOX_COOLDOWN_TICKS, 9, PAD_CIRCLE)) {
          mboxActivate(moby, i);
          break;
        }
      }
      break;
    }
    case MYSTERY_BOX_STATE_HIDDEN:
    {
      moby->DrawDist = 0;
      moby->CollActive = -1;

      if (MapConfig.State && (MysteryBoxRespawnImmediately || MapConfig.State->RoundNumber != pvars->RoundHidden)) {
        vector_copy(moby->Position, pvars->SpawnpointPosition);
        vector_copy(moby->Rotation, pvars->SpawnpointRotation);
        mobySetState(moby, MYSTERY_BOX_STATE_IDLE, -1);
        mboxPlayRespawnSound(moby);
      }
      break;
    }
  }
}

//--------------------------------------------------------------------------
int mboxHandleEvent_Spawned(Moby* moby, GuberEvent* event)
{
  DPRINTF("mbox spawned: %08X\n", (u32)moby);
  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
	// read event
	guberEventRead(event, moby->Position, 12);
	guberEventRead(event, moby->Rotation, 12);

	// set update
	moby->PUpdate = &mboxUpdate;

	// indicate to survival mode that we can damage players
  moby->Bolts = -1;

  // init pvars
  pvars->BoltCostMultiplier = 1;

  // update mode reference
  if (MapConfig.State) MapConfig.State->MysteryBoxMoby = moby;

  // set default state
	mobySetState(moby, MYSTERY_BOX_STATE_IDLE, -1);
  return 0;
}

//--------------------------------------------------------------------------
int mboxHandleEvent_Activate(Moby* moby, GuberEvent* event)
{
  int activatedByPlayerId, item, random;
  
  //DPRINTF("mbox activate: %08X\n", (u32)moby);
  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  guberEventRead(event, &activatedByPlayerId, 4);
  guberEventRead(event, &item, 4);
  guberEventRead(event, &random, 4);

#if DEBUGMBOX
  MysteryBoxTotalRolls += 1;
  MysteryBoxItemCounts[item] += 1;
  printf("Total Rolls: %d\n", MysteryBoxTotalRolls);
  int i;
  for (i = 0; i < MysteryBoxItemProbabilitiesLuckyCount; ++i) {
    int it = MysteryBoxItemProbabilitiesLucky[i].Item;
    char* str = ITEM_NAMES[it];
    float p = MysteryBoxItemCounts[it] / (float)MysteryBoxTotalRolls;
    if (strlen(str) > 0) {
      printf("%s: %d (%f of %f)\n", str, MysteryBoxItemCounts[it], p, MysteryBoxItemProbabilitiesLucky[i].Probability);
    } else {
      printf("%d: %d (%f of %f)\n", it, MysteryBoxItemCounts[it], p, MysteryBoxItemProbabilitiesLucky[i].Probability);
    }
  }

  printf("\n\n");
#endif
  
  // increment stat
  if (activatedByPlayerId >= 0 && MapConfig.State) {
    MapConfig.State->PlayerStates[activatedByPlayerId].State.TimesRolledMysteryBox += 1;
  }

  // 
  if (moby->State == MYSTERY_BOX_STATE_IDLE) {
    
    pvars->Item = item;
    pvars->Random = random;
    pvars->ActivatedByPlayerId = activatedByPlayerId;
    pvars->ActivatedTime = gameGetTime();
    pvars->StateChangedAtTime = gameGetTime();

    // charge player
    if (MapConfig.State) {
      int cost = mboxGetCost(moby, activatedByPlayerId);
      MapConfig.State->PlayerStates[activatedByPlayerId].State.Bolts -= cost;
    }

    mobySetState(moby, MYSTERY_BOX_STATE_OPENING, -1);
    mboxPlayOpenSound(moby);
  }

  return 0;
}

//--------------------------------------------------------------------------
int mboxHandleEvent_GivePlayer(Moby* moby, GuberEvent* event)
{
  int playerId, item, random, i;
  Player** players = playerGetAll();
  
  //DPRINTF("mbox give player: %08X\n", (u32)moby);
  struct MysteryBoxPVar* pvars = (struct MysteryBoxPVar*)moby->PVar;
  if (!pvars)
    return 0;
  
  // read
  guberEventRead(event, &playerId, 4);
  guberEventRead(event, &item, 4);
  guberEventRead(event, &random, 4);

  // set set to closing
  if (moby->State == MYSTERY_BOX_STATE_DISPLAYING_ITEM) {
    mobySetState(moby, MYSTERY_BOX_STATE_CLOSING, -1);
    pvars->StateChangedAtTime = gameGetTime();
  }

  if (playerId >= 0) {
    struct SurvivalPlayer* playerData = NULL;
    Player* player = players[playerId];
    
    if (MapConfig.State)
      playerData = &MapConfig.State->PlayerStates[playerId];

    if (player && player->PlayerMoby) {

      switch (item)
      {
        case MYSTERY_BOX_ITEM_RESET_GATE:
        {
          pushSnack(-1, "Gate reset!", 60);

#if GATE
          if (gameAmIHost()) {
            gateResetRandomGate();
          }
#endif
          break;
        }
        case MYSTERY_BOX_ITEM_TEDDY_BEAR:
        {
          if (playerId >= 0) pvars->NumVoxPerPlayer[playerId]++;
          DPRINTF("mbox voxed %d %d times\n", playerId, pvars->NumVoxPerPlayer[playerId]);

          spawnExplosion(moby->Position, 5, 0x802060C0);
          damageRadius(moby, moby->Position, 0x00081801, 5, 5);
#if !DEBUGMBOX
          mboxSetRandomRespawn(moby, pvars->Random);
#endif
          break;
        }
        case MYSTERY_BOX_ITEM_RANDOMIZE_WEAPON_PICKUPS:
        {
          if (gameAmIHost()) {
            mboxRandomizeWeaponPickups();
          }
          pushSnack(-1, "Weapon Pickups Randomized!", TPS);
          break;
        }
        case MYSTERY_BOX_ITEM_ACTIVATE_POWER:
        {
#if POWER
          powerOnMysteryBoxActivatePower();
#endif
          break;
        }
        case MYSTERY_BOX_ITEM_UPGRADE_WEAPON:
        {
          if (MapConfig.UpgradePlayerWeaponFunc) {
            MapConfig.UpgradePlayerWeaponFunc(playerId, random, 0);
          }
          break;
        }
        case MYSTERY_BOX_ITEM_INFINITE_AMMO:
        {
          if (MapConfig.State) {
            MapConfig.State->InfiniteAmmoStopTime = gameGetTime() + ITEM_INFAMMO_DURATION;
            pushSnack(-1, "Infinite Ammo!", TPS);
          }
          break;
        }
        case MYSTERY_BOX_ITEM_QUAD:
        {
          if (MapConfig.State) {
            mboxActivateQuad();
            pushSnack(-1, "Quad!", TPS);
          }
          break;
        }
        case MYSTERY_BOX_ITEM_SHIELD:
        {
          if (MapConfig.State) {
            mboxActivateShield();
            pushSnack(-1, "Shield!", TPS);
          }
          break;
        }
        case MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK:
        case MYSTERY_BOX_ITEM_EMP_HEALTH_GUN:
        case MYSTERY_BOX_ITEM_REVIVE_TOTEM:
        {
          if (playerData)
            playerData->State.Item = item;
          break;
        }
        case MYSTERY_BOX_ITEM_DREAD_TOKEN:
        {
          if (playerData) {
            playerData->State.TotalTokens += 1;
            playerData->State.CurrentTokens += 1;
          }
          break;
        }
        case MYSTERY_BOX_ITEM_WEAPON_MOD:
        {
          if (playerData)
	          playerData->State.AlphaMods[random]++;
          if (player->GadgetBox)
		        player->GadgetBox->ModBasic[random-1]++;
          break;
        }
      }
    }
  }

  return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby* mboxGetGuber(Moby* moby)
{
	if (moby->OClass == MYSTERY_BOX_OCLASS && moby->PVar)
		return moby->GuberMoby;
	
	return 0;
}

//--------------------------------------------------------------------------
int mboxHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == MYSTERY_BOX_OCLASS && moby->PVar) {
		u32 mboxEvent = event->NetEvent.EventID;

		switch (mboxEvent)
		{
			case MYSTERY_BOX_EVENT_SPAWN: return mboxHandleEvent_Spawned(moby, event);
			case MYSTERY_BOX_EVENT_ACTIVATE: return mboxHandleEvent_Activate(moby, event);
			case MYSTERY_BOX_EVENT_GIVE_PLAYER: return mboxHandleEvent_GivePlayer(moby, event);
			default:
			{
				DPRINTF("unhandle mbox event %d\n", mboxEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
int mboxCreate(VECTOR position, VECTOR rotation)
{
	// create guber object
	GuberEvent * guberEvent = 0;
	guberMobyCreateSpawned(MYSTERY_BOX_OCLASS, sizeof(struct MysteryBoxPVar), &guberEvent, NULL);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, position, 12);
		guberEventWrite(guberEvent, rotation, 12);
	}
	else
	{
		DPRINTF("failed to guberevent mbox\n");
	}
  
  return guberEvent != NULL;
}

//--------------------------------------------------------------------------
void mboxSpawn(void)
{
  static int spawned = 0;
  
  if (spawned)
    return;

  // spawn
  if (gameAmIHost()) {

    VECTOR p = {184.1991,439.6302,85.858,0};
    VECTOR r = {0,0,0,0};
    mboxGetRandomRespawn(rand(10), p, r);

    mboxCreate(p, r);
  }

  spawned = 1;
}

//--------------------------------------------------------------------------
void mboxInit(void)
{
  Moby* temp = mobySpawn(MYSTERY_BOX_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  u32 mobyFunctionsPtr = (u32)mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("MBOX oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);

#if DEBUGMBOX
  memset(MysteryBoxItemCounts, 0, sizeof(MysteryBoxItemCounts));
#endif
}
