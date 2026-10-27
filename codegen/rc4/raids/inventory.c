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
#include <libdl/string.h>
#include <libdl/hud.h>
#include <libdl/graphics.h>
#include "utils.h"
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
float inventoryDrawFittedText(Window_t* window, float x, float y, float width, float scale, u32 color, char* text, enum TextAlign alignment)
{
  if (width <= 0) return 0;

  char buf[128];
  snprintf(buf, sizeof(buf), "%s", text);
  if (gfxGetFontWidth(buf, -1, scale) > width) {
    float dotsWidth = gfxGetFontWidth("...", -1, scale);
    int length = strlen(buf);
    while (length > 0 && (length > (int)sizeof(buf) - 4 || gfxGetFontWidth(buf, -1, scale) + dotsWidth > width)) buf[--length] = 0;
    if (dotsWidth <= width) strcat(buf, "...");
  }
  windowDrawText(window, TEXT_ALIGN_TOPLEFT, x, y, scale, color, buf, -1, alignment);
  return gfxGetFontWidth(buf, -1, scale);
}

//--------------------------------------------------------------------------
void inventoryDrawProgressBar(Window_t* window, float x, float y, float width, double progress)
{
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  windowDrawBox(window, TEXT_ALIGN_TOPLEFT, x, y, width, 6, 0x80303030, TEXT_ALIGN_TOPLEFT);
  if (progress > 0)
    windowDrawBox(window, TEXT_ALIGN_TOPLEFT, x, y, width * progress, 6, 0x80B0B050, TEXT_ALIGN_TOPLEFT);
}

//--------------------------------------------------------------------------
void inventoryDrawComparisonRow(Window_t* window, char* label, char* equipped, char* selected, u32 selectedColor, int hasComparison)
{
  const u32 textColor = 0x80FFFFFF;
  const float padding = 8;
  const float valueWidth = 75;
  const float gutter = 8;
  float selectedRight = window->Width - padding;
  float equippedRight = selectedRight - valueWidth - gutter;

  inventoryDrawFittedText(window, padding, 2, 73, 0.7, textColor, label, TEXT_ALIGN_TOPLEFT);
  if (hasComparison)
    inventoryDrawFittedText(window, equippedRight, 2, valueWidth, 0.7, textColor, equipped, TEXT_ALIGN_TOPRIGHT);
  inventoryDrawFittedText(window, selectedRight, 2, valueWidth, 0.7, selectedColor, selected, TEXT_ALIGN_TOPRIGHT);
  windowMove(window, 0, 14);
}

