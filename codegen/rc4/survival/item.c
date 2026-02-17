#include <tamtypes.h>
#include <libdl/color.h>
#include <libdl/stdio.h>
#include "item.h"
#include "utils.h"
#include "window.h"
#include "maputils.h"

static int itemCurrentDrawnItemIdx[GAME_MAX_LOCALS] = {-1};

//--------------------------------------------------------------------------
void itemBeginAcquire(int playerId, int itemIdx)
{
	if (!MapConfig.State)
		return;

	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return;

	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return;

	// give player
	MapConfig.State->PlayerStates[playerId].State.ItemCounts[itemIdx] += 1;

	// raise event
	if (!MapConfig.Functions.ModeSendOnPlayerItemAcquiredFunc)
		return;

	MapConfig.Functions.ModeSendOnPlayerItemAcquiredFunc(playerId, itemIdx);
}

//--------------------------------------------------------------------------
void itemBeginConsume(int playerId, int itemIdx)
{
	if (!MapConfig.State)
		return;

	if (playerId < 0 || playerId >= GAME_MAX_PLAYERS)
		return;

	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return;

	int count = MapConfig.State->PlayerStates[playerId].State.ItemCounts[itemIdx];
	if (count <= 0)
		return;

	// subtract from player item count
	MapConfig.State->PlayerStates[playerId].State.ItemCounts[itemIdx] -= 1;

	// raise event
	if (!MapConfig.Functions.ModeSendOnPlayerItemConsumedFunc)
		return;

	MapConfig.Functions.ModeSendOnPlayerItemConsumedFunc(playerId, itemIdx);
}

//--------------------------------------------------------------------------
void itemOnConsumeTriggered(Player *player, int itemId)
{
	if (itemId < 0 || itemId >= MapConfig.ItemDefCount)
		return;

	SurvivalItemDef_t *def = &MapConfig.ItemDefs[itemId];
	if (!def->VTable.OnConsumedFunc)
		return;

	def->VTable.OnConsumedFunc(itemId, def, player->PlayerId);
}

//--------------------------------------------------------------------------
void itemOnAcquireTriggered(Player *player, int itemId)
{
	if (itemId < 0 || itemId >= MapConfig.ItemDefCount)
		return;

	SurvivalItemDef_t *def = &MapConfig.ItemDefs[itemId];
	if (def->VTable.OnAcquiredFunc)
		def->VTable.OnAcquiredFunc(itemId, def, player->PlayerId);

	if (player->IsLocal && def->Type == SURVIVAL_ITEM_CONSUMABLE_IMMEDIATE)
		itemBeginConsume(player->PlayerId, itemId);
}

//--------------------------------------------------------------------------
void itemShowAcquired(int localPlayerIndex, int itemIdx, char *verb)
{
	// show popup
	char buf[64];
	snprintf(buf, sizeof(buf), "%s %s!", verb ? verb : "Got", MapConfig.ItemDefs[itemIdx].Name);
	pushSnack(localPlayerIndex, buf, 60);
}

//--------------------------------------------------------------------------
int itemCanAcquire(Player *player, int itemIdx)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	SurvivalItemDef_t *item = &MapConfig.ItemDefs[itemIdx];
	if (item->VTable.HasRoomForMoreFunc && !item->VTable.HasRoomForMoreFunc(itemIdx, item, player->PlayerId))
		return 0;
	if (item->MaxHeldAtOnce > 0 && playerGetItemCount(player, itemIdx) >= item->MaxHeldAtOnce)
		return 0;

	return 1;
}

