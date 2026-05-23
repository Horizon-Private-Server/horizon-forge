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
#include <libdl/random.h>
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
#include "common.h"
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
      #if SURVIVAL
        if (MapConfig.Functions.ModeOnGetGuberFunc) {
          struct Guber* guber = MapConfig.Functions.ModeOnGetGuberFunc(moby);
          if (guber) return guber;
        }
      #endif
    
      #if RAIDS
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
        #if SURVIVAL
          if (MapConfig.Functions.ModeOnGuberEventFunc) {
            MapConfig.Functions.ModeOnGuberEventFunc(moby, event);
            return;
          }
        #endif
      
        #if RAIDS
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
void mapDrawDebugWatermark(void)
{
  gfxHelperDrawText(5, 5, 0, 0, 1, 0x80FFFFFF, "DEBUG BUILD", -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void draw(void)
{
##DRAWBODY##

#if DEBUG
  mapDrawDebugWatermark();
#endif
}

//--------------------------------------------------------------------------
void start(void)
{
	static int hasRun = 0;
	if (hasRun)
		return;

	hasRun = 1;

##STARTBODY##
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
  HOOK_J(0x005BAE50, &draw);

##INITBODY##

  baseInitialized = 1;
}

//--------------------------------------------------------------------------
void load(void)
{
##LOADBODY##
}

//--------------------------------------------------------------------------
int entrypoint(int a0)
{
  if (baseCleanedUp) return 0;
  if (a0 == 0) {
	load();
	return 0;
  }

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
	start();
##MAINBODYREADY##
  }

##MAINBODY##

  dlPostUpdate();
  return 0;
}
