#ifndef COMMON_MESSAGER_H
#define COMMON_MESSAGER_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define MESSAGER_OCLASS                         (0x4005)
#define MESSAGER_PLAYER_MASK_HOST               (1 << 10)
#define MESSAGER_MAX_DRAW_QUEUE                 (16)

enum MessagerState {
	MESSAGER_STATE_DEACTIVATED,
	MESSAGER_STATE_ACTIVATED,
	MESSAGER_STATE_COMPLETE = 0x7F,
};

enum MessagerDisplayType {
	MESSAGER_DISPLAY_POPUP,
  MESSAGER_DISPLAY_HELP_POPUP,
	MESSAGER_DISPLAY_SCREEN_SPACE,
	MESSAGER_DISPLAY_WORLD_SPACE,
};

struct MessagerRuntimeState
{
  int TimeActivated;
};

struct MessagerMessage
{
  short Length;
  u8 RuntimeSeconds;
  char MoveTo;
  char* Message;
};

struct MessagerPVar
{
  char Init;
  char DefaultState;
  char Log;
  char DefaultMessageIdx;
  char DisplayType;
  char Alignment;
  short TargetPlayersMask;
  short PosX;
  short PosY;
  int RuntimeSeconds;
  u32 Color;
  float Scale;
  struct MessagerRuntimeState State;
  int MessageCount;
  struct MessagerMessage Messages[0];
};

void messagerFrameUpdate(void);
void messagerInit(void);

#endif // COMMON_MESSAGER_H
