#include <string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/stdlib.h>
#include <libdl/color.h>
#include <libdl/moby.h>
#include <libdl/sound.h>
#include <libdl/random.h>
#include <libdl/utils.h>
#include <libdl/net.h>
#include <libdl/ui.h>
#include <libdl/hud.h>
#include <libdl/graphics.h>
#include "maputils.h"
#include "game.h"
#include "window.h"
#include "bank.h"
#include "inventory.h"
#include "common.h"

enum InventoryItemAction
{
  INVENTORY_ITEM_ACTION_NONE = 0,
  INVENTORY_ITEM_ACTION_SELL,
  INVENTORY_ITEM_ACTION_FAV,
  INVENTORY_ITEM_ACTION_SELECT,
};

extern struct RaidsState State;
extern u32 bankRarityColors[];
extern u32 bankPaintColors[];
extern char bankRarityCode[];
RaidsPlayerInventoryPage_t inventoryPage = {};
char inventorySellDialogFooter[] = "\x10 SELL        \x12 CANCEL";

InventoryDrawState_t inventoryDrawState = {
  .SelectedIdx = 0,
  .FilterIdx = 1,
  .ShowSellDialog = 0,
};

char* inventoryRarityNames[] = {
  [RAIDS_ITEM_RARITY_COMMON] "Common",
  [RAIDS_ITEM_RARITY_UNCOMMON] "Uncommon",
  [RAIDS_ITEM_RARITY_RARE] "Rare",
  [RAIDS_ITEM_RARITY_LEGENDARY] "Legendary",
  [RAIDS_ITEM_RARITY_MYTHIC] "Mythic"
};

char* inventoryWeaponModLevels[] = {
  [RAIDS_ITEM_RARITY_COMMON] "I",
  [RAIDS_ITEM_RARITY_UNCOMMON] "II",
  [RAIDS_ITEM_RARITY_RARE] "III",
  [RAIDS_ITEM_RARITY_LEGENDARY] "IV",
  [RAIDS_ITEM_RARITY_MYTHIC] "V"
};

char* inventoryPaintNames[] = {
  "None",
  "Blue",
  "Red",
  "Green",
  "Orange",
  "Yellow",
  "Purple",
  "Aqua",
  "Pink",
  "Olive",
  "Maroon",
};

char* inventoryPaintSpecialNames[] = {
  "None",
  "Glow",
  "Ghost",
  "Glow Ghost"
};

char* inventoryModNames[] = {
  [RAIDS_WEAPON_MOD_NONE] "None",
  [RAIDS_WEAPON_MOD_NAPALM] "Napalm",
  [RAIDS_WEAPON_MOD_TIME_BOMB] "Time Bomb",
  [RAIDS_WEAPON_MOD_FREEZE] "Freeze",
  [RAIDS_WEAPON_MOD_MINI_BOMB] "Mini Bomb",
  [RAIDS_WEAPON_MOD_MORPH] "Morph",
  [RAIDS_WEAPON_MOD_BRAINWASH] "Brainwash",
  [RAIDS_WEAPON_MOD_ACID] "Acid",
  [RAIDS_WEAPON_MOD_SHOCK] "Shock",
  [RAIDS_WEAPON_MOD_WILL_O_WISP] "Will-O-Wisp",
  [RAIDS_WEAPON_MOD_LIGHTFOOT] "Lightfoot",
};

char inventoryAlphaModSpriteIds[] = {
  [ALPHA_MOD_SPEED] 52,
  [ALPHA_MOD_AMMO] 38,
  [ALPHA_MOD_AIMING] 37,
  [ALPHA_MOD_IMPACT] 44,
  [ALPHA_MOD_AREA] 39,
  [ALPHA_MOD_XP] 41,
  [ALPHA_MOD_JACKPOT] 46,
  [ALPHA_MOD_NANOLEECH] 48,
};

char inventoryWeaponSpriteIds[] = {
  [WEAPON_SLOT_WRENCH] 111,
  [WEAPON_SLOT_VIPERS] 24,
  [WEAPON_SLOT_MAGMA_CANNON] 28,
  [WEAPON_SLOT_ARBITER] 27,
  [WEAPON_SLOT_FUSION_RIFLE] 29,
  [WEAPON_SLOT_MINE_LAUNCHER] 25,
  [WEAPON_SLOT_B6] 21,
  [WEAPON_SLOT_OMNI_SHIELD] 22,
  [WEAPON_SLOT_FLAIL] 23,
};

char inventoryWeaponSpriteDims[] = {
  [WEAPON_SLOT_WRENCH] 32,
  [WEAPON_SLOT_VIPERS] 64,
  [WEAPON_SLOT_MAGMA_CANNON] 64,
  [WEAPON_SLOT_ARBITER] 32,
  [WEAPON_SLOT_FUSION_RIFLE] 64,
  [WEAPON_SLOT_MINE_LAUNCHER] 32,
  [WEAPON_SLOT_B6] 32,
  [WEAPON_SLOT_OMNI_SHIELD] 32,
  [WEAPON_SLOT_FLAIL] 32,
};

