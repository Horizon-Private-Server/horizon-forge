#ifndef RAIDS_INVENTORY_H
#define RAIDS_INVENTORY_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "bank.h"

#define INVENTORY_TAB_COUNT                         (WEAPON_SLOT_COUNT)
#define INVENTORY_TAB_BADGES                        (0)
#define INVENTORY_TAB_SPRITE_PADDING                (8)

#define INVENTORY_NOTIFY_SPRITE_ID                  (88) // star
#define INVENTORY_NOTIFY_NEW_COLOR                  (0x80FF4000)
#define INVENTORY_NOTIFY_FAV_COLOR                  (0x8000FFFF)

#define INVENTORY_DRAW_CENTER_X                     (SCREEN_WIDTH / 2.0)
#define INVENTORY_DRAW_CENTER_Y                     (SCREEN_HEIGHT / 2.0)
#define INVENTORY_DRAW_FULL_W                       (SCREEN_WIDTH - 100)
#define INVENTORY_DRAW_FULL_H                       (SCREEN_HEIGHT - 170)
#define INVENTORY_DRAW_FRAME_BORDER_W               (2)

#define INVENTORY_DRAW_WEAPONS_DIM                  (8)
#define INVENTORY_DRAW_WEAPONS_M                    (4)
#define INVENTORY_DRAW_WEAPONS_W                    (INVENTORY_DRAW_FULL_W / 2.0)
#define INVENTORY_DRAW_WEAPONS_H                    (INVENTORY_DRAW_WEAPONS_W)

#define INVENTORY_DRAW_INFO_W                       (INVENTORY_DRAW_FULL_W - (INVENTORY_DRAW_WEAPONS_W))
#define INVENTORY_DRAW_INFO_H                       (INVENTORY_DRAW_INFO_W)

typedef struct InventoryDrawState
{
  int SelectedIdx;
  int FilterIdx;
  int PageIdx;
  char ShowSellDialog;
} InventoryDrawState_t;

void inventoryOpen(void);
void inventoryClose(void);
void inventorySetFilter(int filter);
void inventoryTick(void);
void inventoryInit(void);

#endif // RAIDS_INVENTORY_H
