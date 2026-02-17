/***************************************************
 * FILENAME :		store.c
 *
 * DESCRIPTION :
 * 		Handles logic for the store vendor.
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
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/math3d.h>
#include <libdl/radar.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "store.h"
#include "window.h"
#include "game.h"
#include "gate.h"
#include "messageid.h"
#include "maputils.h"
#include "common.h"

const char *ITEM_TYPE_NAMES[] =
		{
				[SURVIVAL_ITEM_PASSIVE] "Effect",
				[SURVIVAL_ITEM_CONSUMABLE_AUTO] "Totem",
				[SURVIVAL_ITEM_CONSUMABLE_MANUAL] "Consumable",
				[SURVIVAL_ITEM_CONSUMABLE_IMMEDIATE] "Instant",
				[SURVIVAL_ITEM_CONSUMABLE_OTHER] "",
};

Moby *StoreMobys[16] = {};

//--------------------------------------------------------------------------
int storeGetIsMenuOpen(Moby *moby, int localPlayerIndex)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;

	return pvars->MenuOpen[localPlayerIndex] != 0;
}

//--------------------------------------------------------------------------
void storeSetIsMenuOpen(Moby *moby, int localPlayerIndex, int open)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;

	pvars->MenuOpen[localPlayerIndex] = open != 0;
}

//--------------------------------------------------------------------------
struct StoreDef *storeGetStoreDef(Moby *moby, int localPlayerIndex)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	if (!pvars->VTable.GetStoreFunc)
		return NULL;

	return pvars->VTable.GetStoreFunc(moby, localPlayerIndex, pvars->StoreIndex);
}

//--------------------------------------------------------------------------
void storeGetName(Moby *moby, int localPlayerIndex, char *buf, int len)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;

	// default to Store
	strncpy(buf, "Store", len);
	struct StoreDef *storeDef = storeGetStoreDef(moby, localPlayerIndex);
	if (!storeDef)
		return;

	strncpy(buf, storeDef->Name, len);
}

//--------------------------------------------------------------------------
int storeGetPageCount(Moby *moby, int localPlayerIndex)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	struct StoreDef *storeDef = storeGetStoreDef(moby, localPlayerIndex);
	if (!storeDef)
		return 0;

	return storeDef->PagesCount;
}

//--------------------------------------------------------------------------
int storeGetPage(Moby *moby, int localPlayerIndex, int pageIdx, struct StorePageDef *page)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	struct StoreDef *storeDef = storeGetStoreDef(moby, localPlayerIndex);
	if (!storeDef)
		return 0;

	if (pageIdx < 0 || pageIdx >= storeDef->PagesCount)
		return 0;

	memcpy(page, &storeDef->Pages[pageIdx], sizeof(struct StorePageDef));
	return 1;
}

//--------------------------------------------------------------------------
int storeGetItem(Moby *moby, int localPlayerIndex, int itemId, SurvivalItemDef_t *item)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	struct StoreDef *storeDef = storeGetStoreDef(moby, localPlayerIndex);
	if (!storeDef)
		return 0;

	// get default item
	if (itemId < 0 || itemId >= MapConfig.ItemDefCount)
		return 0;

	// return copy of item
	memcpy(item, &MapConfig.ItemDefs[itemId], sizeof(SurvivalItemDef_t));
	return 1;
}

//--------------------------------------------------------------------------
int storeGetPlayerItemPurchaseCount(Moby *moby, int localPlayerIndex, int itemId)
{
	if (!MapConfig.State)
		return 0;

	return MapConfig.State->StorePurchaseCount[localPlayerIndex][itemId];
}

//--------------------------------------------------------------------------
u32 storeGetItemCost(Moby *moby, int localPlayerIndex, int itemId, SurvivalItemDef_t *item)
{
	return itemGetCost(localPlayerIndex, itemId);
}

//--------------------------------------------------------------------------
int storeGetItemPlayerBankAmount(Moby *moby, int localPlayerIndex, int itemId, SurvivalItemDef_t *item)
{
	return itemGetPlayerBankAmount(localPlayerIndex, item);
}

//--------------------------------------------------------------------------
int storeChargeItemPlayerBank(Moby *moby, int localPlayerIndex, int itemId, SurvivalItemDef_t *item, u32 amount)
{
	return itemChargePlayerBank(localPlayerIndex, itemId);
}

//--------------------------------------------------------------------------
enum StoreItemCanBuyResult storeCanBuyItem(Moby *moby, int localPlayerIndex, int itemIdx)
{
	SurvivalItemDef_t itemDef;
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	Player *player = playerGetFromSlot(localPlayerIndex);

	// check we have state
	if (!MapConfig.State)
		return STORE_ITEM_CAN_BUY_BAD_STATE;

	// only handle for local player
	if (!playerIsValid(player) || !player->IsLocal)
		return STORE_ITEM_CAN_BUY_BAD_STATE;

	int playerId = player->PlayerId;
	int count = playerGetItemCount(player, itemIdx);

	// check store is enabled
	if (moby->State != STORE_STATE_ENABLED)
		return STORE_ITEM_CAN_BUY_BAD_STATE;

	// check item is valid
	if (itemIdx < 0 || itemIdx >= MAX_ITEM_COUNT || itemIdx >= MapConfig.ItemDefCount)
		return STORE_ITEM_CAN_BUY_BAD_ITEM;

	// get item
	if (!storeGetItem(moby, localPlayerIndex, itemIdx, &itemDef))
		return STORE_ITEM_CAN_BUY_BAD_ITEM;

	// check if disabled
	if (itemDef.VTable.CanBuyInStoreFunc && !itemDef.VTable.CanBuyInStoreFunc(itemIdx, &itemDef, moby, playerId, count))
		return STORE_ITEM_CAN_BUY_ITEM_DISABLED;

	// has room for more
	if (itemDef.VTable.HasRoomForMoreFunc && !itemDef.VTable.HasRoomForMoreFunc(itemIdx, &itemDef, playerId))
		return STORE_ITEM_CAN_BUY_HAVE_TOO_MANY;
	else if (itemDef.MaxHeldAtOnce > 0 && MapConfig.State->PlayerStates[playerId].State.ItemCounts[itemIdx] >= itemDef.MaxHeldAtOnce)
		return STORE_ITEM_CAN_BUY_HAVE_TOO_MANY;

	// check cost
	u32 cost = storeGetItemCost(moby, localPlayerIndex, itemIdx, &itemDef);
	u32 bank = storeGetItemPlayerBankAmount(moby, localPlayerIndex, itemIdx, &itemDef);
	if (bank < cost)
		return STORE_ITEM_CAN_BUY_NOT_ENOUGH_BOLTS;

	return STORE_ITEM_CAN_BUY_GOOD;
}

//--------------------------------------------------------------------------
void storePlayerBuy(Moby *moby, int localPlayerIndex, int itemIdx)
{
	SurvivalItemDef_t itemDef;
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	Player *player = playerGetFromSlot(localPlayerIndex);

	// check can buy
	if (storeCanBuyItem(moby, localPlayerIndex, itemIdx) != STORE_ITEM_CAN_BUY_GOOD)
		return;

	storeGetItem(moby, localPlayerIndex, itemIdx, &itemDef);
	u32 cost = storeGetItemCost(moby, localPlayerIndex, itemIdx, &itemDef);
	int playerId = player->PlayerId;

	// buy
	storeChargeItemPlayerBank(moby, localPlayerIndex, itemIdx, &itemDef, cost);
	playPaidSound(player);

	// give player
	itemBeginAcquire(playerId, itemIdx);
	itemShowAcquired(player->LocalPlayerIndex, itemIdx, "Purchased");
}

//--------------------------------------------------------------------------
void storeSetState(Moby *moby, int state)
{
	int i;

	// only host can change state
	if (!gameAmIHost())
		return;

	// create event
	GuberEvent *guberEvent = guberCreateEvent(moby, STORE_EVENT_SET_STATE);
	if (guberEvent)
	{
		guberEventWrite(guberEvent, &state, 4);
	}
}

//--------------------------------------------------------------------------
void storeDrawItemList(Moby *moby, int localPlayerIndex, Window_t *drawWindow, int selectedIdx, struct StorePageDef *page)
{
	static int drawItemsFrom = 0;
	const u32 bgColor = 0x70000000;						// dark gray
	const u32 textColor = 0x80FFFFFF;					// white
	const u32 spriteColor = 0x80808080;				// gray
	const u32 selectedColor = 0x40008080;			// yellow
	const u32 cannotAffordColor = 0x80000080; // red
	const u32 canAffordColor = 0x80108010;		// green
	const u32 borderColor = 0x80000020;				// dark red
	const int lineHeight = 12;
	const float paddingTop = 0;
	const float paddingLeft = 5;
	const float nameLeft = 30;
	const int boltTexId = 4;
	const int tokenTexId = 32;
	char strBuf[64];
	Player *player = playerGetFromSlot(localPlayerIndex);

	// draw box
	windowFill(drawWindow, bgColor);

	// create items window
	Window_t drawWindowItem;
	windowCreateFrom(&drawWindowItem, drawWindow, 0, 0, drawWindow->Width, drawWindow->Height, TEXT_ALIGN_TOPLEFT);

	// always at least draw from selection
	if (selectedIdx < drawItemsFrom)
		drawItemsFrom = selectedIdx;

	int rowIdx = drawItemsFrom;
	gfxSetupGifPaging(0);
	for (rowIdx = drawItemsFrom; rowIdx < page->ItemsCount; ++rowIdx)
	{
		if (!windowHasArea(&drawWindowItem))
			break;

		// get item
		int itemIdx = page->Items[rowIdx];
		SurvivalItemDef_t itemDef;
		if (!storeGetItem(moby, localPlayerIndex, itemIdx, &itemDef))
			break;

		enum StoreItemCanBuyResult canBuyResult = storeCanBuyItem(moby, localPlayerIndex, itemIdx);
		int numHeld = MapConfig.State->PlayerStates[player->PlayerId].State.ItemCounts[itemIdx];
		u32 cost = storeGetItemCost(moby, localPlayerIndex, itemIdx, &itemDef);
		int currencyTexId = itemDef.StoreCostType >= SURVIVAL_ITEM_STORE_COST_TOKENS_LINEAR ? tokenTexId : boltTexId;
		int disabled = canBuyResult == STORE_ITEM_CAN_BUY_ITEM_DISABLED;
		u32 canBuyColor = colorLerp(textColor, cannotAffordColor, canBuyResult == STORE_ITEM_CAN_BUY_HAVE_TOO_MANY);
		u32 costColor = colorLerp(canBuyResult == STORE_ITEM_CAN_BUY_GOOD ? canAffordColor : cannotAffordColor, 0, disabled ? 0.5 : 0);
		u32 boltColor = colorLerp(textColor, 0, disabled ? 0.5 : 0);
		u32 nameColor = colorLerp(canBuyColor, 0, disabled ? 0.5 : 0);

		// draw selection bar
		if (rowIdx == selectedIdx)
		{
			Window_t windowHighlight;
			windowCreateFrom(&windowHighlight, &drawWindowItem, 0, 0, drawWindowItem.Width, lineHeight, TEXT_ALIGN_TOPLEFT);
			windowFill(&windowHighlight, selectedColor);
		}

		// draw current held count
		if (itemDef.Type != SURVIVAL_ITEM_CONSUMABLE_IMMEDIATE)
		{
			snprintf(strBuf, sizeof(strBuf), "%d", numHeld);
			windowDrawText(&drawWindowItem, TEXT_ALIGN_TOPLEFT, paddingLeft, paddingTop, 0.7, nameColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
		}

		// draw icon
		if (itemDef.TexId > 0)
		{
			windowDrawSprite(&drawWindowItem, TEXT_ALIGN_TOPLEFT, nameLeft + paddingLeft - lineHeight - 2, paddingTop, lineHeight - 2, lineHeight - 2, itemDef.TexId, boltColor, TEXT_ALIGN_TOPLEFT);
		}

		// draw name
		windowDrawText(&drawWindowItem, TEXT_ALIGN_TOPLEFT, nameLeft + paddingLeft, paddingTop, 0.7, nameColor, itemDef.Name, -1, TEXT_ALIGN_TOPLEFT);

		// draw cost
		windowDrawSprite(&drawWindowItem, TEXT_ALIGN_TOPRIGHT, -paddingLeft - 2, paddingTop, lineHeight - 2, lineHeight - 2, currencyTexId, boltColor, TEXT_ALIGN_TOPRIGHT);
		snprintf(strBuf, sizeof(strBuf), "%'d", cost);
		windowDrawText(&drawWindowItem, TEXT_ALIGN_TOPRIGHT, -paddingLeft - 6 - lineHeight, paddingTop, 0.6, costColor, strBuf, -1, TEXT_ALIGN_TOPRIGHT);

		windowMove(&drawWindowItem, 0, lineHeight);
		if (!windowHasArea(&drawWindowItem))
			break;
	}

	gfxDoGifPaging();

	// if we drew, and found that the selected row isn't drawn
	// skip forward to the selected index to guarantee it's drawn next update
	if (rowIdx > drawItemsFrom && rowIdx < selectedIdx)
	{
		drawItemsFrom += (selectedIdx - rowIdx);
	}

	// draw border
	windowBorder(drawWindow, borderColor, 1, 0, 1, 1);
}

//--------------------------------------------------------------------------
void storeDrawTabs(Moby *moby, int localPlayerIndex, Window_t *drawWindow, int selectedIdx)
{
	const u32 bgColor = 0x60000010;
	const u32 bgSolidColor = 0x80000000;
	const u32 textColor = 0x80FFFFFF;
	const u32 borderColor = 0x80000020;
	const float tabPadding = 4;
	int pageCount = storeGetPageCount(moby, localPlayerIndex);
	struct StorePageDef page;
	float tabSize = drawWindow->Height;

	// draw bg
	windowFill(drawWindow, bgColor);

	// draw selected tab text
	storeGetPage(moby, localPlayerIndex, selectedIdx, &page);
	windowDrawText(drawWindow, TEXT_ALIGN_MIDDLERIGHT, -5, 0, 0.9, textColor, page.Name, -1, TEXT_ALIGN_MIDDLERIGHT);

	int i;
	gfxSetupGifPaging(0);
	for (i = 0; i < pageCount; ++i)
	{
		if (!storeGetPage(moby, localPlayerIndex, i, &page))
			continue;

		int isSelectedTab = i == selectedIdx;
		u32 color = isSelectedTab ? 0x80C0C0C0 : 0x80404040;

		Window_t windowTab;
		windowCreateFrom(&windowTab, drawWindow, tabPadding + i * (tabSize + tabPadding), 0, tabSize, tabSize, TEXT_ALIGN_TOPLEFT);
		windowDrawSprite(&windowTab, TEXT_ALIGN_MIDDLECENTER, 0, 0, tabSize, tabSize, page.TexId, color, TEXT_ALIGN_MIDDLECENTER);
	}
	gfxDoGifPaging();

	windowBorder(drawWindow, borderColor, 1, 1, 1, 0);
}

//--------------------------------------------------------------------------
void storeDrawFooter(Moby *moby, int localPlayerIndex, Window_t *drawWindow)
{
	const u32 bgSolidColor = 0x80000000;
	const u32 textColor = 0x80FFFFFF;
	char strBuf[128];

	// draw bg
	windowFill(drawWindow, bgSolidColor);

	// get selected item
	SurvivalItemDef_t *selectedItem = NULL;

	snprintf(strBuf, sizeof(strBuf), "\x14 \x15 PAGE    \x10 BUY    \x12 CLOSE");
	windowDrawText(drawWindow, TEXT_ALIGN_MIDDLELEFT, 2, 0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLELEFT);
}

//--------------------------------------------------------------------------
void storeDrawMenu(Moby *moby, int localPlayerIndex)
{
	u32 bgColor = 0x60000000;
	u32 bgSolidColor = 0x80000000;
	u32 borderColor = 0x80000020;
	u32 textColor = 0x80FFFFFF;
	Window_t drawWindow;
	const float headerHeight = 24;
	const float tabRowHeight = 18;
	const float itemListHeight = 140;
	const float descHeight = 16;
	const float footerHeight = 16;
	const float totalHeight = headerHeight + tabRowHeight + itemListHeight + descHeight + footerHeight;
	const float descPaddingX = 5;
	char nameBuf[32];
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	struct StorePageDef page;
	SurvivalItemDef_t item;

	// get page
	int pageCount = storeGetPageCount(moby, localPlayerIndex);
	int currPage = (int)clamp(pvars->PageIdx[localPlayerIndex], 0, pageCount);
	storeGetPage(moby, localPlayerIndex, currPage, &page);

	// get selected item
	int currRow = pvars->RowIdx[localPlayerIndex];
	if (currRow < page.ItemsCount)
		storeGetItem(moby, localPlayerIndex, page.Items[currRow], &item);

	// get name
	storeGetName(moby, localPlayerIndex, nameBuf, sizeof(nameBuf));

	// setup draw state
	windowCreate(&drawWindow, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, 350, totalHeight, TEXT_ALIGN_MIDDLECENTER);
	windowSetScreen(&drawWindow, localPlayerIndex);

	// draw frame
	windowFill(&drawWindow, bgColor);

	// draw title text
	Window_t windowHeader;
	windowCreateFrom(&windowHeader, &drawWindow, 0, 0, drawWindow.Width, headerHeight, TEXT_ALIGN_TOPCENTER);
	windowDrawText(&windowHeader, TEXT_ALIGN_MIDDLECENTER, 0, 0, 1.1, textColor, nameBuf, -1, TEXT_ALIGN_MIDDLECENTER);

	// draw tabs
	Window_t drawWindowTabs;
	windowCreateFrom(&drawWindowTabs, &drawWindow, 0, headerHeight, drawWindow.Width, tabRowHeight, TEXT_ALIGN_TOPLEFT);
	storeDrawTabs(moby, localPlayerIndex, &drawWindowTabs, pvars->PageIdx[localPlayerIndex]);

	// draw item list
	Window_t drawWindowItemList;
	windowCreateFrom(&drawWindowItemList, &drawWindow, 0, tabRowHeight + headerHeight, drawWindow.Width, itemListHeight, TEXT_ALIGN_TOPLEFT);
	storeDrawItemList(moby, localPlayerIndex, &drawWindowItemList, pvars->RowIdx[localPlayerIndex], &page);

	// draw selected item decription
	Window_t windowDesc;
	windowCreateFrom(&windowDesc, &drawWindow, 0, tabRowHeight + headerHeight + itemListHeight, drawWindow.Width, descHeight, TEXT_ALIGN_TOPCENTER);
	windowFill(&windowDesc, bgColor);
	windowDrawText(&windowDesc, TEXT_ALIGN_MIDDLELEFT, descPaddingX, 0, 0.6, textColor, item.Description, -1, TEXT_ALIGN_MIDDLELEFT);
	windowDrawText(&windowDesc, TEXT_ALIGN_MIDDLERIGHT, -descPaddingX, 0, 0.6, textColor, ITEM_TYPE_NAMES[item.Type], -1, TEXT_ALIGN_MIDDLERIGHT);

	// draw footer
	Window_t windowFooter;
	windowCreateFrom(&windowFooter, &drawWindow, 0, 0, drawWindow.Width, footerHeight, TEXT_ALIGN_BOTTOMCENTER);
	storeDrawFooter(moby, localPlayerIndex, &windowFooter);

	// draw border
	windowBorder(&drawWindow, borderColor, 1, 1, 1, 1);
}

//--------------------------------------------------------------------------
void storeDraw(Moby *moby)
{
	// draw menu for locals
	int i;
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		int isMenuOpen = storeGetIsMenuOpen(moby, i);
		if (!playerIsValid(player) || !isMenuOpen)
			continue;

		storeDrawMenu(moby, i);
	}
}

//--------------------------------------------------------------------------
void storeHandleInput(Moby *moby, int localPlayerIndex)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;

	// disable input
	Player *player = playerGetFromSlot(localPlayerIndex);
	player->timers.noInput = 2;
	player->timers.noCamInputTimer = 2;

	// close store if start menu is opened or store is disabled
	if (gameIsStartMenuOpen(localPlayerIndex) || moby->State != STORE_STATE_ENABLED)
	{
		storeSetIsMenuOpen(moby, localPlayerIndex, 0);
		return;
	}

	// close store
	if (padGetButtonDown(localPlayerIndex, PAD_TRIANGLE) > 0)
	{
		storeSetIsMenuOpen(moby, localPlayerIndex, 0);
		uiPlaySound(UI_SOUND_ID_CLOSE_MENU_DECLINE, 0);
		return;
	}
	else if (padGetButtonDown(localPlayerIndex, PAD_UP) > 0)
	{
		int currRow = pvars->RowIdx[localPlayerIndex];
		if (currRow > 0)
		{
			pvars->RowIdx[localPlayerIndex]--;
			uiPlaySound(UI_SOUND_ID_NAV_UP_DOWN, 0);
		}
	}
	else if (padGetButtonDown(localPlayerIndex, PAD_DOWN) > 0)
	{
		int currPage = pvars->PageIdx[localPlayerIndex];
		struct StorePageDef page;
		if (!storeGetPage(moby, localPlayerIndex, currPage, &page))
			return;

		int currRow = pvars->RowIdx[localPlayerIndex];
		if (currRow < (page.ItemsCount - 1))
		{
			pvars->RowIdx[localPlayerIndex]++;
			uiPlaySound(UI_SOUND_ID_NAV_UP_DOWN, 0);
		}
	}
	else if (padGetButtonDown(localPlayerIndex, PAD_L1) > 0)
	{
		int currPage = pvars->PageIdx[localPlayerIndex];
		if (currPage > 0)
		{
			pvars->RowIdx[localPlayerIndex] = 0;
			pvars->PageIdx[localPlayerIndex]--;
			uiPlaySound(UI_SOUND_ID_NAV_UP_DOWN, 0);
		}
	}
	else if (padGetButtonDown(localPlayerIndex, PAD_R1) > 0)
	{
		int pageCount = storeGetPageCount(moby, localPlayerIndex);
		int currPage = pvars->PageIdx[localPlayerIndex];
		if (currPage < (pageCount - 1))
		{
			pvars->RowIdx[localPlayerIndex] = 0;
			pvars->PageIdx[localPlayerIndex]++;
			uiPlaySound(UI_SOUND_ID_NAV_UP_DOWN, 0);
		}
	}
	else if (padGetButtonDown(localPlayerIndex, PAD_CROSS) > 0)
	{
		int currPage = pvars->PageIdx[localPlayerIndex];
		struct StorePageDef page;
		if (!storeGetPage(moby, localPlayerIndex, currPage, &page))
			return;

		int currRow = pvars->RowIdx[localPlayerIndex];
		if (currRow >= page.ItemsCount)
			return;

		int itemIdx = page.Items[currRow];
		switch (storeCanBuyItem(moby, localPlayerIndex, itemIdx))
		{
		case STORE_ITEM_CAN_BUY_BAD_STATE:
		case STORE_ITEM_CAN_BUY_BAD_ITEM:
		case STORE_ITEM_CAN_BUY_ITEM_DISABLED:
		case STORE_ITEM_CAN_BUY_HAVE_TOO_MANY:
		case STORE_ITEM_CAN_BUY_NOT_ENOUGH_BOLTS:
			uiPlaySound(UI_SOUND_ID_BAD_SELECT, 0);
			break;
		case STORE_ITEM_CAN_BUY_GOOD:
			uiPlaySound(UI_SOUND_ID_SELECT, 0);
			storePlayerBuy(moby, localPlayerIndex, itemIdx);
			break;
		}
	}
}

//--------------------------------------------------------------------------
int storeTryInteract(Moby *moby, Player *player, char *buf)
{
	VECTOR storePosition;
	VECTOR playerToStore;
	VECTOR cameraToStore;
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	float maxDistSqr = STORE_MAX_DIST * STORE_MAX_DIST;

	vector_copy(storePosition, moby->Position);
	if (pvars->BoundMoby && !mobyIsDestroyed(pvars->BoundMoby))
		vector_scale(storePosition, pvars->BoundMoby->BSphere, 1 / 1024.0);

	vector_subtract(playerToStore, player->PlayerPosition, storePosition);
	vector_subtract(cameraToStore, player->CameraPos, storePosition);
	switch (pvars->InteractType)
	{
	case STORE_INTERACT_NEAR:
		if (vector_sqrmag(playerToStore) > maxDistSqr)
			return 0;
		break;
	case STORE_INTERACT_LOOK_AT:
		if (vector_sqrmag(playerToStore) > maxDistSqr)
			return 0;
		if (vector_innerproduct(cameraToStore, player->CameraForward) > -0.9)
			return 0;
		break;
	case STORE_INTERACT_STAND_ON:
		if (player->Ground.pMoby != moby && player->Ground.pMoby != pvars->BoundMoby)
			return 0;
		break;
	}

	return tryPlayerInteract(moby, player, buf, NULL, 0, 0, PLAYER_STORE_COOLDOWN_TICKS, 100, PAD_TRIANGLE, 0);
}

//--------------------------------------------------------------------------
void storeUpdate(Moby *moby)
{
	Player **players = playerGetAll();
	int i;
	char buf[64];
	if (!moby || !moby->PVar)
		return;

	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;

	// set state between rounds
	if (pvars->DisableDuringRound)
	{
		if (MapConfig.State && MapConfig.State->RoundCompleteTime)
		{
			if (moby->State != STORE_STATE_ENABLED)
			{
				// show bound moby
				storeSetState(moby, STORE_STATE_ENABLED);
			}
		}
		else
		{
			if (moby->State != STORE_STATE_DISABLED)
			{
				storeSetState(moby, STORE_STATE_DISABLED);
			}
		}
	}

	// post draw
	// has glitches so moved to frame tick
	// gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&storeDraw, moby);

	// map icon
	if (pvars->AppearOnRadar && moby->State == STORE_STATE_ENABLED)
	{
		int blipIdx = radarGetBlipIndex(moby);
		if (blipIdx >= 0)
		{
			RadarBlip *blip = radarGetBlips() + blipIdx;
			blip->X = moby->Position[0];
			blip->Y = moby->Position[1];
			blip->Life = 0x1F;
			blip->Type = pvars->RadarBlipType;
			blip->Team = pvars->RadarBlipTeam;
		}
	}

	// check for any locals that are nearby
	// prompt to open menu
	for (i = 0; i < GAME_MAX_LOCALS; ++i)
	{
		Player *player = playerGetFromSlot(i);
		int isMenuOpen = storeGetIsMenuOpen(moby, i);
		if (!playerIsValid(player))
			continue;

		// get name
		char nameBuf[32];
		storeGetName(moby, i, nameBuf, sizeof(nameBuf));

		// prompt to open menu
		if (!isMenuOpen)
		{
			if (moby->State == STORE_STATE_ENABLED && localPlayerHasInput())
			{
				snprintf(buf, sizeof(buf), "\x12 Open %s", nameBuf);
				if (storeTryInteract(moby, player, buf))
				{
					storeSetIsMenuOpen(moby, i, 1);
				}
			}
			else if (moby->State == STORE_STATE_DISABLED)
			{
				snprintf(buf, sizeof(buf), "%s Closed", nameBuf);
				storeTryInteract(moby, player, buf);
			}
		}
		else
		{
			storeHandleInput(moby, i);
		}
	}
}

//--------------------------------------------------------------------------
int storeHandleEvent_SetState(Moby *moby, GuberEvent *event)
{
	int state;
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;
	if (!pvars)
		return 0;

	guberEventRead(event, &state, 4);

	// set state
	mobySetState(moby, state, -1);
	return 0;
}

//--------------------------------------------------------------------------
struct GuberMoby *storeGetGuber(Moby *moby)
{
	if (moby->OClass == STORE_MOBY_OCLASS && moby->PVar)
		return moby->GuberMoby;

	return 0;
}

//--------------------------------------------------------------------------
int storeHandleEvent(Moby *moby, GuberEvent *event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == STORE_MOBY_OCLASS && moby->PVar)
	{
		u32 storeEvent = event->NetEvent.EventID;

		switch (storeEvent)
		{
		// case STORE_EVENT_SPAWN: return storeHandleEvent_Spawned(moby, event);
		case STORE_EVENT_SET_STATE:
			return storeHandleEvent_SetState(moby, event);
		default:
		{
			DPRINTF("unhandle store event %d\n", storeEvent);
			break;
		}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void storeOnGuberCreated(Moby *moby, struct StoreVTable *defaultVTable)
{
	struct StorePVar *pvars = (struct StorePVar *)moby->PVar;

	moby->PUpdate = &storeUpdate;
	moby->UpdateDist = -1;
	moby->DrawDist = 64;
	moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;

	// copy vtable
	if (defaultVTable)
	{
		memcpy(&pvars->VTable, defaultVTable, sizeof(struct StoreVTable));
	}

	// add bound moby
	// should be stored initially in pvars exported from forge
	// as a moby idx
	Moby *boundMoby = pvars->BoundMoby = mobyGetFromIdxOrNull((int)pvars->BoundMoby);
	if (boundMoby)
	{
		// teleport store to bound moby
		vector_copy(moby->Position, boundMoby->Position);
	}

	// set default state
	mobySetState(moby, STORE_STATE_ENABLED, -1);
}

//--------------------------------------------------------------------------
void storeFrameTick(void)
{
	int i;
	for (i = 0; i < COUNT_OF(StoreMobys); ++i)
	{
		if (!StoreMobys[i])
			continue;

		storeDraw(StoreMobys[i]);
	}
}

//--------------------------------------------------------------------------
void storeInit(struct StoreVTable *defaultVTable)
{
	Moby *testMoby = mobySpawn(STORE_MOBY_OCLASS, 0);
	if (testMoby)
	{
		u32 mobyFunctionsPtr = (u32)mobyGetFunctions(testMoby);
		if (mobyFunctionsPtr)
		{
			mapInstallMobyFunctions(mobyFunctionsPtr);
		}

		mobyDestroy(testMoby);
	}

	// create gubers for stores
	int count = 0;
	Moby *moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, STORE_MOBY_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar)
		{
			struct Guber *guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
			DPRINTF("found store %08X %08X\n", (u32)moby, (u32)guber);
			if (guber)
			{
				storeOnGuberCreated(moby, defaultVTable);
				if (count < COUNT_OF(StoreMobys))
					StoreMobys[count] = moby;
				++count;
			}
		}

		++moby;
	}
}