char* inventoryBadgeNames[] = {
  [RAIDS_BADGE_TYPE_HEALTH_REGEN] "Health Regen",
  [RAIDS_BADGE_TYPE_AMMO_REGEN] "Ammo Regen",
  [RAIDS_BADGE_TYPE_SHARPSHOOTER] "Sharpshooter",
  [RAIDS_BADGE_TYPE_BERSERKER] "Berserker",
  [RAIDS_BADGE_TYPE_FLINCH_RESISTANCE] "Flinch Resistance",
  [RAIDS_BADGE_TYPE_HEALTH_BUFF] "Nanotech Reserves",
  [RAIDS_BADGE_TYPE_ALPHA_AMMO_BUFF] "Extra Mags",
  [RAIDS_BADGE_TYPE_ALPHA_AREA_BUFF] "Explosive Mags",
  [RAIDS_BADGE_TYPE_ALPHA_SPEED_BUFF] "Fire Rate",
  [RAIDS_BADGE_TYPE_ALPHA_IMPACT_BUFF] "High-Impact Rounds",
  [RAIDS_BADGE_TYPE_EXPLODING_ENEMIES] "Detonating Enemies",
  [RAIDS_BADGE_TYPE_COUNT] NULL,
};

//--------------------------------------------------------------------------
int inventoryGetHasInventoryPage(void)
{
  return inventoryPage.HasFlag && inventoryPage.Filter == inventoryDrawState.FilterIdx && inventoryPage.Page == inventoryDrawState.PageIdx;
}

//--------------------------------------------------------------------------
int inventoryHasPendingInventoryPageRequest(void)
{
  long dtMs = (timerGetSystemTime() - inventoryPage.LastRequestTime) / SYSTEM_TIME_TICKS_PER_MS;
  return inventoryPage.LastRequestTime && dtMs < (2*TIME_SECOND);
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* inventoryGetLocalItem(int index)
{
  if (index < 0) return NULL;
  if (index >= BANK_MAX_ITEMS) return NULL;

  RaidsInventoryItem_t* item = &inventoryPage.Items[index];
  if (!item->Type) return NULL;

  return item;
}

//--------------------------------------------------------------------------
void inventoryRequestPage(void)
{
  bankRequestInventoryFromServer(&inventoryPage, inventoryDrawState.FilterIdx, inventoryDrawState.PageIdx);
}

//--------------------------------------------------------------------------
void inventoryOpen(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (gameHasEnded()) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_NONE) return;

  state->MenuOpen = RAIDS_CUSTOM_MENU_INVENTORY;
  inventoryDrawState.SelectedIdx = 0;
  inventoryDrawState.PageIdx = 0;
  inventoryRequestPage();
  padDisableInput();
}

//--------------------------------------------------------------------------
void inventoryClose(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (state->MenuOpen != RAIDS_CUSTOM_MENU_INVENTORY) return;
  
  state->MenuOpen = RAIDS_CUSTOM_MENU_NONE;
  padEnableInput();
  //bankSendInventoryToServer();
}

//--------------------------------------------------------------------------
u32 inventoryDrawGetCompareColor(int compare)
{
  if (compare < 0) return 0x800000FF; // red
  if (compare == 0) return 0x80FFFFFF; // white
  return 0x8000FFFF; // yellow
}

//--------------------------------------------------------------------------
void inventoryDrawSpinner(Window_t* drawWindow)
{
  gfxSetupGifPaging(0);

  float x = -16; // middle align (16*(3-1))/2
  float t = (int)(4*fastmodf(gameGetTime() / 1000.0, 1.0));
  int i = 0;
  while (i < t) {
    windowDrawSprite(drawWindow, TEXT_ALIGN_MIDDLECENTER, x, 0, 16, 16, 80, 64, 64, 0x80FFFFFF, TEXT_ALIGN_MIDDLECENTER);
    //gfxHelperDrawSprite(UPGRADE_DRAW_CENTER_X, UPGRADE_DRAW_CENTER_Y, x, y, 16, 16, 64, 64, 80, 0x80FFFFFF, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
    ++i;
    x += 16;
  }
  
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryGetSelectedItemInteraction(int* canSell, int* alreadyEquipped, int* canEquip, int* tooStrong)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (selectedItem) {
    if (bankItemIsWeapon(selectedItem)) {
      int accountProf = getProficiencyFromXp(localBank->Account.WeaponXp[bankGetEquipSlotFromGadgetId(selectedItem->WeaponData.GadgetId)]);
      RaidsInventoryItem_t* equippedWeapon = bankGetLocalEquippedWeapon(selectedItem->WeaponData.GadgetId);
      *tooStrong = selectedItem->WeaponData.GadgetId && selectedItem->WeaponData.Proficiency > accountProf;
#if DEBUG
      *tooStrong = 0;
#endif
      *alreadyEquipped = equippedWeapon == selectedItem;
      *canEquip = !*tooStrong && selectedItem->WeaponData.GadgetId && !*alreadyEquipped; // already equipped
      *canSell = selectedItem->WeaponData.GadgetId && equippedWeapon != selectedItem && selectedItem->Notify != RAIDS_ITEM_NOTIFY_FAV; // can't sell equipped
    } else {
      RaidsInventoryItem_t* equippedBadge = bankGetLocalEquippedBadge();
      int badgeAndMissionActive = missionIsActive() && missionIsBossRaid();
      *tooStrong = 0;
      *alreadyEquipped = equippedBadge == selectedItem;
      *canEquip = !*alreadyEquipped && !badgeAndMissionActive; // already equipped
      *canSell = equippedBadge != selectedItem && selectedItem->Notify != RAIDS_ITEM_NOTIFY_FAV; // can't sell equipped
    }
  } else {
    *tooStrong = 0;
    *canEquip = 0;
    *alreadyEquipped = 0;
    *canSell = 0;
  }
}

