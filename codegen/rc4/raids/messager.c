/***************************************************
 * FILENAME :		messager.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the messagers.
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
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "common.h"
#include "maputils.h"
#include "shared.h"
#include "spawner.h"
#include "gate.h"
#include "mover.h"
#include "messager.h"
#include "mob.h"
#include "game.h"

#if DEBUG
#define DLOG(moby, format, ...) if (((struct MessagerPVar*)moby->PVar)->Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#else
#define DLOG(moby, format, ...) 
#endif

int messagerDrawQueueCount = 0;
Moby* messagerDrawQueueMobys[MESSAGER_MAX_DRAW_QUEUE] = {};
gfxDrawFuncDef messagerDrawQueueCallbacks[MESSAGER_MAX_DRAW_QUEUE] = {};

//--------------------------------------------------------------------------
void messagerQueueDraw(Moby* moby, gfxDrawFuncDef callback)
{
  if (messagerDrawQueueCount >= MESSAGER_MAX_DRAW_QUEUE) return;

  messagerDrawQueueMobys[messagerDrawQueueCount] = moby;
  messagerDrawQueueCallbacks[messagerDrawQueueCount] = callback;
  messagerDrawQueueCount++;
}

//--------------------------------------------------------------------------
void messagerDrawWorldSpaceMessage(Moby* moby)
{
  struct MessagerPVar* pvars = (struct MessagerPVar*)moby->PVar;
  int c = 0;
  int start = 0;
  int x,y;

  // get current msg
  int msgIdx = moby->State - MESSAGER_STATE_ACTIVATED;
  struct MessagerMessage* msg = &pvars->Messages[msgIdx];
  if (msgIdx < 0 || msgIdx >= pvars->MessageCount) return;

  if (gfxWorldSpaceToScreenSpace(moby->Position, &x, &y)) {
    float scale = pvars->Scale / sqrtf(vector_distance(moby->Position, cameraGetGameCamera(0)->pos));
    while (c < msg->Length) {
      while (c < msg->Length && msg->Message[c] != 0x01) c++;

      gfxHelperDrawText(x, y, 0, 0, scale, pvars->Color, &msg->Message[start], c-start, pvars->Alignment, COMMON_DZO_DRAW_NORMAL);
      start = ++c;
      y += scale * 16;
    }
  }
}

//--------------------------------------------------------------------------
void messagerDrawScreenSpaceMessage(Moby* moby)
{
  struct MessagerPVar* pvars = (struct MessagerPVar*)moby->PVar;
  int c = 0;
  int start = 0;
  int x = pvars->PosX, y = pvars->PosY;
  
  // get current msg
  int msgIdx = moby->State - MESSAGER_STATE_ACTIVATED;
  struct MessagerMessage* msg = &pvars->Messages[msgIdx];
  if (msgIdx < 0 || msgIdx >= pvars->MessageCount) return;

  while (c < msg->Length) {
    while (c < msg->Length && msg->Message[c] != 0x01) c++;

    gfxHelperDrawText(x, y, 0, 0, pvars->Scale, pvars->Color, &msg->Message[start], c-start, pvars->Alignment, COMMON_DZO_DRAW_NORMAL);
    start = ++c;
    y += pvars->Scale * 16;
  }
}

//--------------------------------------------------------------------------
void messagerOnStateChanged(Moby* moby)
{
  struct MessagerPVar* pvars = (struct MessagerPVar*)moby->PVar;
  pvars->State.TimeActivated = -1;
}

//--------------------------------------------------------------------------
void messagerUpdate(Moby* moby)
{
  int i;
  struct MessagerPVar* pvars = (struct MessagerPVar*)moby->PVar;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    messagerOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  if (moby->State == MESSAGER_STATE_DEACTIVATED) { pvars->State.TimeActivated = -1; return; }
  if (moby->State == MESSAGER_STATE_COMPLETE) { pvars->State.TimeActivated = -1; return; }
  if (pvars->State.TimeActivated <= 0) pvars->State.TimeActivated = gameGetTime();
  
  // get current msg
  int msgIdx = moby->State - MESSAGER_STATE_ACTIVATED;
  struct MessagerMessage* msg = &pvars->Messages[msgIdx];
  if (msgIdx < 0 || msgIdx >= pvars->MessageCount) return;

  // run
  float t = (gameGetTime() - pvars->State.TimeActivated) / 1000.0;

  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!player) continue;

    if ((pvars->TargetPlayersMask & (1 << player->PlayerId)) || (gameAmIHost() && (pvars->TargetPlayersMask & MESSAGER_PLAYER_MASK_HOST))) {
      switch (pvars->DisplayType) {
        case MESSAGER_DISPLAY_POPUP: uiShowPopup(i, msg->Message); break;
        case MESSAGER_DISPLAY_HELP_POPUP: uiShowHelpPopup(i, msg->Message, 10); break;
        case MESSAGER_DISPLAY_SCREEN_SPACE: messagerQueueDraw(moby, &messagerDrawScreenSpaceMessage); break;
        case MESSAGER_DISPLAY_WORLD_SPACE: messagerQueueDraw(moby, &messagerDrawWorldSpaceMessage); break;
      }
    }
  }

  // complete
  if (msg->RuntimeSeconds > 0 && t >= msg->RuntimeSeconds) {
    mobySetState(moby, msg->MoveTo > 0 ? msg->MoveTo : MESSAGER_STATE_COMPLETE, -1);
    return;
  }
}

//--------------------------------------------------------------------------
void messagerFrameUpdate(void)
{
  // draw callbacks
  while (messagerDrawQueueCount > 0) {
    --messagerDrawQueueCount;
    Moby* moby = messagerDrawQueueMobys[messagerDrawQueueCount];
    gfxDrawFuncDef callback = messagerDrawQueueCallbacks[messagerDrawQueueCount];
    if (callback && moby) callback(moby);

    messagerDrawQueueCallbacks[messagerDrawQueueCount] = NULL;
  }
}

//--------------------------------------------------------------------------
void messagerInit(void)
{
  int i;

  // set update functions
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, MESSAGER_OCLASS)))
	{
    struct MessagerPVar* pvars = (struct MessagerPVar*)moby->PVar;
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      DLOG(moby, "found messager %08X\n", (u32)moby);
      moby->PUpdate = &messagerUpdate;
      moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;

      // update message refs
      for (i = 0; i < pvars->MessageCount; ++i) {
        if (pvars->Messages[i].Message) {
          pvars->Messages[i].Message += (u32)pvars; // rel to abs address
        }
      }

      int state = pvars->DefaultState > 0 ? (pvars->DefaultMessageIdx + MESSAGER_STATE_ACTIVATED) : 0;
      mobySetState(moby, state, -1);
    }

		++moby;
	}

  DPRINTF("messager pvar size %d\n", sizeof(struct MessagerPVar));
}
