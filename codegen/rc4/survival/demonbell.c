#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/color.h>
#include <libdl/collision.h>
#include <libdl/moby.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include "demonbell.h"
#include "maputils.h"
#include "utils.h"
#include "mobs/mob.h"

GuberEvent* demonbellCreateEvent(Moby* moby, u32 eventType);
int demonbellCount = 0;

//--------------------------------------------------------------------------
void demonbellPlayActivateSound(Moby* moby)
{
  mobyPlaySoundByClass(0, 0, moby, 0x2751);
}	

//--------------------------------------------------------------------------
void demonbellOnRoundChanged(int roundNo)
{
  if (!MapConfig.State) return;

  // force demonbell on every 10 rounds
  int forcedOnCount = (roundNo+1) / 10;
  if (forcedOnCount > MapConfig.State->DemonBellCount)
    forcedOnCount = MapConfig.State->DemonBellCount;

  // force on
  Moby* m = mobyListGetStart();
  Moby* mEnd = mobyListGetEnd();
  while (m < mEnd)
  {
    m = mobyFindNextByOClass(m, DEMONBELL_MOBY_OCLASS);
    if (!m)
      break;

    struct DemonBellPVar* pvars = (struct DemonBellPVar*)m->PVar;
    if (pvars && (pvars->Id < forcedOnCount || pvars->ForcedOn)) {
      pvars->ForcedOn = 1;
      MapConfig.State->RoundDemonBellCount += 1;
    }

    ++m;
  }
}

//--------------------------------------------------------------------------
void demonbellUpdate(Moby* moby)
{
	struct DemonBellPVar* pvars = (struct DemonBellPVar*)moby->PVar;
	if (!pvars) return;
  if (!MapConfig.State) return;

  // force on
  if (pvars->ForcedOn) {
    pvars->HitAmount = 1;
    mobySetState(moby, 1, -1);
  }

  switch (moby->State)
  {
    case 1: // activated
    {
      // check if new round and deactivate
      if (!pvars->ForcedOn && MapConfig.State->RoundNumber != pvars->RoundActivated && gameAmIHost()) {
        demonbellCreateEvent(moby, DEMONBELL_EVENT_DEACTIVATE);
        return;
      }

      // rotate
      moby->Rotation[2] = clampAngle(moby->Rotation[2] - pvars->HitAmount*7*MATH_DT*MATH_PI);

      // pulse
      float t = (sinf(DEMONBELL_PULSE_SPEED * gameGetTime() / 1000.0)+1) / 2.0;
      moby->GlowRGBA = colorLerp(DEMONBELL_PULSE_COLOR_1, DEMONBELL_PULSE_COLOR_2, t);

      // ignore any hits
      moby->CollDamage = -1;
      break;
    }
    case 0: // awaiting activation
    {
      // hit
      int damageIndex = moby->CollDamage;
      if (damageIndex >= 0) {
        MobyColDamage* colDamage = mobyGetDamage(moby, 0x00481C40, 0);
        if (colDamage && pvars->HitAmount < 1) {
          pvars->HitAmount = clamp(pvars->HitAmount + colDamage->DamageHp * DEMONBELL_DAMAGE_SCALE, 0, 1);
          pvars->RecoverCooldownTicks = DEMONBELL_HIT_COOLDOWN_TICKS;

          // activate
          if (pvars->HitAmount >= 1 && gameAmIHost()) {
            GuberEvent * guberEvent = demonbellCreateEvent(moby, DEMONBELL_EVENT_ACTIVATE);
            Player* sourcePlayer = guberMobyGetPlayerDamager(colDamage->Damager);
            int activatedByPlayerId = -1;
            if (sourcePlayer) {
              activatedByPlayerId = sourcePlayer->PlayerId;
            }

            if (guberEvent) {
              guberEventWrite(guberEvent, &activatedByPlayerId, 4);
            }
          }
        }

        moby->CollDamage = -1;
      }

      // cooldown
      if (pvars->RecoverCooldownTicks) {
        --pvars->RecoverCooldownTicks;
      } else if (pvars->HitAmount > 0) {
        pvars->HitAmount = clamp(pvars->HitAmount - 1*MATH_DT, 0, 1);
      }

      // rotate
      moby->Rotation[2] = clampAngle(moby->Rotation[2] - pvars->HitAmount*3*MATH_DT*MATH_PI);

      // default color
      moby->GlowRGBA = DEMONBELL_DEFAULT_COLOR;
      break;
    }
  }
}

//--------------------------------------------------------------------------
GuberEvent* demonbellCreateEvent(Moby* moby, u32 eventType)
{
	GuberEvent * event = NULL;

	// create guber object
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		event = guberEventCreateEvent(guber, eventType, 0, 0);

	return event;
}

