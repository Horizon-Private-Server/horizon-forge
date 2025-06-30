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
#define BADGE_AREA_MOD_BUFF_AMOUNT     (3)
#define BADGE_SPEED_MOD_BUFF_AMOUNT    (4)
#define BADGE_IMPACT_MOD_BUFF_AMOUNT   (4)

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
  RAIDS_BADGE_TYPE_HEALTH_BUFF,
  RAIDS_BADGE_TYPE_ALPHA_AMMO_BUFF,
  RAIDS_BADGE_TYPE_ALPHA_AREA_BUFF,
  RAIDS_BADGE_TYPE_ALPHA_SPEED_BUFF,
  RAIDS_BADGE_TYPE_ALPHA_IMPACT_BUFF,
  RAIDS_BADGE_TYPE_EXPLODING_ENEMIES,
  RAIDS_BADGE_TYPE_COUNT
};

enum RaidsWeaponModType
{
  RAIDS_WEAPON_MOD_NONE = 0,
  RAIDS_WEAPON_MOD_NAPALM,
  RAIDS_WEAPON_MOD_TIME_BOMB,
  RAIDS_WEAPON_MOD_FREEZE,
  RAIDS_WEAPON_MOD_MINI_BOMB,
  RAIDS_WEAPON_MOD_MORPH,
  RAIDS_WEAPON_MOD_BRAINWASH,
  RAIDS_WEAPON_MOD_ACID,
  RAIDS_WEAPON_MOD_SHOCK,
  RAIDS_WEAPON_MOD_WILL_O_WISP,
  RAIDS_WEAPON_MOD_LIGHTFOOT,
  RAIDS_WEAPON_MOD_COUNT
};

enum RaidsItemTypes
{
  RAIDS_ITEM_NONE = 0,
  RAIDS_ITEM_WEAPON,
  RAIDS_ITEM_BADGE,
};

enum RaidsItemUpdateAction
{
  RAIDS_ITEM_UPDATE_NONE = 0,
  RAIDS_ITEM_UPDATE_SELL,
  RAIDS_ITEM_UPDATE_SET_NOTIFY,
  RAIDS_ITEM_UPDATE_EQUIP,
  RAIDS_ITEM_UPDATE_UNEQUIP,
  RAIDS_ITEM_UPDATE_UPGRADE,
  RAIDS_ITEM_UPDATE_UPGRADE_RARITY,
  RAIDS_ITEM_UPDATE_DESTROY,
};

typedef struct RaidsInventoryItem
{
  char Type;
  char Notify;
  u8 Quality; // determines rarity + values on probability curve
  u32 Price;
  u32 Uid;

  union {
    struct {
      int Damage; // damage
      u8 GadgetId;
      u8 Paint : 4; // 0=none, 1=blue, etc (teams)
      u8 PaintSpecialMask : 4; // RaidsGadgetPaintSpecialMask
      u8 Proficiency; // what proficiency the item was created at (v1-v99)
      u8 CritChance; // 0-255 (0-100%) chance crit
      u8 ModType; // RaidsWeaponModType
      u8 ModQuality; // 0-255
      u8 AlphaModCounts[ALPHA_MOD_COUNT-1];
      u8 Upgrades;
      u8 MaxUpgrades;
    } WeaponData;

    struct {
      u8 Effects[BANK_BADGE_EFFECT_COUNT];
      u8 EffectStrength[BANK_BADGE_EFFECT_COUNT];
    } BadgeData;
  };
} RaidsInventoryItem_t;

typedef struct RaidsPlayerInventoryPage
{
  RaidsInventoryItem_t Items[BANK_MAX_ITEMS];
  u32 Total;
  u16 TotalByFilter[9];
  u16 FilterHasNewMask;
  int RefreshLocalInventory;
  int Filter;
  int Page;
  int HasFlag;
  long LastRequestTime;
} RaidsPlayerInventoryPage_t;

typedef struct RaidsPlayerAccount
{
  double WeaponXp[WEAPON_SLOT_COUNT-1];
  u64 Experience;
  u32 Bolts;
} RaidsPlayerAccount_t;

typedef struct RaidsPlayerEquippedInventory
{
  RaidsInventoryItem_t Items[WEAPON_SLOT_COUNT-1];
  RaidsInventoryItem_t Badge;
  int RefreshLocalInventory;
} RaidsPlayerEquippedInventory_t;

typedef struct RaidsPlayerBank
{
  RaidsPlayerAccount_t Account;
  RaidsPlayerEquippedInventory_t EquippedInventory;
} RaidsPlayerBank_t;

struct RaidsGetBankRequest
{
  u32 DestAddress;
  u32 DestHasFlagAddress;
  u32 DestTimeFlagAddress;
  int Filter;
  int Page;
};

