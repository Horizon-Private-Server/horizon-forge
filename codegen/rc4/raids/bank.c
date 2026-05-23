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
#include "maputils.h"
#include "game.h"
#include "bank.h"
#include "common.h"

extern struct RaidsState State;

// send as binary payload from server
RaidsPlayerBank_t bankLocalBank = {};
struct BankVTable bankVTable = {
  .GetLocalBank = &bankGetLocalBank,
  .GetItemName = &bankGetItemName,
  .GetRarityFromQuality = &bankGetRarityFromQuality,
  .GetEquippedBadgeEffectStrength = &bankGetEquippedBadgeEffectStrength,
  .GetEquippedWeaponModRarity = &bankGetEquippedWeaponModRarity,
  .GetEquippedWeaponFromGadgetBox = &bankGetEquippedWeaponFromGadgetBox,

  .RequestInventoryFromServer = &bankRequestInventoryFromServer,
  .SendInventoryItemToServer = &bankSendInventoryItemToServer,
  .RequestEquippedInventoryFromServer = &bankRequestEquippedInventoryFromServer,
  .RequestAccountFromServer = &bankRequestAccountFromServer,
  .SendAccountToServer = &bankSendAccountToServer,
  .RequestContractsFromServer = &bankRequestContractsFromServer,
  .SendContractStatsToServer = &bankSendContractStatsToServer,
  .RequestMapStats = &bankRequestMapStats,

  .GetHasEquippedInventory = &bankGetHasEquippedInventory,
  .HasPendingEquippedInventoryRequest = &bankHasPendingEquippedInventoryRequest,
  .GetHasAccount = &bankGetHasAccount,
  .HasPendingAccountRequest = &bankHasPendingAccountRequest,
  .GetHasContracts = &bankGetHasContracts,
  .HasPendingContractsRequest = &bankHasPendingContractsRequest,

  .GetXP = &bankGetXP,
  .AddXP = &bankAddXP,
  .GetLevel = &bankGetLevel,
  .GetBolts = &bankGetBolts,
  .AddBolts = &bankAddBolts,
  .SubBolts = &bankSubtractBolts,
  .GetWeaponXP = &bankGetWeaponXP,
  .AddWeaponXP = &bankAddWeaponXP,
};

u32 bankRarityColors[] = {
  [RAIDS_ITEM_RARITY_COMMON] 0x80D0D0D0,
  [RAIDS_ITEM_RARITY_UNCOMMON] 0x80000000,
  [RAIDS_ITEM_RARITY_RARE] 0x80000000,
  [RAIDS_ITEM_RARITY_LEGENDARY] 0x80000000,
  [RAIDS_ITEM_RARITY_MYTHIC] 0x80000000,
};

u32 bankPaintColors[] = {
  0x00FFFFFF,         // white
  0x00FF6000,         // blue
  0x000000FF,         // red
  0x0006FF00,         // green
  0x00008AFF,         // orange
  0x0000EAFF,         // yellow
  0x00FF00E4,         // purple
  0x00F0FF00,         // aqua
  0x00C674FF,         // pink
  0x0000FF9C,         // olive
  0x006000FF,         // maroon
};

int bankHasEquippedInventory = 0;
int bankHasAccount = 0;
int bankHasContracts = 0;
long bankLastEquippedInventoryRequestTime = 0;
long bankLastAccountRequestTime = 0;
long bankLastContractsRequestTime = 0;
char bankLevelUpBuf[64];

char bankRarityCode[] = {
  [RAIDS_ITEM_RARITY_COMMON] '\x08',
  [RAIDS_ITEM_RARITY_UNCOMMON] '\x0A',
  [RAIDS_ITEM_RARITY_RARE] '\x09',
  [RAIDS_ITEM_RARITY_LEGENDARY] '\x0B',
  [RAIDS_ITEM_RARITY_MYTHIC] '\x0E'
};

//--------------------------------------------------------------------------
int bankOnSetPlayerEquippedInventoryRemote(void * connection, void * data)
{
  struct RaidsBankSetPlayerEquippedInventoryMsg msg;
  memcpy(&msg, data, sizeof(msg));

  struct RaidsState* state = MapConfig.State;
  if (!state) return sizeof(msg);

  GameSettings* gs = gameGetSettings();
  if (!gs) return sizeof(msg);

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (gs->PlayerClients[i] != msg.ClientId) continue;

    memcpy(&state->PlayerStates[i].Inventory, &msg.EquippedInventory, sizeof(state->PlayerStates[i].Inventory));
  }

  return sizeof(msg);
}

//--------------------------------------------------------------------------
int bankOnSetPlayerAccountRemote(void * connection, void * data)
{
  struct RaidsBankSetPlayerAccountMsg msg;
  memcpy(&msg, data, sizeof(msg));

  struct RaidsState* state = MapConfig.State;
  if (!state) return sizeof(msg);

  GameSettings* gs = gameGetSettings();
  if (!gs) return sizeof(msg);

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (gs->PlayerClients[i] != msg.ClientId) continue;

    state->PlayerStates[i].State.Level = getLevelFromXp(msg.Account.Experience);
  }

  return sizeof(msg);
}

//--------------------------------------------------------------------------
void bankBroadcastEquippedInventory(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  Player* player = playerGetFromSlot(0);
  if (!player) return;

  int playerId = player->PlayerId;

  // broadcast
  void* connection = netGetDmeServerConnection();
  if (connection) {
    struct RaidsBankSetPlayerEquippedInventoryMsg msg = {
      .ClientId = gameGetMyClientId()
    };
    memcpy(&msg.EquippedInventory, &state->PlayerStates[playerId].Inventory, sizeof(msg.EquippedInventory));
    netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CUSTOM_MSG_SET_PLAYER_EQUIPPED_INVENTORY, sizeof(msg), &msg);
  }
}

//--------------------------------------------------------------------------
void bankBroadcastAccount(void)
{
  // broadcast
  void* connection = netGetDmeServerConnection();
  if (connection) {
    struct RaidsBankSetPlayerAccountMsg msg = {
      .ClientId = gameGetMyClientId()
    };
    memcpy(&msg.Account, &bankLocalBank.Account, sizeof(msg.Account));
    netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection, CUSTOM_MSG_SET_PLAYER_ACCOUNT, sizeof(msg), &msg);
  }
}

