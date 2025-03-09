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

##DECLARATIONS##

##FUNCTIONS##

//--------------------------------------------------------------------------
void initialize(void)
{
  static int initialized = 0;
  if (initialized)
    return;

##INITBODY##

  initialized = 1;
}

//--------------------------------------------------------------------------
int main(void)
{
  int i;
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
