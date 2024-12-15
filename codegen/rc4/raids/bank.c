#include <string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/stdlib.h>
#include <libdl/color.h>
#include <libdl/moby.h>
#include <libdl/sound.h>
#include <libdl/random.h>
#include <libdl/hud.h>
#include <libdl/utils.h>
#include <libdl/net.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include "game.h"
#include "bank.h"
#include "common.h"

extern struct RaidsMapConfig MapConfig;

char* bankBadgeNames[] = {
  [RAIDS_BADGE_TYPE_HEALTH_REGEN] "%cHealth Regen %s\x08",
  [RAIDS_BADGE_TYPE_AMMO_REGEN] "%cAmmo Regen %s\x08",
  [RAIDS_BADGE_TYPE_SHARPSHOOTER] "%cSharpshooter %s\x08",
  [RAIDS_BADGE_TYPE_BERSERKER] "%cBerserker %s\x08",
  [RAIDS_BADGE_TYPE_FLINCH_RESISTANCE] "%cFlinch Resistance %s\x08",
  [RAIDS_BADGE_TYPE_EXPLOSIVE_WRENCH] "%cExplosive Wrench %s\x08",
  [RAIDS_BADGE_TYPE_EXTRALIFE] "%cSecond Chance\x08",
  [RAIDS_BADGE_TYPE_COUNT] NULL,
};

char* bankBadgeLevelNames[] = {
  [RAIDS_ITEM_RARITY_COMMON] "I",
  [RAIDS_ITEM_RARITY_UNCOMMON] "II",
  [RAIDS_ITEM_RARITY_RARE] "III",
  [RAIDS_ITEM_RARITY_LEGENDARY] "IV",
  [RAIDS_ITEM_RARITY_MYTHIC] "V",
};

char bankRarityCode[] = {
  [RAIDS_ITEM_RARITY_COMMON] '\x08',
  [RAIDS_ITEM_RARITY_UNCOMMON] '\x0A',
  [RAIDS_ITEM_RARITY_RARE] '\x09',
  [RAIDS_ITEM_RARITY_LEGENDARY] '\x0B',
  [RAIDS_ITEM_RARITY_MYTHIC] '\x0E'
};

//--------------------------------------------------------------------------
int bankItemIsWeapon(RaidsInventoryItem_t* item)
{
  return item && item->GadgetId && item->GadgetId != BANK_BADGE_GADGET_ID;
}

//--------------------------------------------------------------------------
int bankItemIsBadge(RaidsInventoryItem_t* item)
{
  return item && item->GadgetId == BANK_BADGE_GADGET_ID && item->BadgeType > RAIDS_BADGE_TYPE_NONE && item->BadgeType < RAIDS_BADGE_TYPE_COUNT;
}

//--------------------------------------------------------------------------
int bankGetEquipSlotFromGadgetId(int gadgetId)
{
  return weaponIdToSlot(gadgetId) - 1;
}

//--------------------------------------------------------------------------
enum RaidsItemRarity bankGetRarityFromQuality(u8 quality)
{
  if (quality < 64) return RAIDS_ITEM_RARITY_COMMON;
  if (quality < 128) return RAIDS_ITEM_RARITY_UNCOMMON;
  if (quality < 196) return RAIDS_ITEM_RARITY_RARE;
  if (quality < 255) return RAIDS_ITEM_RARITY_LEGENDARY;
  return RAIDS_ITEM_RARITY_MYTHIC;
}

//--------------------------------------------------------------------------
void bankGetItemName(RaidsInventoryItem_t* item, char* buf, int bufSize)
{
  if (!item) return;

  int rarity = bankGetRarityFromQuality(item->Quality);
  if (bankItemIsBadge(item)) {
    snprintf(buf, bufSize, bankBadgeNames[item->BadgeType], bankRarityCode[rarity], bankBadgeLevelNames[rarity]);
  } else {
    struct GadgetDef* gadgetDef = weaponGetDef(item->GadgetId, 0);
    snprintf(buf, bufSize, "%c%s P%d\x08", bankRarityCode[rarity], uiMsgString(rarity >= RAIDS_ITEM_RARITY_LEGENDARY ? gadgetDef->upgQSTag : gadgetDef->quickSelectTag), item->Proficiency + 1);
  }
}

//--------------------------------------------------------------------------
int bankGetPlayerIdxFromGadgetBox(GadgetBox* gbox)
{
  if (!gbox) return NULL;
  
  return gbox->Initialized - 1;
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* bankGetLocalEquippedBadge(void)
{
  if (!MapConfig.GetBankFunc) return NULL;

  RaidsPlayerBank_t* bank = MapConfig.GetBankFunc();
  if (!bank) return NULL;
  
  int idx = bank->Inventory.EquippedBadgeIdx;
  if (idx < 0) return NULL;
  RaidsInventoryItem_t* item = &bank->Inventory.Items[idx];

  if (!bankItemIsBadge(item)) return NULL;
  return item;
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* bankGetLocalEquippedWeapon(int gadgetId)
{
  if (!MapConfig.GetBankFunc) return NULL;

  RaidsPlayerBank_t* bank = MapConfig.GetBankFunc();
  if (!bank) return NULL;
  
  int slotId = bankGetEquipSlotFromGadgetId(gadgetId);
  if (slotId < 0) return NULL;

  int equipIdx = bank->Inventory.EquippedWeaponIdxs[slotId];
  if (equipIdx < 0) return NULL;
  RaidsInventoryItem_t* item = &bank->Inventory.Items[equipIdx];

  if (item->GadgetId != gadgetId) return NULL;
  return item;
}
