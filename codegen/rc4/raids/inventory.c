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

InventoryDrawState_t inventoryDrawState = {
  .SelectedIdx = 0,
  .FilterIdx = 0,
  .ShowSellDialog = 0,
};

char* inventoryRarityNames[] = {
  [RAIDS_ITEM_RARITY_COMMON] "Common",
  [RAIDS_ITEM_RARITY_UNCOMMON] "Uncommon",
  [RAIDS_ITEM_RARITY_RARE] "Rare",
  [RAIDS_ITEM_RARITY_LEGENDARY] "Legendary",
  [RAIDS_ITEM_RARITY_MYTHIC] "Mythic"
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
  [RAIDS_WEAPON_MOD_WILL_O_WISP] "Will-O-Wisp"
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
void inventoryDrawDialog(InventoryDrawState_t* drawState, char* titleStr, char* bodyStr)
{
  float offX = -(INVENTORY_DRAW_WEAPONS_W/2.0);
  float offY = 0;
  u32 textColor = 0x80FFFFFF; // white
  char* buttonStr = "\x10 CONFIRM        \x12 CANCEL";

  // determine width of dialog
  float fw = gfxGetFontWidth(buttonStr, -1, 0.9);
  float fh = 20;
  if (titleStr) { fw = maxf(fw, gfxGetFontWidth(titleStr, -1, 0.9)); fh += 20; }
  if (bodyStr) { fw = maxf(fw, gfxGetFontWidth(bodyStr, -1, 0.9)); fh += 20; }

  // draw frame
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, fw, fh, 0x80101030, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

  // draw title text
  offY = -fh/2.0;
  if (titleStr) {
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.9, textColor, titleStr, -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
    offY += 20;
  }
  
  // draw price
  if (bodyStr) {
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.9, textColor, bodyStr, -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
    offY += 20;
  }
  
  // footer buttons
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.9, textColor, buttonStr, -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
  offY += 20;
}

//--------------------------------------------------------------------------
void inventoryDrawSpinner(InventoryDrawState_t* drawState, float x, float y)
{
  gfxSetupGifPaging(0);

  x -= 16; // middle align (16*(3-1))/2
  float t = (int)(4*fastmodf(gameGetTime() / 1000.0, 1.0));
  int i = 0;
  while (i < t) {
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, x, y, 16, 16, 64, 64, 80, 0x80FFFFFF, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
    ++i;
    x += 16;
  }
  
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryDrawAccountInfo(InventoryDrawState_t* drawState)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  u32 textColor = 0x80FFFFFF; // white
  u32 brightTextColor = 0x8000FFFF; // yellow
  u32 spriteColor = 0x80808080; // gray
  float fw = (INVENTORY_DRAW_FULL_W - INVENTORY_DRAW_WEAPONS_W);
  float fh = INVENTORY_DRAW_INFO_H;
  float offX = 5;
  float offY = -fh/2 + 3;
  char strBuf[64];

  if (!bankGetHasAccount()) {
    inventoryDrawSpinner(drawState, fw/2.0, -fh/4.0);
    return;
  }

  // draw box
  //gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, fw, fh, bgColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

  // stats
  snprintf(strBuf, sizeof(strBuf), "Bolts: %'d", localBank->Account.Bolts);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  offY += 12;
  snprintf(strBuf, sizeof(strBuf), "Level: %d", bankGetLevel() + 1);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  offY += 12;
  // snprintf(strBuf, sizeof(strBuf), "Skill Points: %d", localBank->Account.SkillPoints);
  // gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  offY += 12;

  // item proficiency
  gfxSetupGifPaging(0);
  int i;
  for (i = 1; i < WEAPON_SLOT_COUNT; ++i) {
    int iconSpriteId = inventoryWeaponSpriteIds[i];
    int iconSpriteDim = inventoryWeaponSpriteDims[i];
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 16, 16, iconSpriteDim, iconSpriteDim, iconSpriteId, spriteColor, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    snprintf(strBuf, sizeof(strBuf), "P%d", getProficiencyFromXp(localBank->Account.WeaponXp[i-1]) + 1);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + 0, offY + 10, 0.6, brightTextColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    offX += 25;
  }
  offY += 22;
  
  // skills
  // offX = 5;
  // for (i = 0; i < RAIDS_SKILLS_UNUSED; ++i) {
  //   gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 16, 16, 32, 32, inventorySkillSpriteIds[i], spriteColor, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  //   snprintf(strBuf, sizeof(strBuf), "%d", localBank->Account.Skills[i]);
  //   gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + 0, offY + 10, 0.7, brightTextColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  //   offX += 25;
  // }
  offY += 18;
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryDrawItemInfo(InventoryDrawState_t* drawState)
{
  u32 bgColor = 0x70101010; // dark gray
  u32 textColor = 0x80FFFFFF; // white
  u32 textRedColor = 0x804040D0; // light red
  u32 spriteColor = 0x80808080; // gray
  float fw = INVENTORY_DRAW_INFO_W;
  float fh = INVENTORY_DRAW_INFO_H;
  float offX = fw/2;
  float offY = 0;
  char strBuf[128];

  // draw box
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, fw, fh, bgColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

  RaidsInventoryItem_t* selectedItem = inventoryGetLocalItem(drawState->SelectedIdx);
  if (!selectedItem) return;

  int hasComparison = 0;
  int isBadge = bankItemIsBadge(selectedItem);
  RaidsInventoryItem_t* baseItem = NULL;
  RaidsInventoryItem_t* equippedItem = isBadge ? bankGetLocalEquippedBadge() : bankGetLocalEquippedWeapon(selectedItem->WeaponData.GadgetId);
  hasComparison = equippedItem && equippedItem != selectedItem;
  baseItem = hasComparison ? equippedItem : selectedItem;

  // name
  offX = 5;
  offY = -19;
  bankGetItemName(selectedItem, strBuf, sizeof(strBuf));
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.95, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  offY += 14;

  if (isBadge) {

    float height = 100;
    if (missionIsActive() && missionIsBossRaid()) {
      gfxHelperDrawTextWindow(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY + 80, fw - 10, 20, 5, 5, 0.7, textRedColor, "Cannot equip Class Mods in the middle of a mission.", -1, TEXT_ALIGN_TOPLEFT, FONT_WINDOW_FLAGS_NO_SCISSOR, COMMON_DZO_DRAW_NORMAL);
      height = 80;
    }

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

    return;
  }

  // stats
  offX = 10;
  snprintf(strBuf, sizeof(strBuf), "Damage: %d", (int)bankGetWeaponDamage(baseItem));
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    snprintf(strBuf, sizeof(strBuf), "> %d", (int)bankGetWeaponDamage(selectedItem));
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, inventoryDrawGetCompareColor(bankGetWeaponDamage(selectedItem) - bankGetWeaponDamage(baseItem)), strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12;
  snprintf(strBuf, sizeof(strBuf), "Critical Hit: %.f%%", (baseItem->WeaponData.CritChance / 255.0) * 100);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    snprintf(strBuf, sizeof(strBuf), "> %.f%%", (selectedItem->WeaponData.CritChance / 255.0) * 100);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, inventoryDrawGetCompareColor(selectedItem->WeaponData.CritChance - baseItem->WeaponData.CritChance), strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12;
  int baseRarity = bankGetRarityFromQuality(baseItem->Quality);
  snprintf(strBuf, sizeof(strBuf), "Rarity: %s", inventoryRarityNames[baseRarity]);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    int selRarity = bankGetRarityFromQuality(selectedItem->Quality);
    snprintf(strBuf, sizeof(strBuf), "=> %s", inventoryRarityNames[selRarity]);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, inventoryDrawGetCompareColor(selRarity - baseRarity), strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12;
  snprintf(strBuf, sizeof(strBuf), "Paint: %s", inventoryPaintNames[baseItem->WeaponData.Paint]);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    snprintf(strBuf, sizeof(strBuf), "> %s", inventoryPaintNames[selectedItem->WeaponData.Paint]);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12;
  snprintf(strBuf, sizeof(strBuf), "Special: %s", inventoryPaintSpecialNames[baseItem->WeaponData.PaintSpecialMask]);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    snprintf(strBuf, sizeof(strBuf), "> %s", inventoryPaintSpecialNames[selectedItem->WeaponData.PaintSpecialMask]);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12;
  snprintf(strBuf, sizeof(strBuf), "Mod: %s", inventoryModNames[baseItem->WeaponData.ModType]);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    snprintf(strBuf, sizeof(strBuf), "> %s", inventoryModNames[selectedItem->WeaponData.ModType]);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12;
  snprintf(strBuf, sizeof(strBuf), "Upgrades: %d/%d", baseItem->WeaponData.Upgrades, baseItem->WeaponData.MaxUpgrades);
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  if (hasComparison) {
    float strW = gfxGetFontWidth(strBuf, -1, 0.7);
    snprintf(strBuf, sizeof(strBuf), "> %d/%d", selectedItem->WeaponData.Upgrades, selectedItem->WeaponData.MaxUpgrades);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + strW + 5, offY, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
  }
  offY += 12; // since Speed is hidden

  // alpha mods
  gfxSetupGifPaging(0);
  int i;
  offX = 5;
  for (i = 1; i < ALPHA_MOD_COUNT; ++i) {
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, 16, 16, 32, 32, inventoryAlphaModSpriteIds[i], spriteColor, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    snprintf(strBuf, sizeof(strBuf), "%d", baseItem->WeaponData.AlphaModCounts[i-1]);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + 0, offY + 10, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    if (hasComparison) {
      float strW = gfxGetFontWidth(strBuf, -1, 0.7);
      snprintf(strBuf, sizeof(strBuf), ">%d", selectedItem->WeaponData.AlphaModCounts[i-1]);
      gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX + 2 + strW, offY + 11, 0.6, inventoryDrawGetCompareColor(selectedItem->WeaponData.AlphaModCounts[i-1] - baseItem->WeaponData.AlphaModCounts[i-1]), strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    }
    
    offX += 25;
  }
  gfxDoGifPaging();

}

//--------------------------------------------------------------------------
void inventoryDrawItem(InventoryDrawState_t* drawState, int row, int col, RaidsInventoryItem_t* item)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  u32 selectedColor = 0x40008080; // yellow
  u32 equippedColor = 0x40000080; // red
  float fw = (INVENTORY_DRAW_WEAPONS_W / INVENTORY_DRAW_WEAPONS_DIM);
  float fh = (INVENTORY_DRAW_WEAPONS_H / INVENTORY_DRAW_WEAPONS_DIM);
  float w = fw - INVENTORY_DRAW_WEAPONS_M*2.0;
  float h = fh - INVENTORY_DRAW_WEAPONS_M*2.0;
  float offX = -(INVENTORY_DRAW_FULL_W/2.0) + fw/2 + col*fw;
  float offY = -(INVENTORY_DRAW_WEAPONS_H/2.0) + fh/2 + row*fh ;
  int idx = row*8 + col;
  int isSelected = idx == drawState->SelectedIdx;
  int isEquipped = 0;

  // draw bg on first item
  if (col == 0 && row == 0) {
    gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -(INVENTORY_DRAW_WEAPONS_W/2.0), 0, INVENTORY_DRAW_WEAPONS_W, INVENTORY_DRAW_WEAPONS_H, 0x60101010, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw box
  if (isSelected) {
    gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, fw, fh, selectedColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // no item info
  if (!item) return;

  int isBadge = bankItemIsBadge(item);
  int isWeapon = bankItemIsWeapon(item);

  // seen
  if (isSelected && item->Notify == RAIDS_ITEM_NOTIFY_NEW) {
    item->Notify = RAIDS_ITEM_NOTIFY_NONE;
    bankSendInventoryItemToServer(item, RAIDS_ITEM_UPDATE_SET_NOTIFY);
    // UPDATE ITEM
    //inventorySetFilter(drawState->FilterIdx);
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
    gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, fw, fh, equippedColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
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
  gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX-1, offY-1, w+2, h+2, iconSpriteDim, iconSpriteDim, iconSpriteId, 0x80000000, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX, offY, w, h, iconSpriteDim, iconSpriteDim, iconSpriteId, iconColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

  // draw equipped
  if (isEquipped) {
    //gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX-w/2, offY-h/2, 12, 12, 32, 32, 80, equippedColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw notify
  if (item->Notify) {
    u32 notifyColor = item->Notify == RAIDS_ITEM_NOTIFY_NEW ? INVENTORY_NOTIFY_NEW_COLOR : INVENTORY_NOTIFY_FAV_COLOR;
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX+w/2+1, offY-h/2-1, 8, 8, 32, 32, INVENTORY_NOTIFY_SPRITE_ID, notifyColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw paint
  if (isWeapon && (item->WeaponData.Paint || item->WeaponData.PaintSpecialMask)) {
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX-w/2+1, offY+h/2-1, 8, 8, 32, 32, 80 + (item->WeaponData.PaintSpecialMask>0?1:0), bankPaintColors[item->WeaponData.Paint] | 0x80000000, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw omega
  if (isWeapon && item->WeaponData.ModType) {
    u32 omegaColor = hudGetTeamColor(TEAM_RED, 0);
    if (item->WeaponData.ModType <= RAIDS_WEAPON_MOD_SHOCK)
      omegaColor = ((u32 (*)(int))0x00541fd0)(item->WeaponData.ModType);
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, offX+w/2-2, offY+h/2-2, 12, 12, 32, 32, 79, omegaColor | 0x80000000, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  }

  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void inventoryDrawInventory(InventoryDrawState_t* drawState)
{
  int i,j;
  char strBuf[128];

  if (!inventoryGetHasInventoryPage()) {
    inventoryDrawSpinner(drawState, -INVENTORY_DRAW_WEAPONS_W/2.0, 0);
    return;
  }

  // draw tabs
  gfxSetupGifPaging(0);
  for (i = 0; i < INVENTORY_TAB_COUNT; ++i) {
    int isTabSelected = inventoryDrawState.FilterIdx == i;
    float tabW = (INVENTORY_DRAW_WEAPONS_W / INVENTORY_TAB_COUNT);
    float tabH = tabW-2;
    float tabD = tabW - INVENTORY_TAB_SPRITE_PADDING;
    float offX = (tabW * i);
    int spriteId = inventoryWeaponSpriteIds[i];
    int spriteDim = inventoryWeaponSpriteDims[i];
    int inventoryTabHasNew = inventoryPage.FilterHasNewMask & (1 << i);
    gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -(INVENTORY_DRAW_FULL_W/2) + offX, -(INVENTORY_DRAW_FULL_H/2), tabW, tabH, isTabSelected ? 0x40004040 : 0x40101010, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -(INVENTORY_DRAW_FULL_W/2) + offX + (INVENTORY_TAB_SPRITE_PADDING/2), -(INVENTORY_DRAW_FULL_H/2)+2, tabD, tabD, spriteDim, spriteDim, spriteId, isTabSelected ? 0x80FFFFFF : 0x80808080, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    if (inventoryTabHasNew) {
      gfxHelperDrawSprite(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -(INVENTORY_DRAW_FULL_W/2) + offX + tabW, -(INVENTORY_DRAW_FULL_H/2), 8, 8, 32, 32, INVENTORY_NOTIFY_SPRITE_ID, INVENTORY_NOTIFY_NEW_COLOR, TEXT_ALIGN_TOPRIGHT, COMMON_DZO_DRAW_NORMAL);
    }
    snprintf(strBuf, sizeof(strBuf), "%d", inventoryPage.TotalByFilter[i]);
    gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -(INVENTORY_DRAW_FULL_W/2) + offX + tabW, -(INVENTORY_DRAW_FULL_H/2)+2+tabD, 0.6, 0x80FFFFFF, strBuf, -1, TEXT_ALIGN_BOTTOMRIGHT, COMMON_DZO_DRAW_NORMAL);
  }
  gfxDoGifPaging();

  // draw grid
  for (i = 0; i < INVENTORY_DRAW_WEAPONS_DIM; ++i) {
    for (j = 0; j < INVENTORY_DRAW_WEAPONS_DIM; ++j) {
      int idx = (i*INVENTORY_DRAW_WEAPONS_DIM)+j;
      RaidsInventoryItem_t* item = inventoryGetLocalItem(idx);
      inventoryDrawItem(drawState, i, j, item);
    }
  }

  inventoryDrawItemInfo(drawState);
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
void inventoryDrawFooter(InventoryDrawState_t* drawState)
{
  u32 textColor = 0x80FFFFFF;
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
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -INVENTORY_DRAW_FULL_W/2 + 5, INVENTORY_DRAW_FULL_H/2 - 5, 0.8, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMLEFT, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void inventoryDraw(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  u32 bgColor = 0x60000000;
  u32 borderColor = 0x80000020;
  u32 textColor = 0x80FFFFFF;
  float borderSizeH = INVENTORY_DRAW_FRAME_BORDER_W * SCREEN_RATIO_INV;
  float borderSizeV = INVENTORY_DRAW_FRAME_BORDER_W;

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

  // draw frame
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, 0, 0, INVENTORY_DRAW_FULL_W, INVENTORY_DRAW_FULL_H, bgColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

  // draw title text
  gfxHelperDrawText(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, INVENTORY_DRAW_INFO_W/2, -INVENTORY_DRAW_FULL_H/2 + 2, 1.1, textColor, gs->PlayerNames[localPlayer->PlayerId], -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
  
  // draw inventory section
  inventoryDrawInventory(&inventoryDrawState);

  // draw account section
  inventoryDrawAccountInfo(&inventoryDrawState);

  // draw footer
  inventoryDrawFooter(&inventoryDrawState);

  // draw frame borders
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, INVENTORY_DRAW_FULL_W/2, 0, borderSizeH, INVENTORY_DRAW_FULL_H + borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, -INVENTORY_DRAW_FULL_W/2, 0, borderSizeH, INVENTORY_DRAW_FULL_H + borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, 0, INVENTORY_DRAW_FULL_H/2, INVENTORY_DRAW_FULL_W + borderSizeH, borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox(INVENTORY_DRAW_CENTER_X, INVENTORY_DRAW_CENTER_Y, 0, -INVENTORY_DRAW_FULL_H/2, INVENTORY_DRAW_FULL_W + borderSizeH, borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

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
    inventoryDrawDialog(&inventoryDrawState, sellStrBuf, sellPriceBuf);
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
      if (inventoryDrawState.FilterIdx < 0) inventoryDrawState.FilterIdx = INVENTORY_TAB_COUNT - 1;
      inventoryRequestPage();
    } else if (padGetButtonDown(0, PAD_R1) > 0) {                         // TAB RIGHT
      selIdx = 0;
      inventoryDrawState.PageIdx = 0;
      inventoryDrawState.FilterIdx = (inventoryDrawState.FilterIdx + 1) % INVENTORY_TAB_COUNT;
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