//--------------------------------------------------------------------------
void inventoryDrawSpinner(Window_t* drawWindow)
{
  gfxSetupGifPaging(0);

  float x = -16; // middle align (16*(3-1))/2
  float t = (int)(4*fastmodf(gameGetTime() / 1000.0, 1.0));
  int i = 0;
  while (i < t) {
    windowDrawSprite(drawWindow, TEXT_ALIGN_MIDDLECENTER, x, 0, 16, 16, 80, 0x80FFFFFF, TEXT_ALIGN_MIDDLECENTER);
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
  const u32 textColor = 0x80FFFFFF;
  const u32 secondaryColor = 0x80B0B0B0;
  const u32 spriteColor = 0x80808080;
  char equippedBuf[64];
  char selectedBuf[64];

  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (!selectedItem) return;

  RaidsInventoryItem_t* equippedItem = bankGetLocalEquippedWeapon(selectedItem->WeaponData.GadgetId);
  int hasComparison = equippedItem && equippedItem->Uid != selectedItem->Uid;
  int isEquipped = equippedItem && equippedItem->Uid == selectedItem->Uid;
  RaidsInventoryItem_t* baseItem = hasComparison ? equippedItem : selectedItem;
  float selectedRight = drawWindow->Width - 8;
  float equippedRight = selectedRight - 75 - 8;

  // fixed comparison column headings
  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 8, 0, 0.55, secondaryColor, "Stat", -1, TEXT_ALIGN_TOPLEFT);
  if (hasComparison)
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, equippedRight, 0, 0.55, secondaryColor, "Equipped", -1, TEXT_ALIGN_TOPRIGHT);
  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, selectedRight, 0, 0.55, secondaryColor, isEquipped ? "Equipped" : "Selected", -1, TEXT_ALIGN_TOPRIGHT);
  windowMove(drawWindow, 0, 12);

  float baseDamage = bankGetWeaponDamage(baseItem);
  float selectedDamage = bankGetWeaponDamage(selectedItem);
  snprintf(equippedBuf, sizeof(equippedBuf), "%d", (int)baseDamage);
  snprintf(selectedBuf, sizeof(selectedBuf), "%d", (int)selectedDamage);
  inventoryDrawComparisonRow(drawWindow, "Damage", equippedBuf, selectedBuf, inventoryDrawGetCompareColor(selectedDamage - baseDamage), hasComparison);

  snprintf(equippedBuf, sizeof(equippedBuf), "%.f%%", (baseItem->WeaponData.CritChance / 255.0) * 100);
  snprintf(selectedBuf, sizeof(selectedBuf), "%.f%%", (selectedItem->WeaponData.CritChance / 255.0) * 100);
  inventoryDrawComparisonRow(drawWindow, "Critical Hit", equippedBuf, selectedBuf, inventoryDrawGetCompareColor(selectedItem->WeaponData.CritChance - baseItem->WeaponData.CritChance), hasComparison);

  int baseRarity = bankGetRarityFromQuality(baseItem->Quality);
  int selectedRarity = bankGetRarityFromQuality(selectedItem->Quality);
  inventoryDrawComparisonRow(drawWindow, "Rarity", inventoryRarityNames[baseRarity], inventoryRarityNames[selectedRarity], inventoryDrawGetCompareColor(selectedRarity - baseRarity), hasComparison);
  inventoryDrawComparisonRow(drawWindow, "Paint", inventoryPaintNames[baseItem->WeaponData.Paint], inventoryPaintNames[selectedItem->WeaponData.Paint], textColor, hasComparison);
  inventoryDrawComparisonRow(drawWindow, "Special", inventoryPaintSpecialNames[baseItem->WeaponData.PaintSpecialMask], inventoryPaintSpecialNames[selectedItem->WeaponData.PaintSpecialMask], textColor, hasComparison);

  int baseWeaponModRarity = bankGetRarityFromQuality(baseItem->WeaponData.ModQuality);
  int baseHasMod = baseItem->WeaponData.ModType != RAIDS_WEAPON_MOD_NONE;
  int selectedWeaponModRarity = bankGetRarityFromQuality(selectedItem->WeaponData.ModQuality);
  int selectedHasMod = selectedItem->WeaponData.ModType != RAIDS_WEAPON_MOD_NONE;
  snprintf(equippedBuf, sizeof(equippedBuf), "%s%s%s", inventoryModNames[baseItem->WeaponData.ModType], baseHasMod ? " " : "", baseHasMod ? inventoryWeaponModLevels[baseWeaponModRarity] : "");
  snprintf(selectedBuf, sizeof(selectedBuf), "%s%s%s", inventoryModNames[selectedItem->WeaponData.ModType], selectedHasMod ? " " : "", selectedHasMod ? inventoryWeaponModLevels[selectedWeaponModRarity] : "");
  inventoryDrawComparisonRow(drawWindow, "Mod", equippedBuf, selectedBuf, textColor, hasComparison);

  snprintf(equippedBuf, sizeof(equippedBuf), "%d/%d", baseItem->WeaponData.Upgrades, baseItem->WeaponData.MaxUpgrades);
  snprintf(selectedBuf, sizeof(selectedBuf), "%d/%d", selectedItem->WeaponData.Upgrades, selectedItem->WeaponData.MaxUpgrades);
  inventoryDrawComparisonRow(drawWindow, "Upgrades", equippedBuf, selectedBuf, textColor, hasComparison);

  // alpha mods keep compact, icon-based comparisons
  windowMove(drawWindow, 0, 4);
  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 8, 0, 0.55, secondaryColor, "Alpha Mods", -1, TEXT_ALIGN_TOPLEFT);
  windowMove(drawWindow, 0, 12);
  gfxSetupGifPaging(0);
  int i;
  float alphamodWidth = (drawWindow->Width - 16) / (ALPHA_MOD_COUNT - 1);
  for (i = 1; i < ALPHA_MOD_COUNT; ++i) {
    Window_t windowAlphaMod;
    windowCreateFrom(&windowAlphaMod, drawWindow, 8 + ((i-1) * alphamodWidth), 0, alphamodWidth, 32, TEXT_ALIGN_TOPLEFT);
    windowDrawSprite(&windowAlphaMod, TEXT_ALIGN_TOPCENTER, 0, 0, 16, 16, inventoryAlphaModSpriteIds[i], spriteColor, TEXT_ALIGN_TOPCENTER);

    int baseCount = baseItem->WeaponData.AlphaModCounts[i-1];
    int selectedCount = selectedItem->WeaponData.AlphaModCounts[i-1];
    if (hasComparison && baseCount != selectedCount) {
      snprintf(equippedBuf, sizeof(equippedBuf), "%d", baseCount);
      snprintf(selectedBuf, sizeof(selectedBuf), ">%d", selectedCount);
      float scale = 0.6;
      float width = gfxGetFontWidth(equippedBuf, -1, scale) + gfxGetFontWidth(selectedBuf, -1, scale);
      if (width > alphamodWidth - 2) scale *= (alphamodWidth - 2) / width;
      float baseWidth = gfxGetFontWidth(equippedBuf, -1, scale);
      width = baseWidth + gfxGetFontWidth(selectedBuf, -1, scale);
      float x = (alphamodWidth - width) / 2;
      windowDrawText(&windowAlphaMod, TEXT_ALIGN_TOPLEFT, x, 18, scale, textColor, equippedBuf, -1, TEXT_ALIGN_TOPLEFT);
      windowDrawText(&windowAlphaMod, TEXT_ALIGN_TOPLEFT, x + baseWidth, 18, scale, inventoryDrawGetCompareColor(selectedCount - baseCount), selectedBuf, -1, TEXT_ALIGN_TOPLEFT);
    } else {
      snprintf(selectedBuf, sizeof(selectedBuf), "%d", selectedCount);
      windowDrawText(&windowAlphaMod, TEXT_ALIGN_TOPCENTER, 0, 18, 0.7, textColor, selectedBuf, -1, TEXT_ALIGN_TOPCENTER);
    }
  }
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryDrawItemInfo(Window_t* drawWindow)
{
  const u32 bgColor = 0x78101010;
  const u32 textColor = 0x80FFFFFF;
  const u32 secondaryColor = 0x80B0B0B0;
  const u32 textRedColor = 0x804040D0;
  const u32 borderColor = 0x80000020;
  char strBuf[128];

  windowFill(drawWindow, bgColor);
  windowBorder(drawWindow, borderColor, 1, 0, 0, 0);
  if (!bankGetHasAccount() || !inventoryGetHasInventoryPage()) {
    inventoryDrawSpinner(drawWindow);
    return;
  }

  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(inventoryDrawState.SelectedIdx);
  if (!selectedItem) {
    windowDrawText(drawWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.7, secondaryColor, "No item selected", -1, TEXT_ALIGN_MIDDLECENTER);
    return;
  }

  bankGetItemName(selectedItem, strBuf, sizeof(strBuf));
  inventoryDrawFittedText(drawWindow, 8, 8, drawWindow->Width - 16, 0.9, textColor, strBuf, TEXT_ALIGN_TOPLEFT);
  windowMove(drawWindow, 0, 26);

  RaidsInventoryItem_t* equippedItem = NULL;
  if (bankItemIsBadge(selectedItem)) equippedItem = bankGetLocalEquippedBadge();
  else if (bankItemIsWeapon(selectedItem)) equippedItem = bankGetLocalEquippedWeapon(selectedItem->WeaponData.GadgetId);

  int canSell = 0, alreadyEquipped = 0, canEquip = 0, tooStrong = 0;
  inventoryGetSelectedItemInteraction(&canSell, &alreadyEquipped, &canEquip, &tooStrong);
  int classRestricted = bankItemIsWeapon(selectedItem) && bankGetHasEquippedClass() && !bankCanUseItem(selectedItem);

  // Reserve this line so status changes do not move the comparison table.
  if (tooStrong) {
    snprintf(strBuf, sizeof(strBuf), "Must be P%d to equip%s", selectedItem->WeaponData.Proficiency + 1, classRestricted ? "; class restricted" : "");
    inventoryDrawFittedText(drawWindow, 8, 0, drawWindow->Width - 16, 0.6, textRedColor, strBuf, TEXT_ALIGN_TOPLEFT);
  } else if (classRestricted) {
    inventoryDrawFittedText(drawWindow, 8, 0, drawWindow->Width - 16, 0.6, textRedColor, "Not usable by current class", TEXT_ALIGN_TOPLEFT);
  } else if (equippedItem && equippedItem->Uid == selectedItem->Uid) {
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 8, 0, 0.6, secondaryColor, "Equipped", -1, TEXT_ALIGN_TOPLEFT);
  }
  windowMove(drawWindow, 0, 14);

  if (bankItemIsBadge(selectedItem)) inventoryDrawBadgeInfo(drawWindow);
  else if (bankItemIsWeapon(selectedItem)) inventoryDrawWeaponInfo(drawWindow);
  windowReset(drawWindow);
}