//--------------------------------------------------------------------------
u32 itemGetCost(int localPlayerIndex, int itemIdx)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	int count = 0;
	if (MapConfig.State)
		count = MapConfig.State->StorePurchaseCount[localPlayerIndex][itemIdx];

	Player *player = playerGetFromSlot(localPlayerIndex);
	SurvivalItemDef_t *item = &MapConfig.ItemDefs[itemIdx];

	// use vtable if it exists
	if (item->VTable.GetStoreCostFunc)
		return item->VTable.GetStoreCostFunc(itemIdx, item, NULL, player->PlayerId, count);

	// compute cost
	u32 cost = item->StoreCost;
	switch (item->StoreCostType)
	{
	case SURVIVAL_ITEM_STORE_COST_BOLTS_LINEAR:
	case SURVIVAL_ITEM_STORE_COST_TOKENS_LINEAR:
		cost += item->StoreCostIncrease.Linear * count;
		break;
	case SURVIVAL_ITEM_STORE_COST_BOLTS_EXPONENTIAL:
	case SURVIVAL_ITEM_STORE_COST_TOKENS_EXPONENTIAL:
		cost *= powf(item->StoreCostIncrease.Exponential, count);
		break;
	}

	return cost;
}

//--------------------------------------------------------------------------
int itemGetPlayerBankAmount(int localPlayerIndex, SurvivalItemDef_t *item)
{
	Player *player = playerGetFromSlot(localPlayerIndex);
	if (!playerIsValid(player))
		return 0;

	switch (item->StoreCostType)
	{
	case SURVIVAL_ITEM_STORE_COST_BOLTS_LINEAR:
	case SURVIVAL_ITEM_STORE_COST_BOLTS_EXPONENTIAL:
		return MapConfig.State->PlayerStates[player->PlayerId].State.Bolts;
	case SURVIVAL_ITEM_STORE_COST_TOKENS_LINEAR:
	case SURVIVAL_ITEM_STORE_COST_TOKENS_EXPONENTIAL:
		return MapConfig.State->PlayerStates[player->PlayerId].State.CurrentTokens;
	}

	return 0;
}

//--------------------------------------------------------------------------
int itemChargePlayerBank(int localPlayerIndex, int itemIdx)
{
	Player *player = playerGetFromSlot(localPlayerIndex);
	if (!playerIsValid(player))
		return 0;

	SurvivalItemDef_t *item = &MapConfig.ItemDefs[itemIdx];
	u32 cost = itemGetCost(localPlayerIndex, itemIdx);

	// log
	if (MapConfig.State)
		MapConfig.State->StorePurchaseCount[localPlayerIndex][itemIdx]++;

	switch (item->StoreCostType)
	{
	case SURVIVAL_ITEM_STORE_COST_BOLTS_LINEAR:
	case SURVIVAL_ITEM_STORE_COST_BOLTS_EXPONENTIAL:
		MapConfig.State->PlayerStates[player->PlayerId].State.Bolts -= cost;
		return 1;
	case SURVIVAL_ITEM_STORE_COST_TOKENS_LINEAR:
	case SURVIVAL_ITEM_STORE_COST_TOKENS_EXPONENTIAL:
		MapConfig.State->PlayerStates[player->PlayerId].State.CurrentTokens -= cost;
		return 1;
	}

	return 0;
}

//--------------------------------------------------------------------------
int itemFindNextManualConsumable(int localPlayerIndex, int startItemIdx)
{
	int i;
	Player *player = playerGetFromSlot(localPlayerIndex);
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		int idx = (i + startItemIdx) % MapConfig.ItemDefCount;
		SurvivalItemDef_t *def = &MapConfig.ItemDefs[idx];
		if (def->Type != SURVIVAL_ITEM_CONSUMABLE_MANUAL)
			continue;

		if (playerGetItemCount(player, idx) == 0)
			continue;

		return idx;
	}

	return -1;
}

//--------------------------------------------------------------------------
float itemGetMysteryboxChanceWeight(int itemIdx, SurvivalItemDef_t *itemDef, int playerId)
{
	float itemWeight = itemDef->MysteryboxChanceWeight;
	if (itemDef->VTable.GetMysteryboxChanceFunc)
		itemWeight = itemDef->VTable.GetMysteryboxChanceFunc(itemIdx, itemDef, playerId);

	return itemWeight;
}

