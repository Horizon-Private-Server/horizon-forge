#include <libdl/game.h>
#include <libdl/net.h>
#include <libdl/pad.h>
#include <libdl/player.h>
#include <libdl/random.h>
#include <libdl/stdio.h>
#include <libdl/string.h>
#include "bank.h"
#include "debugmenu.h"
#include "game.h"
#include "menu.h"
#include "utils.h"

#if DEBUG && defined(DEBUG_MENU)

#define DEBUGMENU_WEAPON_FIRST (2)
#define DEBUGMENU_CLASS_LEVEL  (DEBUGMENU_WEAPON_FIRST + WEAPON_SLOT_COUNT - 1)
#define DEBUGMENU_BOLTS        (DEBUGMENU_CLASS_LEVEL + 1)
#define DEBUGMENU_LOOT         (DEBUGMENU_BOLTS + 1)
#define DEBUGMENU_ACTION_COUNT (DEBUGMENU_LOOT + 1)

enum DebugMenuPending
{
  DEBUGMENU_PENDING_NONE,
  DEBUGMENU_PENDING_RESET,
};

struct DebugMenuState
{
  int IsOpen;
  int SelectedIndex;
  int ResetConfirm;
  int Pending;
  char Message[96];
} debugmenuState;

char* debugmenuActions[] = {
  "Reset account",
  "Hero level +1",
  "Dual Vipers proficiency +1",
  "Magma Cannon proficiency +1",
  "Arbiter proficiency +1",
  "Fusion Rifle proficiency +1",
  "Mine Launcher proficiency +1",
  "B6 proficiency +1",
  "Scorpion Flail proficiency +1",
  "Holoshield proficiency +1",
  "Equipped class level +1",
  "Give 500,000 bolts",
  "Spawn random loot drop",
};

//--------------------------------------------------------------------------
int debugmenuActionIsEnabled(int action)
{
  if (debugmenuState.Pending || !bankGetHasAccount() || bankHasPendingAccountRequest() || !netGetLobbyServerConnection()) return 0;
  if (action == 1) return bankGetLevel() < LEVELUP_MAX_PLAYER_LEVEL;
  if (action >= DEBUGMENU_WEAPON_FIRST && action < DEBUGMENU_CLASS_LEVEL) {
    int gadgetId = weaponSlotToId(action - DEBUGMENU_WEAPON_FIRST + 1);
    return getProficiencyFromXp(bankGetWeaponXP(gadgetId)) < LEVELUP_MAX_PROF_LEVEL;
  }
  if (action == DEBUGMENU_CLASS_LEVEL) {
    RaidsPlayerClassProgress_t* playerClass = &bankGetLocalBank()->EquippedClassProgress;
    return bankGetHasEquippedClassProgress() && !bankHasPendingEquippedClassProgressRequest()
      && classesGetDefinition(playerClass->ClassType)
      && bankGetClassLevel(playerClass, bankGetClassXP(playerClass->ClassType)) < RAIDS_CLASS_MAX_LEVEL;
  }
  if (action == DEBUGMENU_BOLTS) return bankGetBolts() < 0xFFFFFFFFU;
  if (action == DEBUGMENU_LOOT) return MapConfig.RequestPrestigeLootFunc != NULL;
  return action == 0;
}

//--------------------------------------------------------------------------
void debugmenuAction(int action)
{
  if (!debugmenuActionIsEnabled(action)) {
    safe_strcpy(debugmenuState.Message, "Action unavailable: check loaded data, connection or level cap.", sizeof(debugmenuState.Message));
    return;
  }

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (action == 0) {
    bankRequestAccountReset();
    // Do not upload pre-reset contract progress with the refresh request.
    memset(bank->Contracts, 0, sizeof(bank->Contracts));
    bankRequestAccountFromServer();
    bankRequestEquippedInventoryFromServer();
    bankRequestEquippedClassProgressFromServer();
    bankRequestAllClassProgressFromServer();
    bankRequestContractsFromServer();
    debugmenuState.Pending = DEBUGMENU_PENDING_RESET;
    safe_strcpy(debugmenuState.Message, "Refreshing the reset account...", sizeof(debugmenuState.Message));
  } else if (action == 1) {
    int level = bankGetLevel();
    bankAddXP(getXpForLevel(level + 1) - bankGetXP());
    safe_strcpy(debugmenuState.Message, "Hero XP awarded.", sizeof(debugmenuState.Message));
  } else if (action >= DEBUGMENU_WEAPON_FIRST && action < DEBUGMENU_CLASS_LEVEL) {
    int gadgetId = weaponSlotToId(action - DEBUGMENU_WEAPON_FIRST + 1);
    double xp = bankGetWeaponXP(gadgetId);
    int level = getProficiencyFromXp(xp);
    bankAddWeaponXP(getXpForProficiency(level + 1) - xp, gadgetId);
    safe_strcpy(debugmenuState.Message, "Weapon XP awarded.", sizeof(debugmenuState.Message));
  } else if (action == DEBUGMENU_CLASS_LEVEL) {
    RaidsPlayerClassProgress_t* playerClass = &bank->EquippedClassProgress;
    int level = bankGetClassLevel(playerClass, bankGetClassXP(playerClass->ClassType));
    bankAddClassXP(bankGetClassXpForLevel(playerClass, level + 1) - bankGetClassXP(playerClass->ClassType), playerClass->ClassType);
    safe_strcpy(debugmenuState.Message, "Class XP awarded.", sizeof(debugmenuState.Message));
  } else if (action == DEBUGMENU_BOLTS) {
    u32 amount = 500000;
    if (amount > 0xFFFFFFFFU - bankGetBolts()) amount = 0xFFFFFFFFU - bankGetBolts();
    bankAddBolts(amount);
    bankSendAccountToServer();
    snprintf(debugmenuState.Message, sizeof(debugmenuState.Message), "Granted %'u bolts.", amount);
  } else if (action == DEBUGMENU_LOOT) {
    MapConfig.RequestPrestigeLootFunc(weaponSlotToId(1 + rand(WEAPON_SLOT_COUNT - 1)));
    safe_strcpy(debugmenuState.Message, "Random loot requested.", sizeof(debugmenuState.Message));
  }
}

