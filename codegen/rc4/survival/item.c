#include <tamtypes.h>
#include <libdl/color.h>
#include <libdl/stdio.h>
#include <libdl/ui.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include "item.h"
#include "utils.h"
#include "window.h"
#include "maputils.h"

static int itemCurrentDrawnItemIdx[GAME_MAX_LOCALS] = {-1};
static u32 itemCooldownTicks[GAME_MAX_PLAYERS][MAX_ITEM_COUNT] = {};

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

	// set cooldown
	itemCooldownTicks[playerId][itemIdx] = itemGetCooldownTicks(playerId, itemIdx);

	// subtract from player item count
	switch (MapConfig.ItemDefs[itemIdx].Type)
	{
		// these don't get removed on consumption
	case SURVIVAL_ITEM_PASSIVE:
		break;
		// remove on consumption by default
	default:
		MapConfig.State->PlayerStates[playerId].State.ItemCounts[itemIdx] -= 1;
		break;
	}

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

	// don't check cooldown as effect is immediate
	// maybe should think about how cooldown could work with immediate consumables
	if (player->IsLocal && def->Type == SURVIVAL_ITEM_CONSUMABLE_IMMEDIATE)
		itemBeginConsume(player->PlayerId, itemId);
}

//--------------------------------------------------------------------------
void itemShowMessage(int localPlayerIndex, int itemIdx, char *format, int ticks)
{
	// show popup
	char buf[64];
	snprintf(buf, sizeof(buf), format, MapConfig.ItemDefs[itemIdx].Name);
	pushSnack(localPlayerIndex, buf, ticks);
}

