#ifndef RAIDS_LEVELSELECT_H
#define RAIDS_LEVELSELECT_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "game.h"

#define LEVELSELECT_DRAW_CENTER_X                     (SCREEN_WIDTH / 2.0)
#define LEVELSELECT_DRAW_CENTER_Y                     (SCREEN_HEIGHT / 2.0)
#define LEVELSELECT_DRAW_FULL_W                       (SCREEN_WIDTH - 100)
#define LEVELSELECT_DRAW_FULL_H                       (SCREEN_HEIGHT - 170)
#define LEVELSELECT_DRAW_FRAME_BORDER_W               (2)

#define LEVELSELECT_TITLE_H                           (30)

#define LEVELSELECT_MAPLIST_W                         (LEVELSELECT_DRAW_FULL_W / 2.0)
#define LEVELSELECT_MAPLIST_H                         (LEVELSELECT_MAPLIST_W)
#define LEVELSELECT_MAPLIST_ITEM_H                    (20)
#define LEVELSELECT_MAPLIST_ITEM_COUNT                (LEVELSELECT_MAPLIST_H / LEVELSELECT_MAPLIST_ITEM_H)

#define LEVELSELECT_MAPINFO_W                         (LEVELSELECT_DRAW_FULL_W - (LEVELSELECT_MAPLIST_W))
#define LEVELSELECT_MAPINFO_H                         (LEVELSELECT_MAPINFO_W)

#define LEVELSELECT_MAPDESC_H                         (130)

#define LEVELSELECT_STAR_W                            (32)
#define LEVELSELECT_STAR_H                            (LEVELSELECT_STAR_W)
#define LEVELSELECT_STAR_SPRITE_ID                    (88)

#define LEVELSELECT_INPUT_COOLDOWN                    (30)

typedef struct LevelselectDrawState
{
  int InputCooldownTicks;
  int SelectedIdx;
  int SelectedDifficulty;
  int NumPlanets;
  int ShowChallengesDialog;
  int ChallengesDialogSelectedIdx;
  char SelectedMapFilename[64];
  struct RaidsBankMapStats MapStats;
  CustomMapDef_t* SelectedMapDef;
  union {
    struct RaidsCustomMapExtraData SelectedMapExtraData;
    char SelectedMapExtraDataBuf[RAIDS_MAX_EXDATA_SIZE];
  };
} LevelselectDrawState_t;

void levelselectOpen(void);
void levelselectClose(void);
void levelselectFrameTick(void);
void levelselectStart(void);
void levelselectInit(void);

#endif // RAIDS_LEVELSELECT_H
