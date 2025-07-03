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
#include "mob.h"
#include "shared.h"
#include "pathfind.h"
#include "window.h"
#include "maputils.h"

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
extern char inventoryWeaponSpriteDims[];
extern char inventorySkillSpriteIds[];
extern char* inventorySkillNames[];
extern char* inventoryRarityNames[];
extern char* inventoryBadgeNames[];
extern char* inventoryPaintNames[];
extern char* inventoryPaintSpecialNames[];
extern char* inventoryModNames[];
extern char bankRarityCode[];

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

  // apply
  bankAddBolts(item->RewardBolts);
  bankAddXP(item->RewardPlayerXp);
  if (item->RequiredKillsGadgetId > 0)
    bankAddWeaponXP(item->RewardWeaponXp, item->RequiredKillsGadgetId);

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
}

//--------------------------------------------------------------------------
void contractsReroll(RaidsContract_t* item)
{
  void* connection = netGetLobbyServerConnection();
  if (!connection) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

  // not enough money
#if !DEV_BUILD
  if (bankGetBolts() < CONTRACTS_REROLL_COST) return;
  bankSubtractBolts(CONTRACTS_REROLL_COST);
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
  if (gameHasEnded()) return;
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
void contractsDrawContractInfo(Window_t* drawWindow, RaidsContract_t* contract, int idx)
{
  u32 selectedColor = 0x40008080; // yellow
  u32 progressColor = 0x40008000; // green
  u32 textColor = 0x80FFFFFF; // white
  u32 spriteColor = 0x80808080; // gray
  u32 starColor = 0x80008080; // yellow
  const u32 contractBorderColor = 0x80202020; // gray
  const u32 contractSelectedBorderColor = 0x80008080; // yellow
  const u32 bgColor = 0x70303030; // dark gray
  const u32 contractActivatedBgColor = 0x40206020; // green
  const u32 contractExpiredBgColor = 0x40202060; // red
  const u32 rewardTextColor = 0x8000E0E0; // yellow
  const int titlePadding = 5;
  const int lineHeight = 12;
  char strBuf[128];
  char progBuf[32];
  float strW = 0;
  float progress = 0;
  int i;
  int isSelected = contractsState.SelectedIndex == idx && contractsState.CanInteract;
  u32 borderColor = contractBorderColor;
  
  // bg
  windowFill(drawWindow, bgColor);

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;
  if (!contract || !contract->Uid) return;

  int contractIsActivated = contract->Activated;
  int contractIsExpired = contractsIsContractExpired(contract);
  int contractIsCompleted = contractsIsContractCompleted(contract);
  int slotId = weaponIdToSlot(contract->RequiredKillsGadgetId);

  if (contractIsCompleted) {
    windowFill(drawWindow, contractActivatedBgColor);
    borderColor = colorLerp(contractActivatedBgColor, 0x80000000, 0.5);
  } else if (contractIsExpired) {
    windowFill(drawWindow, contractExpiredBgColor);
    borderColor = colorLerp(contractExpiredBgColor, 0x80000000, 0.5);
  } else if (contractIsActivated) {
    windowFill(drawWindow, contractActivatedBgColor);
    borderColor = colorLerp(contractActivatedBgColor, 0x80000000, 0.5);
  }

  // 
  Window_t progressWindow;
  windowCreateFrom(&progressWindow, drawWindow, 0, 0, drawWindow->Width, 30, TEXT_ALIGN_BOTTOMCENTER);

  // map
  snprintf(strBuf, sizeof(strBuf), "%s", contract->MapName[0] ? contract->MapName : "Any Map");
  windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, 2, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
  windowMove(drawWindow, 0, 12);
  if (contract->RequiredRaidTimeMs > 0) {

    int seconds = (contract->RequiredRaidTimeMs / 1000);
      
    // difficulty
    if (contract->RequiredDifficultyStars >= 0) {
      gfxSetupGifPaging(0);
      float starWidth = 20;
      float starsWidth = contract->RequiredDifficultyStars * starWidth;
      for (i = 0; i <= contract->RequiredDifficultyStars; ++i) {
        windowDrawSprite(drawWindow, TEXT_ALIGN_TOPCENTER, (i*starWidth) - (starsWidth/2), 6, 16, 16, 88, 32, 32, starColor, TEXT_ALIGN_TOPCENTER);
      }
      gfxDoGifPaging();
    }
    windowMove(drawWindow, 0, 20);

    // raid time
    snprintf(strBuf, sizeof(strBuf), "Complete in under");
    windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, 0, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
    windowMove(drawWindow, 0, 16);
    snprintf(strBuf, sizeof(strBuf), "\x0A%d:%02d\x08 min", seconds / 60, seconds % 60);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, 0, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
    windowMove(drawWindow, 0, 20);

    // progress
    progress = contract->CompletedTimeMs > 0 && contract->CompletedTimeMs < contract->RequiredRaidTimeMs ? 1 : 0;
    snprintf(progBuf, sizeof(progBuf), "%.1f%%", 100 * progress);

  } else if (contract->RequiredKills > 0) {

    windowMove(drawWindow, 0, 16);

    // weapon
    float killsOffsetX = 0;
    if (slotId > 0) {
      int spriteId = inventoryWeaponSpriteIds[slotId];
      int spriteDim = inventoryWeaponSpriteDims[slotId];
      gfxSetupGifPaging(0);
      windowDrawSprite(drawWindow, TEXT_ALIGN_TOPCENTER, -14, 0, 16, 16, spriteId, spriteDim, spriteDim, textColor, TEXT_ALIGN_TOPCENTER);
      gfxDoGifPaging();
      killsOffsetX += 10;
    }

    // kills
    snprintf(strBuf, sizeof(strBuf), "Kills");
    windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, killsOffsetX, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
    windowMove(drawWindow, 0, 16);

    // mob count
    snprintf(strBuf, sizeof(strBuf), "\x0A%'d\x08 %s", contract->RequiredKills, contract->RequiredKillsMobOClass > 0 ? contract->MobName : "");
    windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, 0, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
    windowMove(drawWindow, 0, 20 + 4);
    
    // progress
    progress = contract->Kills / (float)contract->RequiredKills;
    snprintf(progBuf, sizeof(progBuf), "%'d - %.1f%%", contract->Kills, 100 * progress);

  } else {
    
    // invalid
  }

  // rewards
  snprintf(strBuf, sizeof(strBuf), "Rewards");
  windowDrawText(drawWindow, TEXT_ALIGN_TOPCENTER, 0, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
  windowMove(drawWindow, 0, 14);
  gfxSetupGifPaging(0);
  windowDrawSprite(drawWindow, TEXT_ALIGN_TOPLEFT, 6, 2, 12, 12, inventoryAlphaModSpriteIds[ALPHA_MOD_JACKPOT], 32, 32, textColor, TEXT_ALIGN_TOPLEFT);
  gfxDoGifPaging();
  snprintf(strBuf, sizeof(strBuf), "+%'d", contract->RewardBolts);
  windowDrawText(drawWindow, TEXT_ALIGN_TOPRIGHT, -6, 2, 0.8, rewardTextColor, strBuf, -1, TEXT_ALIGN_TOPRIGHT);
  windowMove(drawWindow, 0, 12);
  snprintf(strBuf, sizeof(strBuf), "XP");
  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 6, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
  snprintf(strBuf, sizeof(strBuf), "+%'d", contract->RewardPlayerXp);
  windowDrawText(drawWindow, TEXT_ALIGN_TOPRIGHT, -6, 2, 0.8, rewardTextColor, strBuf, -1, TEXT_ALIGN_TOPRIGHT);
  windowMove(drawWindow, 0, 12);
  
  // weapon xp reward
  if (slotId > 0) {
    int spriteId = inventoryWeaponSpriteIds[slotId];
    int spriteDim = inventoryWeaponSpriteDims[slotId];
    gfxSetupGifPaging(0);
    windowDrawSprite(drawWindow, TEXT_ALIGN_TOPLEFT, 6, 2, 12, 12, spriteId, spriteDim, spriteDim, textColor, TEXT_ALIGN_TOPLEFT);
    gfxDoGifPaging();
    snprintf(strBuf, sizeof(strBuf), " XP");
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, 12+6, 2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
    snprintf(strBuf, sizeof(strBuf), "+%'d", contract->RewardWeaponXp);
    windowDrawText(drawWindow, TEXT_ALIGN_TOPRIGHT, -6, 2, 0.8, rewardTextColor, strBuf, -1, TEXT_ALIGN_TOPRIGHT);
  }

  // progress bar
  windowFill(&progressWindow, bgColor);
  windowDrawBox(&progressWindow, TEXT_ALIGN_MIDDLELEFT, 0, 0, ceilf(progress * progressWindow.Width), progressWindow.Height, progressColor, TEXT_ALIGN_MIDDLELEFT);
  windowBorder(&progressWindow, borderColor, 0, 1, 0, 0);

  // progress text
  windowDrawText(&progressWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.7, textColor, progBuf, -1, TEXT_ALIGN_MIDDLECENTER);

  // expiration
  windowReset(drawWindow);
  if (!contract->Activated) {
    snprintf(strBuf, sizeof(strBuf), "refresh in %d:%02d hours", contract->RefreshInMinutes / 60, contract->RefreshInMinutes % 60);
  } else if (contractIsCompleted) {
    snprintf(strBuf, sizeof(strBuf), "%cCOMPLETED", '\x0A');
  } else if (contractIsExpired) {
    snprintf(strBuf, sizeof(strBuf), "%cEXPIRED", '\x0E');
  } else {
    snprintf(strBuf, sizeof(strBuf), "expires in \x0E%d\x08 minutes", contract->ExpiresInMinutes);
  }
  windowDrawText(drawWindow, TEXT_ALIGN_BOTTOMCENTER, 0, -32, 0.7, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMCENTER);
  
  // border
  windowBorder(drawWindow, isSelected ? contractSelectedBorderColor : borderColor, 1, 1, 1, 1);
}

//--------------------------------------------------------------------------
void contractsDrawContractList(Window_t* drawWindow, int selectedIdx, RaidsContract_t* items, int count)
{
  const u32 bgColor = 0x50101010; // dark gray
  const u32 textColor = 0x80FFFFFF; // white
  const u32 selectedColor = 0x40008080; // yellow
  const int lineHeight = 12;
  char strBuf[64];
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  u32 bolts = bankGetBolts();

  // draw box
  windowFill(drawWindow, bgColor);

  // draw contracts
  int i;
  float padding = 8;
  float contractWidth = drawWindow->Width / BANK_MAX_CONTRACTS;
  float contractHeight = drawWindow->Height;
  for (i = 0; i < BANK_MAX_CONTRACTS; ++i) {
    Window_t contractWindow;
    windowCreateFrom(&contractWindow, drawWindow, ((i-(BANK_MAX_CONTRACTS/2)) * contractWidth), 0, contractWidth - padding, contractHeight - padding, TEXT_ALIGN_MIDDLECENTER);
    contractsDrawContractInfo(&contractWindow, &bank->Contracts[i], i);
  }
}

//--------------------------------------------------------------------------
void contractsDrawFooter(Window_t* drawWindow)
{
  const u32 bgSolidColor = 0x80000000;
  const u32 textColor = 0x80FFFFFF;
  char strBuf[128];
  RaidsPlayerBank_t* bank = bankGetLocalBank();

  // draw bg
  windowFill(drawWindow, bgSolidColor);

  // get selected item
  RaidsContract_t* selectedContract = &bank->Contracts[contractsState.SelectedIndex];
  int hasSelected = selectedContract && selectedContract->Uid != 0;
  int isSelectedCompleted = contractsIsContractCompleted(selectedContract);
  int isSelectedExpired = contractsIsContractExpired(selectedContract);
  int isSelectedActivated = selectedContract->Activated;

  strBuf[0] = 0;
  if (contractsState.CanInteract && bankGetHasContracts() && contractsIsPlayerHighEnoughLevel()) {
    if (!isSelectedActivated) strcat(strBuf, "\x10 ACCEPT    ");
    else if (isSelectedCompleted) strcat(strBuf, "\x10 CLAIM    ");
    else if (isSelectedExpired) strcat(strBuf, "\x10 DISMISS    ");
    if (hasSelected && !isSelectedCompleted && !isSelectedExpired) strcat(strBuf, "\x13 BUYOUT    ");
  }
  strcat(strBuf, "\x12 CLOSE");
  windowDrawText(drawWindow, TEXT_ALIGN_BOTTOMLEFT, 2, -2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMLEFT);
}

//--------------------------------------------------------------------------
void contractsDrawRerollDialog(Window_t* drawWindow, int contractInfoHeight)
{
  char buttonBuf[64];
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  RaidsContract_t* selectedContract = &bank->Contracts[contractsState.SelectedIndex];

  snprintf(buttonBuf, sizeof(buttonBuf), "\x10 \x0E%'d\x08 PAY        \x12 CANCEL", contractsIsContractExpired(selectedContract) ? 0 : CONTRACTS_REROLL_COST);
  windowDrawDialog(drawWindow, "Buyout Contract", NULL, "", buttonBuf);

  Window_t windowContractInfo;
  windowCreateFrom(&windowContractInfo, drawWindow, 0, 0, 150, contractInfoHeight, TEXT_ALIGN_MIDDLECENTER);
  contractsDrawContractInfo(&windowContractInfo, selectedContract, -1);
}

//--------------------------------------------------------------------------
void contractsDraw(void)
{
  u32 bgColor = 0x60000000;
  u32 bgSolidColor = 0x80000000;
  u32 borderColor = 0x80000020;
  u32 textColor = 0x80FFFFFF;
  const float footerHeight = 20;
  const float itemListDetailsHeight = 180;
  struct RaidsState* state = MapConfig.State;
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  float x,y;
  char strBuf[64];
  Window_t drawWindow;

  if (!state) return;

  // setup draw state
  windowCreate(&drawWindow, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, 450, itemListDetailsHeight + 40, TEXT_ALIGN_MIDDLECENTER);

  // draw frame
  windowFill(&drawWindow, bgColor);

  // draw title text
  windowDrawText(&drawWindow, TEXT_ALIGN_TOPCENTER, 0, 0, 1.1, textColor, "Contracts", -1, TEXT_ALIGN_TOPCENTER);

  //snprintf(strBuf, sizeof(strBuf), "Store Refresh in %d:%02d:%02d", (itemRotationInSeconds / (60*60)), (itemRotationInSeconds / 60) % 60, itemRotationInSeconds % 60);
  //windowDrawText(&drawWindow, TEXT_ALIGN_TOPCENTER, 0, 20, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);

  // draw footer
  Window_t windowFooter;
  windowCreateFrom(&windowFooter, &drawWindow, 0, 0, drawWindow.Width, footerHeight, TEXT_ALIGN_BOTTOMCENTER);
  contractsDrawFooter(&windowFooter);

  // draw contracts
  Window_t drawWindowContractList;
  windowCreateFrom(&drawWindowContractList, &drawWindow, 0, -footerHeight, 450, itemListDetailsHeight, TEXT_ALIGN_BOTTOMLEFT);
  if (contractsIsPlayerHighEnoughLevel()) {
    contractsDrawContractList(&drawWindowContractList, contractsState.SelectedIndex, bank->Contracts, BANK_MAX_CONTRACTS);
  } else {
    snprintf(strBuf, sizeof(strBuf), "You must be at least level %d to collect Contracts.", CONTRACTS_MIN_LEVEL + 1);
    windowDrawText(&drawWindowContractList, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLECENTER);
  }
    
  // draw border
  windowBorder(&drawWindow, borderColor, 1, 1, 1, 1);
  
  // draw reroll dialog
  if (contractsState.ShowRerollDialog) {
    Window_t windowRerollDialog;
    windowCreateFrom(&windowRerollDialog, &drawWindow, 0, 0, 300, itemListDetailsHeight + 40, TEXT_ALIGN_MIDDLECENTER);
    contractsDrawRerollDialog(&windowRerollDialog, itemListDetailsHeight);
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
      if (bankGetBolts() < CONTRACTS_REROLL_COST) {
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
  if (!isInGame()) return;

  // force close if game has ended
  if (gameHasEnded()) {
    contractsClose();
    return;
  }

  // periodically pull contracts
  --contractsState.AutoRefreshTicker;
  if (contractsState.AutoRefreshTicker <= 0) {
    contractsGetContracts();
  }

  // handle close input
  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_CONTRACTS && gameIsAnyStartMenuOpen()) {
    contractsClose();
    return;
  }

  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_CONTRACTS) {
    contractsHandleInput();
    return;
  }

  // check for open
  if (MapConfig.State->MenuOpen != RAIDS_CUSTOM_MENU_NONE) return;
  if (PATCH_POINTERS_PATCHMENU != 0 || gameIsAnyStartMenuOpen()) return;

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
