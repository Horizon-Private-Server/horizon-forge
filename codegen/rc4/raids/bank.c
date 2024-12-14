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
#include "utils.h"
#include "game.h"
#include "bank.h"
#include "common.h"

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
int bankGetPlayerIdxFromGadgetBox(GadgetBox* gbox)
{
  if (!gbox) return NULL;
  
  return gbox->Initialized - 1;
}