//--------------------------------------------------------------------------
int bankGetHasEquippedInventory(void)
{
  return bankHasEquippedInventory;
}

//--------------------------------------------------------------------------
int bankHasPendingEquippedInventoryRequest(void)
{
  long dtMs = (timerGetSystemTime() - bankLastEquippedInventoryRequestTime) / SYSTEM_TIME_TICKS_PER_MS;
  return bankLastEquippedInventoryRequestTime && dtMs < (2*TIME_SECOND);
}

//--------------------------------------------------------------------------
int bankGetHasAccount(void)
{
  return bankHasAccount;
}

//--------------------------------------------------------------------------
int bankHasPendingAccountRequest(void)
{
  long dtMs = (timerGetSystemTime() - bankLastAccountRequestTime) / SYSTEM_TIME_TICKS_PER_MS;
  return bankLastAccountRequestTime && dtMs < (2*TIME_SECOND);
}

//--------------------------------------------------------------------------
int bankGetHasContracts(void)
{
  return bankHasContracts;
}

//--------------------------------------------------------------------------
int bankHasPendingContractsRequest(void)
{
  long dtMs = (timerGetSystemTime() - bankLastContractsRequestTime) / SYSTEM_TIME_TICKS_PER_MS;
  return bankLastContractsRequestTime && dtMs < (2*TIME_SECOND);
}

//--------------------------------------------------------------------------
void bankRequestEquippedInventoryFromServer(void)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  bankLastEquippedInventoryRequestTime = timerGetSystemTime();
  bankHasEquippedInventory = 0;
  struct RaidsGetBankRequest msg = {
    .DestAddress = (u32)&bankLocalBank.EquippedInventory,
    .DestHasFlagAddress = (u32)&bankHasEquippedInventory,
    .DestTimeFlagAddress = (u32)&bankLastEquippedInventoryRequestTime
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_GET_RAIDS_BANK_EQUIPPED_INVENTORY_REQUEST, sizeof(msg), &msg);
  DPRINTF("request equipped inventory\n");
}

//--------------------------------------------------------------------------
void bankRequestInventoryFromServer(RaidsPlayerInventoryPage_t* inventory, int filter, int page)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  inventory->LastRequestTime = timerGetSystemTime();
  inventory->HasFlag = 0;
  struct RaidsGetBankRequest msg = {
    .DestAddress = (u32)inventory,
    .DestHasFlagAddress = (u32)&inventory->HasFlag,
    .DestTimeFlagAddress = (u32)&inventory->LastRequestTime,
    .Filter = filter,
    .Page = page
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_GET_RAIDS_BANK_INVENTORY_REQUEST, sizeof(msg), &msg);
  DPRINTF("request inventory\n");
}

//--------------------------------------------------------------------------
void bankSendInventoryItemToServer(RaidsInventoryItem_t* item, enum RaidsItemUpdateAction action)
{
  struct RaidsUpdateBankInventoryItemRequest msg;

  if (!item) return;
  if (!item->Type) return;

  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  if (!localBank) return;
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  memcpy(&msg.Item, item, sizeof(RaidsInventoryItem_t));
  msg.Action = action;
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_UPDATE_RAIDS_BANK_INVENTORY_ITEM_REQUEST, sizeof(msg), &msg);
  DPRINTF("sent inventory item %08X action:%d\n", item->Uid, action);
}

//--------------------------------------------------------------------------
void bankRequestAccountFromServer(void)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  bankLastAccountRequestTime = timerGetSystemTime();
  bankHasAccount = 0;
  struct RaidsGetBankRequest msg = {
    .DestAddress = (u32)&bankLocalBank.Account,
    .DestHasFlagAddress = (u32)&bankHasAccount,
    .DestTimeFlagAddress = (u32)&bankLastAccountRequestTime
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_GET_RAIDS_BANK_ACCOUNT_REQUEST, sizeof(msg), &msg);
  DPRINTF("request account\n");
}

//--------------------------------------------------------------------------
void bankSendAccountToServer(void)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  if (!localBank) return;
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;
  if (!bankHasAccount) return;

  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_UPDATE_RAIDS_BANK_ACCOUNT_REQUEST, sizeof(localBank->Account), &localBank->Account);
  DPRINTF("sent account\n");
}

//--------------------------------------------------------------------------
void bankRequestAccountReset(void)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  if (!localBank) return;
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_RESET_ACCOUNT_REQUEST, 0, NULL);
  DPRINTF("sent account reset request\n");
}

//--------------------------------------------------------------------------
void bankRequestContractsFromServer(void)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  struct RaidsGetContractsRequest msg = {
    .DestAddress = (u32)&bankLocalBank.Contracts,
    .DestHasFlagAddress = (u32)&bankHasContracts,
  };

  // when requesting contracts
  // we want to avoid a situation where our local progress is overwritten by the server
  // so send the current stats over to make sure the server has the latest
  if (bankHasContracts) {
    int i;
    for (i = 0; i < BANK_MAX_CONTRACTS; ++i) {
      RaidsContract_t* contract = &bankLocalBank.Contracts[i];
      if (!contract->Uid || !contract->Activated) continue;

      msg.ContractStats[i].ContractUid = contract->Uid;
      msg.ContractStats[i].Kills = contract->Kills;
      msg.ContractStats[i].CompletedTimeMs = contract->CompletedTimeMs;
    }
  }

  bankLastContractsRequestTime = timerGetSystemTime();
  bankHasContracts = 0;
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_GET_RAIDS_CONTRACTS_REQUEST, sizeof(msg), &msg);
  DPRINTF("request contracts\n");
}

//--------------------------------------------------------------------------
void bankSendContractStatsToServer(RaidsContract_t* contract)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;
  if (!contract) return;

  struct RaidsUpdateContractStatsRequest msg = {
    .ContractUid = contract->Uid,
    .Kills = contract->Kills,
    .CompletedTimeMs = contract->CompletedTimeMs
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_UPDATE_CONTRACT_STATS_REQUEST, sizeof(msg), &msg);
  DPRINTF("send contract stats\n");
}

