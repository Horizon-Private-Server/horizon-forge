#include <string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/stdlib.h>
#include <libdl/color.h>
#include <libdl/moby.h>
#include <libdl/sound.h>
#include <libdl/pad.h>
#include <libdl/random.h>
#include <libdl/utils.h>
#include <libdl/net.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include "levelselect.h"
#include "game.h"
#include "maputils.h"
#include "window.h"
#include "common.h"

extern struct RaidsState State;

LevelselectDrawState_t levelselectDrawState = {
  .SelectedIdx = 0,
  .SelectedDifficulty = 0,
  .NumPlanets = 0,
  .InputCooldownTicks = 10
};

char* levelselectMissionTypes[] = {
  [RAIDS_MISSION_OPEN_WORLD] "Open World",
  [RAIDS_MISSION_HUB] "Hub",
  [RAIDS_MISSION_RAID] "Raid",
};

//--------------------------------------------------------------------------
void levelselectOpen(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;
  if (gameHasEnded()) return;
  if (!PATCH_INTEROP) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_NONE) return;

  state->MenuOpen = RAIDS_CUSTOM_MENU_LEVELSELECT;
  levelselectDrawState.SelectedMapFilename[0] = 0; // reset
  padDisableInput();
}

//--------------------------------------------------------------------------
void levelselectClose(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_LEVELSELECT) return;

  state->MenuOpen = RAIDS_CUSTOM_MENU_NONE;
  padEnableInput();
}

//--------------------------------------------------------------------------
void levelselectGo(LevelselectDrawState_t* drawState)
{
  if (hasPendingWorldHop() || !gameAmIHost() || !drawState || !drawState->SelectedMapFilename[0]) {
    playEquipRejectSound(playerGetFromSlot(0));
    return;
  }

  // force to 1 star difficulty for non-raids
  int missionType = drawState->SelectedMapExtraData.MissionType;
  int isBossRaid = missionType == RAIDS_MISSION_RAID;
  if (!isBossRaid) {
    drawState->SelectedDifficulty = 0;
  }

#if !DEBUG
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  int cost = drawState->SelectedMapExtraData.Cost[drawState->SelectedDifficulty];
  int level = bankGetLevel();
  int hasMinLevel = (level + 1) >= levelselectDrawState.SelectedMapExtraData.MinPlayerLevel;
  if (cost < 0 || cost > localBank->Account.Bolts || !hasMinLevel) {
    playEquipRejectSound(playerGetFromSlot(0));
    return;
  }

#else
  int cost = 0;
#endif

  // hop
  int isHub = strncmp(drawState->SelectedMapFilename, RAIDS_HUB_MAPFILENAME, sizeof(drawState->SelectedMapFilename)) == 0;
  if (MapConfig.BeginWorldHopFunc) {
    MapConfig.BeginWorldHopFunc(drawState->SelectedMapFilename, isHub ? 0 : drawState->SelectedDifficulty, isHub ? 0 : cost, 5 * TIME_SECOND);
  }

  levelselectClose();
}

