#ifndef RAIDS_BANK_H
#define RAIDS_BANK_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define BANK_MAX_ITEMS                 (64)
#define BANK_UPDATE_SIZE               (16)
#define BANK_BADGE_EFFECT_COUNT        (8)

#define BADGE_HEALTH_BUFF_AMOUNT       (200)
#define BADGE_AMMO_MOD_BUFF_AMOUNT     (6)

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
  RAIDS_BADGE_TYPE_HEATH_BUFF,
  RAIDS_BADGE_TYPE_AMMO_BUFF,
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

enum RaidsItemTypes
{
  RAIDS_ITEM_NONE = 0,
  RAIDS_ITEM_WEAPON,
  RAIDS_ITEM_BADGE,
};

typedef struct RaidsInventoryItem
{
  char Type;
  char Notify;
  u8 Quality; // determines rarity + values on probability curve
  u32 Price;

  union {
    struct {
      int Damage; // damage
      u8 GadgetId;
      u8 Paint; // 0=none, 1=blue, etc (teams)
      u8 PaintSpecialMask; // RaidsGadgetPaintSpecialMask
      u8 Proficiency; // what proficiency the item was created at (v1-v99)
      u8 CritChance; // 0-255 (0-100%) chance crit
      u8 OmegaMod;
      u8 AlphaModCounts[ALPHA_MOD_COUNT-1];
    } WeaponData;

    struct {
      u8 Effects[BANK_BADGE_EFFECT_COUNT];
      u8 EffectStrength[BANK_BADGE_EFFECT_COUNT];
    } BadgeData;
  };
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
  double WeaponXp[WEAPON_SLOT_COUNT-1];
  u32 Experience;
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

struct RaidsBankGetMapStatsRequest
{
  u32 ResponseAddress;
  int CollectiblesCount;
  int ChallengesCount;
  char MapFilename[64];
};

struct RaidsBankMapStats
{
  int Invalid;
  int CollectiblesCount;
  u32 CollectiblesMask;
  int ChallengesCount;
  u32 ChallengesMask;
  float PercentageComplete;
  u32 BestTimeMsPerDifficulty[5];
  char MapFilename[64];
};

struct RaidsBankSetMapStatsRequest
{
  int CollectiblesCount;
  u32 CollectiblesMask;
  int ChallengesCount;
  u32 ChallengesMask;
  char MapFilename[64];
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

typedef RaidsPlayerBank_t* (*BankGetLocalBank_func)(void);
typedef void (*BankGetItemName_func)(RaidsInventoryItem_t* item, char* buf, int bufSize);
typedef enum RaidsItemRarity (*BankGetRarityFromQuality_func)(int quality);
typedef float (*BankGetEquippedBadgeEffectStrength_func)(int playerId, enum RaidsBadgeType effect);
typedef RaidsInventoryItem_t* (*BankGetEquippedWeaponFromGadgetBox_func)(GadgetBox* gbox, int gadgetId);

typedef void (*BankRequestInventoryFromServer_func)(void);
typedef void (*BankSendInventoryToServer_func)(void);
typedef void (*BankRequestAccountFromServer_func)(void);
typedef void (*BankSendAccountToServer_func)(void);
typedef void (*BankRequestMapStats_func)(char* mapFilename, struct RaidsBankMapStats* dest);

typedef int (*BankGetHasInventory_func)(void);
typedef int (*BankHasPendingInventoryRequest_func)(void);
typedef int (*BankGetHasAccount_func)(void);
typedef int (*BankHasPendingAccountRequest_func)(void);

typedef u32 (*BankGetXP_func)(void);
typedef u32 (*BankGetBolts_func)(void);
typedef u32 (*BankAddBolts_func)(u32 amt);
typedef u32 (*BankSubBolts_func)(u32 amt);
typedef double (*BankGetWeaponXP_func)(int gadgetId);
typedef double (*BankAddWeaponXP_func)(double amt, int gadgetId);

struct BankVTable
{
  BankGetLocalBank_func GetLocalBank;
  BankGetItemName_func GetItemName;
  BankGetRarityFromQuality_func GetRarityFromQuality;
  BankGetEquippedBadgeEffectStrength_func GetEquippedBadgeEffectStrength;
  BankGetEquippedWeaponFromGadgetBox_func GetEquippedWeaponFromGadgetBox;

  BankRequestInventoryFromServer_func RequestInventoryFromServer;
  BankSendInventoryToServer_func SendInventoryToServer;
  BankRequestAccountFromServer_func RequestAccountFromServer;
  BankSendAccountToServer_func SendAccountToServer;
  BankRequestMapStats_func RequestMapStats;

  BankGetHasInventory_func GetHasInventory;
  BankHasPendingInventoryRequest_func HasPendingInventoryRequest;
  BankGetHasAccount_func GetHasAccount;
  BankHasPendingAccountRequest_func HasPendingAccountRequest;
  
  BankGetXP_func GetXP;
  BankGetBolts_func GetBolts;
  BankAddBolts_func AddBolts;
  BankSubBolts_func SubBolts;
  BankGetWeaponXP_func GetWeaponXP;
  BankAddWeaponXP_func AddWeaponXP;
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
u32 bankGetXP(void);
u32 bankAddXP(u32 amount);
double bankGetWeaponXP(int gadgetId);
double bankAddWeaponXP(double amount, int gadgetId);

void bankRequestInventoryFromServer(void);
void bankSendInventoryToServer(void);
void bankRequestAccountFromServer(void);
void bankSendAccountToServer(void);
void bankRequestMapStats(char* mapFilename, struct RaidsBankMapStats* dest);
void bankSendMapStats(struct RaidsBankMapStats* mapStats);

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
RaidsInventoryItem_t* bankGetEquippedBadgeFromGadgetBox(GadgetBox* gbox);
RaidsInventoryItem_t* bankGetEquippedWeaponFromGadgetBox(GadgetBox* gbox, int gadgetId);
enum RaidsItemRarity bankGetRarityFromQuality(u8 quality);
float bankGetEquippedBadgeEffectStrength(int playerId, enum RaidsBadgeType effect);

void bankTick(void);
void bankInit(void);

#endif // RAIDS_BANK_H
