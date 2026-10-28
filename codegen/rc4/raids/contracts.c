/***************************************************
 * FILENAME :		contracts.c
 * 
 * DESCRIPTION :
 * 		
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/hud.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/collision.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/weapon.h>
#include "common.h"
#include "gate.h"
#include "game.h"
#include "messager.h"
#include "spawner.h"
#include "mover.h"
#include "bank.h"
#include "mobs/mob.h"
#include "shared.h"
#include "pathfind.h"
#include "window.h"
#include "menu.h"
#include "utils.h"

#define CONTRACTS_REROLL_COST (50000)
#define CONTRACTS_MIN_LEVEL   (9)

struct RaidsActionContractRequest
{
  u32 ContractUid;
  int Action;
};

struct ContractsState {
  Moby* VendorMoby;
  char VendorLocalStrBufs[GAME_MAX_LOCALS][64];
  int SelectedIndex;
  int ShowRerollDialog;
  int CanInteract;
  int AutoRefreshTicker;
} contractsState;

extern char inventoryAlphaModSpriteIds[];
extern char inventoryWeaponSpriteIds[];
extern char inventorySkillSpriteIds[];
extern char* inventorySkillNames[];
extern char* inventoryRarityNames[];
extern char* inventoryBadgeNames[];
extern char* inventoryPaintNames[];
extern char* inventoryPaintSpecialNames[];
extern char* inventoryModNames[];
extern char bankRarityCode[];

int contractsIsContractExpired(RaidsContract_t* contract);

//--------------------------------------------------------------------------
void contractsGetContracts(void)
{
  contractsState.AutoRefreshTicker = TPS * 60; // every minute
  bankRequestContractsFromServer();
}

//--------------------------------------------------------------------------
void contractsActivate(RaidsContract_t* item)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;
  if (item->Activated) return;
  if (item->RefreshInMinutes <= 0) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

  // pass to server
  struct RaidsActionContractRequest msg = {
    .ContractUid = item->Uid,
    .Action = 2
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_ACTION_CONTRACT_REQUEST, sizeof(msg), &msg);
  DPRINTF("activate contract\n");

  // refresh list of items
  contractsGetContracts();
}

//--------------------------------------------------------------------------
void contractsComplete(RaidsContract_t* item)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

  if (!bankGetHasAccount()) return;
  if (item->RewardClassXp > 0 && !classesGetDefinition(item->CompletedClassType)) return;

  // apply
  bankAddBolts(item->RewardBolts);
  bankAddXP(item->RewardPlayerXp);
  if (item->RequiredKillsGadgetId > 0)
    bankAddWeaponXP(item->RewardWeaponXp, item->RequiredKillsGadgetId);
  if (item->RewardClassXp > 0)
    bankAddClassXP(item->RewardClassXp, item->CompletedClassType);
  bankSendAccountToServer();

  // pass to server
  struct RaidsActionContractRequest msg = {
    .ContractUid = item->Uid,
    .Action = 1
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_ACTION_CONTRACT_REQUEST, sizeof(msg), &msg);
  DPRINTF("complete contract\n");

  // refresh list of items
  contractsGetContracts();
  bankRequestAccountFromServer();
  bankRequestEquippedClassProgressFromServer();
  bankRequestAllClassProgressFromServer();
}

//--------------------------------------------------------------------------
void contractsReroll(RaidsContract_t* item)
{
  if (!item || !item->Uid) return;

  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

  // Expired contracts can be dismissed for free; other buyouts cost bolts.
#if !DEV_BUILD
  if (!contractsIsContractExpired(item)) {
    if (bankGetBolts() < CONTRACTS_REROLL_COST) return;
    bankSubtractBolts(CONTRACTS_REROLL_COST);
  }
#endif

  // tell server we bought the item
  struct RaidsActionContractRequest msg = {
    .ContractUid = item->Uid,
    .Action = 0
  };
  netSendCustomAppMessage(NET_DELIVERY_CRITICAL, connection, NET_LOBBY_CLIENT_INDEX, CUSTOM_MSG_ID_RAIDS_ACTION_CONTRACT_REQUEST, sizeof(msg), &msg);
  DPRINTF("reroll contract\n");

  // refresh list of items
  contractsGetContracts();
  bankSendAccountToServer();
}

//--------------------------------------------------------------------------
void contractsOpen(int allowInteract)
{
  struct RaidsState* state = MapConfig.State;

  if (!state) return;
  if (shouldCloseMenu() || (allowInteract && !isOnHubWorld())) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_NONE) return;

  state->MenuOpen = RAIDS_CUSTOM_MENU_CONTRACTS;
  contractsState.SelectedIndex = 0;
  contractsState.ShowRerollDialog = 0;
  contractsState.CanInteract = allowInteract;
  contractsGetContracts();
  padDisableInput();
}

//--------------------------------------------------------------------------
void contractsClose(void)
{
  struct RaidsState* state = MapConfig.State;

  if (!state) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_CONTRACTS) return;
  
  state->MenuOpen = RAIDS_CUSTOM_MENU_NONE;
  padEnableInput();
}

//--------------------------------------------------------------------------
int contractsIsPlayerHighEnoughLevel(void)
{
  return bankGetLevel() >= CONTRACTS_MIN_LEVEL;
}

//--------------------------------------------------------------------------
int contractsIsContractCompleted(RaidsContract_t* contract)
{
  if (!contract || contract->Uid == 0 || !contract->Activated) return 0;

  // raid completed in time
  if (contract->RequiredRaidTimeMs > 0 && contract->CompletedTimeMs > 0 && contract->CompletedTimeMs < contract->RequiredRaidTimeMs)
    return 1;

  // kills reached
  if (contract->RequiredKills > 0 && contract->Kills >= contract->RequiredKills)
    return 1;

  return 0;
}

//--------------------------------------------------------------------------
int contractsIsContractExpired(RaidsContract_t* contract)
{
  if (!contract || contract->Uid == 0 || !contract->Activated) return 0;

  return contract->ExpiresInMinutes <= 0 && !contractsIsContractCompleted(contract);
}

//--------------------------------------------------------------------------
void contractsDrawRewardRow(Window_t* window, float y, char* label, u32 value)
{
  const float padding = RAIDS_MENU_PADDING;
  const float labelWidth = 55;
  char strBuf[32];
  snprintf(strBuf, sizeof(strBuf), "+%'u", value);
  menuDrawFittedText(window, padding, y, labelWidth, 0.65, RAIDS_MENU_TEXT_COLOR, label, TEXT_ALIGN_TOPLEFT);
  menuDrawFittedText(window, window->Width - padding, y, window->Width - 2*padding - labelWidth - 8, 0.65, 0x8000E0E0, strBuf, TEXT_ALIGN_TOPRIGHT);
}

//--------------------------------------------------------------------------
void contractsDrawContractInfo(Window_t* drawWindow, RaidsContract_t* contract, int idx)
{
  const float padding = RAIDS_MENU_PADDING;
  const float contentWidth = drawWindow->Width - 2*padding;
  const u32 textColor = RAIDS_MENU_TEXT_COLOR;
  const u32 secondaryColor = RAIDS_MENU_SECONDARY_COLOR;
  const u32 completedColor = 0x8060C080;
  char strBuf[128];
  char progressBuf[64];
  int isSelected = contractsState.CanInteract && contractsState.SelectedIndex == idx;
  u32 borderColor = isSelected ? 0x80008080 : RAIDS_MENU_BORDER_COLOR;

  windowFill(drawWindow, RAIDS_MENU_PANEL_COLOR);
  windowBorder(drawWindow, borderColor, 1, 1, 1, 1);
  if (!contract || !contract->Uid) {
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 8, contentWidth, 0.8, secondaryColor, "Empty slot", TEXT_ALIGN_TOPCENTER);
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, drawWindow->Height * 0.5, contentWidth, 0.7, secondaryColor, "No contract", TEXT_ALIGN_MIDDLECENTER);
    return;
  }

  int isCompleted = contractsIsContractCompleted(contract);
  int isExpired = contractsIsContractExpired(contract);
  int isRaid = contract->RequiredRaidTimeMs > 0;
  int slotId = weaponIdToSlot(contract->RequiredKillsGadgetId);
  int hasWeapon = slotId > 0 && slotId < WEAPON_SLOT_COUNT;
  char* status = !contract->Activated ? "Available" : "Active";
  u32 statusColor = contract->Activated ? 0x80B0B050 : textColor;
  if (isCompleted) {
    status = "Completed";
    statusColor = completedColor;
  } else if (isExpired) {
    status = "Expired";
    statusColor = RAIDS_MENU_WARNING_COLOR;
  } else if (!contract->Activated && contract->RefreshInMinutes <= 0) {
    status = "Refreshing";
    statusColor = secondaryColor;
  }

  menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 8, contentWidth, 0.85, textColor, contract->MapName[0] ? contract->MapName : "Any Map", TEXT_ALIGN_TOPCENTER);
  float statusWidth = gfxGetFontWidth(status, -1, 0.55) + 12;
  windowDrawBox(drawWindow, TEXT_ALIGN_TOPCENTER, 0, 26, statusWidth, 14, RAIDS_MENU_TRACK_COLOR, TEXT_ALIGN_TOPCENTER);
  windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, 0, 28, 0.55, statusColor, status, -1, TEXT_ALIGN_TOPCENTER);
  menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 48, contentWidth, 0.55, secondaryColor, "Objective", TEXT_ALIGN_TOPCENTER);

  float progress = 0;
  if (isRaid) {
    if (contract->RequiredDifficultyStars >= 0) {
      int stars = contract->RequiredDifficultyStars + 1;
      if (stars > RAIDS_DIFFICULTY_COUNT) stars = RAIDS_DIFFICULTY_COUNT;
      float starsWidth = stars*16 + (stars - 1)*4;
      int i;
      gfxSetupGifPaging(0);
      for (i = 0; i < stars; ++i)
        windowDrawSprite(drawWindow, TEXT_ALIGN_TOPLEFT, (drawWindow->Width - starsWidth)*0.5 + i*20, 62, 16, 16, 88, 0x8000C0C0, TEXT_ALIGN_TOPLEFT);
      gfxDoGifPaging();
    } else {
      menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 64, contentWidth, 0.65, textColor, "Any difficulty", TEXT_ALIGN_TOPCENTER);
    }
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 82, contentWidth, 0.65, secondaryColor, "Finish under", TEXT_ALIGN_TOPCENTER);
    snprintf(strBuf, sizeof(strBuf), "%u:%02u.%03u", contract->RequiredRaidTimeMs / 60000, (contract->RequiredRaidTimeMs / 1000) % 60, contract->RequiredRaidTimeMs % 1000);
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 96, contentWidth, 0.85, textColor, strBuf, TEXT_ALIGN_TOPCENTER);
    progress = contract->CompletedTimeMs > 0 && contract->CompletedTimeMs < contract->RequiredRaidTimeMs ? 1 : 0;
    if (contract->CompletedTimeMs > 0)
      snprintf(progressBuf, sizeof(progressBuf), "Best %u:%02u.%03u", contract->CompletedTimeMs / 60000, (contract->CompletedTimeMs / 1000) % 60, contract->CompletedTimeMs % 1000);
    else
      snprintf(progressBuf, sizeof(progressBuf), "Not completed");
  } else if (contract->RequiredKills > 0) {
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 62, contentWidth, 0.85, textColor, contract->MobName, TEXT_ALIGN_TOPCENTER);
    snprintf(strBuf, sizeof(strBuf), "Kill %'u", contract->RequiredKills);
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 82, contentWidth, 0.8, textColor, strBuf, TEXT_ALIGN_TOPCENTER);
    if (hasWeapon) {
      struct GadgetDef* gadgetDef = weaponGetDef(contract->RequiredKillsGadgetId, 0);
      char* weaponName = gadgetDef ? uiMsgString(gadgetDef->quickSelectTag) : "Required weapon";
      gfxSetupGifPaging(0);
      windowDrawSprite(drawWindow, TEXT_ALIGN_TOPLEFT, padding, 100, 16, 16, inventoryWeaponSpriteIds[slotId], textColor, TEXT_ALIGN_TOPLEFT);
      gfxDoGifPaging();
      menuDrawFittedText(drawWindow, padding + 22, 103, contentWidth - 22, 0.65, textColor, weaponName, TEXT_ALIGN_TOPLEFT);
    } else {
      menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 103, contentWidth, 0.65, secondaryColor, "Any weapon", TEXT_ALIGN_TOPCENTER);
    }
    progress = contract->Kills / (float)contract->RequiredKills;
    snprintf(progressBuf, sizeof(progressBuf), "%'u / %'u", contract->Kills, contract->RequiredKills);
  } else {
    menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, 82, contentWidth, 0.7, secondaryColor, "Unavailable", TEXT_ALIGN_TOPCENTER);
    snprintf(progressBuf, sizeof(progressBuf), "--");
  }

  windowDrawBox(drawWindow, TEXT_ALIGN_TOPLEFT, padding, 120, contentWidth, 1, RAIDS_MENU_TRACK_COLOR, TEXT_ALIGN_TOPLEFT);
  menuDrawFittedText(drawWindow, padding, 126, contentWidth, 0.55, secondaryColor, "Rewards", TEXT_ALIGN_TOPLEFT);
  contractsDrawRewardRow(drawWindow, 140, "Bolts", contract->RewardBolts);
  contractsDrawRewardRow(drawWindow, 154, "Hero XP", contract->RewardPlayerXp);
  float rewardY = 168;
  if (contract->RewardClassXp > 0) {
    contractsDrawRewardRow(drawWindow, rewardY, "Class XP", contract->RewardClassXp);
    rewardY += 14;
  }
  if (hasWeapon)
    contractsDrawRewardRow(drawWindow, rewardY, "Weapon XP", contract->RewardWeaponXp);

  if (!contract->Activated) {
    int minutes = contract->RefreshInMinutes > 0 ? contract->RefreshInMinutes : 0;
    snprintf(strBuf, sizeof(strBuf), "Refresh in %dh %02dm", minutes / 60, minutes % 60);
  } else if (isCompleted) {
    snprintf(strBuf, sizeof(strBuf), "Ready to claim");
  } else if (isExpired) {
    snprintf(strBuf, sizeof(strBuf), "Expired");
  } else {
    snprintf(strBuf, sizeof(strBuf), "Expires in %d min", contract->ExpiresInMinutes);
  }
  menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, drawWindow->Height - 46, contentWidth, 0.6, statusColor, strBuf, TEXT_ALIGN_TOPCENTER);
  menuDrawFittedText(drawWindow, drawWindow->Width * 0.5, drawWindow->Height - 30, contentWidth, 0.65, textColor, progressBuf, TEXT_ALIGN_TOPCENTER);
  menuDrawProgressBar(drawWindow, padding, drawWindow->Height - 14, contentWidth, progress);
}

//--------------------------------------------------------------------------
void contractsDrawContractList(Window_t* drawWindow, int selectedIdx, RaidsContract_t* items, int count)
{
  const float padding = RAIDS_MENU_PADDING;
  const float gap = 8;
  float contractWidth = (drawWindow->Width - 2*padding - (count - 1)*gap) / count;
  int i;
  for (i = 0; i < count; ++i) {
    Window_t contractWindow;
    windowCreateFrom(&contractWindow, drawWindow, padding + i*(contractWidth + gap), padding, contractWidth, drawWindow->Height - 2*padding, TEXT_ALIGN_TOPLEFT);
    contractsDrawContractInfo(&contractWindow, &items[i], i);
  }
}

//--------------------------------------------------------------------------
void contractsDrawFooter(Window_t* drawWindow)
{
  char strBuf[128];
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  RaidsContract_t* selectedContract = bank ? &bank->Contracts[contractsState.SelectedIndex] : NULL;
  int hasSelected = selectedContract && selectedContract->Uid != 0;

  strBuf[0] = 0;
  if (contractsState.CanInteract && bankGetHasContracts() && contractsIsPlayerHighEnoughLevel()) {
    strcat(strBuf, "\x1A \x1B CONTRACT    ");
    if (hasSelected) {
      int isCompleted = contractsIsContractCompleted(selectedContract);
      int isExpired = contractsIsContractExpired(selectedContract);
      if (!selectedContract->Activated && selectedContract->RefreshInMinutes > 0) strcat(strBuf, "\x10 ACCEPT    ");
      else if (isCompleted) strcat(strBuf, "\x10 CLAIM    ");
      else if (isExpired) strcat(strBuf, "\x10 DISMISS    ");
      if (!isCompleted && !isExpired) strcat(strBuf, "\x13 BUYOUT    ");
    }
  }
  strcat(strBuf, "\x12 CLOSE");
  menuDrawFooter(drawWindow, strBuf, 0.7, TEXT_ALIGN_MIDDLELEFT, RAIDS_MENU_PADDING);
}

//--------------------------------------------------------------------------
void contractsDrawRerollDialog(Window_t* drawWindow, int contractInfoHeight)
{
  char strBuf[64];
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  RaidsContract_t* selectedContract = &bank->Contracts[contractsState.SelectedIndex];
  u32 cost = contractsIsContractExpired(selectedContract) ? 0 : CONTRACTS_REROLL_COST;
  const float padding = RAIDS_MENU_POPUP_PADDING;
  const float detailsX = 192;
  float detailsWidth = drawWindow->Width - detailsX - padding;

  menuDrawPopupFrame(drawWindow, "Buyout Contract", RAIDS_MENU_TEXT_COLOR, "\x10 PAY        \x12 CANCEL");
  Window_t windowContractInfo;
  windowCreateFrom(&windowContractInfo, drawWindow, padding, 40, 168, contractInfoHeight, TEXT_ALIGN_TOPLEFT);
  contractsDrawContractInfo(&windowContractInfo, selectedContract, -1);

  Window_t costWindow;
  windowCreateFrom(&costWindow, drawWindow, detailsX, 40, detailsWidth, 144, TEXT_ALIGN_TOPLEFT);
  windowFill(&costWindow, RAIDS_MENU_TRACK_COLOR);
  menuDrawFittedText(&costWindow, detailsWidth * 0.5, 12, detailsWidth - 16, 0.55, RAIDS_MENU_SECONDARY_COLOR, "Buyout cost", TEXT_ALIGN_TOPCENTER);
  snprintf(strBuf, sizeof(strBuf), "%'u bolts", cost);
  menuDrawFittedText(&costWindow, detailsWidth * 0.5, 30, detailsWidth - 16, 0.95, bankGetBolts() < cost ? RAIDS_MENU_WARNING_COLOR : RAIDS_MENU_TEXT_COLOR, strBuf, TEXT_ALIGN_TOPCENTER);
  menuDrawFittedText(&costWindow, detailsWidth * 0.5, 66, detailsWidth - 16, 0.55, RAIDS_MENU_SECONDARY_COLOR, "Bolts available", TEXT_ALIGN_TOPCENTER);
  snprintf(strBuf, sizeof(strBuf), "%'u", bankGetBolts());
  menuDrawFittedText(&costWindow, detailsWidth * 0.5, 84, detailsWidth - 16, 0.8, RAIDS_MENU_TEXT_COLOR, strBuf, TEXT_ALIGN_TOPCENTER);
  if (bankGetBolts() < cost)
    menuDrawFittedText(&costWindow, detailsWidth * 0.5, 116, detailsWidth - 16, 0.65, RAIDS_MENU_WARNING_COLOR, "Not enough bolts", TEXT_ALIGN_TOPCENTER);
}

//--------------------------------------------------------------------------
void contractsDraw(void)
{
  const float windowWidth = 480;
  const float windowHeight = 328;
  const float titleHeight = RAIDS_MENU_TITLE_HEIGHT;
  const float summaryHeight = 24;
  const float footerHeight = RAIDS_MENU_FOOTER_HEIGHT;
  const float bodyHeight = windowHeight - titleHeight - summaryHeight - footerHeight;
  struct RaidsState* state = MapConfig.State;
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  char strBuf[64];
  Window_t drawWindow;

  if (!state) return;

  windowCreate(&drawWindow, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, windowWidth, windowHeight, TEXT_ALIGN_MIDDLECENTER);
  menuDrawFrame(&drawWindow, contractsState.CanInteract ? "Manage Contracts" : "Contracts", titleHeight);

  Window_t summaryWindow;
  windowCreateFrom(&summaryWindow, &drawWindow, 0, titleHeight, windowWidth, summaryHeight, TEXT_ALIGN_TOPLEFT);
  windowFill(&summaryWindow, RAIDS_MENU_PANEL_COLOR);
  if (bank && bankGetHasAccount()) snprintf(strBuf, sizeof(strBuf), "Bolts: %'u", bankGetBolts());
  else snprintf(strBuf, sizeof(strBuf), "Bolts: --");
  menuDrawFittedText(&summaryWindow, 8, 6, 164, 0.7, RAIDS_MENU_TEXT_COLOR, strBuf, TEXT_ALIGN_TOPLEFT);
  if (bank && bankGetHasAccount()) snprintf(strBuf, sizeof(strBuf), "Hero Level: %d", bankGetLevel() + 1);
  else snprintf(strBuf, sizeof(strBuf), "Hero Level: --");
  menuDrawFittedText(&summaryWindow, windowWidth * 0.5, 6, 112, 0.7, RAIDS_MENU_TEXT_COLOR, strBuf, TEXT_ALIGN_TOPCENTER);
  menuDrawFittedText(&summaryWindow, windowWidth - 8, 6, 120, 0.6, RAIDS_MENU_SECONDARY_COLOR, contractsState.CanInteract ? "Select a contract" : "View only", TEXT_ALIGN_TOPRIGHT);

  Window_t listWindow;
  windowCreateFrom(&listWindow, &drawWindow, 0, titleHeight + summaryHeight, windowWidth, bodyHeight, TEXT_ALIGN_TOPLEFT);
  if (!bank || !bankGetHasAccount()) {
    windowDrawText(&listWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.7, RAIDS_MENU_SECONDARY_COLOR, "Loading account...", -1, TEXT_ALIGN_MIDDLECENTER);
  } else if (!contractsIsPlayerHighEnoughLevel()) {
    snprintf(strBuf, sizeof(strBuf), "Requires Hero Level %d to collect contracts", CONTRACTS_MIN_LEVEL + 1);
    menuDrawFittedText(&listWindow, windowWidth * 0.5, bodyHeight * 0.5, windowWidth - 16, 0.8, RAIDS_MENU_WARNING_COLOR, strBuf, TEXT_ALIGN_MIDDLECENTER);
  } else if (!bankGetHasContracts()) {
    windowDrawText(&listWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.7, RAIDS_MENU_SECONDARY_COLOR, bankHasPendingContractsRequest() ? "Loading contracts..." : "Contracts unavailable", -1, TEXT_ALIGN_MIDDLECENTER);
  } else {
    contractsDrawContractList(&listWindow, contractsState.SelectedIndex, bank->Contracts, BANK_MAX_CONTRACTS);
  }

  Window_t footerWindow;
  windowCreateFrom(&footerWindow, &drawWindow, 0, 0, windowWidth, footerHeight, TEXT_ALIGN_BOTTOMCENTER);
  contractsDrawFooter(&footerWindow);
  windowBorder(&drawWindow, RAIDS_MENU_BORDER_COLOR, 1, 1, 1, 1);

  if (contractsState.ShowRerollDialog) {
    menuDrawBackdrop(&drawWindow);
    Window_t popupWindow;
    windowCreateFrom(&popupWindow, &drawWindow, 0, 0, 400, 328, TEXT_ALIGN_MIDDLECENTER);
    contractsDrawRerollDialog(&popupWindow, bodyHeight - 2*RAIDS_MENU_PADDING);
  }
}

//--------------------------------------------------------------------------
void contractsHandleInput(void)
{
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  RaidsContract_t* selectedContract = &bank->Contracts[contractsState.SelectedIndex];
  Player* localPlayer = playerGetFromSlot(0);

  if (contractsState.ShowRerollDialog) {

    // handle close input
    if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
      contractsState.ShowRerollDialog = 0;
      return;
    } else if (padGetButtonDown(0, PAD_CROSS) > 0) {
      u32 cost = contractsIsContractExpired(selectedContract) ? 0 : CONTRACTS_REROLL_COST;
      if (bankGetBolts() < cost) {
        playEquipRejectSound(localPlayer);
        return;
      }
      contractsState.ShowRerollDialog = 0;
      playPaidSound(localPlayer);
      contractsReroll(selectedContract);
    }

    return;
  }

  if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
    contractsClose();
  } else if (!contractsState.CanInteract) {
    return; // interaction disabled
  } else if (!contractsIsPlayerHighEnoughLevel()) {
    return; // not high enough level to interact
  } else if (padGetButtonDown(0, PAD_CROSS) > 0 && !selectedContract->Activated) {
    contractsActivate(selectedContract);
    playPaidSound(localPlayer);
  } else if (padGetButtonDown(0, PAD_CROSS) > 0 && contractsIsContractCompleted(selectedContract)) {
    contractsComplete(selectedContract);
    playPaidSound(localPlayer);
  } else if (padGetButtonDown(0, PAD_CROSS) > 0 && contractsIsContractExpired(selectedContract)) {
    contractsReroll(selectedContract);
  }  else if (padGetButtonDown(0, PAD_SQUARE) > 0 && !contractsIsContractCompleted(selectedContract) && !contractsIsContractExpired(selectedContract)) {
    contractsState.ShowRerollDialog = 1;
  } else if (padGetButtonDown(0, PAD_LEFT) > 0 && contractsState.SelectedIndex > 0) {
    --contractsState.SelectedIndex;
  } else if (padGetButtonDown(0, PAD_RIGHT) > 0 && (contractsState.SelectedIndex+1) < BANK_MAX_CONTRACTS) {
    ++contractsState.SelectedIndex;
  }
}

//--------------------------------------------------------------------------
void contractsFrameTick(void)
{
  if (!MapConfig.State) return;
  if (!contractsState.VendorMoby) return;

  // draw
  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_CONTRACTS) {
    contractsDraw();
    return;
  }
}

//--------------------------------------------------------------------------
int contractsPlayerIsLookingAtVendor(Player* player, float radius, float radians)
{
  if (!contractsState.VendorMoby) return 0;

  float distance = vector_distance(player->PlayerPosition, contractsState.VendorMoby->Position);
  if (distance > radius) return 0;

  VECTOR playerToVendor;
  vector_subtract(playerToVendor, contractsState.VendorMoby->Position, player->PlayerPosition);
	float playerToVendorLen = vector_length(playerToVendor);
	float playerToVendorYaw = atan2f(playerToVendor[1] / playerToVendorLen, playerToVendor[0] / playerToVendorLen);
  float yaw = fabsf(clampAngle(playerToVendorYaw - player->PlayerYaw));
  if (yaw > radians) return 0;

  return 1;
}

//--------------------------------------------------------------------------
void contractsTick(void)
{
  if (!MapConfig.State) return;

  if (shouldCloseMenu()
      || (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_CONTRACTS
          && contractsState.CanInteract && !isOnHubWorld())) {
    contractsClose();
    return;
  }

  // periodically pull contracts
  --contractsState.AutoRefreshTicker;
  if (contractsState.AutoRefreshTicker <= 0) {
    contractsGetContracts();
  }

  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_CONTRACTS) {
    contractsHandleInput();
    return;
  }

  // check for open
  if (MapConfig.State->MenuOpen != RAIDS_CUSTOM_MENU_NONE) return;

  Player* localPlayer = playerGetFromSlot(0);
  if (!localPlayer) return;

  // open view only
  if (padGetButtonDown(0, PAD_RIGHT) > 0) {
    contractsOpen(0);
  }

  // open management via vendor moby
#if CONTRACTS_VENDOR_MOBY_OCLASS
  char buf[64];
  if (contractsPlayerIsLookingAtVendor(localPlayer, 5, 60 * MATH_DEG2RAD)) {
    snprintf(contractsState.VendorLocalStrBufs[0], sizeof(contractsState.VendorLocalStrBufs[0]), "\x12 Manage Contracts");
    uiShowPopup(0, contractsState.VendorLocalStrBufs[0]);
    if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
      hudHidePopup();
      contractsOpen(1);
    }
  }
#endif
}

//--------------------------------------------------------------------------
void contractsInit(void)
{
#if CONTRACTS_VENDOR_MOBY_OCLASS
  Moby* mobyStart = mobyListGetStart();
  contractsState.VendorMoby = mobyFindNextByOClass(mobyStart, CONTRACTS_VENDOR_MOBY_OCLASS);
  DPRINTF("found contracts vendor %08X\n", (u32)contractsState.VendorMoby);
#endif
}