//--------------------------------------------------------------------------
void levelselectDrawSpinner(LevelselectDrawState_t* drawState, float x, float y)
{
  gfxSetupGifPaging(0);

  x -= 16; // middle align (16*(3-1))/2
  float t = (int)(4*fastmodf(gameGetTime() / 1000.0, 1.0));
  int i = 0;
  while (i < t) {
    gfxHelperDrawSprite(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, x, y, 16, 16, 64, 64, 80, 0x80FFFFFF, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
    ++i;
    x += 16;
  }
  
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void levelselectDrawChallengesDialog(Window_t* drawWindow, int selectedIdx)
{
  static int drawItemsFrom = 0;
  u32 bgColor = 0x80020202; // dark gray
  u32 textColor = 0x80FFFFFF; // white
  u32 completedColor = 0x8000C000;
  u32 selectedColor = 0x40008080; // yellow
  u32 borderColor = 0x80000020;
  const int lineHeight = 16;
  const int titleHeight = 20;
  char* desc = NULL;
  char strBuf[64];
  int i;
  
  // draw box
  windowFill(drawWindow, bgColor);

  if (!levelselectDrawState.SelectedMapDef)
    return;

  // draw title text
  Window_t windowTitle;
  windowCreateFrom(&windowTitle, drawWindow, 0, 0, drawWindow->Width, titleHeight, TEXT_ALIGN_TOPCENTER);
  windowFill(&windowTitle, borderColor);
  snprintf(strBuf, sizeof(strBuf), "%s Challenges", levelselectDrawState.SelectedMapDef->Name);
  windowDrawText(&windowTitle, TEXT_ALIGN_MIDDLECENTER, 0, 0, 1.1, textColor, strBuf, -1, TEXT_ALIGN_MIDDLECENTER);
  windowMove(drawWindow, 0, titleHeight);

  if (selectedIdx < drawItemsFrom) drawItemsFrom = selectedIdx;
  int itemIdx = drawItemsFrom;
  gfxSetupGifPaging(0);
  while (itemIdx < levelselectDrawState.MapStats.ChallengesCount) {
    int cNameOff = (int)levelselectDrawState.SelectedMapExtraData.Challenges[(itemIdx*2) + 0];
    int cDescOff = (int)levelselectDrawState.SelectedMapExtraData.Challenges[(itemIdx*2) + 1];
    if (!cNameOff || !cDescOff) break;
    if (!windowHasArea(drawWindow)) { --itemIdx; break; }

    char* cName = (u32)levelselectDrawState.SelectedMapExtraDataBuf + cNameOff;
    char* cDesc = (u32)levelselectDrawState.SelectedMapExtraDataBuf + cDescOff;
    int isCompleted = (levelselectDrawState.MapStats.ChallengesMask & (1 << itemIdx)) != 0;

    // create line window
    Window_t windowLine;
    windowCreateFrom(&windowLine, drawWindow, 0, 0, drawWindow->Width * 0.5, lineHeight, TEXT_ALIGN_TOPLEFT);

    // selection highlight
    if (itemIdx == selectedIdx) {
      windowFill(&windowLine, selectedColor);
      desc = cDesc;
    }

    if (isCompleted)
      windowDrawSprite(&windowLine, TEXT_ALIGN_MIDDLELEFT, 5, 0, 12, 12, 35, 32, 32, completedColor, TEXT_ALIGN_MIDDLELEFT);

    // draw name
    windowDrawText(&windowLine, TEXT_ALIGN_MIDDLELEFT, 5 + (isCompleted ? 16 : 0), 0, 0.7, textColor, cName, -1, TEXT_ALIGN_MIDDLELEFT);
    windowMove(drawWindow, 0, lineHeight);
    ++itemIdx;
  }
  gfxDoGifPaging();

  // create desc window
  Window_t windowDesc;
  windowReset(drawWindow);
  windowMove(drawWindow, 0, titleHeight);
  windowCreateFrom(&windowDesc, drawWindow, 0, 0, drawWindow->Width * 0.5, drawWindow->Height - titleHeight, TEXT_ALIGN_TOPRIGHT);
  windowFill(&windowDesc, 0x80080808);
  if (desc) {
    windowDrawTextWindow(&windowDesc, TEXT_ALIGN_TOPLEFT, 5, 5, 0.8, textColor, desc, -1, TEXT_ALIGN_TOPLEFT);
  }

  // draw border
  windowReset(drawWindow);
  windowBorder(drawWindow, borderColor, 1, 1, 1, 1);
}

//--------------------------------------------------------------------------
int levelselectDrawMapList(Window_t* drawWindow, int selectedIdx, CustomMapDef_t** selectedMapDef)
{
  static int init = 0;
  static int drawItemsFrom = 0;
  u32 bgColor = 0x70000000; // dark gray
  u32 textColor = 0x80FFFFFF; // white
  u32 headerTextColor = 0x80C0C0FF; // light red
  u32 selectedColor = 0x40008080; // yellow
  const int lineHeight = 16;
  const int headerLineHeight = 20;
  int totalMapDefCount = 0;
  int numMaps = 0;
  int lastMissionType = 0;
  int i;

  // draw box
  windowFill(drawWindow, bgColor);

  // init
  if (!init && MapConfig.State) {
    levelselectDrawState.SelectedDifficulty = MapConfig.State->DifficultyStars;
  }

  if (drawItemsFrom > selectedIdx) drawItemsFrom = selectedIdx;

  if (PATCH_INTEROP && PATCH_INTEROP->GetCustomMapDefCount && PATCH_INTEROP->GetCustomMapDef) {
    if (PATCH_INTEROP->GetCustomMapDefCount) totalMapDefCount = PATCH_INTEROP->GetCustomMapDefCount();

    i = 0;
    for (; i < totalMapDefCount; ++i) {
      if (!windowHasArea(drawWindow)) break;
      
      Window_t windowLine;
      CustomMapDef_t* def = PATCH_INTEROP->GetCustomMapDef(i);
      if (!def) continue;
      if (def->ForcedCustomModeId != CUSTOM_MODE_RAIDS) continue;
      if (!(def->CustomModeExtraDataMask & (1<<CUSTOM_MODE_RAIDS))) continue;
      //if (strncmp(def->Filename, RAIDS_HUB_MAPFILENAME, sizeof(def->Filename)) == 0) continue;
      if (!init && MapConfig.State && def == MapConfig.State->CurrentMapDef) levelselectDrawState.SelectedIdx = numMaps;
      if (i < drawItemsFrom) { ++numMaps; continue; }

      int missionType = def->Subsort / 10000;
      if (lastMissionType != missionType) {
        windowCreateFrom(&windowLine, drawWindow, 0, 0, drawWindow->Width, headerLineHeight, TEXT_ALIGN_TOPRIGHT);
        windowDrawText(&windowLine, TEXT_ALIGN_BOTTOMCENTER, 5, -1, 1.0, headerTextColor, levelselectMissionTypes[missionType], -1, TEXT_ALIGN_BOTTOMCENTER);
        windowMove(drawWindow, 0, headerLineHeight);
        if (!windowHasArea(drawWindow)) break;
      }

      windowCreateFrom(&windowLine, drawWindow, 0, 0, drawWindow->Width, lineHeight, TEXT_ALIGN_TOPRIGHT);
      
      // draw selection line
      if (numMaps == selectedIdx) {
        *selectedMapDef = def;
        windowFill(&windowLine, selectedColor);
      }

      // draw map name
      char strBuf[64];
      snprintf(strBuf, sizeof(strBuf), "%s", def->Name);
      windowDrawText(&windowLine, TEXT_ALIGN_MIDDLELEFT, 5, 0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLELEFT);
      windowMove(drawWindow, 0, lineHeight);
      ++numMaps;
    }
  }

  init = 1;
  return numMaps;
}

//--------------------------------------------------------------------------
void levelselectDrawMapInfo(Window_t* drawWindow)
{
  static int drawItemsFrom = 0;
  u32 bgColor = 0x70000000; // dark gray
  u32 textColor = 0x80FFFFFF; // white
  u32 spriteColor = 0x80808080; // gray
  u32 selectedColor = 0x40008080; // yellow
  u32 starActiveColor = 0x80008080;
  u32 starInactiveColor = 0x80101010;
  u32 redColor = 0x800000C0;
  u32 yellowColor = 0x8000C0C0;
  u32 greenColor = 0x8000C000;
  const int authorHeight = 16;
  const int missionTypeHeight = 0; //16;
  const int descHeight = 128;
  const int progressHeight = 40;
  const int difficultySelectHeight = 32;
  const int difficultyCostHeight = 24;
  const int starHeight = 24;
  const int starSpacing = 5;
  const int descFullHeight = descHeight + starHeight + difficultySelectHeight;
  int totalMapDefCount = 0;
  int numMaps = 0;
  int i;
  char strBuf[128];
  int missionType = levelselectDrawState.SelectedMapExtraData.MissionType;
  int isBossRaid = missionType == RAIDS_MISSION_RAID;
  int difficulty = isBossRaid ? levelselectDrawState.SelectedDifficulty : 0;

  if (levelselectDrawState.NumPlanets > 0 && levelselectDrawState.SelectedMapFilename[0]) {

    RaidsPlayerBank_t* localBank = bankGetLocalBank();
    int level = bankGetLevel();
    char selectChar = gameAmIHost() ? '\x10' : '\x08';
    int cost = levelselectDrawState.SelectedMapExtraData.Cost[difficulty];
    int canAfford = cost <= localBank->Account.Bolts;
    int hasMinLevel = (level + 1) >= levelselectDrawState.SelectedMapExtraData.MinPlayerLevel;

    // author
    Window_t windowAuthor;
    windowCreateFrom(&windowAuthor, drawWindow, 0, 0, drawWindow->Width, authorHeight, TEXT_ALIGN_TOPLEFT);
    windowFill(&windowAuthor, 0x70000040);
    snprintf(strBuf, sizeof(strBuf), "Author: %s", levelselectDrawState.SelectedMapExtraData.Author);
    windowDrawText(&windowAuthor, TEXT_ALIGN_MIDDLELEFT, 5, 0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLELEFT);
    
    // mission type
    //Window_t windowMissionType;
    //windowCreateFrom(&windowMissionType, drawWindow, 0, authorHeight, drawWindow->Width, missionTypeHeight, TEXT_ALIGN_TOPLEFT);
    //windowFill(&windowMissionType, 0x70000040);
    //snprintf(strBuf, sizeof(strBuf), "Mission Type: %s", levelselectMissionTypes[missionType]);
    //windowDrawTextWindow(&windowMissionType, TEXT_ALIGN_TOPLEFT, 5, 5, 0.7, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);
    
    // description
    Window_t windowDescription;
    windowCreateFrom(&windowDescription, drawWindow, 0, authorHeight + missionTypeHeight, drawWindow->Width, isBossRaid ? descHeight : descFullHeight, TEXT_ALIGN_TOPLEFT);
    windowFill(&windowDescription, 0x60101010);
    windowDrawTextWindow(&windowDescription, TEXT_ALIGN_TOPLEFT, 5, 5, 0.7, textColor, levelselectDrawState.SelectedMapExtraData.Description, -1, TEXT_ALIGN_TOPLEFT);
    
    // progress
    Window_t windowProgress;
    windowCreateFrom(&windowProgress, drawWindow, 0, isBossRaid ? -(difficultyCostHeight + difficultySelectHeight) : -5, drawWindow->Width, progressHeight, TEXT_ALIGN_BOTTOMLEFT);
    windowFill(&windowProgress, 0x20000000);

    // gold bolts
    gfxSetupGifPaging(0);
    windowDrawSprite(&windowProgress, TEXT_ALIGN_TOPLEFT, 5, 5, 12, 12, 4, 32, 32, textColor, TEXT_ALIGN_TOPLEFT);
    gfxDoGifPaging();
    snprintf(strBuf, sizeof(strBuf), "%d/%d", countBits(levelselectDrawState.MapStats.CollectiblesMask), levelselectDrawState.MapStats.CollectiblesCount);
    windowDrawText(&windowProgress, TEXT_ALIGN_TOPLEFT, 5 + 16, 5, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT);

    // challenges
    gfxSetupGifPaging(0);
    windowDrawSprite(&windowProgress, TEXT_ALIGN_TOPRIGHT, -5, 5, 12, 12, 31, 32, 32, textColor, TEXT_ALIGN_TOPRIGHT);
    gfxDoGifPaging();
    snprintf(strBuf, sizeof(strBuf), "%d/%d", countBits(levelselectDrawState.MapStats.ChallengesMask), levelselectDrawState.MapStats.ChallengesCount);
    windowDrawText(&windowProgress, TEXT_ALIGN_TOPRIGHT, -(5+16), 5, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPRIGHT);

    // % complete
    float percentComplete = clamp(levelselectDrawState.MapStats.PercentageComplete, 0, 1);
    u32 progressColor = (percentComplete == 0) ? (redColor) : (percentComplete == 1 ? greenColor : yellowColor);
    snprintf(strBuf, sizeof(strBuf), "%.f%%", percentComplete * 100);
    windowDrawText(&windowProgress, TEXT_ALIGN_TOPCENTER, 0, 5, 1.0, progressColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);

    // best time
    if (isBossRaid && cost >= 0) {
      int bestTimeMs = levelselectDrawState.MapStats.BestTimeMsPerDifficulty[difficulty];
      int bestTimeSeconds = bestTimeMs / 1000;
      int bestTimeMinutes = bestTimeSeconds / 60;
      if (bestTimeMs > 0) {
        snprintf(strBuf, sizeof(strBuf), "Best Time: \x0A%d:%02d.%03d", bestTimeMinutes, bestTimeSeconds % 60, bestTimeMs % 1000);
      } else {
        snprintf(strBuf, sizeof(strBuf), "Best Time: \x0EIncomplete");
      }
      windowDrawText(&windowProgress, TEXT_ALIGN_BOTTOMCENTER, 0, -16, 0.9, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER);
    }
    
    // star select
    if (isBossRaid) {
      Window_t windowStars;
      windowCreateFrom(&windowStars, drawWindow, 0, -difficultyCostHeight, drawWindow->Width, difficultySelectHeight, TEXT_ALIGN_BOTTOMLEFT);
      windowFill(&windowStars, 0x30404040);
      gfxSetupGifPaging(0);
      windowMove(&windowStars, windowStars.Width/2 - (RAIDS_DIFFICULTY_COUNT/2.0)*(starHeight+starSpacing), 0);
      for (i = 0; i < RAIDS_DIFFICULTY_COUNT; ++i) {
        windowDrawSprite(&windowStars, TEXT_ALIGN_TOPLEFT, 0, 5, starHeight, starHeight, LEVELSELECT_STAR_SPRITE_ID, 32, 32, i <= levelselectDrawState.SelectedDifficulty ? starActiveColor : starInactiveColor, TEXT_ALIGN_TOPLEFT);
        windowMove(&windowStars, starHeight + starSpacing, 0);
      }
      gfxDoGifPaging();
    }

    // interact text
    Window_t windowCostText;
    windowCreateFrom(&windowCostText, drawWindow, 0, 0, drawWindow->Width, difficultyCostHeight, TEXT_ALIGN_BOTTOMLEFT);
    
    if (cost < 0) snprintf(strBuf, sizeof(strBuf), "\x0EInaccessible");
    else if (!hasMinLevel) snprintf(strBuf, sizeof(strBuf), "\x0EYou must be at least level %d", levelselectDrawState.SelectedMapExtraData.MinPlayerLevel);
    else if (cost > 0) snprintf(strBuf, sizeof(strBuf), "%c VISIT %c%'d", canAfford ? selectChar : '\x0E', canAfford ? '\x0A' : '\x0E', cost);
    else snprintf(strBuf, sizeof(strBuf), "%c VISIT\x0A FREE", selectChar);
    windowDrawText(&windowCostText, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLECENTER);
  }
}

//--------------------------------------------------------------------------
void levelselectDrawFooter(Window_t* drawWindow)
{
  const u32 bgSolidColor = 0x80000000;
  u32 textColor = 0x80FFFFFF;
  char strBuf[128];

  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // draw bg
  windowFill(drawWindow, bgSolidColor);

  // draw footer text
  strBuf[0] = 0;
  //if (!state->OnHubWorld) strcat(strBuf, "\x1E RETURN TO HUB    ");
  if (!levelselectDrawState.MapStats.Invalid && levelselectDrawState.MapStats.ChallengesCount > 0) strcat(strBuf, "\x11 CHALLENGES    ");
  strcat(strBuf, "\x13 REFRESH    ");
  strcat(strBuf, "\x1A \x1B STARS    ");
  //strcat(strBuf, "\x12 CLOSE");
  windowDrawText(drawWindow, TEXT_ALIGN_BOTTOMLEFT, 2, -2, 0.8, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMLEFT);
}

//--------------------------------------------------------------------------
void levelselectDraw(void)
{
  u32 bgColor = 0x60000000;
  u32 selectColor = 0x60008080;
  u32 borderColor = 0x80000020;
  u32 textColor = 0x80FFFFFF;
  u32 starActiveColor = 0x80008080;
  u32 starInactiveColor = 0x80101010;
  const float titleHeight = 30;
  const float footerHeight = 20;
  const float mapListDetailsHeight = 250;
  const float mapInfoWidth = 200;
  int i;
  int totalMapDefCount = 0;
  char strBuf[128];
  Window_t drawWindow;
  levelselectDrawState.NumPlanets = 0;

  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // setup draw state
  windowCreate(&drawWindow, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, 450, 300, TEXT_ALIGN_MIDDLECENTER);

  // draw frame
  windowFill(&drawWindow, bgColor);

  // draw title text
  Window_t windowTitle;
  windowCreateFrom(&windowTitle, &drawWindow, 0, 0, drawWindow.Width, titleHeight, TEXT_ALIGN_TOPCENTER);
  windowFill(&windowTitle, borderColor);
  windowDrawText(&windowTitle, TEXT_ALIGN_MIDDLECENTER, 0, 0, 1.1, textColor, "Planet Select", -1, TEXT_ALIGN_MIDDLECENTER);
  //windowMove(&drawWindow, 0, titleHeight);

  // draw map list
  Window_t drawWindowMapList;
  windowCreateFrom(&drawWindowMapList, &drawWindow, 0, -footerHeight, 450 - mapInfoWidth, mapListDetailsHeight, TEXT_ALIGN_BOTTOMLEFT);
  int numMaps = levelselectDrawState.NumPlanets = levelselectDrawMapList(&drawWindowMapList, levelselectDrawState.SelectedIdx, &levelselectDrawState.SelectedMapDef);

  // check for selected map changed
  if (levelselectDrawState.SelectedMapDef) {
    CustomMapDef_t* def = levelselectDrawState.SelectedMapDef;
    if (strncmp(def->Filename, levelselectDrawState.SelectedMapFilename, sizeof(levelselectDrawState.SelectedMapFilename)) != 0) {
      strncpy(levelselectDrawState.SelectedMapFilename, def->Filename, sizeof(levelselectDrawState.SelectedMapFilename));
      PATCH_INTEROP->ReadCustomMapExtraData(def->Filename, levelselectDrawState.SelectedMapExtraDataBuf, sizeof(levelselectDrawState.SelectedMapExtraDataBuf), CUSTOM_MODE_RAIDS);
      levelselectDrawState.MapStats.ChallengesCount = levelselectDrawState.SelectedMapExtraData.ChallengesCount;
      levelselectDrawState.MapStats.CollectiblesCount = levelselectDrawState.SelectedMapExtraData.CollectiblesCount;
      int missionType = levelselectDrawState.SelectedMapExtraData.MissionType;
      bankRequestMapStats(def->Filename, &levelselectDrawState.MapStats, missionType);
    }
  }

  // draw item details
  Window_t drawWindowMapDetails;
  windowCreateFrom(&drawWindowMapDetails, &drawWindow, 0, -footerHeight, mapInfoWidth, mapListDetailsHeight, TEXT_ALIGN_BOTTOMRIGHT);
  levelselectDrawMapInfo(&drawWindowMapDetails);

  // draw footer
  Window_t windowFooter;
  windowCreateFrom(&windowFooter, &drawWindow, 0, 0, drawWindow.Width, footerHeight, TEXT_ALIGN_BOTTOMCENTER);
  levelselectDrawFooter(&windowFooter);

  // draw border
  windowBorder(&drawWindow, borderColor, 1, 1, 1, 1);
  
  // challenges dialog
  if (levelselectDrawState.ShowChallengesDialog) {

    // fade
    windowFill(&drawWindow, 0x40000000);

    Window_t windowChallengesDialog;
    windowCreateFrom(&windowChallengesDialog, &drawWindow, 0, 0, 400, 200, TEXT_ALIGN_MIDDLECENTER);
    levelselectDrawChallengesDialog(&windowChallengesDialog, levelselectDrawState.ChallengesDialogSelectedIdx);
  }
}

//--------------------------------------------------------------------------
void levelselectHandleInput(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;
  if (levelselectDrawState.InputCooldownTicks > 0) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

  if (levelselectDrawState.ShowChallengesDialog) {
      
    // handle close input
    if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
      levelselectDrawState.ShowChallengesDialog = 0;
      levelselectDrawState.InputCooldownTicks = LEVELSELECT_INPUT_COOLDOWN;
      return;
    }

    // handle navigation
    if (levelselectDrawState.MapStats.ChallengesCount > 0 && padGetButtonDown(0, PAD_DOWN) > 0) {                           // NAV DOWN
      levelselectDrawState.ChallengesDialogSelectedIdx = (levelselectDrawState.ChallengesDialogSelectedIdx+1) % levelselectDrawState.MapStats.ChallengesCount;
    } else if (levelselectDrawState.MapStats.ChallengesCount > 0 && padGetButtonDown(0, PAD_UP) > 0) {                         // NAV UP
      levelselectDrawState.ChallengesDialogSelectedIdx = (levelselectDrawState.ChallengesDialogSelectedIdx-1) % levelselectDrawState.MapStats.ChallengesCount;
      if (levelselectDrawState.ChallengesDialogSelectedIdx < 0) levelselectDrawState.ChallengesDialogSelectedIdx = levelselectDrawState.MapStats.ChallengesCount - 1;
    }

    return;
  }

  // handle close input
  if (padGetButtonDown(0, PAD_TRIANGLE) > 0) {
    levelselectClose();
    return;
  }

  // handle input
  if (padGetButtonDown(0, PAD_LEFT) > 0) {                                                      // NAV LEFT
    levelselectDrawState.SelectedDifficulty = (levelselectDrawState.SelectedDifficulty-1) % RAIDS_DIFFICULTY_COUNT;
    if (levelselectDrawState.SelectedDifficulty < 0) levelselectDrawState.SelectedDifficulty = RAIDS_DIFFICULTY_COUNT - 1;
  } else if (padGetButtonDown(0, PAD_RIGHT) > 0) {                                              // NAV RIGHT
    levelselectDrawState.SelectedDifficulty = (levelselectDrawState.SelectedDifficulty+1) % RAIDS_DIFFICULTY_COUNT;
  } else if (levelselectDrawState.NumPlanets > 0 && padGetButtonDown(0, PAD_DOWN) > 0) {                       // NAV DOWN
    levelselectDrawState.SelectedIdx = (levelselectDrawState.SelectedIdx+1) % levelselectDrawState.NumPlanets;
  } else if (levelselectDrawState.NumPlanets > 0 && padGetButtonDown(0, PAD_UP) > 0) {                         // NAV UP
    levelselectDrawState.SelectedIdx = (levelselectDrawState.SelectedIdx-1) % levelselectDrawState.NumPlanets;
    if (levelselectDrawState.SelectedIdx < 0) levelselectDrawState.SelectedIdx = levelselectDrawState.NumPlanets - 1;
  } else if (PATCH_INTEROP && PATCH_INTEROP->RefreshCustomMapDefs && padGetButtonDown(0, PAD_SQUARE) > 0) {   // REFRESH
    PATCH_INTEROP->RefreshCustomMapDefs();
    levelselectDrawState.InputCooldownTicks = LEVELSELECT_INPUT_COOLDOWN;
  } else if (!levelselectDrawState.MapStats.Invalid && levelselectDrawState.MapStats.ChallengesCount > 0 && padGetButtonDown(0, PAD_CIRCLE) > 0) {   // CHALLENGES
    levelselectDrawState.ChallengesDialogSelectedIdx = 0;
    levelselectDrawState.ShowChallengesDialog = 1;
  } else if (gameAmIHost() && padGetButtonDown(0, PAD_CROSS) > 0) {                             // TRAVEL
    levelselectGo(&levelselectDrawState);
  }

  // clamp selected index
  if (levelselectDrawState.SelectedIdx >= levelselectDrawState.NumPlanets) levelselectDrawState.SelectedIdx = levelselectDrawState.NumPlanets - 1;
  if (levelselectDrawState.SelectedIdx < 0) levelselectDrawState.SelectedIdx = 0;
}

//--------------------------------------------------------------------------
void levelselectFrameTick(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // draw
  if (state->MenuOpen == RAIDS_CUSTOM_MENU_LEVELSELECT) {
    levelselectDraw();
  }
}

//--------------------------------------------------------------------------
void levelselectStart(void)
{
  static int missionFailed = 0; // resets when mission is reloaded
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  decTimerU32(&levelselectDrawState.InputCooldownTicks);

  if (gameHasEnded() || !isInGame() || gameIsAnyStartMenuOpen()) {
    levelselectClose();
    return;
  }

  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_LEVELSELECT) {
    levelselectHandleInput();
    return;
  }

  int canOpen = state->MenuOpen == RAIDS_CUSTOM_MENU_NONE && PATCH_POINTERS_PATCHMENU == 0 && !gameIsAnyStartMenuOpen() && padGetButtonDown(0, PAD_UP) > 0;
  if (canOpen) {
    levelselectOpen();
  } else if (!missionFailed && missionIsFailed() && !hasPendingWorldHop() && gameAmIHost()) {
    levelselectOpen();
    missionFailed = 1;
  }
}

//--------------------------------------------------------------------------
void levelselectInit(void)
{
  levelselectDrawState.SelectedIdx = 0;
  levelselectDrawState.SelectedDifficulty = MapConfig.State ? MapConfig.State->DifficultyStars : 0;
}