//--------------------------------------------------------------------------
int demonbellHandleEvent_Activate(Moby* moby, GuberEvent* event)
{
  int activatedByPlayerId = -1;
	struct DemonBellPVar* pvars = (struct DemonBellPVar*)moby->PVar;
	if (!pvars)
		return 0;

  guberEventRead(event, &activatedByPlayerId, 4);

  // increment demon bell stat
  if (activatedByPlayerId >= 0 && MapConfig.State) {
    MapConfig.State->PlayerStates[activatedByPlayerId].State.TimesActivatedDemonBell += 1;
  }

  // once activated, stay activated
  pvars->ForcedOn = 1;

  pvars->HitAmount = 1;
  pvars->RoundActivated = 0;
  if (MapConfig.State) {
    pvars->RoundActivated = MapConfig.State->RoundNumber;
    MapConfig.State->RoundDemonBellCount += 1;
  }

  mobySetState(moby, 1, -1);
  demonbellPlayActivateSound(moby);
  pushSnack(0, "Spawn Rate Increased!", 120);
  DPRINTF("demonbell activated at %08X by %d\n", (u32)moby, activatedByPlayerId);
	return 0;
}

//--------------------------------------------------------------------------
int demonbellHandleEvent_Deactivate(Moby* moby, GuberEvent* event)
{
	struct DemonBellPVar* pvars = (struct DemonBellPVar*)moby->PVar;
	if (!pvars)
		return 0;
    
  mobySetState(moby, 0, -1);
  pvars->HitAmount = 0;
  pvars->RecoverCooldownTicks = 0;
	DPRINTF("demonbell deactivated at %08X\n", (u32)moby);
	return 0;
}

//--------------------------------------------------------------------------
struct Guber* demonbellGetGuber(Moby* moby)
{
	if (moby->OClass == DEMONBELL_MOBY_OCLASS)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int demonbellHandleEvent(Moby* moby, GuberEvent* event)
{
	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == DEMONBELL_MOBY_OCLASS && moby->PVar) {
		u32 demonbellEvent = event->NetEvent.EventID;

		switch (demonbellEvent)
		{
			case DEMONBELL_EVENT_ACTIVATE: return demonbellHandleEvent_Activate(moby, event);
			case DEMONBELL_EVENT_DEACTIVATE: return demonbellHandleEvent_Deactivate(moby, event);
			default:
			{
				DPRINTF("unhandle demonbell event %d\n", demonbellEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void demonbellOnGuberCreated(Moby* moby, int id)
{
	// set update
	moby->PUpdate = &demonbellUpdate;
  moby->ModeBits = 0x5050;
  moby->UpdateDist = -1;

	// update pvars
	struct DemonBellPVar* pvars = (struct DemonBellPVar*)moby->PVar;

  // set id
  pvars->Id = id;
  
	// set team
	Guber* guber = guberGetObjectByMoby(moby);
	if (guber)
		((GuberMoby*)guber)->TeamNum = 10;
	
	// 
	mobySetState(moby, 0, -1);
	DPRINTF("demonbell spawned at %08X\n", (u32)moby);
}

//--------------------------------------------------------------------------
void demonbellTick(void)
{
  static int lastRoundNumber = 0;
  if (!MapConfig.State) return;

  MapConfig.State->DemonBellCount = demonbellCount;
  
  // check for round change
  if (MapConfig.State->RoundNumber != lastRoundNumber) {
    MapConfig.State->RoundDemonBellCount = 0;
    lastRoundNumber = MapConfig.State->RoundNumber;
    demonbellOnRoundChanged(lastRoundNumber);
  }
}

//--------------------------------------------------------------------------
void demonbellInit(void)
{
	Moby* temp = mobySpawn(DEMONBELL_MOBY_OCLASS, 0);
	if (temp) {
		MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
		if (mobyFunctionsPtr) {
      mapInstallMobyFunctions(mobyFunctionsPtr);
		  DPRINTF("DEMONBELL oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, (u32)mobyFunctionsPtr->GetGuberObject, (u32)mobyFunctionsPtr->MobyEventHandler);
		}

		mobyDestroy(temp);
	}

  // create gubers for demonbells
  Moby* moby = mobyListGetStart();
  demonbellCount = 0;
	while ((moby = mobyFindNextByOClass(moby, DEMONBELL_MOBY_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DPRINTF("found demonbell %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        demonbellOnGuberCreated(moby, demonbellCount);
        ++demonbellCount;
      }
    }

		++moby;
	}
}
