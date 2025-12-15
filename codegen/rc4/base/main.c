/***************************************************
 * FILENAME :		main.c
 * 
 * DESCRIPTION :
 * 		Entrypoint for custom code logic.
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
#include <libdl/collision.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/moby.h>
##INCLUDES##

MobyGetGuberObject_func baseGetGuberFunc = NULL;
MobyEventHandler_func baseHandleGuberEventFunc = NULL;
char baseInitialized = 0;
char baseCleanedUp = 0;

##DECLARATIONS##

##FUNCTIONS##

//--------------------------------------------------------------------------
struct Guber* mapGetGuber(Moby* moby)
{
#if RAIDS || SURVIVAL
  if (mobyIsMob(moby)) return (Guber*)moby->GuberMoby;
#endif

  switch (moby->OClass)
  {
##GETGUBERCASES##
    default:
    {
      #if RAIDS || SURVIVAL
        if (MapConfig.OnGetGuberFunc) {
          struct Guber* guber = MapConfig.OnGetGuberFunc(moby);
          if (guber) return guber;
        }
      #endif
    
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
  ##HANDLEEVENTCASES##
      default:
			{
        #if RAIDS || SURVIVAL
          if (MapConfig.OnGuberEventFunc) {
            MapConfig.OnGuberEventFunc(moby, event);
            return;
          }
        #endif
      
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
  //if (!baseHandleGuberEventFunc) baseHandleGuberEventFunc = mobyFunctions->MobyEventHandler;

  mobyFunctions->GetGuberObject = &mapGetGuber;
  mobyFunctions->GetMobyInterface = NULL;
  mobyFunctions->MobyEventHandler = &mapHandleEvent;
}

//--------------------------------------------------------------------------
void cleanup(void)
{
  if (baseCleanedUp)
    return;

##CLEANUPBODY##

  baseCleanedUp = 1;
}

//--------------------------------------------------------------------------
void baseOnLoadLevel(void)
{
  if (baseInitialized && !baseCleanedUp) {
    cleanup();
  }
}

//--------------------------------------------------------------------------
void initialize(void)
{
  if (baseInitialized)
    return;
  GameSettings* gs = gameGetSettings();
  randSeed(gs->GameLoadStartTime);
  
  HOOK_J(0x004E24C8, &baseOnLoadLevel);

##INITBODY##

  baseInitialized = 1;
}

//--------------------------------------------------------------------------
int main(void)
{
  int i;
  if (baseCleanedUp) return 0;
  if (!isInGame() && !isSceneLoadedNotYetInGame())
    return 0;

  dlPreUpdate();

  // init
  initialize();

  if (!isInGame()) return 0;

  // check if all clients have loaded
  int clientsReady = 0;
  if (PATCH_INTEROP && PATCH_INTEROP->PatchStateContainer) {
    clientsReady = PATCH_INTEROP->PatchStateContainer->AllClientsReady;
  }

  if (clientsReady || !netGetDmeServerConnection()) {
##MAINBODYREADY##
  }

##MAINBODY##

  dlPostUpdate();
  return 0;
}