//--------------------------------------------------------------------------
void bankRequestMapStats(char* mapFilename, char* mapName, struct RaidsBankMapStats* dest, int missionType)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  if (!localBank) return;
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;
  
  struct RaidsBankGetMapStatsRequest msg = {
    .ResponseAddress = (u32)dest,
    .ChallengesCount = dest->ChallengesCount,
    .CollectiblesCount = dest->CollectiblesCount,
    .MissionType = missionType
  };

  if (strstr(mapFilename, "..") || strstr(mapName, "..")) return;
  safe_strcpy(msg.MapFilename, mapFilename, sizeof(msg.MapFilename));
  safe_strcpy(msg.MapName, mapName, sizeof(msg.MapName));
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_GET_MAP_STATS_REQUEST, sizeof(msg), &msg);
}

//--------------------------------------------------------------------------
void bankSendMapStats(struct RaidsBankMapStats* mapStats)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;
  if (!mapStats || mapStats->Invalid || !mapStats->MapFilename[0]) return;

  struct RaidsBankSetMapStatsRequest msg;
  msg.ChallengesCount = mapStats->ChallengesCount;
  msg.ChallengesMask = mapStats->ChallengesMask;
  msg.CollectiblesCount = mapStats->CollectiblesCount;
  msg.CollectiblesMask = mapStats->CollectiblesMask;
  safe_strcpy(msg.MapFilename, mapStats->MapFilename, sizeof(msg.MapFilename));
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_SET_MAP_STATS_REQUEST, sizeof(msg), &msg);
  DPRINTF("sent map stats\n");
}

//--------------------------------------------------------------------------
int bankSendMapContractRules(void)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return 0;
  if (!MapConfig.State || !MapConfig.State->CurrentMapDef || !MapConfig.State->CurrentMapDef->Filename[0]) return 0;

  struct RaidsBankUpdateMapContractRulesRequest
  {
    char MapFilename[64];
    struct RaidsMobContractRule ContractRules[16];
  };

  struct RaidsBankUpdateMapContractRulesRequest msg;
  memset(&msg, 0, sizeof(msg));
  safe_strcpy(msg.MapFilename, MapConfig.State->CurrentMapDef->Filename, sizeof(msg.MapFilename));
  memcpy(msg.ContractRules, MapConfig.MobContractRules, sizeof(struct RaidsMobContractRule) * MapConfig.MobContractRulesCount);
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_UPDATE_MAP_CONTRACT_RULES_REQUEST, sizeof(msg), &msg);
  DPRINTF("bankSendMapContractRules\n");
  return 1;
}

//--------------------------------------------------------------------------
void bankSendMapMobMetadata(int mobOClass, int difficultyStars)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;
  if (!isInGame() || !missionIsActive()) return;
  if (!MapConfig.State) return;
  if (!MapConfig.State->CurrentMapDef) return;

  struct RaidsBankUpdateMapMetadataRequest msg = {
    .AddMobOClass = mobOClass,
    .AddMobOClassAtDifficulty = difficultyStars
  };
  safe_strcpy(msg.MapFilename, MapConfig.State->CurrentMapDef->Filename, sizeof(msg.MapFilename));
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_UPDATE_MAP_METADATA_REQUEST, sizeof(msg), &msg);
  DPRINTF("bankSendMapMobMetadata\n");
}

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
u32 bankGetBolts(void) { return bankLocalBank.Account.Bolts; }
u32 bankAddBolts(u32 amount) { return bankLocalBank.Account.Bolts += amount; }
u32 bankSubtractBolts(u32 amount)
{
  if (amount >= bankLocalBank.Account.Bolts) bankLocalBank.Account.Bolts = 0;
  else bankLocalBank.Account.Bolts -= amount;

  return amount;
}

//--------------------------------------------------------------------------
u64 bankGetXP(void) { return bankLocalBank.Account.Experience; }
u64 bankAddXP(u64 amount)
{
  int level = bankGetLevel();

  // add xp
  u64 xp = bankLocalBank.Account.Experience;
  bankLocalBank.Account.Experience += amount;

  int nextLevel = bankGetLevel();
  //printf("add xp %'d (+%'d)\n", xp, amount);
  if (nextLevel > level) {
    //bankLocalBank.Account.SkillPoints += 1;

    snprintf(bankLevelUpBuf, sizeof(bankLevelUpBuf), "You have reached level %d", nextLevel + 1);
    pushSnack(0, bankLevelUpBuf, 120);
    bankSendAccountToServer(); // send to server
    bankUpdateLocalState(playerGetFromSlot(0));
  }

  return bankLocalBank.Account.Experience;
}

//--------------------------------------------------------------------------
int bankGetLevel(void)
{
  static u64 lastXp = 0;
  static int lastLevel = 0;

  u64 xp = bankLocalBank.Account.Experience;
  if (xp == lastXp) return lastLevel;

  lastXp = xp;
  return lastLevel = getLevelFromXp(xp);
}

//--------------------------------------------------------------------------
float bankGetLevelProgress(void)
{
  u64 xp = bankLocalBank.Account.Experience;
  int level = getLevelFromXp(xp);
  if (level >= LEVELUP_MAX_PLAYER_LEVEL) return 1.0f;

  u64 lastXp = getXpForLevel(level);
  u64 nextXp = getXpForLevel(level + 1);
  if (xp < lastXp) xp = lastXp;
  float xpPerc = (float)((xp - lastXp) / (double)(nextXp - lastXp));
  return xpPerc;
}

//--------------------------------------------------------------------------
double bankGetWeaponXP(int gadgetId) { return bankLocalBank.Account.WeaponXp[bankGetEquipSlotFromGadgetId(gadgetId)]; }
double bankAddWeaponXP(double amount, int gadgetId)
{
  int slot = bankGetEquipSlotFromGadgetId(gadgetId);
  if (slot < 0) return 0;

  // add xp
  double xp = bankLocalBank.Account.WeaponXp[slot];
  bankLocalBank.Account.WeaponXp[slot] += amount;

  int level = getProficiencyFromXp(xp);
  int nextLevel = getProficiencyFromXp(xp + amount);
  if (nextLevel > level) {
    struct GadgetDef* gadgetDef = weaponGetDef(gadgetId, 0);
    //bankAddXP(LEVELUP_PLAYER_INCREMENT_AMOUNT);
    snprintf(bankLevelUpBuf, sizeof(bankLevelUpBuf), "You have reached %s P%d", uiMsgString(gadgetDef->quickSelectTag), nextLevel + 1);
    pushSnack(0, bankLevelUpBuf, 120);
    bankSendAccountToServer(); // send to server
  }

  return bankLocalBank.Account.WeaponXp[slot];
}