struct RaidsUpdateBankInventoryItemRequest
{
  RaidsInventoryItem_t Item;
  char Action;
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
  int MissionType;
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
typedef enum RaidsItemRarity (*BankGetEquippedWeaponModRarity_func)(int playerId, int gadgetId, enum RaidsWeaponModType modType);
typedef RaidsInventoryItem_t* (*BankGetEquippedWeaponFromGadgetBox_func)(GadgetBox* gbox, int gadgetId);

typedef void (*BankRequestInventoryFromServer_func)(RaidsPlayerInventoryPage_t* inventory, int filter, int page);
typedef void (*BankSendInventoryItemToServer_func)(RaidsInventoryItem_t* item, enum RaidsItemUpdateAction action);
typedef void (*BankRequestEquippedInventoryFromServer_func)(void);
typedef void (*BankRequestAccountFromServer_func)(void);
typedef void (*BankSendAccountToServer_func)(void);
typedef void (*BankRequestMapStats_func)(char* mapFilename, struct RaidsBankMapStats* dest, int missionType);

typedef int (*BankGetHasEquippedInventory_func)(void);
typedef int (*BankHasPendingEquippedInventoryRequest_func)(void);
typedef int (*BankGetHasAccount_func)(void);
typedef int (*BankHasPendingAccountRequest_func)(void);

typedef u64 (*BankGetXP_func)(void);
typedef u64 (*BankAddXP_func)(u64 amt);
typedef int (*BankGetLevel_func)(void);
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
  BankGetEquippedWeaponModRarity_func GetEquippedWeaponModRarity;
  BankGetEquippedWeaponFromGadgetBox_func GetEquippedWeaponFromGadgetBox;

  BankRequestInventoryFromServer_func RequestInventoryFromServer;
  BankSendInventoryItemToServer_func SendInventoryItemToServer;
  BankRequestEquippedInventoryFromServer_func RequestEquippedInventoryFromServer;
  BankRequestAccountFromServer_func RequestAccountFromServer;
  BankSendAccountToServer_func SendAccountToServer;
  BankRequestMapStats_func RequestMapStats;

  BankGetHasEquippedInventory_func GetHasEquippedInventory;
  BankHasPendingEquippedInventoryRequest_func HasPendingEquippedInventoryRequest;
  BankGetHasAccount_func GetHasAccount;
  BankHasPendingAccountRequest_func HasPendingAccountRequest;
  
  BankGetXP_func GetXP;
  BankAddXP_func AddXP;
  BankGetLevel_func GetLevel;
  BankGetBolts_func GetBolts;
  BankAddBolts_func AddBolts;
  BankSubBolts_func SubBolts;
  BankGetWeaponXP_func GetWeaponXP;
  BankAddWeaponXP_func AddWeaponXP;
};

int bankGetHasEquippedInventory(void);
int bankHasPendingEquippedInventoryRequest(void);
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
int bankGetLevel(void);
float bankGetLevelProgress(void);
double bankGetWeaponXP(int gadgetId);
double bankAddWeaponXP(double amount, int gadgetId);
float bankGetWeaponDamage(RaidsInventoryItem_t* item);

void bankRequestInventoryFromServer(RaidsPlayerInventoryPage_t* inventory, int filter, int page);
void bankSendInventoryItemToServer(RaidsInventoryItem_t* item, enum RaidsItemUpdateAction action);
void bankRequestEquippedInventoryFromServer(void);
void bankRequestAccountFromServer(void);
void bankSendAccountToServer(void);
void bankRequestAccountReset(void);
void bankRequestMapStats(char* mapFilename, struct RaidsBankMapStats* dest, int missionType);
void bankSendMapStats(struct RaidsBankMapStats* mapStats);

RaidsPlayerBank_t* bankGetLocalBank(void);
RaidsInventoryItem_t* bankGetLocalItemFromBank(int index);
RaidsInventoryItem_t* bankGetLocalWeaponFromBank(int index);
RaidsInventoryItem_t* bankGetLocalBadgeFromBank(int index);
RaidsInventoryItem_t* bankGetLocalEquippedBadge(void);
int bankEquipItem(RaidsInventoryItem_t* item);
int bankSellItem(RaidsInventoryItem_t* item);
int bankGetEquipSlotFromGadgetId(int gadgetId);
RaidsInventoryItem_t* bankGetLocalEquippedWeapon(int gadgetId);
int bankGetPlayerIdxFromGadgetBox(GadgetBox* gbox);
RaidsPlayerEquippedInventory_t* bankGetEquippedFromGadgetBox(GadgetBox* gbox);
RaidsInventoryItem_t* bankGetEquippedBadgeFromGadgetBox(GadgetBox* gbox);
RaidsInventoryItem_t* bankGetEquippedWeaponFromGadgetBox(GadgetBox* gbox, int gadgetId);
enum RaidsItemRarity bankGetRarityFromQuality(u8 quality);
float bankGetEquippedBadgeEffectStrength(int playerId, enum RaidsBadgeType effect);
int bankGetEquippedWeaponModRarity(int playerId, int gadgetId, enum RaidsWeaponModType modType);
u32 bankGetWeaponOmegaModColor(RaidsInventoryItem_t* item);

void bankTick(void);
void bankInit(void);

#endif // RAIDS_BANK_H
