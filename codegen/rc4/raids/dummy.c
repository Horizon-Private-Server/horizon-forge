/***************************************************
 * FILENAME :		dummy.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the dummys.
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
#include <libdl/hud.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/collision.h>
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "controller.h"
#include "dummy.h"
#include "mob.h"
#include "pathfind.h"
#include "game.h"

#if DEBUG
#define DLOG(moby, format, ...) if (((struct DummyPVar*)moby->PVar)->Config.Log) { DPRINTF(format, ##__VA_ARGS__); }
#else
#define DLOG(moby, format, ...) 
#endif

//--------------------------------------------------------------------------
struct DummyDifficultyConfig* dummyGetDifficultyConfig(Moby* moby)
{
  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  int stars = MapConfig.State ? MapConfig.State->DifficultyStars : 0;

  return &pvars->Config.DifficultyConfigs[stars];
}

//--------------------------------------------------------------------------
void dummyBroadcastState(Moby* moby, enum DummyState state, float health)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, DUMMY_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
    guberEventWrite(guberEvent, &health, 4);
  }

  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  pvars->TargetVars.hitPoints = health;
  mobySetState(moby, state, -1);
}

//--------------------------------------------------------------------------
void dummyBroadcastHealth(Moby* moby, float newHealth)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, DUMMY_EVENT_SET_HEALTH);
  if (guberEvent) {
    guberEventWrite(guberEvent, &newHealth, 4);
  }

  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  pvars->TargetVars.hitPoints = newHealth;
}

//--------------------------------------------------------------------------
void dummyOnStateChanged(Moby* moby)
{
  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  struct DummyDifficultyConfig* difficultyConfig = dummyGetDifficultyConfig(moby);
  Moby* targetMoby = pvars->Config.TargetMoby;

  switch (moby->State)
  {
    case DUMMY_STATE_DEAD:
    {
      // handle death event
      if (pvars->Config.OnDeathType != DUMMY_ON_DEATH_NONE) {
        
        // 
        if (targetMoby && !mobyIsDestroyed(targetMoby)) {

          // explode
          if (pvars->Config.OnDeathType >= DUMMY_ON_DEATH_EXPLODE) {
            spawnExplosionDamage(targetMoby->Position, difficultyConfig->ExplosionRadius, 0x80004080, moby, difficultyConfig->ExplosionDamage, 0x00081801);
          }

          // blow corn
          if (targetMoby->PClass && pvars->Config.OnDeathType == DUMMY_ON_DEATH_EXPLODE_BLOW_CORN) {
            blowCorn(targetMoby);
          }

          guberMobyDestroy(targetMoby);
        }

        // destroy
        guberMobyDestroy(moby);
      }

      // reset on deactivated
      pvars->TargetVars.hitPoints = difficultyConfig->Health;
      break;
    }
    case DUMMY_STATE_DEACTIVATED:
    case DUMMY_STATE_RESET:
    {
      // reset on deactivated
      pvars->TargetVars.hitPoints = difficultyConfig->Health;
      break;
    }
    case DUMMY_STATE_ACTIVATED:
    {
      break;
    }
  }

}

//--------------------------------------------------------------------------
void dummyDrawHealthbar(Moby* moby)
{
  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  struct DummyDifficultyConfig* difficultyConfig = dummyGetDifficultyConfig(moby);
  float scale = pvars->Config.HealthbarScale;
  float hp = clamp(pvars->TargetVars.hitPoints / difficultyConfig->Health, 0, 1);
  u32 bgColor = 0x80000000;
  u32 frColor = 0x80101010;
  u32 fgColor = hudGetTeamColor(pvars->Config.IsOnEnemyTeam ? TEAM_RED : TEAM_BLUE, 1);

  gfxResetGsRegisters();

  VECTOR pos = {0,0,1,0};
  vector_scale(pos, pos, pvars->Config.HealthbarOffset);
  vector_add(pos, pos, moby->Position);
  gfxHelperDrawBox_WS(pos, 50 * scale + 2, 2 + 5 * scale, frColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox_WS(pos, 50 * scale, 5 * scale, bgColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox_WS(pos, 50 * scale * hp, 5 * scale, fgColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void dummyUpdate(Moby* moby)
{
  int i;
  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  struct DummyDifficultyConfig* difficultyConfig = dummyGetDifficultyConfig(moby);

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    dummyOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  // state changes, host only
  if (gameAmIHost()) {

    // check if completed
    if (moby->State == DUMMY_STATE_ACTIVATED && (pvars->TargetVars.hitPoints <= 0 || !pvars->Config.TargetMoby || mobyIsDestroyed(pvars->Config.TargetMoby))) {
      DLOG(moby, "dummy %08X dead\n", (u32)moby);
      dummyBroadcastState(moby, DUMMY_STATE_DEAD, 0);
    }

    if (moby->State == DUMMY_STATE_RESET) {
      if (gameAmIHost()) dummyBroadcastState(moby, DUMMY_STATE_ACTIVATED, difficultyConfig->Health);
      return;
    }
  }

  // if not active, disable
  if (moby->State != DUMMY_STATE_ACTIVATED) {
    moby->ModeBits &= ~(MOBY_MODE_BIT_CAN_BE_AUTO_TARGETED | MOBY_MODE_BIT_CAN_BE_DAMAGED);
    moby->CollActive = -1;
    return;
  }
  
  if (!missionIsActive()) return;

  // copy target moby
  Moby* targetMoby = pvars->Config.TargetMoby;
  if (pvars->Config.IsOnEnemyTeam) moby->ModeBits |= MOBY_MODE_BIT_CAN_BE_AUTO_TARGETED;
  moby->ModeBits |= MOBY_MODE_BIT_CAN_BE_DAMAGED;
  moby->ModeBits &= ~MOBY_MODE_BIT_NO_POST_UPDATE;
  vector_copy(moby->Position, targetMoby->Position);
  vector_copy(moby->LSphere, targetMoby->LSphere);
  vector_copy(moby->BSphere, targetMoby->BSphere);
  memcpy(moby->M0_03, targetMoby->M0_03, 0x40);
  moby->Scale = targetMoby->Scale;
  moby->CollData = targetMoby->CollData;
  moby->CollActive = 1;
  moby->PClass = targetMoby->PClass;
  moby->MClass = targetMoby->MClass;
  moby->AnimSeq = targetMoby->AnimSeq;
  moby->AnimSeqId = moby->LSeq = targetMoby->AnimSeqId;
  targetMoby->CollActive = -1;

  // register target if friendly and targetable
  if (!pvars->Config.IsOnEnemyTeam && pvars->Config.MobTargetType != DUMMY_MOB_AGGRO_IGNORE) {
    mobRegisterTarget(moby);
  }

  // healthbar
  if (pvars->Config.Healthbar) {
    gfxRegisterDrawFunction((void**)0x0022251C, &dummyDrawHealthbar, moby);
  }

  // handle damage
  if (gameAmIHost()) {
    int damageIndex = moby->CollDamage;
    u32 damageFlags = 0;
    MobyColDamage* colDamage = NULL;
    float damage = 0.0;
    if (damageIndex >= 0) {
      colDamage = mobyGetDamage(moby, 0x80481C41, 0);
      if (colDamage) {
        damage = colDamage->DamageHp;
        damageFlags = colDamage->DamageFlags;
      }
    }
    ((void (*)(Moby*, float*, MobyColDamage*))0x005184d0)(moby, &damage, colDamage);

    if (colDamage && colDamage->Damager && damage > 0) {
      DLOG(moby, "dummy %08X hit for %f by %08X\n", (u32)moby, damage, (u32)colDamage->Damager);
      
      float newHealth = maxf(0, pvars->TargetVars.hitPoints - damage);
      if (newHealth == 0) {
        DLOG(moby, "dummy %08X dead\n", (u32)moby);
        dummyBroadcastState(moby, DUMMY_STATE_DEAD, 0);
        if (pvars->Config.OnKilledControllerMoby && !mobyIsDestroyed(pvars->Config.OnKilledControllerMoby)) {
          controllerSetTriggerMoby(pvars->Config.OnKilledControllerMoby, colDamage->Damager);
          controllerBroadcastNewState(pvars->Config.OnKilledControllerMoby, CONTROLLER_STATE_ACTIVATED);
        }
      } else {
        dummyBroadcastHealth(moby, newHealth);
      }

      // hit
      if (pvars->Config.OnHitControllerMoby && !mobyIsDestroyed(pvars->Config.OnHitControllerMoby)) {
        controllerSetTriggerMoby(pvars->Config.OnHitControllerMoby, colDamage->Damager);
        controllerBroadcastNewState(pvars->Config.OnHitControllerMoby, CONTROLLER_STATE_ACTIVATED);
      }
    }
  }

  moby->CollDamage = -1;
}

//--------------------------------------------------------------------------
void dummyOnGuberCreated(Moby* moby)
{
  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  struct DummyDifficultyConfig* difficultyConfig = dummyGetDifficultyConfig(moby);

  moby->PUpdate = &dummyUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE | MOBY_MODE_BIT_HAS_SPECIAL_VARS;
  moby->Bolts = -1; // can damage players

  // update moby target ref
  pvars->Config.TargetMoby = mobyGetFromIdxOrNull((int)pvars->Config.TargetMoby);
  pvars->Config.OnHitControllerMoby = mobyGetFromIdxOrNull((int)pvars->Config.OnHitControllerMoby);
  pvars->Config.OnKilledControllerMoby = mobyGetFromIdxOrNull((int)pvars->Config.OnKilledControllerMoby);

  pvars->TargetVarsPtr = &pvars->TargetVars;
  pvars->FlashVarsPtr = &pvars->FlashVars;
  pvars->TargetVars.hitPoints = difficultyConfig->Health;
  pvars->TargetVars.targetHeight = pvars->Config.TargetHeight;
  pvars->TargetVars.targetRadiusIn8ths = (u8)(pvars->Config.TargetRadius * 8);
  pvars->TargetVars.team = pvars->Config.IsOnEnemyTeam ? TEAM_WHITE : TEAM_BLUE;

  struct Guber* guber = guberGetObjectByMoby(moby);
  ((GuberMoby*)guber)->TeamNum = pvars->TargetVars.team;

  mobySetState(moby, pvars->Config.DefaultState, -1);

  DLOG(moby, "DUMMY %08X found target %08X\n", (u32)moby, (u32)pvars->Config.TargetMoby);
}

//--------------------------------------------------------------------------
int dummyHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  float health;
  if (!moby || !moby->PVar)
    return 0;

  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &state, 4);
	guberEventRead(event, &health, 4);
  pvars->TargetVars.hitPoints = health;
  mobySetState(moby, state, -1);
  return 0;
}

//--------------------------------------------------------------------------
int dummyHandleEvent_SetHealth(Moby* moby, GuberEvent* event)
{
  float health;
  if (!moby || !moby->PVar)
    return 0;

  struct DummyPVar* pvars = (struct DummyPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &health, 4);

  float dh = health - pvars->TargetVars.hitPoints;
  pvars->TargetVars.hitPoints = health;
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* dummyGetGuber(Moby* moby)
{
	if (moby->OClass == DUMMY_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int dummyHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == DUMMY_OCLASS && moby->PVar) {
		u32 eventId = event->NetEvent.EventID;

		switch (eventId)
		{
      case DUMMY_EVENT_SET_STATE: { return dummyHandleEvent_SetState(moby, event); }
      case DUMMY_EVENT_SET_HEALTH: { return dummyHandleEvent_SetHealth(moby, event); }
			default:
			{
				DLOG(moby, "unhandle dummy event %d\n", eventId);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void dummyStart(void)
{

}

//--------------------------------------------------------------------------
void dummyInit(void)
{
  Moby* temp = mobySpawn(DUMMY_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("DUMMY oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for dummys
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, DUMMY_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found dummy %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        dummyOnGuberCreated(moby);
      }
    }

		++moby;
	}

  DPRINTF("dummy pvar size %d\n", sizeof(struct DummyPVar));
}