//--------------------------------------------------------------------------
RaidsPlayerBank_t* bankGetLocalBank(void)
{
  return &bankLocalBank;
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
void bankGetItemName(RaidsInventoryItem_t* item, char* buf, int bufSize)
{
  if (!item) return;

  int rarity = bankGetRarityFromQuality(item->Quality);
  if (bankItemIsBadge(item)) {
    snprintf(buf, bufSize, "%cClass Mod\x08", bankRarityCode[rarity]);
  } else {
    struct GadgetDef* gadgetDef = weaponGetDef(item->WeaponData.GadgetId, 0);
    snprintf(buf, bufSize, "%c%s P%d\x08", bankRarityCode[rarity], uiMsgString(rarity > RAIDS_ITEM_RARITY_LEGENDARY ? gadgetDef->upgQSTag : gadgetDef->quickSelectTag), item->WeaponData.Proficiency + 1);
  }
}

//--------------------------------------------------------------------------
int bankGetPlayerIdxFromGadgetBox(GadgetBox* gbox)
{
  if (!gbox) return NULL;
  
  return gbox->Initialized - 1;
}

//--------------------------------------------------------------------------
RaidsPlayerEquippedInventory_t* bankGetEquippedFromGadgetBox(GadgetBox* gbox)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return NULL;

  if (!gbox) return NULL;
  if (gbox->Initialized <= 0) return NULL;

  return &state->PlayerStates[(int)gbox->Initialized - 1].Inventory;
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* bankGetEquippedBadgeFromGadgetBox(GadgetBox* gbox)
{
  RaidsPlayerEquippedInventory_t* inventory = bankGetEquippedFromGadgetBox(gbox);
  if (!inventory) return NULL;
  if (!bankItemIsBadge(&inventory->Badge)) return NULL;

  return &inventory->Badge;
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* bankGetEquippedWeaponFromGadgetBox(GadgetBox* gbox, int gadgetId)
{
  RaidsPlayerEquippedInventory_t* inventory = bankGetEquippedFromGadgetBox(gbox);
  if (!inventory) return NULL;
  if (gadgetId <= 0) return NULL;

  int idx = weaponIdToSlot(gadgetId)-1;
  if (idx < 0 || idx >= COUNT_OF(inventory->Items)) return NULL;
  if (inventory->Items[idx].Type != RAIDS_ITEM_WEAPON || inventory->Items[idx].WeaponData.GadgetId != gadgetId) return NULL;

  return &inventory->Items[idx];
}

//--------------------------------------------------------------------------
float bankGetWeaponXpProgressFromGadgetBox(GadgetBox* gbox, int gadgetId)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();

  int slot = bankGetEquipSlotFromGadgetId(gadgetId);
  if (slot < 0) return 0;

  double xp = localBank->Account.WeaponXp[slot];
  int prof = getProficiencyFromXp(xp);
  if (prof == 98) return 1; // maxed

  double lastXp = getXpForProficiency(prof);
  double nextXp = getXpForProficiency(prof + 1);
  if (xp < lastXp) xp = lastXp;
  float xpPerc = (float)((xp - lastXp) / (double)(nextXp - lastXp));
  //DPRINTF("lvl:%d perc:%f %ld=>%ld xp:%ld\n", prof, xpPerc, lastXp, nextXp, xp);
  return xpPerc;
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* bankGetLocalEquippedWeapon(int gadgetId)
{
  int slotId = bankGetEquipSlotFromGadgetId(gadgetId);
  if (slotId < 0) return NULL;

  RaidsInventoryItem_t* item = &bankLocalBank.EquippedInventory.Items[slotId];

  if (item->Type != RAIDS_ITEM_WEAPON) return NULL;
  if (item->WeaponData.GadgetId != gadgetId) return NULL;
  return item;
}

//--------------------------------------------------------------------------
RaidsInventoryItem_t* bankGetLocalEquippedBadge(void)
{
  RaidsInventoryItem_t* badge = &bankLocalBank.EquippedInventory.Badge;
  if (badge->Type != RAIDS_ITEM_BADGE) return NULL;

  return badge;
}

//--------------------------------------------------------------------------
u32 bankGetGadgetColor(int localPlayerIndex, int gadgetId)
{
  RaidsInventoryItem_t* bankWeapon = bankGetLocalEquippedWeapon(gadgetId);
  if (bankWeapon) {
    int rarity = bankGetRarityFromQuality(bankWeapon->Quality);
    return bankRarityColors[rarity];
  }

  return 0x80D0D0D0;
}

//--------------------------------------------------------------------------
u32 bankGetOmegaModColor(int omegaMod)
{
  switch (omegaMod)
  {
    case OMEGA_MOD_NAPALM: return hudGetTeamColor(TEAM_ORANGE, 1);
    case OMEGA_MOD_TIME_BOMB: return hudGetTeamColor(TEAM_BLUE, 0);
    case OMEGA_MOD_FREEZE: return hudGetTeamColor(TEAM_BLUE, 3);
    case OMEGA_MOD_MINI_BOMB: return hudGetTeamColor(TEAM_WHITE, 2);
    case OMEGA_MOD_MORPH: return hudGetTeamColor(TEAM_PURPLE, 0);
    case OMEGA_MOD_BRAINWASH: return hudGetTeamColor(TEAM_ORANGE, 0);
    case OMEGA_MOD_ACID: return hudGetTeamColor(TEAM_GREEN, 1);
    case OMEGA_MOD_SHOCK: return hudGetTeamColor(TEAM_AQUA, 0);
    case RAIDS_WEAPON_MOD_WILL_O_WISP: return hudGetTeamColor(TEAM_RED, 1);
    case RAIDS_WEAPON_MOD_LIGHTFOOT: return hudGetTeamColor(TEAM_WHITE, 0);
    default: return 0;
  }
}

//--------------------------------------------------------------------------
u32 bankGetWeaponOmegaModColor(RaidsInventoryItem_t* item)
{
  if (!item) return 0;
  if (!bankItemIsWeapon(item)) return 0;

  return bankGetOmegaModColor(item->WeaponData.ModType);
}

//--------------------------------------------------------------------------
float bankGetEquippedBadgeEffectStrength(int playerId, enum RaidsBadgeType effect)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return 0;

  RaidsInventoryItem_t* badge = &state->PlayerStates[playerId].Inventory.Badge;
  if (!badge || badge->Type != RAIDS_ITEM_BADGE) return 0;

  int i;
  for (i = 0; i < BANK_BADGE_EFFECT_COUNT; ++i) {
    if (badge->BadgeData.Effects[i] == effect)
      return badge->BadgeData.EffectStrength[i] / 255.0;
  }

  return 0;
}

//--------------------------------------------------------------------------
int bankGetEquippedWeaponModRarity(int playerId, int gadgetId, enum RaidsWeaponModType modType)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return -1;

  int slotId = bankGetEquipSlotFromGadgetId(gadgetId);
  if (slotId < 0) return -1;

  RaidsInventoryItem_t* weapon = &state->PlayerStates[playerId].Inventory.Items[slotId];
  if (!weapon || weapon->Type != RAIDS_ITEM_WEAPON) return -1;
  if (weapon->WeaponData.ModType != modType) return -1;

  return (int)bankGetRarityFromQuality(weapon->WeaponData.ModQuality);
}

