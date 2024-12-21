#ifndef RAIDS_BANK_H
#define RAIDS_BANK_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define BANK_MAX_ITEMS                 (64)
#define BANK_UPDATE_SIZE                 (16)
#define BANK_BADGE_GADGET_ID             (0x41)

enum RaidsGadgetPaintSpecialMask
{
  RAIDS_GADGET_PAINTSPECIAL_NONE = 0,
  RAIDS_GADGET_PAINTSPECIAL_GLOW = 0x01,
  RAIDS_GADGET_PAINTSPECIAL_ADDITIVE = 0x02,
};

enum RaidsItemRarity
{
  RAIDS_ITEM_RARITY_COMMON = 0,
  RAIDS_ITEM_RARITY_UNCOMMON,
  RAIDS_ITEM_RARITY_RARE,
  RAIDS_ITEM_RARITY_LEGENDARY,
  RAIDS_ITEM_RARITY_MYTHIC,
  RAIDS_ITEM_RARITY_COUNT,
};

enum RaidsInventoryItemNotify
{
  RAIDS_ITEM_NOTIFY_NONE = 0,
  RAIDS_ITEM_NOTIFY_NEW,
  RAIDS_ITEM_NOTIFY_FAV,
};

enum RaidsBadgeType
{
  RAIDS_BADGE_TYPE_NONE = 0,
  RAIDS_BADGE_TYPE_HEALTH_REGEN,
  RAIDS_BADGE_TYPE_AMMO_REGEN,
  RAIDS_BADGE_TYPE_SHARPSHOOTER,
  RAIDS_BADGE_TYPE_BERSERKER,
  RAIDS_BADGE_TYPE_FLINCH_RESISTANCE,
  RAIDS_BADGE_TYPE_EXPLOSIVE_WRENCH,
  RAIDS_BADGE_TYPE_COUNT
};

enum RaidsSkills
{
  RAIDS_SKILLS_HEALTH = 0,
  RAIDS_SKILLS_DAMAGE = 1,
  RAIDS_SKILLS_SPEED = 2,
  RAIDS_SKILLS_UNUSED = 3,
  RAIDS_SKILLS_COUNT
};

typedef struct RaidsInventoryItem
{
  int Damage; // damage
  u32 Price;
  u8 GadgetId;
  u8 Paint; // 0=none, 1=blue, etc (teams)
  u8 PaintSpecialMask; // RaidsGadgetPaintSpecialMask
  union {
    u8 Proficiency; // what proficiency the item was created at (v1-v99)
    u8 BadgeType;
  };
  u8 Quality; // determines rarity + values on probability curve
  u8 CritChance; // 0-255 (0-100%) chance crit
  u8 OmegaMod;
  char Notify;
  u8 AlphaModCounts[ALPHA_MOD_COUNT-1];
} RaidsInventoryItem_t;

typedef struct RaidsPlayerInventory
{
  RaidsInventoryItem_t Items[BANK_MAX_ITEMS];
  u32 TotalWeapons;
  int RefreshLocalInventory;
  char EquippedBadgeIdx;
  char EquippedWeaponIdxs[WEAPON_SLOT_COUNT-1];
} RaidsPlayerInventory_t;

typedef struct RaidsPlayerAccount
{
  u64 Experience;
  u64 WeaponXp[WEAPON_SLOT_COUNT-1];
  u32 Bolts;
  u32 SkillPoints;
  u16 Skills[RAIDS_SKILLS_COUNT];
} RaidsPlayerAccount_t;

typedef struct RaidsPlayerBank
{
  RaidsPlayerInventory_t Inventory;
  RaidsPlayerAccount_t Account;
} RaidsPlayerBank_t;

typedef struct RaidsPlayerEquippedInventory
{
  RaidsInventoryItem_t Items[WEAPON_SLOT_COUNT-1];
  RaidsInventoryItem_t Badge;
} RaidsPlayerEquippedInventory_t;

struct RaidsGetBankRequest
{
  u32 DestAddress;
  u32 DestHasFlagAddress;
  u32 DestTimeFlagAddress;
};

struct RaidsUpdateBankInventoryRequest
{
  int Index;
  int Count;
  RaidsInventoryItem_t Items[BANK_UPDATE_SIZE];
  char EquippedBadgeIdx;
  char EquippedWeaponIdxs[WEAPON_SLOT_COUNT-1];
};

struct RaidsBankSetPlayerEquippedInventoryMsg
{
  int ClientId;
  RaidsPlayerEquippedInventory_t EquippedInventory;
};

struct RaidsBankSetPlayerAccountMsg
{
  int ClientId;
  RaidsPlayerAccount_t Account;
};

int bankGetHasInventory(void);
int bankHasPendingInventoryRequest(void);
int bankGetHasAccount(void);
int bankHasPendingAccountRequest(void);

int bankItemIsWeapon(RaidsInventoryItem_t* item);
int bankItemIsBadge(RaidsInventoryItem_t* item);
void bankGetItemName(RaidsInventoryItem_t* item, char* buf, int bufSize);

u32 bankGetBolts(void);
u32 bankAddBolts(u32 amount);
u32 bankSubtractBolts(u32 amount);
u64 bankGetXP(void);
u64 bankAddXP(u64 amount);
u64 bankGetWeaponXP(int gadgetId);
u64 bankAddWeaponXP(u64 amount, int gadgetId);

void bankRequestInventoryFromServer(void);
void bankSendInventoryToServer(void);
void bankRequestAccountFromServer(void);
void bankSendAccountToServer(void);

RaidsPlayerBank_t* bankGetLocalBank(void);
RaidsInventoryItem_t* bankGetLocalItemFromBank(int index);
RaidsInventoryItem_t* bankGetLocalWeaponFromBank(int index);
RaidsInventoryItem_t* bankGetLocalBadgeFromBank(int index);
RaidsInventoryItem_t* bankGetLocalEquippedBadge(void);
void bankEquipLocalItemAtIndex(int weaponIdx);
void bankSellLocalItemAtIndex(int weaponIdx);
int bankGetEquipSlotFromGadgetId(int gadgetId);
RaidsInventoryItem_t* bankGetLocalEquippedWeapon(int gadgetId);
int bankGetPlayerIdxFromGadgetBox(GadgetBox* gbox);
RaidsPlayerEquippedInventory_t* bankGetEquippedFromGadgetBox(GadgetBox* gbox);
RaidsInventoryItem_t* bankGetEquippedWeaponFromGadgetBox(GadgetBox* gbox, int gadgetId);
enum RaidsItemRarity bankGetRarityFromQuality(u8 quality);

void bankOpen(void);
void bankClose(void);

void bankTick(void);
void bankInit(void);

#endif // RAIDS_BANK_H