//--------------------------------------------------------------------------
void inventoryDrawBadgeInfo(Window_t* drawWindow)
{
  u32 bgColor = 0x70101010; // dark gray
  u32 textColor = 0x80FFFFFF; // white
  u32 textRedColor = 0x804040D0; // light red
  u32 spriteColor = 0x80808080; // gray
  char strBuf[128];

  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (!selectedItem) return;

  int hasComparison = 0;
  RaidsInventoryItem_t* baseItem = NULL;
  RaidsInventoryItem_t* equippedItem = bankGetLocalEquippedBadge();
  hasComparison = equippedItem && equippedItem->Uid != selectedItem->Uid;
  baseItem = hasComparison ? equippedItem : selectedItem;

  float height = 100;
  if (missionIsActive() && missionIsBossRaid()) {
    windowDrawTextWindow(drawWindow, TEXT_ALIGN_BOTTOMLEFT, 0, 0, 0.7, textRedColor, "Cannot equip Badges in the middle of a mission.", -1, TEXT_ALIGN_TOPLEFT);
    height = 80;
  }

  /*
    // description
    offX = 10;
    int i;
    for (i = 0; i < BANK_BADGE_EFFECT_COUNT; ++i) {
      int badgeType = selectedItem->BadgeData.Effects[i];
      float badgeStrength = selectedItem->BadgeData.EffectStrength[i] / 255.0;
      if (!badgeType) continue;

      int badgeRarity = bankGetRarityFromQuality(selectedItem->BadgeData.EffectStrength[i]);
      snprintf(strBuf, sizeof(strBuf), "%c%s (%f)", bankRarityCode[badgeRarity], inventoryBadgeNames[badgeType], badgeStrength);
      gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
      offY += 12;
    }

    if (hasComparison) {
      offY += 12;
      gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.6, textColor, "Equipped Class Mod:", -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
      offY += 10;
      for (i = 0; i < BANK_BADGE_EFFECT_COUNT; ++i) {
        int badgeType = equippedItem->BadgeData.Effects[i];
        float badgeStrength = equippedItem->BadgeData.EffectStrength[i] / 255.0;
        if (!badgeType) continue;

        int badgeRarity = bankGetRarityFromQuality(equippedItem->BadgeData.EffectStrength[i]);
        snprintf(strBuf, sizeof(strBuf), "%c%s (%f)", bankRarityCode[badgeRarity], inventoryBadgeNames[badgeType], badgeStrength);
        gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
        offY += 12;
      }
    }
  */
}