//--------------------------------------------------------------------------
int bankGetAlphaModCount(GadgetBox* gadgetBox, int gadgetId, int alphaModId)
{
  int extra = 0;
  if (!gadgetBox || !gadgetBox->Initialized) return 0;
  if (alphaModId > 8) return 0;
  if (alphaModId <= 0) return 0;

  int pIdx = bankGetPlayerIdxFromGadgetBox(gadgetBox);
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(gadgetBox, gadgetId);
  if (!bankWeapon) return 0;

  // switch (alphaModId)
  // {
  //   case ALPHA_MOD_AMMO: extra = (int)ceilf(BADGE_AMMO_MOD_BUFF_AMOUNT * bankGetEquippedBadgeEffectStrength(pIdx, RAIDS_BADGE_TYPE_ALPHA_AMMO_BUFF)); break;
  //   case ALPHA_MOD_AREA: extra = (int)ceilf(BADGE_AREA_MOD_BUFF_AMOUNT * bankGetEquippedBadgeEffectStrength(pIdx, RAIDS_BADGE_TYPE_ALPHA_AREA_BUFF)); break;
  //   case ALPHA_MOD_SPEED: extra = (int)ceilf(BADGE_SPEED_MOD_BUFF_AMOUNT * bankGetEquippedBadgeEffectStrength(pIdx, RAIDS_BADGE_TYPE_ALPHA_SPEED_BUFF)); break;
  //   case ALPHA_MOD_IMPACT: extra = (int)ceilf(BADGE_IMPACT_MOD_BUFF_AMOUNT * bankGetEquippedBadgeEffectStrength(pIdx, RAIDS_BADGE_TYPE_ALPHA_IMPACT_BUFF)); break;
  // }

  return extra + bankWeapon->WeaponData.AlphaModCounts[alphaModId-1];
}

//--------------------------------------------------------------------------
float bankGetArbiterNapalmDamage(Player* player)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, WEAPON_ID_ARBITER);
  if (!bankWeapon) return 0;

  int strength = bankGetEquippedWeaponModRarity(player->PlayerId, WEAPON_ID_ARBITER, RAIDS_WEAPON_MOD_NAPALM) + 1;
  return bankGetWeaponDamage(bankWeapon) * MOB_POSTFX_NAPALM_DMG_PERC * strength;
}

float bankGetArbiterMinibombDamage(Player* player)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, WEAPON_ID_ARBITER);
  if (!bankWeapon) return 0;

  int strength = bankGetEquippedWeaponModRarity(player->PlayerId, WEAPON_ID_ARBITER, RAIDS_WEAPON_MOD_MINI_BOMB) + 1;
  return bankGetWeaponDamage(bankWeapon) * MOB_POSTFX_MINIBOMB_DMG_PERC * strength;
}

//--------------------------------------------------------------------------
float bankGetMineLauncherNapalmDamage(Player* player)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, WEAPON_ID_MINE_LAUNCHER);
  if (!bankWeapon) return 0;

  int strength = bankGetEquippedWeaponModRarity(player->PlayerId, WEAPON_ID_MINE_LAUNCHER, RAIDS_WEAPON_MOD_NAPALM) + 1;
  return bankGetWeaponDamage(bankWeapon) * MOB_POSTFX_NAPALM_DMG_PERC;
}

//--------------------------------------------------------------------------
float bankGetMineLauncherMinibombDamage(Player* player)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, WEAPON_ID_MINE_LAUNCHER);
  if (!bankWeapon) return 0;

  int strength = bankGetEquippedWeaponModRarity(player->PlayerId, WEAPON_ID_MINE_LAUNCHER, RAIDS_WEAPON_MOD_MINI_BOMB) + 1;
  return bankGetWeaponDamage(bankWeapon) * MOB_POSTFX_MINIBOMB_DMG_PERC;
}

//--------------------------------------------------------------------------
float bankGetB6NapalmDamage(Player* player)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, WEAPON_ID_B6);
  if (!bankWeapon) return 0;

  int strength = bankGetEquippedWeaponModRarity(player->PlayerId, WEAPON_ID_B6, RAIDS_WEAPON_MOD_NAPALM) + 1;
  return bankGetWeaponDamage(bankWeapon) * MOB_POSTFX_NAPALM_DMG_PERC;
}

//--------------------------------------------------------------------------
float bankGetB6MinibombDamage(Player* player)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, WEAPON_ID_B6);
  if (!bankWeapon) return 0;

  int strength = bankGetEquippedWeaponModRarity(player->PlayerId, WEAPON_ID_B6, RAIDS_WEAPON_MOD_MINI_BOMB) + 1;
  return bankGetWeaponDamage(bankWeapon) * MOB_POSTFX_MINIBOMB_DMG_PERC;
}

