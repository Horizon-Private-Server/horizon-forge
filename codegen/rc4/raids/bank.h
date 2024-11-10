#ifndef RAIDS_BANK_H
#define RAIDS_BANK_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define BANK_MAX_WEAPONS                 (64)
#define BANK_UPDATE_WEAPONS_SIZE         (16)

enum RaidsGadgetPaintSpecialMask
{
  RAIDS_GADGET_PAINTSPECIAL_NONE = 0,
  RAIDS_GADGET_PAINTSPECIAL_GLOW = 0x01,
  RAIDS_GADGET_PAINTSPECIAL_ADDITIVE = 0x02,
};

enum RaidsWeaponRarity
{
  RAIDS_WEAPON_RARITY_COMMON = 0,
  RAIDS_WEAPON_RARITY_UNCOMMON,
  RAIDS_WEAPON_RARITY_RARE,
  RAIDS_WEAPON_RARITY_LEGENDARY,
  RAIDS_WEAPON_RARITY_COUNT,
};

enum RaidsInventoryWeaponNotify
{
  RAIDS_WEAPON_NOTIFY_NONE = 0,
  RAIDS_WEAPON_NOTIFY_NEW,
  RAIDS_WEAPON_NOTIFY_FAV,
};

enum RaidsSkills
{
  RAIDS_SKILLS_HEALTH = 0,
  RAIDS_SKILLS_DAMAGE = 1,
  RAIDS_SKILLS_SPEED = 2,
  RAIDS_SKILLS_UNUSED = 3,
  RAIDS_SKILLS_COUNT
};

typedef struct RaidsInventoryWeapon
{
  int Damage; // damage
  float Speed;  // speed of projectile
  u8 GadgetId;
  u8 Paint; // 0=none, 1=blue, etc (teams)
  u8 PaintSpecialMask; // RaidsGadgetPaintSpecialMask
  u8 Proficiency; // what proficiency the item was created at (v1-v99)
  u8 Quality; // determines rarity + values on probability curve
  u8 CritChance; // 0-255 (0-100%) chance crit
  u8 OmegaMod;
  char Notify;
  u8 AlphaModCounts[ALPHA_MOD_COUNT-1];
} RaidsInventoryWeapon_t;

typedef struct RaidsPlayerInventory
{
  RaidsInventoryWeapon_t Weapons[BANK_MAX_WEAPONS];
  u32 TotalWeapons;
  int RefreshLocalInventory;
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
  RaidsInventoryWeapon_t Weapons[WEAPON_SLOT_COUNT-1];
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
  RaidsInventoryWeapon_t Weapons[BANK_UPDATE_WEAPONS_SIZE];
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
RaidsInventoryWeapon_t* bankGetLocalWeaponFromBank(int index);
void bankEquipLocalWeaponAtIndex(int weaponIdx);
void bankSellLocalWeaponAtIndex(int weaponIdx);
int bankGetEquipSlotFromGadgetId(int gadgetId);
RaidsInventoryWeapon_t* bankGetLocalEquippedWeapon(int gadgetId);
RaidsPlayerEquippedInventory_t* bankGetEquippedFromGadgetBox(GadgetBox* gbox);
RaidsInventoryWeapon_t* bankGetEquippedWeaponFromGadgetBox(GadgetBox* gbox, int gadgetId);
enum RaidsWeaponRarity bankGetRarityFromQuality(u8 quality);

void bankOpen(void);
void bankClose(void);

void bankTick(void);
void bankInit(void);

#endif // RAIDS_BANK_H