//--------------------------------------------------------------------------
void itemGetDescription(char *buf, int size, int itemIdx, int playerId)
{
	SurvivalItemDef_t *item = &MapConfig.ItemDefs[itemIdx];
	safe_strcpy(buf, item->Description, size);
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
int itemCanConsume(Player *player, int itemIdx)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	// SurvivalItemDef_t *item = &MapConfig.ItemDefs[itemIdx];
	if (playerGetItemCount(player, itemIdx) <= 0)
		return 0;

	if (itemCooldownTicks[player->PlayerId][itemIdx] > 0)
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
u32 itemGetCooldownTicks(int playerId, int itemIdx)
{
	if (itemIdx < 0 || itemIdx >= MapConfig.ItemDefCount)
		return 0;

	SurvivalItemDef_t *item = &MapConfig.ItemDefs[itemIdx];
	if (item->VTable.GetConsumeCooldownTicksFunc)
		return item->VTable.GetConsumeCooldownTicksFunc(itemIdx, item, playerId);

	return item->ConsumeCooldownTicks;
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
void itemDrawIcon(Window_t *window, Player *player, int itemIdx, float texDim, float countTextScale, float opacity)
{
	const u32 colorTexFaded = 0x20000000; // faded black
	const u32 colorCount = 0x8000FFFF;		// yellow
	const u32 colorCooldown = 0x80FFFFFF; // white
	char buf[32];

	SurvivalItemDef_t *def = &MapConfig.ItemDefs[itemIdx];
	int count = playerGetItemCount(player, itemIdx);
	int canConsume = itemCanConsume(player, itemIdx);
	float opacityT = (canConsume ? 1 : 0.5) * opacity;
	u32 colorTexBg = colorLerp(0, colorTexFaded, opacityT);
	u32 colorTexFg = colorLerp(0, def->TexColor, opacityT);
	u32 cooldownTicks = itemGetCooldownTicks(player->PlayerId, itemIdx);
	float consumeBarPerc = 0;
	if (cooldownTicks > 0)
		consumeBarPerc = clamp(itemCooldownTicks[player->PlayerId][itemIdx] / (float)cooldownTicks, 0, 1);

	// draw icon
	windowDrawSprite(window, TEXT_ALIGN_MIDDLECENTER, 1, 1, texDim, texDim, def->TexId, colorTexBg, TEXT_ALIGN_MIDDLECENTER);
	windowDrawSprite(window, TEXT_ALIGN_MIDDLECENTER, 0, 0, texDim, texDim, def->TexId, colorTexFg, TEXT_ALIGN_MIDDLECENTER);

	// draw count
	if (count > 1 && countTextScale > 0)
	{
		snprintf(buf, sizeof(buf), "%d", count);
		windowDrawText(window, TEXT_ALIGN_BOTTOMCENTER, 0, 0, countTextScale, colorCount, buf, -1, TEXT_ALIGN_TOPCENTER);
	}

	// draw cooldown
	if (consumeBarPerc > 0)
	{
		windowDrawBox(window, TEXT_ALIGN_BOTTOMLEFT, 0, 0, ceilf(maxf(1, texDim - 2) * consumeBarPerc), 2, colorCooldown, TEXT_ALIGN_MIDDLELEFT);
	}
}

//--------------------------------------------------------------------------
void itemDraw(void)
{
	char buf[16];

	gfxSetupGifPaging(0);

	// call item OnDraw func
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
		if (gameIsStartMenuOpen(l))
			continue;

		int activeIdx = itemFindNextManualConsumable(l, itemCurrentDrawnItemIdx[l]);
		Player *player = playerGetFromSlot(l);
		if (!playerIsValid(player))
			continue;

		// draw
		if (activeIdx >= 0)
		{
			itemCurrentDrawnItemIdx[l] = activeIdx;
			// SurvivalItemDef_t *def = &MapConfig.ItemDefs[activeIdx];
			// int count = playerGetItemCount(player, activeIdx);
			// int canConsume = itemCanConsume(player, i);
			u32 cooldownTicks = itemGetCooldownTicks(player->PlayerId, i);
			float consumeBarPerc = 0;
			if (cooldownTicks > 0)
				consumeBarPerc = clamp(itemCooldownTicks[player->PlayerId][i] / (float)cooldownTicks, 0, 1);

			// create window
			Window_t itemWindow;
			windowCreate(&itemWindow, 20, SCREEN_HEIGHT - 15, 0, 0, 32, 32, TEXT_ALIGN_BOTTOMLEFT);
			windowSetScreen(&itemWindow, l);
			itemDrawIcon(&itemWindow, player, activeIdx, 32, 0.7, 1);

			// draw use button
			windowDrawText(&itemWindow, TEXT_ALIGN_BOTTOMLEFT, -2, 0, 0.7, 0x80FFFFFF, "\x1C", 1, TEXT_ALIGN_BOTTOMRIGHT);

			// draw next item
			int nextItem = itemFindNextManualConsumable(l, activeIdx + 1);
			if (nextItem >= 0 && nextItem != activeIdx)
			{
				// create window
				windowCreate(&itemWindow, itemWindow.AnchorPoint[0], itemWindow.AnchorPoint[1], itemWindow.Width + 2, 5, 16, 16, TEXT_ALIGN_BOTTOMLEFT);
				windowSetScreen(&itemWindow, l);
				itemDrawIcon(&itemWindow, player, nextItem, 16, 0, 0.9);
			}
		}

		itemCurrentDrawnItemIdx[l] = activeIdx;
	}

	// draw passive/auto consumables
	for (l = 0; l < GAME_MAX_LOCALS; ++l)
	{
		if (gameIsStartMenuOpen(l))
			continue;

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
		windowSetScreen(&itemsWindow, l);

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

			// draw item
			Window_t itemWindow;
			windowCreateFrom(&itemWindow, &itemsWindow, x, 0, texWidth + pad * 2, texWidth, TEXT_ALIGN_TOPLEFT);
			itemDrawIcon(&itemWindow, player, i, texWidth, countScale, 1);

			// move along
			x += itemWindow.Width + 4;

			// wrap
			if (x >= itemsWindow.Width)
			{
				x = 0;
				itemsWindow.WindowPoint[1] -= texWidth + 10;
			}
		}
	}

	gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void itemTick(void)
{
	int i, j;

	// dec cooldown ticks
	for (i = 0; i < GAME_MAX_PLAYERS; ++i)
		for (j = 0; j < MapConfig.ItemDefCount; ++j)
			decTimerU32(&itemCooldownTicks[i][j]);

	// tick items
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
		else if (padGetButtonDown(l, PAD_DOWN) > 0 && itemCanConsume(player, itemIdx))
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