//--------------------------------------------------------------------------
float bankGetSniperBeamHitAngle(u128 a0, u128 a1, u128 a2, void* a3)
{
  float r = ((float (*)(u128,u128,u128,void*))0x003fbb10)(a0, a1, a2, a3);

  // increase area of beam per area mod
  Player* player = *(Player**)(*(void**)((u32)a3 + 0x40) + 0x64);
  if (player && player->GadgetBox) {
    r /= (1 + bankGetAlphaModCount(player->GadgetBox, WEAPON_ID_FUSION_RIFLE, ALPHA_MOD_AREA));
  }

  return r;
}

//--------------------------------------------------------------------------
void bankSpawnMinibombs(Moby* pParent, int count, VECTOR rootVel, float randSpeed, float damage, int damageFlags, int lifeTimeMin, int lifeTimeMax, int weaponSource)
{
  if (pParent) {
    Player* parent = guberMobyGetPlayerDamager(pParent);
    if (parent && !parent->IsLocal) {
      count = 1;
    }
  }

  mobySpawnMinibombs(pParent, count, rootVel, randSpeed, damage, damageFlags, lifeTimeMin, lifeTimeMax, weaponSource);
}

//--------------------------------------------------------------------------
float bankGetGadgetDamage(GadgetBox* gbox, int gadgetId, int damageType, int multiplier)
{
  RaidsInventoryItem_t* bankWeapon = bankGetEquippedWeaponFromGadgetBox(gbox, gadgetId);
  if (bankWeapon) return bankGetWeaponDamage(bankWeapon) * multiplier;

  int level = gbox->Gadgets[gadgetId].Level;
  return ((float (*)(int gadgetId, int level, int damageType, int multiplier))0x00627520)(gadgetId, level, damageType, multiplier);
}

//--------------------------------------------------------------------------
float bankGetWeaponDamage(RaidsInventoryItem_t* item)
{
  if (!item) return 0;
  if (!bankItemIsWeapon(item)) return 0;
  
  // base
  float damage = item->WeaponData.Damage;

  // upgrades
  damage += 5 * item->WeaponData.Upgrades;
  damage *= 1 + (0.025) * item->WeaponData.Upgrades * (bankGetRarityFromQuality(item->Quality)+1);

  return damage;
}

//--------------------------------------------------------------------------
int bankTryAddGadgetToQuickSelect(Player* player, int gadgetId)
{
  int i;

  if (!player || !player->IsLocal) return -1;

  int freeSlot = -1;
  for (i = 0; i < 3; ++i) {
    int gId = playerGetLocalEquipslot(player->LocalPlayerIndex, i);
    if (gId == gadgetId) return i;
    if (gId == 0 && freeSlot < 0) freeSlot = i;
  }

  if (freeSlot >= 0) {
    playerSetLocalEquipslot(player->LocalPlayerIndex, freeSlot, gadgetId);
    if (freeSlot == 0) playerEquipWeapon(player, gadgetId);
  }

  return freeSlot;
}

//--------------------------------------------------------------------------
void bankApplyGadgetMoby(Player* player, RaidsInventoryItem_t* item, Moby* moby)
{
  u32 glowAlpha = 0xFF000000;
  float glowOpacity = 0.5;
  if (!moby) return;

  if (item->WeaponData.Paint) {
    moby->PrimaryColor = bankPaintColors[item->WeaponData.Paint];
  } else {
    moby->PrimaryColor = player->PlayerMoby->PrimaryColor;
    glowOpacity = 1;
  }

  if (item->WeaponData.PaintSpecialMask & RAIDS_GADGET_PAINTSPECIAL_ADDITIVE) {
    moby->Opacity = 0x50;
    moby->ModeBits |= MOBY_MODE_BIT_DRAW_TRANSPARENT_WEIRD;
    glowAlpha = 0x80000000;
    glowOpacity *= 0.5;
  } else {
    moby->Opacity = 0x80;
    moby->ModeBits &= ~MOBY_MODE_BIT_DRAW_TRANSPARENT_WEIRD;
  }
  
  if (item->WeaponData.PaintSpecialMask & RAIDS_GADGET_PAINTSPECIAL_GLOW) {
    moby->PrimaryColor = colorLerp(0, moby->PrimaryColor, glowOpacity) | glowAlpha;
    moby->Lights = 0;
    //moby->ModeBits2 |= 8;
  } else {
    moby->Lights = player->PlayerMoby->Lights;
    //moby->ModeBits2 &= ~8;
  }
}

//--------------------------------------------------------------------------
void bankRemoveGadget(Player* player, int gadgetId)
{
  player->GadgetBox->Gadgets[gadgetId].Level = -1;

  if (player->IsLocal) {
    if (playerGetLocalEquipslot(player->LocalPlayerIndex, 0) == gadgetId) {
      playerSetLocalEquipslot(player->LocalPlayerIndex, 0, playerGetLocalEquipslot(player->LocalPlayerIndex, 1));
      playerSetLocalEquipslot(player->LocalPlayerIndex, 1, playerGetLocalEquipslot(player->LocalPlayerIndex, 2));
      playerSetLocalEquipslot(player->LocalPlayerIndex, 2, 0);
    } else if (playerGetLocalEquipslot(player->LocalPlayerIndex, 1) == gadgetId) {
      playerSetLocalEquipslot(player->LocalPlayerIndex, 1, playerGetLocalEquipslot(player->LocalPlayerIndex, 2));
      playerSetLocalEquipslot(player->LocalPlayerIndex, 2, 0);
    } else if (playerGetLocalEquipslot(player->LocalPlayerIndex, 2) == gadgetId) {
      playerSetLocalEquipslot(player->LocalPlayerIndex, 2, 0);
    }
  }
}