//--------------------------------------------------------------------------
void inventoryDrawWeaponInfo(Window_t* drawWindow)
{
  u32 bgColor = 0x70101010; // dark gray
  u32 textColor = 0x80FFFFFF; // white
  u32 textRedColor = 0x804040D0; // light red
  u32 spriteColor = 0x80808080; // gray
  char strBuf[128];
  float strW;

  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (!selectedItem) return;

  int hasComparison = 0;
  RaidsInventoryItem_t* baseItem = NULL;
  RaidsInventoryItem_t* equippedItem = bankGetLocalEquippedWeapon(selectedItem->WeaponData.GadgetId);
  hasComparison = equippedItem && equippedItem != selectedItem;
  baseItem = hasComparison ? equippedItem : selectedItem;

  // stats
  snprintf(strBuf, sizeof(strBuf), "Damage: %d", (int)bankGetWeaponDamage(baseItem));
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    snprintf(strBuf, sizeof(strBuf), "> %d", (int)bankGetWeaponDamage(selectedItem));
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, inventoryDrawGetCompareColor(bankGetWeaponDamage(selectedItem) - bankGetWeaponDamage(baseItem)), strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 12);
  
  snprintf(strBuf, sizeof(strBuf), "Critical Hit: %.f%%", (baseItem->WeaponData.CritChance / 255.0) * 100);
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    snprintf(strBuf, sizeof(strBuf), "> %.f%%", (selectedItem->WeaponData.CritChance / 255.0) * 100);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, inventoryDrawGetCompareColor(selectedItem->WeaponData.CritChance - baseItem->WeaponData.CritChance), strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 12);
  
  int baseRarity = bankGetRarityFromQuality(baseItem->Quality);
  snprintf(strBuf, sizeof(strBuf), "Rarity: %s", inventoryRarityNames[baseRarity]);
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    int selRarity = bankGetRarityFromQuality(selectedItem->Quality);
    snprintf(strBuf, sizeof(strBuf), "=> %s", inventoryRarityNames[selRarity]);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, inventoryDrawGetCompareColor(selRarity - baseRarity), strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 12);

  snprintf(strBuf, sizeof(strBuf), "Paint: %s", inventoryPaintNames[baseItem->WeaponData.Paint]);
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    snprintf(strBuf, sizeof(strBuf), "> %s", inventoryPaintNames[selectedItem->WeaponData.Paint]);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 12);

  snprintf(strBuf, sizeof(strBuf), "Special: %s", inventoryPaintSpecialNames[baseItem->WeaponData.PaintSpecialMask]);
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    snprintf(strBuf, sizeof(strBuf), "> %s", inventoryPaintSpecialNames[selectedItem->WeaponData.PaintSpecialMask]);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 12);

  int baseWeaponModRarity = bankGetRarityFromQuality(baseItem->WeaponData.ModQuality);
  int baseHasMod = baseItem->WeaponData.ModType != RAIDS_WEAPON_MOD_NONE;
  snprintf(strBuf, sizeof(strBuf), "Mod: %s %s", inventoryModNames[baseItem->WeaponData.ModType], baseHasMod ? inventoryWeaponModLevels[baseWeaponModRarity] : "");
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    int selWeaponModRarity = bankGetRarityFromQuality(selectedItem->WeaponData.ModQuality);
    int selHasMod = selectedItem->WeaponData.ModType != RAIDS_WEAPON_MOD_NONE;
    snprintf(strBuf, sizeof(strBuf), "> %s %s", inventoryModNames[selectedItem->WeaponData.ModType], selHasMod ? inventoryWeaponModLevels[selWeaponModRarity] : "");
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 12);
  
  snprintf(strBuf, sizeof(strBuf), "Upgrades: %d/%d", baseItem->WeaponData.Upgrades, baseItem->WeaponData.MaxUpgrades);
  strW = windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison) {
    snprintf(strBuf, sizeof(strBuf), "> %d/%d", selectedItem->WeaponData.Upgrades, selectedItem->WeaponData.MaxUpgrades);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 10 + strW + 5, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 14);
  
  // alpha mods
  gfxSetupGifPaging(0);
  int i;
  float alphamodWidth = (drawWindow->Width - 10) / (ALPHA_MOD_COUNT - 1);
  for (i = 1; i < ALPHA_MOD_COUNT; ++i) {
      
    // get alphamods window
    Window_t windowAlphaMod;
    windowCreateFrom(&windowAlphaMod, drawWindow, 5 + ((i-1) * alphamodWidth), 0, alphamodWidth, alphamodWidth, TEXT_ALIGN_TOPLEFT);

    // sprite
    windowDrawSprite(&windowAlphaMod, TEXT_ALIGN_MIDDLECENTER, 0, 0, 16, 16, inventoryAlphaModSpriteIds[i], 32, 32, spriteColor, TEXT_ALIGN_MIDDLECENTER);
    snprintf(strBuf, sizeof(strBuf), "%d", baseItem->WeaponData.AlphaModCounts[i-1]);
    strW = windowDrawText(&windowAlphaMod, TEXT_ALIGN_BOTTOMLEFT, 0, 0, 0.7, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMLEFT);
    if (hasComparison) {
      snprintf(strBuf, sizeof(strBuf), ">%d", selectedItem->WeaponData.AlphaModCounts[i-1]);
      windowDrawText(&windowAlphaMod, TEXT_ALIGN_BOTTOMLEFT, 2 + strW, -1, 0.6, inventoryDrawGetCompareColor(selectedItem->WeaponData.AlphaModCounts[i-1] - baseItem->WeaponData.AlphaModCounts[i-1]), strBuf, -1, TEXT_ALIGN_BOTTOMLEFT);
    }
  }
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryDrawItemInfo(Window_t* drawWindow)
{
  const u32 bgColor = 0x70101010; // dark gray
  const u32 textColor = 0x80FFFFFF; // white
  const u32 textRedColor = 0x804040D0; // light red
  const u32 spriteColor = 0x80808080; // gray
  const u32 borderColor = 0x80000020; // maroon
  char strBuf[128];

  // draw box
  windowFill(drawWindow, bgColor);

  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (!selectedItem) return;

  // name
  bankGetItemName(selectedItem, strBuf, sizeof(strBuf));
  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 5, 2, 0.95, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  windowMove(drawWindow, 0, 12);

  // badge
  if (bankItemIsBadge(selectedItem)) {
    inventoryDrawBadgeInfo(drawWindow);
  } else if (bankItemIsWeapon(selectedItem)) {
    inventoryDrawWeaponInfo(drawWindow);
  }
  
  windowReset(drawWindow);
  windowBorder(drawWindow, borderColor, 0, 1, 0, 0);
}