//--------------------------------------------------------------------------
void debugmenuTickPending(void)
{
  if (debugmenuState.Pending == DEBUGMENU_PENDING_RESET) {
    if (!bankGetHasAccount() && !bankHasPendingAccountRequest()) bankRequestAccountFromServer();
    if (!bankGetHasEquippedInventory() && !bankHasPendingEquippedInventoryRequest()) bankRequestEquippedInventoryFromServer();
    if (!bankGetHasEquippedClassProgress() && !bankHasPendingEquippedClassProgressRequest()) bankRequestEquippedClassProgressFromServer();
    if (!bankGetHasAllClassProgress() && !bankHasPendingAllClassProgressRequest()) bankRequestAllClassProgressFromServer();
    if (!bankGetHasContracts() && !bankHasPendingContractsRequest()) bankRequestContractsFromServer();
    if (!bankGetHasAccount() || !bankGetHasEquippedInventory() || !bankGetHasEquippedClassProgress()
        || !bankGetHasAllClassProgress() || !bankGetHasContracts()) return;
    bankBroadcastAccount();
    bankBroadcastEquippedInventory();
    bankBroadcastEquippedClassProgress();
    debugmenuState.Pending = DEBUGMENU_PENDING_NONE;
    safe_strcpy(debugmenuState.Message, "Account reset.", sizeof(debugmenuState.Message));
  }
}

//--------------------------------------------------------------------------
void debugmenuDraw(void)
{
  Window_t window, footer;
  windowCreate(&window, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, 480, 300, TEXT_ALIGN_MIDDLECENTER);
  menuDrawFrame(&window, "Raids Debug", 24);
  windowDrawText(&window, TEXT_ALIGN_TOPLEFT, 8, 32, 0.6, RAIDS_MENU_SECONDARY_COLOR,
    bankGetHasAccount() ? "Actions affect your account." : "Loading account...", -1, TEXT_ALIGN_TOPLEFT);
  int i;
  for (i = 0; i < DEBUGMENU_ACTION_COUNT; ++i) {
    float y = 50 + i * 16;
    int enabled = debugmenuActionIsEnabled(i);
    u32 color = enabled ? RAIDS_MENU_TEXT_COLOR : RAIDS_MENU_SECONDARY_COLOR;
    if (i == debugmenuState.SelectedIndex)
      windowDrawBox(&window, TEXT_ALIGN_TOPLEFT, 4, y, 472, 16, 0x40008080, TEXT_ALIGN_TOPLEFT);
    menuDrawFittedText(&window, 8, y + 1, 330, 0.7, color, debugmenuActions[i], TEXT_ALIGN_TOPLEFT);
    char value[64] = "";
    if (bankGetHasAccount()) {
      int level = -1;
      int maxLevel = LEVELUP_MAX_PLAYER_LEVEL + 1;
      char* prefix = "Level ";
      if (i == 1) level = bankGetLevel() + 1;
      else if (i >= DEBUGMENU_WEAPON_FIRST && i < DEBUGMENU_CLASS_LEVEL) {
        level = getProficiencyFromXp(bankGetWeaponXP(weaponSlotToId(i - DEBUGMENU_WEAPON_FIRST + 1))) + 1;
        maxLevel = LEVELUP_MAX_PROF_LEVEL + 1;
        prefix = "P";
      } else if (i == DEBUGMENU_CLASS_LEVEL && bankGetHasEquippedClassProgress()) {
        RaidsPlayerClassProgress_t* playerClass = &bankGetLocalBank()->EquippedClassProgress;
        if (classesGetDefinition(playerClass->ClassType)) level = bankGetClassLevel(playerClass, bankGetClassXP(playerClass->ClassType));
        maxLevel = RAIDS_CLASS_MAX_LEVEL;
      } else if (i == DEBUGMENU_BOLTS) {
        snprintf(value, sizeof(value), "%'u", bankGetBolts());
      }
      if (level >= 0) {
        if (level >= maxLevel) snprintf(value, sizeof(value), "%s%d (MAX)", prefix, level);
        else snprintf(value, sizeof(value), "%s%d -> %d", prefix, level, level + 1);
      }
    }
    menuDrawFittedText(&window, 472, y + 2, 120, 0.6, color, value, TEXT_ALIGN_TOPRIGHT);
  }
  menuDrawFittedText(&window, 8, 264, 464, 0.6, RAIDS_MENU_PROGRESS_COLOR, debugmenuState.Message, TEXT_ALIGN_TOPLEFT);
  windowCreateFrom(&footer, &window, 0, 0, 480, 16, TEXT_ALIGN_BOTTOMLEFT);
  menuDrawFooter(&footer, "\x1D \x1C OPTION    \x10 APPLY    \x12 CLOSE", 0.7, TEXT_ALIGN_MIDDLECENTER, 8);
  if (debugmenuState.ResetConfirm)
    menuDrawPopup(&window, "Reset account?", "Reset all Raids progression, inventory and contracts?", "\x10 RESET    \x12 CANCEL", 340, 140);
}