//--------------------------------------------------------------------------
void bankApplyItem(Player* player, RaidsInventoryItem_t* item)
{
  int gadgetId = item->WeaponData.GadgetId;
  int rarity = bankGetRarityFromQuality(item->Quality);
  GadgetBox* gbox = player->GadgetBox;

  // give
  if (gbox->Gadgets[gadgetId].Level < 0) {
    playerGiveWeapon(gbox, gadgetId, 0, 1);
    bankTryAddGadgetToQuickSelect(player, gadgetId);
  }
  gbox->Gadgets[gadgetId].Level = bankGetRarityFromQuality(item->Quality) > RAIDS_ITEM_RARITY_LEGENDARY ? 9 : 0;
  gbox->Gadgets[gadgetId].UNK_10 = (gbox->Gadgets[gadgetId].UNK_10 & 0xFF) | (rarity << 8) | (item->WeaponData.Proficiency << 16);
  gbox->Gadgets[gadgetId].Experience = (int)(bankGetWeaponXpProgressFromGadgetBox(gbox, gadgetId) * 100000);

  // configure mobys
  if (player->Gadgets[0].id == gadgetId) {
    bankApplyGadgetMoby(player, item, player->Gadgets[0].pMoby);
    bankApplyGadgetMoby(player, item, player->Gadgets[0].pMoby2);
  }

  // apply weapon omega mod
  gbox->Gadgets[gadgetId].OmegaMod = item->WeaponData.ModType;
}

//--------------------------------------------------------------------------
void bankUpdateLocalState(Player * player)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (!player || !player->PlayerMoby || !player->pNetPlayer || !player->IsLocal) return;
  if (!playerIsConnected(player)) return;

  int playerId = player->PlayerId;
  RaidsPlayerBank_t* localBank = bankGetLocalBank();

  // save level
  state->PlayerStates[playerId].State.Level = bankGetLevel();

  // save to equipped
  int i;
  for (i = 0; i < WEAPON_SLOT_OMNI_SHIELD; ++i) {
    memcpy(&state->PlayerStates[playerId].Inventory, &localBank->EquippedInventory, sizeof(RaidsPlayerEquippedInventory_t));
  }

  if (player->LocalPlayerIndex == 0) {
    bankBroadcastEquippedInventory();
    bankBroadcastAccount();
  }
}

//--------------------------------------------------------------------------
int bankSellItem(RaidsInventoryItem_t* item)
{
  int isEquipped = 0;
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  if (!localBank) return 0;
  if (!item) return 0;
  if (!item->Type) return 0;

  if (bankItemIsBadge(item)) {
    RaidsInventoryItem_t* badge = bankGetLocalEquippedBadge();
    if (badge != NULL && badge->Uid == item->Uid) {
      isEquipped = 1;
      memset(&localBank->EquippedInventory.Badge, 0, sizeof(localBank->EquippedInventory.Badge));
    }
  } else if (bankItemIsWeapon(item)) {
    int slotId = bankGetEquipSlotFromGadgetId(item->WeaponData.GadgetId);
    RaidsInventoryItem_t* weapon = bankGetLocalEquippedWeapon(item->WeaponData.GadgetId);
    if (weapon != NULL && weapon->Uid == item->Uid) {
      isEquipped = 1;
      memset(&localBank->EquippedInventory.Items[slotId], 0, sizeof(RaidsInventoryItem_t));
    }
  }

  // broadcast
  bankSendInventoryItemToServer(item, RAIDS_ITEM_UPDATE_SELL);

  // apply sell
  localBank->Account.Bolts += item->Price;
  memset(item, 0, sizeof(RaidsInventoryItem_t));

  // refresh equipped inventory
  if (isEquipped) {
    localBank->EquippedInventory.RefreshLocalInventory = 1;
  }

  return 1;
}

//--------------------------------------------------------------------------
int bankEquipItem(RaidsInventoryItem_t* item)
{
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  if (!localBank) return 0;
  if (!item) return 0;
  if (!item->Type) return 0;

  if (bankItemIsBadge(item)) {
    memcpy(&localBank->EquippedInventory.Badge, item, sizeof(RaidsInventoryItem_t));
  } else {
    memcpy(&localBank->EquippedInventory.Items[bankGetEquipSlotFromGadgetId(item->WeaponData.GadgetId)], item, sizeof(RaidsInventoryItem_t));
  }

  bankSendInventoryItemToServer(item, RAIDS_ITEM_UPDATE_EQUIP);
  bankRequestEquippedInventoryFromServer();
  //localBank->EquippedInventory.RefreshLocalInventory = 1;
  return 1;
}

//--------------------------------------------------------------------------
void bankTickPlayer(Player * player)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (!player || !player->PlayerMoby || !player->pNetPlayer) return;
  if (!playerIsConnected(player)) return;

  GadgetBox* gbox = player->GadgetBox;
  if (!gbox) return;

  // default equipslots
  if (player->IsLocal && !playerGetLocalEquipslot(player->LocalPlayerIndex, 0) && state->PlayerStates[player->PlayerId].LastEquipslots[0]) {
    playerSetLocalEquipslot(player->LocalPlayerIndex, 0, state->PlayerStates[player->PlayerId].LastEquipslots[0]);
    playerSetLocalEquipslot(player->LocalPlayerIndex, 1, state->PlayerStates[player->PlayerId].LastEquipslots[1]);
    playerSetLocalEquipslot(player->LocalPlayerIndex, 2, state->PlayerStates[player->PlayerId].LastEquipslots[2]);
  }

  // apply items
  int i;
  for (i = 0; i < WEAPON_SLOT_OMNI_SHIELD; ++i) {
    int gadgetId = weaponSlotToId(i+1);
    RaidsInventoryItem_t* item = bankGetEquippedWeaponFromGadgetBox(gbox, gadgetId);
    if (item) {
      bankApplyItem(player, item);
    } else if (gbox->Gadgets[gadgetId].Level >= 0)  {
      bankRemoveGadget(player, gadgetId);
    }
  }
  
  if (gbox->Gadgets[WEAPON_ID_HACKER_RAY].Level < 0)
    playerGiveWeapon(gbox, WEAPON_ID_HACKER_RAY, 0, 0); // give hacker ray
}

//--------------------------------------------------------------------------
void bankOnResurrectWeaponStripMe(Player* player)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (!player->IsLocal)
    return;

  int pIdx = player->PlayerId;
  int i = player->LocalPlayerIndex;
  state->PlayerStates[pIdx].LastEquipslots[0] = playerGetLocalEquipslot(i, 0);
  state->PlayerStates[pIdx].LastEquipslots[1] = playerGetLocalEquipslot(i, 1);
  state->PlayerStates[pIdx].LastEquipslots[2] = playerGetLocalEquipslot(i, 2);
}