//--------------------------------------------------------------------------
void inventoryDrawInfo(Window_t* drawWindow)
{
  const u32 bgColor = 0x50101010; // dark gray
  const u32 textColor = 0x80FFFFFF; // white
  const u32 brightTextColor = 0x8000FFFF; // yellow
  const u32 spriteColor = 0x80808080; // gray
  const u32 borderColor = 0x80000020; // maroon
  const float proficiencyHeight = 32;
  const float itemInfoHeight = 60;
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  int i;
  int level = bankGetLevel();
  char strBuf[128];

  if (!bankGetHasAccount()) {
    inventoryDrawSpinner(drawWindow);
    return;
  }

  // draw box
  windowFill(drawWindow, bgColor);

  // get stats window
  Window_t windowStats;
  windowCreateFrom(&windowStats, drawWindow, 0, 0, drawWindow->Width, drawWindow->Height, TEXT_ALIGN_TOPLEFT);

  // stats
  snprintf(strBuf, sizeof(strBuf), "Bolts: %'d", localBank->Account.Bolts);
  windowDrawText(&windowStats, TEXT_ALIGN_TOPLEFT, 5, 2, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  windowMove(&windowStats, 0, 12);
  if (level < LEVELUP_MAX_PLAYER_LEVEL) {
    snprintf(strBuf, sizeof(strBuf), "Level: %d   => %c%.1f%%", level + 1, '\x0A', bankGetLevelProgress() * 100);
  } else {
    snprintf(strBuf, sizeof(strBuf), "Level: %d   [MAXED]", level + 1);
  }
  
  windowDrawText(&windowStats, TEXT_ALIGN_TOPLEFT, 5, 2, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  windowMove(&windowStats, 0, 12);

  windowMove(&windowStats, 0, 6);
  windowDrawText(&windowStats, TEXT_ALIGN_TOPLEFT, 5, 2, 0.7, textColor, "Weapon Proficiency:", -1, TEXT_ALIGN_TOPLEFT);
  windowMove(&windowStats, 0, 10);

  // item proficiency
  gfxSetupGifPaging(0);
  float profWidth = (drawWindow->Width - 10) / (WEAPON_SLOT_COUNT - 1);
  for (i = 1; i < WEAPON_SLOT_COUNT; ++i) {
    int iconSpriteId = inventoryWeaponSpriteIds[i];
    int iconSpriteDim = inventoryWeaponSpriteDims[i];

    // get window
    Window_t windowProficiency;
    windowCreateFrom(&windowProficiency, &windowStats, 5 + ((i-1) * profWidth), 0, profWidth, profWidth, TEXT_ALIGN_TOPLEFT);

    // sprite
    windowDrawSprite(&windowProficiency, TEXT_ALIGN_MIDDLECENTER, 0, 0, 16, 16, iconSpriteId, iconSpriteDim, iconSpriteDim, spriteColor, TEXT_ALIGN_MIDDLECENTER);
    snprintf(strBuf, sizeof(strBuf), "%d", getProficiencyFromXp(localBank->Account.WeaponXp[i-1]) + 1);
    windowDrawText(&windowProficiency, TEXT_ALIGN_BOTTOMCENTER, 0, 3, 0.7, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMCENTER);
  }
  gfxDoGifPaging();
  windowMove(&windowStats, 0, 34);

  // draw selected item info
  Window_t windowItemInfo;
  windowCreateFrom(&windowItemInfo, &windowStats, 0, 0, windowStats.Width, windowStats.Height - windowStats.Position[1], TEXT_ALIGN_TOPLEFT);
  inventoryDrawItemInfo(&windowItemInfo);
  
  windowBorder(drawWindow, borderColor, 1, 1, 0, 1);
}

//--------------------------------------------------------------------------
void inventoryDrawItem(Window_t* drawWindow, int idx)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  RaidsInventoryItem_t* item = inventoryGetLocalItem(idx);
  u32 selectedColor = 0x40008080; // yellow
  u32 equippedColor = 0x40000080; // red
  u32 upgradePendingColor = 0x40008000; // green
  u32 materialColor = 0x40800000; // blue
  int isSelected = idx == inventoryDrawState.SelectedIdx;
  int isEquipped = 0;
  float itemDim = minf(drawWindow->Width, drawWindow->Height) - 4;

  // draw box
  if (isSelected) {
    windowFill(drawWindow, selectedColor);
  }

  // no item info
  if (!item) return;

  int isBadge = bankItemIsBadge(item);
  int isWeapon = bankItemIsWeapon(item);

  // seen
  if (isSelected && item->Notify == RAIDS_ITEM_NOTIFY_NEW) {
    item->Notify = RAIDS_ITEM_NOTIFY_NONE;
    bankSendInventoryItemToServer(item, RAIDS_ITEM_UPDATE_SET_NOTIFY);
  }
  
  // draw equipped
  if (isBadge) {
    RaidsInventoryItem_t* equippedItem = bankGetLocalEquippedBadge();
    isEquipped = equippedItem != NULL && equippedItem->Uid == item->Uid;
  } else {
    RaidsInventoryItem_t* equippedItem = bankGetLocalEquippedWeapon(item->WeaponData.GadgetId);
    isEquipped = equippedItem != NULL && equippedItem->Uid == item->Uid;
  }

  if (isEquipped) {
    windowFill(drawWindow, equippedColor);
  }

  int slotId = weaponIdToSlot(item->WeaponData.GadgetId);
  int iconSpriteId = inventoryWeaponSpriteIds[slotId];
  int iconSpriteDim = inventoryWeaponSpriteDims[slotId];
  if (isBadge) {
    slotId = 0;
    iconSpriteId = 114;
    iconSpriteDim = 32;
  }
  
  // draw icon
  u32 iconColor = bankRarityColors[bankGetRarityFromQuality(item->Quality)];
  gfxSetupGifPaging(0);
  windowDrawSprite(drawWindow, TEXT_ALIGN_MIDDLECENTER, -1, -1, itemDim+2, itemDim+2, iconSpriteId, iconSpriteDim, iconSpriteDim, 0x80000000, TEXT_ALIGN_MIDDLECENTER);
  windowDrawSprite(drawWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, itemDim, itemDim, iconSpriteId, iconSpriteDim, iconSpriteDim, iconColor, TEXT_ALIGN_MIDDLECENTER);

  // draw equipped
  if (isEquipped) {
    //gfxHelperDrawSprite(UPGRADE_DRAW_CENTER_X, UPGRADE_DRAW_CENTER_Y, offX-w/2, offY-h/2, 12, 12, 32, 32, 80, equippedColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw notify
  if (item->Notify) {
    u32 notifyColor = item->Notify == RAIDS_ITEM_NOTIFY_NEW ? INVENTORY_NOTIFY_NEW_COLOR : INVENTORY_NOTIFY_FAV_COLOR;
    windowDrawSprite(drawWindow, TEXT_ALIGN_TOPRIGHT, 0, 0, 8, 8, INVENTORY_NOTIFY_SPRITE_ID, 32, 32, notifyColor, TEXT_ALIGN_TOPRIGHT);
  }

  // draw paint
  if (isWeapon && (item->WeaponData.Paint || item->WeaponData.PaintSpecialMask)) {
    windowDrawSprite(drawWindow, TEXT_ALIGN_BOTTOMLEFT, 0, 0, 8, 8, 80 + (item->WeaponData.PaintSpecialMask>0?1:0), 32, 32, bankPaintColors[item->WeaponData.Paint] | 0x80000000, TEXT_ALIGN_BOTTOMLEFT);
  }

  // draw omega
  if (isWeapon && item->WeaponData.ModType) {
    u32 omegaColor = bankGetWeaponOmegaModColor(item);
    windowDrawSprite(drawWindow, TEXT_ALIGN_BOTTOMRIGHT, 0, 0, 8, 8, 79, 32, 32, omegaColor | 0x80000000, TEXT_ALIGN_BOTTOMRIGHT);
  }

  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryDrawTab(Window_t* drawWindow)
{
  u32 bgColor = 0x50101010; // dark gray
  u32 tabBgColor = 0x80202020; // gray
  u32 textColor = 0x80FFFFFF; // white
  u32 textFadedColor = 0x80808080; // light gray
  u32 selectedColor = 0x40008080; // yellow
  u32 borderColor = 0x80000020; // maroon
  int i,j;

  if (!inventoryGetHasInventoryPage()) {
    inventoryDrawSpinner(drawWindow);
    return;
  }

  // draw box
  windowFill(drawWindow, bgColor);
  //windowBorder(drawWindow, borderColor, 0, 1, 0, 1);

  // draw items
  const int gridSize = 8;
  float itemW = drawWindow->Width / gridSize;
  float itemH = drawWindow->Height / gridSize;
  for (i = 0; i < gridSize; ++i) {
    for (j = 0; j < gridSize; ++j) {
      int idx = (i*gridSize)+j;
    
      Window_t windowItem;
      windowCreateFrom(&windowItem, drawWindow, (j * itemW), (i * itemH), itemW, itemH, TEXT_ALIGN_TOPLEFT);
      inventoryDrawItem(&windowItem, idx);
    }
  }
}

//--------------------------------------------------------------------------
void inventoryDrawTabs(Window_t* drawWindow)
{
  u32 bgColor = 0x60404040; // dark gray
  u32 tabBgColor = 0x40101010; // gray
  u32 textColor = 0x80FFFFFF; // white
  u32 textFadedColor = 0x80808080; // light gray
  u32 selectedColor = 0x40008080; // yellow
  u32 borderColor = 0x80000020; // maroon
  int i;

  // draw box
  windowFill(drawWindow, bgColor);

  // draw tabs
  Window_t windowTabs;
  windowCreateFrom(&windowTabs, drawWindow, 0, 0, drawWindow->Width, drawWindow->Height, TEXT_ALIGN_TOPLEFT);
  gfxSetupGifPaging(0);
  for (i = 1; i < WEAPON_SLOT_COUNT; ++i) {
    int isTabSelected = i == inventoryDrawState.FilterIdx;
    float tabW = (windowTabs.Width / (WEAPON_SLOT_COUNT - 1));
    float tabH = windowTabs.Height;
    float spriteWH = tabH * 0.7;
    float spriteBgWH = tabH * 0.8;
    int spriteId = inventoryWeaponSpriteIds[i];
    int spriteDim = inventoryWeaponSpriteDims[i];
    u32 color = isTabSelected ? textColor : textFadedColor;
    u32 bgColor = isTabSelected ? selectedColor : tabBgColor;

    if (!windowHasArea(&windowTabs)) break;
    
    Window_t windowTab;
    windowCreateFrom(&windowTab, &windowTabs, 0, 0, tabW, tabH, TEXT_ALIGN_TOPLEFT);
    windowFill(&windowTab, bgColor);

    windowDrawSprite(&windowTab, TEXT_ALIGN_MIDDLECENTER, 0, 0, spriteBgWH, spriteBgWH, spriteId, spriteDim, spriteDim, 0x70000000, TEXT_ALIGN_MIDDLECENTER);
    windowDrawSprite(&windowTab, TEXT_ALIGN_MIDDLECENTER, 0, 0, spriteWH, spriteWH, spriteId, spriteDim, spriteDim, color, TEXT_ALIGN_MIDDLECENTER);
    windowMove(&windowTabs, tabW, 0);
  }
  gfxDoGifPaging();
  
  windowBorder(drawWindow, borderColor, 0, 1, 0, 0);
}

//--------------------------------------------------------------------------
void inventoryDrawFooter(Window_t* drawWindow)
{
  const u32 bgSolidColor = 0x80000000;
  const u32 textColor = 0x80FFFFFF;
  char strBuf[128];
  int canEquip = 0;
  int selectedTooStrong = 0;
  int canSell = 0;
  int alreadyEquipped = 0;
  int badgeAndMissionActive = 0;
  int isLoading = !inventoryGetHasInventoryPage();
  u32 sellPrice = 0;
  Player* localPlayer = playerGetFromSlot(0);

  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (selectedItem) {
    sellPrice = selectedItem->Price;
    inventoryGetSelectedItemInteraction(&canSell, &alreadyEquipped, &canEquip, &selectedTooStrong);
  }
  
  // draw footer text
  char sellPriceStrBuf[32];
  strBuf[0] = 0;
  if (!isLoading) {
    strcat(strBuf, "\x14 \x15 FILTER    ");
    if (canEquip) strcat(strBuf, "\x10 EQUIP    ");
    if (selectedTooStrong) { snprintf(sellPriceStrBuf, sizeof(sellPriceStrBuf), "MUST BE P%d TO EQUIP    ", selectedItem->WeaponData.Proficiency+1); strcat(strBuf, sellPriceStrBuf); }
    if (selectedItem) strcat(strBuf, "\x11 FAV    ");
    if (canSell) { strcat(strBuf, "\x13 SELL    "); }
  }
  strcat(strBuf, "\x12 CLOSE");
  
  // draw bg
  windowFill(drawWindow, bgSolidColor);
  windowDrawText(drawWindow, TEXT_ALIGN_BOTTOMLEFT, 2, -2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMLEFT);
}

//--------------------------------------------------------------------------
void inventoryDraw(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  u32 bgColor = 0x60000000;
  u32 borderColor = 0x80000020;
  u32 textColor = 0x80FFFFFF;
  const float windowWidth = 450;
  const float windowHeight = 250;
  const float titleHeight = 24;
  const float tabHeaderHeight = 24;
  const float footerHeight = 20;
  const float itemListDetailsHeight = 180;
  char strBuf[128];
  Window_t drawWindow;

  GameSettings* gs = gameGetSettings();
  Player* localPlayer = playerGetFromSlot(0);
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  int isLoadingInventory = inventoryHasPendingInventoryPageRequest();
  int isLoadingAccount = bankHasPendingAccountRequest();

  // bad state
  if ((!isLoadingInventory && !inventoryGetHasInventoryPage()) || (!isLoadingAccount && !bankGetHasAccount())) {
    inventoryClose();
    return;
  }

  // setup draw state
  windowCreate(&drawWindow, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, windowWidth, windowHeight, TEXT_ALIGN_MIDDLECENTER);

  // draw frame
  windowFill(&drawWindow, bgColor);

  // draw title text
  snprintf(strBuf, sizeof(strBuf), "Inventory - %s", gs->PlayerNames[localPlayer->PlayerId]);
  windowDrawText(&drawWindow, TEXT_ALIGN_TOPCENTER, 0, 0, 1.1, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);

  // draw tabs
  Window_t drawWindowTabs;
  windowCreateFrom(&drawWindowTabs, &drawWindow, 0, titleHeight, windowWidth * 0.5, tabHeaderHeight, TEXT_ALIGN_TOPLEFT);
  inventoryDrawTabs(&drawWindowTabs);

  // draw tab
  Window_t drawWindowTab;
  windowCreateFrom(&drawWindowTab, &drawWindow, 0, titleHeight + tabHeaderHeight, windowWidth * 0.5, windowHeight - (tabHeaderHeight + footerHeight + titleHeight), TEXT_ALIGN_TOPLEFT);
  inventoryDrawTab(&drawWindowTab);

  // draw info
  Window_t drawWindowInfo;
  windowCreateFrom(&drawWindowInfo, &drawWindow, windowWidth * 0.5, titleHeight, windowWidth * 0.5, windowHeight - (footerHeight + titleHeight), TEXT_ALIGN_TOPLEFT);
  inventoryDrawInfo(&drawWindowInfo);

  // draw footer
  Window_t windowFooter;
  windowCreateFrom(&windowFooter, &drawWindow, 0, 0, drawWindow.Width, footerHeight, TEXT_ALIGN_BOTTOMCENTER);
  inventoryDrawFooter(&windowFooter);

  // draw border
  windowBorder(&drawWindow, borderColor, 1, 1, 1, 1);

  // draw sell dialog
  if (inventoryDrawState.ShowSellDialog) {
        
    // invalid
    if (!selectedItem) {
      inventoryDrawState.ShowSellDialog = 0;
      return;
    }

    char sellStrBuf[64];
    char sellPriceBuf[64];
    char itemNameBuf[64];
    u32 sellPrice = selectedItem->Price;
    bankGetItemName(selectedItem, itemNameBuf, sizeof(itemNameBuf));
    snprintf(sellStrBuf, sizeof(sellStrBuf), "Sell %s?", itemNameBuf);
    snprintf(sellPriceBuf, sizeof(sellPriceBuf), "\x0A%'d", sellPrice);
    
    Window_t windowSellDialog;
    windowCreateFrom(&windowSellDialog, &drawWindow, 0, 0, 200, 75, TEXT_ALIGN_MIDDLECENTER);
    windowDrawDialog(&windowSellDialog, sellStrBuf, NULL, sellPriceBuf, inventorySellDialogFooter);
    return;
  }
}

//--------------------------------------------------------------------------
void inventoryHandleInput(void)
{
  int canEquip = 0;
  int selectedTooStrong = 0;
  int canSell = 0;
  int alreadyEquipped = 0;

  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

  if (inventoryDrawState.ShowSellDialog) {

    // handle close input
    if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
      inventoryDrawState.ShowSellDialog = 0;
      return;
    } else if (padGetButtonDown(0, PAD_CROSS) > 0) {
      inventoryDrawState.ShowSellDialog = 0;
      if (bankSellItem(&inventoryPage.Items[inventoryDrawState.SelectedIdx])) {
        //inventoryPage.RefreshLocalInventory = 1;
        inventoryRequestPage();
      }
      //bankSellLocalItemAtIndex(inventoryFilterMapping[inventoryDrawState.SelectedIdx]);
    }

    return;
  }
  
  Player* localPlayer = playerGetFromSlot(0);
  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  inventoryGetSelectedItemInteraction(&canSell, &alreadyEquipped, &canEquip, &selectedTooStrong);

  // handle close input
  if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
    inventoryClose();
    return;
  }

  // handle input
  if (inventoryGetHasInventoryPage()) {
    int selIdx = inventoryDrawState.SelectedIdx;
    if (padGetButtonDown(0, PAD_LEFT) > 0) {                              // NAV LEFT
      int row = (selIdx-1)%INVENTORY_DRAW_WEAPONS_DIM;
      if (row < 0 || row > (selIdx%INVENTORY_DRAW_WEAPONS_DIM)) selIdx += INVENTORY_DRAW_WEAPONS_DIM-1;
      else selIdx--;
    } else if (padGetButtonDown(0, PAD_RIGHT) > 0) {                      // NAV RIGHT
      int row = (selIdx+1)%INVENTORY_DRAW_WEAPONS_DIM;
      if (row < (selIdx%INVENTORY_DRAW_WEAPONS_DIM)) selIdx -= INVENTORY_DRAW_WEAPONS_DIM-1;
      else selIdx++;
    } else if (padGetButtonDown(0, PAD_DOWN) > 0) {                       // NAV DOWN
      selIdx = (selIdx+INVENTORY_DRAW_WEAPONS_DIM) % BANK_MAX_ITEMS;
    } else if (padGetButtonDown(0, PAD_UP) > 0) {                         // NAV UP
      selIdx = (selIdx-INVENTORY_DRAW_WEAPONS_DIM) % BANK_MAX_ITEMS;
      if (selIdx < 0) selIdx += BANK_MAX_ITEMS;
    } else if (padGetButtonDown(0, PAD_L1) > 0) {                         // TAB LEFT
      selIdx = 0;
      inventoryDrawState.FilterIdx--;
      inventoryDrawState.PageIdx = 0;
      if (inventoryDrawState.FilterIdx < 1) inventoryDrawState.FilterIdx = INVENTORY_TAB_COUNT - 1;
      inventoryRequestPage();
    } else if (padGetButtonDown(0, PAD_R1) > 0) {                         // TAB RIGHT
      selIdx = 0;
      inventoryDrawState.PageIdx = 0;
      inventoryDrawState.FilterIdx = (inventoryDrawState.FilterIdx + 1) % INVENTORY_TAB_COUNT;
      if (inventoryDrawState.FilterIdx < 1) inventoryDrawState.FilterIdx = 1;
      inventoryRequestPage();
    } else if (padGetButtonDown(0, PAD_CROSS) > 0) {                      // EQUIP
      if (canEquip || alreadyEquipped) {
        if (bankEquipItem(&inventoryPage.Items[selIdx])) {
          playEquipSound(localPlayer);
          inventoryPage.RefreshLocalInventory = 1;
        }
      } else {
        playEquipRejectSound(localPlayer);
      }
    } else if (canSell && padGetButtonDown(0, PAD_SQUARE) > 0) {          // SELL
      inventoryDrawState.ShowSellDialog = 1;
    } else if (selectedItem && padGetButtonDown(0, PAD_CIRCLE) > 0) {   // FAVORITE
      if (selectedItem->Notify == RAIDS_ITEM_NOTIFY_FAV) selectedItem->Notify = 0;
      else selectedItem->Notify = RAIDS_ITEM_NOTIFY_FAV;

      bankSendInventoryItemToServer(selectedItem, RAIDS_ITEM_UPDATE_SET_NOTIFY);
    }

    inventoryDrawState.SelectedIdx = selIdx;
  }
}

//--------------------------------------------------------------------------
void inventoryFrameTick(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // draw
  if (state->MenuOpen == RAIDS_CUSTOM_MENU_INVENTORY) {
    inventoryDraw();
  }
}

//--------------------------------------------------------------------------
void inventoryTick(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (gameHasEnded()) {
    inventoryClose();
    return;
  }

  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_INVENTORY) {
      
    // reset filter when inventory changes while menu is open
    RaidsPlayerBank_t* localBank = bankGetLocalBank();
    if (inventoryPage.RefreshLocalInventory) {
      //inventorySetFilter(inventoryDrawState.FilterIdx);
      inventoryPage.RefreshLocalInventory = 0;
    }

    inventoryHandleInput();
    return;
  }

  int canOpen = state->MenuOpen == RAIDS_CUSTOM_MENU_NONE && PATCH_POINTERS_PATCHMENU == 0 && !gameIsAnyStartMenuOpen() && padGetButtonDown(0, PAD_LEFT) > 0;
  if (canOpen) {
    inventoryOpen();
  }
}

//--------------------------------------------------------------------------
void inventoryInit(void)
{

}