//--------------------------------------------------------------------------
void inventoryDrawAccountInfo(Window_t* drawWindow, float statsWidth)
{
  const u32 bgColor = 0x78101010;
  const u32 textColor = 0x80FFFFFF;
  const u32 secondaryColor = 0x80B0B0B0;
  const u32 spriteColor = 0x80808080;
  const u32 borderColor = 0x80000020;
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  char strBuf[128];

  windowFill(drawWindow, bgColor);
  windowBorder(drawWindow, borderColor, 0, 0, 1, 1);
  if (!bankGetHasAccount()) {
    inventoryDrawFittedText(drawWindow, 8, 4, statsWidth - 16, 0.7, secondaryColor, "Loading account...", TEXT_ALIGN_TOPLEFT);
    return;
  }

  snprintf(strBuf, sizeof(strBuf), "Bolts: %'u", localBank->Account.Bolts);
  inventoryDrawFittedText(drawWindow, 8, 4, statsWidth - 16, 0.7, textColor, strBuf, TEXT_ALIGN_TOPLEFT);
  const float barWidth = 80;
  float barX = statsWidth - 8 - barWidth;
  float labelWidth = barX - 16;
  int level = bankGetLevel();
  if (level < LEVELUP_MAX_PLAYER_LEVEL) {
    snprintf(strBuf, sizeof(strBuf), "Hero Level: %d", level + 1);
  } else {
    snprintf(strBuf, sizeof(strBuf), "Hero Level: %d  [MAXED]", level + 1);
  }
  inventoryDrawFittedText(drawWindow, 8, 16, labelWidth, 0.7, textColor, strBuf, TEXT_ALIGN_TOPLEFT);
  inventoryDrawProgressBar(drawWindow, barX, 18, barWidth, bankGetLevelProgress());

  if (bankGetHasEquippedClass() && localBank->EquippedClass.Name[0]) {
    char suffix[16];
    RaidsPlayerClass_t* playerClass = &localBank->EquippedClass;
    int classLevel = bankGetClassLevel(playerClass, playerClass->CurrentExperience);
    snprintf(strBuf, sizeof(strBuf), "Class: %.32s", localBank->EquippedClass.Name);
    snprintf(suffix, sizeof(suffix), " %s", bankGetClassLevelSuffix(classLevel));
    float suffixWidth = gfxGetFontWidth(suffix, -1, 0.7);
    float nameWidth = inventoryDrawFittedText(drawWindow, 8, 28, labelWidth - suffixWidth, 0.7, textColor, strBuf, TEXT_ALIGN_TOPLEFT);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 8 + nameWidth, 28, 0.7, textColor, suffix, -1, TEXT_ALIGN_TOPLEFT);

    double progress = 1;
    if (classLevel < 10) {
      u64 startXp = bankGetClassXpForLevel(playerClass, classLevel - 1);
      u64 nextXp = bankGetClassXpForLevel(playerClass, classLevel);
      progress = nextXp > startXp && playerClass->CurrentExperience >= startXp
        ? (double)(playerClass->CurrentExperience - startXp) / (double)(nextXp - startXp) : 0;
    }
    inventoryDrawProgressBar(drawWindow, barX, 30, barWidth, progress);
  } else {
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 8, 28, 0.7, secondaryColor, "Class: Loading...", -1, TEXT_ALIGN_TOPLEFT);
  }

  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, statsWidth + 8, 4, 0.55, secondaryColor, "Weapon Proficiency", -1, TEXT_ALIGN_TOPLEFT);
  float profWidth = (drawWindow->Width - statsWidth - 16) / (WEAPON_SLOT_COUNT - 1);
  gfxSetupGifPaging(0);
  int i;
  for (i = 1; i < WEAPON_SLOT_COUNT; ++i) {
    Window_t windowProficiency;
    windowCreateFrom(&windowProficiency, drawWindow, statsWidth + 8 + ((i-1) * profWidth), 14, profWidth, 28, TEXT_ALIGN_TOPLEFT);
    windowDrawSprite(&windowProficiency, TEXT_ALIGN_TOPCENTER, 0, 0, 16, 16, inventoryWeaponSpriteIds[i], spriteColor, TEXT_ALIGN_TOPCENTER);
    snprintf(strBuf, sizeof(strBuf), "%d", getProficiencyFromXp(localBank->Account.WeaponXp[i-1]) + 1);
    windowDrawText(&windowProficiency, TEXT_ALIGN_TOPCENTER, 0, 18, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
  }
  gfxDoGifPaging();
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
  if (isBadge) {
    slotId = 0;
    iconSpriteId = 114;
  }
  
  // draw icon
  u32 iconColor = bankRarityColors[bankGetRarityFromQuality(item->Quality)];
  gfxSetupGifPaging(0);
  windowDrawSprite(drawWindow, TEXT_ALIGN_MIDDLECENTER, -1, -1, itemDim+2, itemDim+2, iconSpriteId, 0x80000000, TEXT_ALIGN_MIDDLECENTER);
  windowDrawSprite(drawWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, itemDim, itemDim, iconSpriteId, iconColor, TEXT_ALIGN_MIDDLECENTER);

  // draw equipped
  if (isEquipped) {
    //gfxHelperDrawSprite(UPGRADE_DRAW_CENTER_X, UPGRADE_DRAW_CENTER_Y, offX-w/2, offY-h/2, 12, 12, 32, 32, 80, equippedColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw notify
  if (item->Notify) {
    u32 notifyColor = item->Notify == RAIDS_ITEM_NOTIFY_NEW ? INVENTORY_NOTIFY_NEW_COLOR : INVENTORY_NOTIFY_FAV_COLOR;
    windowDrawSprite(drawWindow, TEXT_ALIGN_TOPRIGHT, 0, 0, 8, 8, INVENTORY_NOTIFY_SPRITE_ID, notifyColor, TEXT_ALIGN_TOPRIGHT);
  }

  // draw paint
  if (isWeapon && (item->WeaponData.Paint || item->WeaponData.PaintSpecialMask)) {
    windowDrawSprite(drawWindow, TEXT_ALIGN_BOTTOMLEFT, 0, 0, 8, 8, 80 + (item->WeaponData.PaintSpecialMask>0?1:0), bankPaintColors[item->WeaponData.Paint] | 0x80000000, TEXT_ALIGN_BOTTOMLEFT);
  }

  // draw omega
  if (isWeapon && item->WeaponData.ModType) {
    u32 omegaColor = bankGetWeaponOmegaModColor(item);
    windowDrawSprite(drawWindow, TEXT_ALIGN_BOTTOMRIGHT, 0, 0, 8, 8, 79, omegaColor | 0x80000000, TEXT_ALIGN_BOTTOMRIGHT);
  }

  // class restriction marker (the other corners show notify, paint and mod)
  if (isWeapon && bankGetHasEquippedClass() && !bankCanUseItem(item)) {
    windowDrawBox(drawWindow, TEXT_ALIGN_TOPLEFT, 1, 1, 10, 10, 0x80000000, TEXT_ALIGN_TOPLEFT);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 6, 1, 0.6, 0x804040D0, "!", -1, TEXT_ALIGN_TOPCENTER);
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
    u32 color = isTabSelected ? textColor : textFadedColor;
    u32 bgColor = isTabSelected ? selectedColor : tabBgColor;

    if (!windowHasArea(&windowTabs)) break;
    
    Window_t windowTab;
    windowCreateFrom(&windowTab, &windowTabs, 0, 0, tabW, tabH, TEXT_ALIGN_TOPLEFT);
    windowFill(&windowTab, bgColor);

    windowDrawSprite(&windowTab, TEXT_ALIGN_MIDDLECENTER, 0, 0, spriteBgWH, spriteBgWH, spriteId, 0x70000000, TEXT_ALIGN_MIDDLECENTER);
    windowDrawSprite(&windowTab, TEXT_ALIGN_MIDDLECENTER, 0, 0, spriteWH, spriteWH, spriteId, color, TEXT_ALIGN_MIDDLECENTER);
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
  strBuf[0] = 0;
  if (!isLoading) {
    strcat(strBuf, "\x14 \x15 FILTER    ");
    if (canEquip) strcat(strBuf, "\x10 EQUIP    ");
    if (selectedItem) strcat(strBuf, "\x11 FAV    ");
    if (canSell) { strcat(strBuf, "\x13 SELL    "); }
  }
  strcat(strBuf, "\x12 CLOSE");
  
  // draw bg
  windowFill(drawWindow, bgSolidColor);
  windowDrawText(drawWindow, TEXT_ALIGN_MIDDLELEFT, 2, 0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLELEFT);
}

//--------------------------------------------------------------------------
void inventoryDraw(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  u32 bgColor = 0x70000000;
  u32 borderColor = 0x80000020;
  u32 textColor = 0x80FFFFFF;
  const float windowWidth = 480;
  const float windowHeight = 290;
  const float titleHeight = 24;
  const float accountHeight = 44;
  const float gridWidth = 225;
  const float tabHeaderHeight = 24;
  const float footerHeight = 16;
  const float bodyY = titleHeight + accountHeight;
  const float bodyHeight = windowHeight - bodyY - footerHeight;
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
  inventoryDrawFittedText(&drawWindow, windowWidth * 0.5, 0, windowWidth - 16, 1.1, textColor, strBuf, TEXT_ALIGN_TOPCENTER);

  // full-width account strip
  Window_t drawWindowAccount;
  windowCreateFrom(&drawWindowAccount, &drawWindow, 0, titleHeight, windowWidth, accountHeight, TEXT_ALIGN_TOPLEFT);
  inventoryDrawAccountInfo(&drawWindowAccount, gridWidth);

  // draw tabs
  Window_t drawWindowTabs;
  windowCreateFrom(&drawWindowTabs, &drawWindow, 0, bodyY, gridWidth, tabHeaderHeight, TEXT_ALIGN_TOPLEFT);
  inventoryDrawTabs(&drawWindowTabs);

  // draw tab
  Window_t drawWindowTab;
  windowCreateFrom(&drawWindowTab, &drawWindow, 0, bodyY + tabHeaderHeight, gridWidth, bodyHeight - tabHeaderHeight, TEXT_ALIGN_TOPLEFT);
  inventoryDrawTab(&drawWindowTab);

  // draw info
  Window_t drawWindowInfo;
  windowCreateFrom(&drawWindowInfo, &drawWindow, gridWidth, bodyY, windowWidth - gridWidth, bodyHeight, TEXT_ALIGN_TOPLEFT);
  inventoryDrawItemInfo(&drawWindowInfo);

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
