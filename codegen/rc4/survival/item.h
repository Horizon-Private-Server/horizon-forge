#ifndef SURVIVAL_ITEM_H
#define SURVIVAL_ITEM_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define MAX_ITEM_COUNT (64)

struct SurvivalItemDef;

enum SurvivalItemType
{
	// items that offer passive effects (stackable or singular)
	SURVIVAL_ITEM_PASSIVE,
	// items that can be held and used by the player
	// auto = item consumed automatically, ie self revive
	// manual = item consumed by player pressing button
	// immediate = item consumed as soon as it's acquired
	SURVIVAL_ITEM_CONSUMABLE_AUTO,
	SURVIVAL_ITEM_CONSUMABLE_MANUAL,
	SURVIVAL_ITEM_CONSUMABLE_IMMEDIATE,
};

enum SurvivalItemStoreCostType
{
	SURVIVAL_ITEM_STORE_COST_BOLTS_LINEAR,
	SURVIVAL_ITEM_STORE_COST_BOLTS_EXPONENTIAL,
	SURVIVAL_ITEM_STORE_COST_TOKENS_LINEAR,
	SURVIVAL_ITEM_STORE_COST_TOKENS_EXPONENTIAL,
};

typedef void (*ItemInit_func)(int defIdx, struct SurvivalItemDef *def);
typedef void (*ItemTickUpdate_func)(int defIdx, struct SurvivalItemDef *def);
typedef void (*ItemDrawUpdate_func)(int defIdx, struct SurvivalItemDef *def);
typedef void (*ItemOnAcquired_func)(int defIdx, struct SurvivalItemDef *def, int playerId);
typedef void (*ItemOnConsumed_func)(int defIdx, struct SurvivalItemDef *def, int playerId);
typedef u32 (*ItemGetConsumeCooldownTicks_func)(int defIdx, struct SurvivalItemDef *def, int playerId);
typedef int (*ItemHasRoomForMore_func)(int defIdx, struct SurvivalItemDef *def, int playerId);
typedef int (*ItemCanBuyInStore_func)(int defIdx, struct SurvivalItemDef *def, Moby *storeMoby, int playerId, int numTimesPurchased);
typedef u32 (*ItemGetStoreCost_func)(int defIdx, struct SurvivalItemDef *def, Moby *storeMoby, int playerId, int numTimesPurchased);
typedef float (*ItemGetMysteryBoxChance_func)(int defIdx, struct SurvivalItemDef *def, int playerId);
typedef float (*ItemGetDropChance_func)(int defIdx, struct SurvivalItemDef *def, int playerId);
typedef float (*ItemGetVendorRewardChance_func)(int defIdx, struct SurvivalItemDef *def, int playerId);

typedef struct SurvivalItemVTable
{
	ItemInit_func InitFunc;
	ItemTickUpdate_func TickUpdateFunc;
	ItemDrawUpdate_func DrawUpdateFunc;
	ItemOnAcquired_func OnAcquiredFunc;
	ItemOnConsumed_func OnConsumedFunc;
	ItemHasRoomForMore_func HasRoomForMoreFunc;
	ItemGetConsumeCooldownTicks_func GetConsumeCooldownTicksFunc;
	ItemCanBuyInStore_func CanBuyInStoreFunc;
	ItemGetStoreCost_func GetStoreCostFunc;
	ItemGetMysteryBoxChance_func GetMysteryboxChanceFunc;
	ItemGetDropChance_func GetDropChanceFunc;
	ItemGetVendorRewardChance_func GetVendorRewardChanceFunc;
} SurvivalItemVTable_t;

typedef struct SurvivalItemDef
{
	char Name[32];
	char Description[64];
	int TexId;
	u32 TexColor;
	enum SurvivalItemType Type;
	int MaxHeldAtOnce;
	char AppearOnWall;
	u32 ConsumeCooldownTicks;

	// mystery box
	float MysteryboxChanceWeight;
	int MysteryboxForceAcquire;

	// drops
	float DropChanceWeight;

	// chance to get on weapon upgrade at weapon vendor
	float VendorRewardChanceWeight;

	// store
	enum SurvivalItemStoreCostType StoreCostType;
	u32 StoreCost;
	union
	{
		u32 Linear;
		float Exponential;
	} StoreCostIncrease;

	SurvivalItemVTable_t VTable;
} SurvivalItemDef_t;

void itemBeginAcquire(int playerId, int itemIdx);
void itemBeginConsume(int playerId, int itemIdx);

void itemShowMessage(int localPlayerIndex, int itemIdx, char *format, int ticks);
void itemGetDescription(char *buf, int size, int itemIdx, int playerId);
u32 itemGetCost(int localPlayerIndex, int itemIdx);
int itemGetPlayerBankAmount(int localPlayerIndex, SurvivalItemDef_t *item);
int itemChargePlayerBank(int localPlayerIndex, int itemIdx);
u32 itemGetCooldownTicks(int playerId, int itemIdx);

float itemGetMysteryboxChanceWeight(int itemIdx, SurvivalItemDef_t *itemDef, int playerId);
float itemGetDropChanceWeight(int itemIdx, SurvivalItemDef_t *itemDef, int playerId);
float itemGetVendorRewardChanceWeight(int itemIdx, SurvivalItemDef_t *itemDef, int playerId);

void itemOnConsumeTriggered(Player *player, int itemId);
void itemOnAcquireTriggered(Player *player, int itemId);
int itemCanAcquire(Player *player, int itemIdx);
int itemCanConsume(Player *player, int itemIdx);
void itemDraw(void);
void itemTick(void);
void itemInit(void);

#endif // SURVIVAL_ITEM_H
