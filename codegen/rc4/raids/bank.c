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
  return item && item->Type == RAIDS_ITEM_WEAPON;
}

//--------------------------------------------------------------------------
int bankItemIsBadge(RaidsInventoryItem_t* item)
{
  return item && item->Type == RAIDS_ITEM_BADGE;
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
  if (quality < 192) return RAIDS_ITEM_RARITY_RARE;
  if (quality < 255) return RAIDS_ITEM_RARITY_LEGENDARY;
  return RAIDS_ITEM_RARITY_MYTHIC;
}

//--------------------------------------------------------------------------
RaidsPlayerBank_t* bankGetLocalBank(void)
{
  if (MapConfig.GetBankFunc) return MapConfig.GetBankFunc();

  return NULL;
}

//--------------------------------------------------------------------------
void bankGetItemName(RaidsInventoryItem_t* item, char* buf, int bufSize)
{
  if (!item) return;

  int rarity = bankGetRarityFromQuality(item->Quality);
  if (bankItemIsBadge(item)) {
    snprintf(buf, bufSize, "%cClass Mod", bankRarityCode[rarity]);
  } else {
    struct GadgetDef* gadgetDef = weaponGetDef(item->WeaponData.GadgetId, 0);
    snprintf(buf, bufSize, "%c%s P%d\x08", bankRarityCode[rarity], uiMsgString(rarity >= RAIDS_ITEM_RARITY_LEGENDARY ? gadgetDef->upgQSTag : gadgetDef->quickSelectTag), item->WeaponData.Proficiency + 1);
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

  if (item->Type != RAIDS_ITEM_WEAPON) return NULL;
  if (item->WeaponData.GadgetId != gadgetId) return NULL;
  return item;
}

//--------------------------------------------------------------------------
float bankGetEquippedBadgeEffectStrength(int playerId, enum RaidsBadgeType effect)
{
  if (!MapConfig.State) return 0;

  RaidsInventoryItem_t* badge = &MapConfig.State->PlayerStates[playerId].Inventory.Badge;
  if (!badge || badge->Type != RAIDS_ITEM_BADGE) return 0;

  int i;
  for (i = 0; i < BANK_BADGE_EFFECT_COUNT; ++i) {
    if (badge->BadgeData.Effects[i] == effect)
      return badge->BadgeData.EffectStrength[i] / 255.0;
  }

  return 0;
}