//--------------------------------------------------------------------------
void debugmenuClose(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!debugmenuState.IsOpen) return;
  debugmenuState.IsOpen = debugmenuState.ResetConfirm = 0;
  if (state && state->MenuOpen == RAIDS_CUSTOM_MENU_DEBUG) state->MenuOpen = RAIDS_CUSTOM_MENU_NONE;
  if (!state || state->MenuOpen == RAIDS_CUSTOM_MENU_NONE) padEnableInput();
}

//--------------------------------------------------------------------------
void debugmenuTick(void)
{
  struct RaidsState* state = MapConfig.State;
  Player* player = playerGetFromSlot(0);
  debugmenuTickPending();
  if (!state || shouldCloseMenu() || !playerIsValid(player) || playerIsDead(player)) {
    debugmenuClose();
    return;
  }
  if (debugmenuState.IsOpen && state->MenuOpen != RAIDS_CUSTOM_MENU_DEBUG) {
    debugmenuClose();
    return;
  }
  if (!debugmenuState.IsOpen) {
    if (state->MenuOpen == RAIDS_CUSTOM_MENU_NONE && padGetButtonDown(0, PAD_L1 | PAD_R1 | PAD_L2 | PAD_R2) > 0) {
      state->MenuOpen = RAIDS_CUSTOM_MENU_DEBUG;
      debugmenuState.IsOpen = 1;
      debugmenuState.SelectedIndex = debugmenuState.ResetConfirm = 0;
      if (!debugmenuState.Pending) debugmenuState.Message[0] = 0;
      padDisableInput();
    }
    return;
  }
  if (debugmenuState.ResetConfirm) {
    if (padGetButtonDown(0, PAD_TRIANGLE) > 0) debugmenuState.ResetConfirm = 0;
    else if (padGetButtonDown(0, PAD_CROSS) > 0) {
      debugmenuState.ResetConfirm = 0;
      debugmenuAction(0);
    }
    return;
  }
  if (padGetButtonDown(0, PAD_TRIANGLE) > 0) debugmenuClose();
  else if (padGetButtonDown(0, PAD_UP) > 0 && debugmenuState.SelectedIndex > 0) --debugmenuState.SelectedIndex;
  else if (padGetButtonDown(0, PAD_DOWN) > 0 && debugmenuState.SelectedIndex < DEBUGMENU_ACTION_COUNT - 1) ++debugmenuState.SelectedIndex;
  else if (padGetButtonDown(0, PAD_CROSS) > 0) {
    if (debugmenuState.SelectedIndex == 0 && debugmenuActionIsEnabled(0)) debugmenuState.ResetConfirm = 1;
    else debugmenuAction(debugmenuState.SelectedIndex);
  }
}

//--------------------------------------------------------------------------
void debugmenuFrameTick(void)
{
  if (debugmenuState.IsOpen && MapConfig.State && MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_DEBUG) debugmenuDraw();
}

//--------------------------------------------------------------------------
void debugmenuInit(void)
{
  memset(&debugmenuState, 0, sizeof(debugmenuState));
}

#endif
