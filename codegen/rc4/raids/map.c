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
#include "spawner.h"
#include "mover.h"
#include "gate.h"
#include "controller.h"
#include "checkpoint.h"
#include "mob.h"
#include "game.h"
#include "maputils.h"

extern struct RaidsMapConfig MapConfig;

MobyGetGuberObject_func baseGetGuberFunc = NULL;
MobyEventHandler_func baseHandleGuberEventFunc = NULL;

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
void mapOnMobKilled(Moby* moby, int killedByPlayerId, int weaponId)
{
  if (!moby || !moby->PVar) return;

	struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
    spawnerOnChildMobKilled(moby->PParent, moby, pvars->MobVars.Userdata, killedByPlayerId, weaponId);
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

//--------------------------------------------------------------------------
struct Guber* mapGetGuber(Moby* moby)
{
  if (mobyIsMob(moby)) return (Guber*)moby->GuberMoby;

  switch (moby->OClass)
  {
    case SPAWNER_OCLASS: return spawnerGetGuber(moby);
    case MOVER_OCLASS: return moverGetGuber(moby);
    case CONTROLLER_OCLASS: return controllerGetGuber(moby);
    case CHECKPOINT_MANAGER_OCLASS:
    case CHECKPOINT_OCLASS: return checkpointGetGuber(moby);
#if GATE
    case GATE_OCLASS: return gateGetGuber(moby);
#endif
    default:
    {
      // pass up to mode
      if (MapConfig.OnGetGuberFunc) {
        struct Guber* guber = MapConfig.OnGetGuberFunc(moby);
        if (guber) return guber;
      }

      // pass to overwritten game func
      if (baseGetGuberFunc) { 
        //DPRINTF("base get guber object %08X %04X\n", moby, moby->OClass);
        return baseGetGuberFunc(moby);
      }

      // unhandled
      DPRINTF("unhandled get guber for moby %04X at %08X\n", moby->OClass, (u32)moby);
      return NULL;
    }
  }
	
	return 0;
}

//--------------------------------------------------------------------------
void mapHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return;

	if (isInGame() && !mobyIsDestroyed(moby)) {

    switch (moby->OClass)
    {
      case SPAWNER_OCLASS: spawnerHandleEvent(moby, event); break;
      case MOVER_OCLASS: moverHandleEvent(moby, event); break;
      case CONTROLLER_OCLASS: controllerHandleEvent(moby, event); break;
      case CHECKPOINT_MANAGER_OCLASS:
      case CHECKPOINT_OCLASS: checkpointHandleEvent(moby, event); break;
#if GATE
    case GATE_OCLASS: gateHandleEvent(moby, event); break;
#endif
      default:
			{
        // pass up to mode
        if (MapConfig.OnGuberEventFunc && MapConfig.OnGuberEventFunc(moby, event))
          return;
        
        // pass to overwritten game func
        if (baseHandleGuberEventFunc) {
          //DPRINTF("base handle guber event %08X %04X\n", moby, moby->OClass);
          baseHandleGuberEventFunc(moby, event);
          return;
        }

        // unhandled
        DPRINTF("unhandled guber event %d for moby %04X at %08X\n", event->NetEvent.EventID, moby->OClass, (u32)moby);
				break;
			}
    }
	}
}

//--------------------------------------------------------------------------
void mapInstallMobyFunctions(MobyFunctions* mobyFunctions)
{
  if (!baseGetGuberFunc) baseGetGuberFunc = mobyFunctions->GetGuberObject;
  if (!baseHandleGuberEventFunc) baseHandleGuberEventFunc = mobyFunctions->MobyEventHandler;

  mobyFunctions->GetGuberObject = &mapGetGuber;
  mobyFunctions->GetMobyInterface = NULL;
  mobyFunctions->MobyEventHandler = &mapHandleEvent;
}