//--------------------------------------------------------------------------
float itemGetDropChanceWeight(int itemIdx, SurvivalItemDef_t *itemDef, int playerId)
{
	float itemWeight = itemDef->DropChanceWeight;
	if (itemDef->VTable.GetDropChanceFunc)
		itemWeight = itemDef->VTable.GetDropChanceFunc(itemIdx, itemDef, playerId);

	return itemWeight;
}

//--------------------------------------------------------------------------
float itemGetVendorRewardChanceWeight(int itemIdx, SurvivalItemDef_t *itemDef, int playerId)
{
	float itemWeight = itemDef->VendorRewardChanceWeight;
	if (itemDef->VTable.GetVendorRewardChanceFunc)
		itemWeight = itemDef->VTable.GetVendorRewardChanceFunc(itemIdx, itemDef, playerId);

	return itemWeight;
}

//--------------------------------------------------------------------------
void itemDrawIcon(int localPlayerIndex, float x, float y, float w, float h, float fade, SurvivalItemDef_t *def)
{
	// draw item icon
	u32 color = colorLerp(def->TexColor, 0x00ffffff, fade);
	u64 tex = gfxGetFrameTex(def->TexId);
	int texW = gfxGetTexWidth(tex);
	int texH = gfxGetTexHeight(tex);
	transformToSplitscreenPixelCoordinates(localPlayerIndex, &x, &y);
	gfxHelperDrawSprite(x, y, 0, 0, w, h, texW, texH, def->TexId, color, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void itemDraw(void)
{
	char buf[16];
	const u32 colorTexFaded = 0x20000000;

	gfxSetupGifPaging(0);

	// draw items
	int i;
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		SurvivalItemDef_t *def = &MapConfig.ItemDefs[i];
		if (!def->VTable.DrawUpdateFunc)
			continue;

		def->VTable.DrawUpdateFunc(i, def);
	}

	// draw manual consumables
	int l;
	for (l = 0; l < GAME_MAX_LOCALS; ++l)
	{
		int activeIdx = itemFindNextManualConsumable(l, itemCurrentDrawnItemIdx[l]);
		Player *player = playerGetFromSlot(l);
		if (!playerIsValid(player))
			continue;

		// draw
		if (activeIdx >= 0)
		{
			itemCurrentDrawnItemIdx[l] = activeIdx;
			SurvivalItemDef_t *def = &MapConfig.ItemDefs[activeIdx];
			int count = playerGetItemCount(player, activeIdx);

			// draw use button
			gfxHelperDrawText(20 + 16, SCREEN_HEIGHT - 15, 0, 0, 0.7, 0x80FFFFFF, "\x1C", 1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);

			// draw active icon
			itemDrawIcon(l, 20, SCREEN_HEIGHT - 50, 32, 32, 0, def);
			if (count > 1)
			{
				snprintf(buf, sizeof(buf), "%d", count);
				gfxHelperDrawText(20 + 32, SCREEN_HEIGHT - 15, 0, 0, 0.7, 0x8000FFFF, buf, -1, TEXT_ALIGN_BOTTOMRIGHT, COMMON_DZO_DRAW_NORMAL);
			}

			// draw next item
			int nextItem = itemFindNextManualConsumable(l, activeIdx + 1);
			if (nextItem >= 0 && nextItem != activeIdx)
				itemDrawIcon(l, 55, SCREEN_HEIGHT - 32, 16, 16, 0.5, &MapConfig.ItemDefs[nextItem]);
		}

		itemCurrentDrawnItemIdx[l] = activeIdx;
	}

	// draw passive/auto consumables
	for (l = 0; l < GAME_MAX_LOCALS; ++l)
	{
		Player *player = playerGetFromSlot(l);
		if (!playerIsValid(player))
			continue;

		// count how many to draw
		int drawCount = 0;
		float texWidth = 16;
		float drawWidth = 0;
		float countScale = 0.6;
		for (i = 0; i < MapConfig.ItemDefCount; ++i)
		{
			SurvivalItemDef_t *def = &MapConfig.ItemDefs[i];
			if (def->Type != SURVIVAL_ITEM_CONSUMABLE_AUTO && def->Type != SURVIVAL_ITEM_PASSIVE)
				continue;

			int count = playerGetItemCount(player, i);
			if (count <= 0)
				continue;

			++drawCount;
			snprintf(buf, sizeof(buf), "%d", count);
			float pad = maxf(floorf(gfxGetFontWidth(buf, -1, countScale) - texWidth), 0);
			drawWidth += texWidth + (pad * 2) + 4;
		}

		// create window
		Window_t itemsWindow;
		windowCreate(&itemsWindow, SCREEN_WIDTH / 2, SCREEN_HEIGHT - texWidth - 12, 0, 0, minf(drawWidth, SCREEN_WIDTH - 200), texWidth, TEXT_ALIGN_TOPCENTER);

		// draw
		float x = 0;
		for (i = 0; i < MapConfig.ItemDefCount; ++i)
		{
			SurvivalItemDef_t *def = &MapConfig.ItemDefs[i];
			if (def->Type != SURVIVAL_ITEM_CONSUMABLE_AUTO && def->Type != SURVIVAL_ITEM_PASSIVE)
				continue;

			int count = playerGetItemCount(player, i);
			if (count <= 0)
				continue;

			// get width padding
			snprintf(buf, sizeof(buf), "%d", count);
			float countStrWidth = gfxGetFontWidth(buf, -1, countScale);
			float pad = maxf(floorf(countStrWidth - texWidth), 0);

			// build item window
			Window_t itemWindow;
			windowCreateFrom(&itemWindow, &itemsWindow, x, 0, texWidth + pad * 2, texWidth, TEXT_ALIGN_TOPLEFT);
			windowSetScreen(&itemWindow, l);

			// draw active icon
			windowDrawSprite(&itemWindow, TEXT_ALIGN_MIDDLECENTER, 1, 1, texWidth, texWidth, def->TexId, colorTexFaded, TEXT_ALIGN_MIDDLECENTER);
			windowDrawSprite(&itemWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, texWidth, texWidth, def->TexId, def->TexColor, TEXT_ALIGN_MIDDLECENTER);
			if (count > 1)
				windowDrawText(&itemWindow, TEXT_ALIGN_BOTTOMCENTER, 0, -2, countScale, 0x8000FFFF, buf, -1, TEXT_ALIGN_TOPCENTER);

			// move along
			x += itemWindow.Width + 4;

			// wrap
			if (x >= itemsWindow.Width)
			{
				x = 0;
				itemsWindow.WindowPoint[1] -= texWidth + 6;
			}
		}
	}

	gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void itemTick(void)
{
	// tick items
	int i;
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		SurvivalItemDef_t *def = &MapConfig.ItemDefs[i];
		if (!def->VTable.TickUpdateFunc)
			continue;

		def->VTable.TickUpdateFunc(i, def);
	}

	// only when we have input
	if (!localPlayerHasInput())
		return;

	// handle consumable input
	int l;
	for (l = 0; l < GAME_MAX_LOCALS; ++l)
	{
		int itemIdx = itemCurrentDrawnItemIdx[l];
		Player *player = playerGetFromSlot(l);
		if (padGetButtonDown(l, PAD_RIGHT) > 0)
		{
			int nextIdx = itemFindNextManualConsumable(l, itemIdx + 1);
			if (nextIdx >= 0)
				itemCurrentDrawnItemIdx[l] = nextIdx;
		}
		else if (padGetButtonDown(l, PAD_DOWN) > 0 && playerGetItemCount(player, itemIdx) > 0)
		{
			itemBeginConsume(player->PlayerId, itemIdx);
		}
	}
}

//--------------------------------------------------------------------------
void itemInit(void)
{
	// init items
	int i;
	for (i = 0; i < MapConfig.ItemDefCount; ++i)
	{
		SurvivalItemDef_t *def = &MapConfig.ItemDefs[i];
		if (!def->VTable.InitFunc)
			continue;

		def->VTable.InitFunc(i, def);
	}
}
