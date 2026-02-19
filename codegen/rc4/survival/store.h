#ifndef SURVIVAL_STORE_H
#define SURVIVAL_STORE_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include "item.h"

#define STORE_MOBY_OCLASS (0x4100)
#define STORE_MAX_DIST (4)
#define STORE_MAX_PAGES (8)
#define PLAYER_STORE_COOLDOWN_TICKS (5)

enum StoreState
{
	STORE_STATE_DISABLED,
	STORE_STATE_ENABLED,
};

enum StoreEventType
{
	STORE_EVENT_SPAWN,
	STORE_EVENT_SET_STATE,
};

enum StoreInteractType
{
	STORE_INTERACT_NEAR,
	STORE_INTERACT_LOOK_AT,
	STORE_INTERACT_STAND_ON
};

enum StoreItemCanBuyResult
{
	STORE_ITEM_CAN_BUY_BAD_STATE = 0,
	STORE_ITEM_CAN_BUY_BAD_ITEM,
	STORE_ITEM_CAN_BUY_ITEM_DISABLED,
	STORE_ITEM_CAN_BUY_HAVE_TOO_MANY,
	STORE_ITEM_CAN_BUY_NOT_ENOUGH_BOLTS,
	STORE_ITEM_CAN_BUY_GOOD,
};

struct StorePageDef
{
	char Name[32];
	int TexId;
	int ItemsCount;
	int Items[MAX_ITEM_COUNT];
};

struct StoreDef
{
	char Name[32];
	int PagesCount;
	struct StorePageDef *Pages;
};

typedef struct StoreDef *(*StoreGetStore_func)(Moby *moby, int localPlayerIndex, int storeIdx);

struct StoreVTable
{
	StoreGetStore_func GetStoreFunc;
};

struct StorePVar
{
	Moby *BoundMoby;
	int StoreIndex;
	char DisableDuringRound;
	char AppearOnRadar;
	char RadarBlipType;
	char RadarBlipTeam;
	char InteractType;
	struct StoreVTable VTable;
	int PageIdx[GAME_MAX_LOCALS];
	int RowIdx[GAME_MAX_LOCALS];
	char MenuOpen[GAME_MAX_LOCALS];
};

int storeHandleEvent(Moby *moby, GuberEvent *event);
void storeOnGuberCreated(Moby *moby, struct StoreVTable *defaultVTable);
void storeFrameTick(void);
void storeInit(struct StoreVTable *defaultVTable);

#endif // SURVIVAL_STORE_H