//--------------------------------------------------------------------------
void bankOnResurrectGiveMeRandomWeapons(Player* player, int weaponCount)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  if (!player->IsLocal)
    return;

  playerSetLocalEquipslot(player->LocalPlayerIndex, 0, state->PlayerStates[player->PlayerId].LastEquipslots[0]);
  playerSetLocalEquipslot(player->LocalPlayerIndex, 1, state->PlayerStates[player->PlayerId].LastEquipslots[1]);
  playerSetLocalEquipslot(player->LocalPlayerIndex, 2, state->PlayerStates[player->PlayerId].LastEquipslots[2]);
}

//--------------------------------------------------------------------------
void bankTick(void)
{
  int i;

  static int sentContractRules = 0;
  if (!sentContractRules) {
    sentContractRules = bankSendMapContractRules();
  }

  // handle new inventory change
  if (bankLocalBank.EquippedInventory.RefreshLocalInventory) {
    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
      bankUpdateLocalState(playerGetFromSlot(i));
    }

    bankLocalBank.EquippedInventory.RefreshLocalInventory = 0;
  }

  // process players
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    bankTickPlayer(players[i]);
  }
}

//--------------------------------------------------------------------------
void bankInit(void)
{
  int i;

  netInstallCustomMsgHandler(CUSTOM_MSG_SET_PLAYER_EQUIPPED_INVENTORY, &bankOnSetPlayerEquippedInventoryRemote);
  netInstallCustomMsgHandler(CUSTOM_MSG_SET_PLAYER_ACCOUNT, &bankOnSetPlayerAccountRemote);

  // hooks
  POKE_U32(0x005DDF98, 0); // disable player ambient color affecting child mobys
  POKE_U8(0x00171b66, 1); // challenge mode
  HOOK_J_OP(0x00627600, &bankGetGadgetDamage, 0);
  HOOK_J_OP(0x00542078, &bankGetGadgetColor, 0);
  HOOK_J_OP(0x00541fd0, &bankGetOmegaModColor, 0);
  //HOOK_JAL(0x003F29AC, &bankGetArbiterSpeed);
  //POKE_U32(0x003F2984, 0x0240202D);
  HOOK_J(0x006299A8, &bankGetAlphaModCount);

  HOOK_JAL_OP(0x003C9AD0, &bankGetMineLauncherNapalmDamage, 0x0280202D);
  POKE_U32(0x003C9AD8, 0x7A440010); POKE_U32(0x003C9ADC, 0x46000306);
  HOOK_JAL_OP(0x003C99BC, &bankGetMineLauncherMinibombDamage, 0x0280202D);
  POKE_U32(0x003C99C4, 0);

  HOOK_JAL_OP(0x003F6F5C, &bankGetB6NapalmDamage, 0x8E640090);
  POKE_U32(0x003F6F64, 0x7A840010); POKE_U32(0x003F6F68, 0x46000306);
  HOOK_JAL_OP(0x003F6DDC, &bankGetB6MinibombDamage, 0x8E640090);
  POKE_U32(0x003F6DE4, 0);

  HOOK_JAL_OP(0x003F55D4, &bankGetArbiterNapalmDamage, 0x8E2400A8);
  POKE_U32(0x003F55DC, 0x7A440010); POKE_U32(0x003F55E0, 0x46000306);
  HOOK_JAL_OP(0x003F56B4, &bankGetArbiterMinibombDamage, 0x8E2400A8);
  POKE_U32(0x003F56BC, 0);

  HOOK_JAL(0x003C9A44, &bankSpawnMinibombs);
  HOOK_JAL(0x003F573C, &bankSpawnMinibombs);
  HOOK_JAL(0x003F6E70, &bankSpawnMinibombs);

  // sniper shot radius
  POKE_U32(0x003FBC84, 0x3C024200);
  HOOK_JAL(0x003FBD3C, &bankGetSniperBeamHitAngle);
  HOOK_JAL(0x003FBDB8, &bankGetSniperBeamHitAngle);

  // fix arbiter explosion radius
  POKE_U32(0x003F595C, 0);
  POKE_U32(0x003F5760, 0x00028040);

  // hook HudAmmo XP bar
  POKE_U32(0x00552CD8, 0x10000013);
  HOOK_JAL(0x00552D28, &bankGetWeaponXpProgressFromGadgetBox);
  
  // disable randomize weapons on respawn
  POKE_U32(0x005E2B40, 0);
  HOOK_JAL(0x005e2b2c, &bankOnResurrectWeaponStripMe);
  HOOK_JAL(0x005e2b48, &bankOnResurrectGiveMeRandomWeapons);

  //HOOK_J_OP(0x00626d98, &bankGetGadgetMaxLevel, 0);
  //HOOK_J_OP(0x00626fb8, &bankGetGadgetMaxAmmo, 0);
  //HOOK_JAL_OP(0x0060f780, &bankGetGadgetRefireRate, 0x0200282D);

  bankRarityColors[RAIDS_ITEM_RARITY_UNCOMMON] = hudGetTeamColor(TEAM_GREEN, 0);
  bankRarityColors[RAIDS_ITEM_RARITY_RARE] = hudGetTeamColor(TEAM_BLUE, 0);
  bankRarityColors[RAIDS_ITEM_RARITY_LEGENDARY] = hudGetTeamColor(TEAM_PURPLE, 0);
  bankRarityColors[RAIDS_ITEM_RARITY_MYTHIC] = hudGetTeamColor(TEAM_RED, 0);

  // clear inventory
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (players[i] && players[i]->GadgetBox) {
      playerStripWeapons(players[i]);
      playerGiveWeapon(players[i]->GadgetBox, 17, 0, 0); // give cboots
      playerGiveWeapon(players[i]->GadgetBox, WEAPON_ID_HACKER_RAY, 0, 0); // give hacker ray
      players[i]->GadgetBox->Initialized = i+1;
    }
  }

  //bankTick();
  MapConfig.BankVTable = &bankVTable;
}
