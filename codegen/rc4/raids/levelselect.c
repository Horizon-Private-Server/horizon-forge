#include <libdl/string.h>
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
#include "utils.h"
#include "window.h"
#include "menu.h"
#include "common.h"

#ifndef RAIDS_VERSION
#define RAIDS_VERSION (0)
#endif

extern struct RaidsState State;

u8* levelselectCompatibleMaps = NULL;
int levelselectMapCount = 0;

LevelselectDrawState_t levelselectDrawState = {
  .SelectedIdx = 0,
  .SelectedDifficulty = 0,
  .NumPlanets = 0,
  .InputCooldownTicks = 10
};

char* levelselectMissionTypes[] = {
  [RAIDS_MISSION_HUB] "Hub",
  [RAIDS_MISSION_OPEN_WORLD] "Planet",
  [RAIDS_MISSION_RAID] "Raid",
};

//--------------------------------------------------------------------------
void levelselectRefreshMaps(void)
{
  if (levelselectCompatibleMaps) free(levelselectCompatibleMaps);
  levelselectCompatibleMaps = NULL;
  levelselectMapCount = 0;
  levelselectDrawState.SelectedMapDef = NULL;
  levelselectDrawState.SelectedMapFilename[0] = 0;
  levelselectDrawState.NumPlanets = 0;
  levelselectDrawState.MapStats.Invalid = 1;

  if (!PATCH_INTEROP || !PATCH_INTEROP->GetCustomMapDefCount || !PATCH_INTEROP->GetCustomMapDef || !PATCH_INTEROP->ReadCustomMapExtraData) return;

  int count = PATCH_INTEROP->GetCustomMapDefCount();
  if (count <= 0) return;
  levelselectCompatibleMaps = malloc(count);
  if (!levelselectCompatibleMaps) return;
  levelselectMapCount = count;

  int i;
  for (i = 0; i < count; ++i) {
    CustomMapDef_t* def = PATCH_INTEROP->GetCustomMapDef(i);
    int version = 0;
    levelselectCompatibleMaps[i] = def && def->ForcedCustomModeId == CUSTOM_MODE_RAIDS
      && (def->CustomModeExtraDataMask & (1 << CUSTOM_MODE_RAIDS))
      && PATCH_INTEROP->ReadCustomMapExtraData(def->Filename, &version, sizeof(version), CUSTOM_MODE_RAIDS) == sizeof(version)
      && version == RAIDS_VERSION;
    levelselectDrawState.NumPlanets += levelselectCompatibleMaps[i];
  }
  if (levelselectDrawState.SelectedIdx >= levelselectDrawState.NumPlanets) levelselectDrawState.SelectedIdx = 0;
}

//--------------------------------------------------------------------------
void levelselectOpen(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;
  if (shouldCloseMenu()) return;
  if (!PATCH_INTEROP) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_NONE) return;

  state->MenuOpen = RAIDS_CUSTOM_MENU_LEVELSELECT;
  levelselectRefreshMaps();
  padDisableInput();
}

//--------------------------------------------------------------------------
void levelselectClose(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;
  if (state->MenuOpen != RAIDS_CUSTOM_MENU_LEVELSELECT) return;

  state->MenuOpen = RAIDS_CUSTOM_MENU_NONE;
  if (levelselectCompatibleMaps) free(levelselectCompatibleMaps);
  levelselectCompatibleMaps = NULL;
  levelselectMapCount = 0;
  padEnableInput();
}

//--------------------------------------------------------------------------
void levelselectGo(LevelselectDrawState_t* drawState)
{
  if (hasPendingWorldHop() || !gameAmIHost() || !drawState || !drawState->SelectedMapDef
      || !drawState->SelectedMapFilename[0] || drawState->SelectedMapExtraData.RaidsVersion != RAIDS_VERSION) {
    playEquipRejectSound(playerGetFromSlot(0));
    return;
  }

  // force to 1 star difficulty for non-raids
  int missionType = drawState->SelectedMapExtraData.MissionType;
  int isBossRaid = missionType == RAIDS_MISSION_RAID;
  if (!isBossRaid) {
    drawState->SelectedDifficulty = 0;
  }

#if DEBUG
  int cost = 0;
#else
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  int cost = drawState->SelectedMapExtraData.Cost[drawState->SelectedDifficulty];
  int level = bankGetLevel();
  int hasMinLevel = (level + 1) >= levelselectDrawState.SelectedMapExtraData.MinPlayerLevel;
  if (cost < 0 || cost > localBank->Account.Bolts || !hasMinLevel) {
    playEquipRejectSound(playerGetFromSlot(0));
    return;
  }
#endif

  // hop
  int isHub = strncmp(drawState->SelectedMapFilename, RAIDS_HUB_MAPFILENAME, sizeof(drawState->SelectedMapFilename)) == 0;
  if (MapConfig.Functions.ModeBeginWorldHopFunc) {
    MapConfig.Functions.ModeBeginWorldHopFunc(drawState->SelectedMapFilename, isHub ? 0 : drawState->SelectedDifficulty, isHub ? 0 : cost, 5 * TIME_SECOND);
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
  const u32 textColor = RAIDS_MENU_TEXT_COLOR;
  const u32 completedColor = 0x8000C000;
  const u32 selectedColor = 0x40008080;
  const int lineHeight = 16;
  const float padding = RAIDS_MENU_POPUP_PADDING;
  const float bodyY = RAIDS_MENU_POPUP_TITLE_HEIGHT + padding;
  const float gutter = 12;
  float bodyHeight = drawWindow->Height - bodyY - RAIDS_MENU_POPUP_FOOTER_HEIGHT - padding;
  float columnWidth = (drawWindow->Width - 2*padding - gutter) * 0.5;
  int visibleRows = (int)(bodyHeight / lineHeight);
  char* desc = NULL;
  char strBuf[64];
  Window_t listWindow;
  Window_t windowDesc;

  if (!levelselectDrawState.SelectedMapDef) return;

  snprintf(strBuf, sizeof(strBuf), "%s Challenges", levelselectDrawState.SelectedMapDef->Name);
  menuDrawPopupFrame(drawWindow, strBuf, textColor, "\x1D \x1C SELECT        \x12 CLOSE");
  windowCreateFrom(&listWindow, drawWindow, padding, bodyY, columnWidth, bodyHeight, TEXT_ALIGN_TOPLEFT);
  windowCreateFrom(&windowDesc, drawWindow, padding + columnWidth + gutter, bodyY, columnWidth, bodyHeight, TEXT_ALIGN_TOPLEFT);

  if (selectedIdx < drawItemsFrom) drawItemsFrom = selectedIdx;
  if (selectedIdx >= drawItemsFrom + visibleRows) drawItemsFrom = selectedIdx - visibleRows + 1;
  int itemIdx = drawItemsFrom;
  int lastItemIdx = drawItemsFrom + visibleRows;
  gfxSetupGifPaging(0);
  while (itemIdx < levelselectDrawState.MapStats.ChallengesCount && itemIdx < lastItemIdx) {
    int cNameOff = (int)levelselectDrawState.SelectedMapExtraData.Challenges[(itemIdx*2) + 0];
    int cDescOff = (int)levelselectDrawState.SelectedMapExtraData.Challenges[(itemIdx*2) + 1];
    if (!cNameOff || !cDescOff) break;

    char* cName = &levelselectDrawState.SelectedMapExtraDataBuf[cNameOff];
    char* cDesc = &levelselectDrawState.SelectedMapExtraDataBuf[cDescOff];
    int isCompleted = (levelselectDrawState.MapStats.ChallengesMask & (1 << itemIdx)) != 0;

    // create line window
    Window_t windowLine;
    windowCreateFrom(&windowLine, &listWindow, 0, 0, columnWidth, lineHeight, TEXT_ALIGN_TOPLEFT);

    // selection highlight
    if (itemIdx == selectedIdx) {
      windowFill(&windowLine, selectedColor);
      desc = cDesc;
    }

    if (isCompleted)
      windowDrawSprite(&windowLine, TEXT_ALIGN_MIDDLELEFT, 5, 0, 12, 12, 35, completedColor, TEXT_ALIGN_MIDDLELEFT);

    // draw name
    menuDrawFittedText(&windowLine, 5 + (isCompleted ? 16 : 0), lineHeight * 0.5, columnWidth - 10 - (isCompleted ? 16 : 0), 0.7, textColor, cName, TEXT_ALIGN_MIDDLELEFT);
    windowMove(&listWindow, 0, lineHeight);
    ++itemIdx;
  }
  gfxDoGifPaging();

  // Description stays inside the shared popup's body, above its footer.
  windowFill(&windowDesc, RAIDS_MENU_PANEL_COLOR);
  if (desc)
    windowDrawTextWindow(&windowDesc, TEXT_ALIGN_TOPLEFT, 5, 5, 0.8, textColor, desc, -1, TEXT_ALIGN_TOPLEFT);
}

//--------------------------------------------------------------------------
char* levelselectGetMissionTypeName(int missionType)
{
  if (missionType < RAIDS_MISSION_HUB || missionType > RAIDS_MISSION_RAID) return "Unknown";
  return levelselectMissionTypes[missionType];
}

//--------------------------------------------------------------------------
CustomMapDef_t* levelselectGetCompatibleMapDef(int index)
{
  if (!levelselectCompatibleMaps || !levelselectCompatibleMaps[index]) return NULL;
  if (!PATCH_INTEROP || !PATCH_INTEROP->GetCustomMapDef) return NULL;
  CustomMapDef_t* def = PATCH_INTEROP->GetCustomMapDef(index);
  if (!def || def->ForcedCustomModeId != CUSTOM_MODE_RAIDS || !(def->CustomModeExtraDataMask & (1 << CUSTOM_MODE_RAIDS))) return NULL;
  return def;
}

//--------------------------------------------------------------------------
int levelselectDrawMapList(Window_t* drawWindow, int selectedIdx, CustomMapDef_t** selectedMapDef)
{
  static int init = 0;
  static int drawItemsFrom = 0;
  const float padding = RAIDS_MENU_PADDING;
  const int lineHeight = 26;
  const int listY = 32;
  int visibleRows = (int)((drawWindow->Height - listY - padding) / lineHeight);
  int numMaps = 0;
  int i;
  char strBuf[128];

  windowFill(drawWindow, RAIDS_MENU_PANEL_COLOR);
  *selectedMapDef = NULL;
  if (!init && MapConfig.State)
    levelselectDrawState.SelectedDifficulty = MapConfig.State->DifficultyStars;

  // Count all destinations and resolve the selection independently of the viewport.
  for (i = 0; i < levelselectMapCount; ++i) {
    CustomMapDef_t* def = levelselectGetCompatibleMapDef(i);
    if (!def) continue;
    if (!init && MapConfig.State && def == MapConfig.State->CurrentMapDef)
      selectedIdx = levelselectDrawState.SelectedIdx = numMaps;
    ++numMaps;
  }
  init = 1;

  if (selectedIdx < drawItemsFrom) drawItemsFrom = selectedIdx;
  if (selectedIdx >= drawItemsFrom + visibleRows) drawItemsFrom = selectedIdx - visibleRows + 1;
  if (drawItemsFrom < 0) drawItemsFrom = 0;
  int lastVisible = drawItemsFrom + visibleRows;
  if (lastVisible > numMaps) lastVisible = numMaps;

  menuDrawFittedText(drawWindow, padding, 8, 80, 0.55, RAIDS_MENU_SECONDARY_COLOR, "Destinations", TEXT_ALIGN_TOPLEFT);
  snprintf(strBuf, sizeof(strBuf), "%d-%d / %d", numMaps ? drawItemsFrom + 1 : 0, lastVisible, numMaps);
  menuDrawFittedText(drawWindow, drawWindow->Width - padding, 8, drawWindow->Width - 2*padding - 88, 0.5, RAIDS_MENU_SECONDARY_COLOR, strBuf, TEXT_ALIGN_TOPRIGHT);
  if (!numMaps) {
    menuDrawFittedText(drawWindow, padding, listY, drawWindow->Width - 2*padding, 0.7, RAIDS_MENU_SECONDARY_COLOR, "No compatible maps", TEXT_ALIGN_TOPLEFT);
    return 0;
  }

  int mapIdx = 0;
  for (i = 0; i < levelselectMapCount; ++i) {
    CustomMapDef_t* def = levelselectGetCompatibleMapDef(i);
    if (!def) continue;
    if (mapIdx == selectedIdx) *selectedMapDef = def;

    if (mapIdx >= drawItemsFrom && mapIdx < lastVisible) {
      Window_t rowWindow;
      windowCreateFrom(&rowWindow, drawWindow, 4, listY + (mapIdx - drawItemsFrom)*lineHeight, drawWindow->Width - 8, lineHeight, TEXT_ALIGN_TOPLEFT);
      if (mapIdx == selectedIdx) windowFill(&rowWindow, 0x40008080);
      int isCurrent = MapConfig.State && def == MapConfig.State->CurrentMapDef;
      snprintf(strBuf, sizeof(strBuf), "%s%s", isCurrent ? "* " : "", def->Name);
      menuDrawFittedText(&rowWindow, 4, 2, rowWindow.Width - 8, 0.7, RAIDS_MENU_TEXT_COLOR, strBuf, TEXT_ALIGN_TOPLEFT);
      menuDrawFittedText(&rowWindow, 4, 15, rowWindow.Width - 8, 0.55, RAIDS_MENU_SECONDARY_COLOR, levelselectGetMissionTypeName(def->Subsort / 10000), TEXT_ALIGN_TOPLEFT);
    }
    ++mapIdx;
  }
  return numMaps;
}

//--------------------------------------------------------------------------
void levelselectDrawMapInfo(Window_t* drawWindow)
{
  const float padding = RAIDS_MENU_PADDING;
  const float contentWidth = drawWindow->Width - 2*padding;
  const float travelHeight = 40;
  const float travelY = drawWindow->Height - travelHeight;
  const float starsHeight = 32;
  const float starsY = travelY - starsHeight;
  const float bestTimeY = starsY - 20;
  const u32 textColor = RAIDS_MENU_TEXT_COLOR;
  const u32 secondaryColor = RAIDS_MENU_SECONDARY_COLOR;
  char strBuf[128];
  struct RaidsCustomMapExtraData* extra = &levelselectDrawState.SelectedMapExtraData;
  struct RaidsBankMapStats* stats = &levelselectDrawState.MapStats;
  RaidsPlayerBank_t* bank = bankGetLocalBank();
  int hasAccount = bank && bankGetHasAccount();
  int hasStats = !stats->Invalid && strncmp(stats->MapFilename, levelselectDrawState.SelectedMapFilename, sizeof(stats->MapFilename)) == 0;
  int isRaid = extra->MissionType == RAIDS_MISSION_RAID;
  int difficulty = isRaid ? levelselectDrawState.SelectedDifficulty : 0;
  float countsY = isRaid ? bestTimeY - 22 : travelY - 30;
  float progressY = countsY - 32;

  windowFill(drawWindow, RAIDS_MENU_PANEL_COLOR);
  windowBorder(drawWindow, RAIDS_MENU_BORDER_COLOR, 1, 0, 0, 0);
  if (!levelselectDrawState.SelectedMapDef || !levelselectDrawState.SelectedMapFilename[0]) {
    windowDrawText(drawWindow, TEXT_ALIGN_MIDDLECENTER, 0, 0, 0.7, secondaryColor, "No destination selected", -1, TEXT_ALIGN_MIDDLECENTER);
    return;
  }

  menuDrawFittedText(drawWindow, padding, 8, contentWidth, 0.95, textColor, levelselectDrawState.SelectedMapDef->Name, TEXT_ALIGN_TOPLEFT);
  snprintf(strBuf, sizeof(strBuf), "%s | Author: %.32s", levelselectGetMissionTypeName(extra->MissionType), extra->Author);
  menuDrawFittedText(drawWindow, padding, 28, contentWidth, 0.6, secondaryColor, strBuf, TEXT_ALIGN_TOPLEFT);

  Window_t descriptionWindow;
  windowCreateFrom(&descriptionWindow, drawWindow, padding, 48, contentWidth, progressY - 56, TEXT_ALIGN_TOPLEFT);
  windowDrawTextWindow(&descriptionWindow, TEXT_ALIGN_TOPLEFT, 0, 0, 0.7, textColor, extra->Description, -1, TEXT_ALIGN_TOPLEFT);

  windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, padding, progressY, 0.55, secondaryColor, "Completion", -1, TEXT_ALIGN_TOPLEFT);
  float progress = hasStats ? clamp(stats->PercentageComplete, 0, 1) : 0;
  if (hasStats) snprintf(strBuf, sizeof(strBuf), "%.0f%%", progress * 100);
  else snprintf(strBuf, sizeof(strBuf), "Loading...");
  menuDrawFittedText(drawWindow, drawWindow->Width - padding, progressY, contentWidth * 0.5, 0.7, textColor, strBuf, TEXT_ALIGN_TOPRIGHT);
  menuDrawProgressBar(drawWindow, padding, progressY + 14, contentWidth, progress);

  gfxSetupGifPaging(0);
  windowDrawSprite(drawWindow, TEXT_ALIGN_TOPLEFT, padding, countsY, 12, 12, 4, textColor, TEXT_ALIGN_TOPLEFT);
  gfxDoGifPaging();
  if (hasStats) snprintf(strBuf, sizeof(strBuf), "%d / %d", countBits(stats->CollectiblesMask), stats->CollectiblesCount);
  else snprintf(strBuf, sizeof(strBuf), "-- / %d", extra->CollectiblesCount);
  menuDrawFittedText(drawWindow, padding + 18, countsY, contentWidth * 0.5 - 26, 0.7, textColor, strBuf, TEXT_ALIGN_TOPLEFT);
  if (hasStats) snprintf(strBuf, sizeof(strBuf), "%d / %d", countBits(stats->ChallengesMask), stats->ChallengesCount);
  else snprintf(strBuf, sizeof(strBuf), "-- / %d", extra->ChallengesCount);
  float challengesWidth = menuDrawFittedText(drawWindow, drawWindow->Width - padding, countsY, contentWidth * 0.5 - 18, 0.7, textColor, strBuf, TEXT_ALIGN_TOPRIGHT);
  gfxSetupGifPaging(0);
  windowDrawSprite(drawWindow, TEXT_ALIGN_TOPLEFT, drawWindow->Width - padding - challengesWidth - 18, countsY, 12, 12, 31, textColor, TEXT_ALIGN_TOPLEFT);
  gfxDoGifPaging();

  int cost = extra->Cost[difficulty];
  if (isRaid) {
    windowDrawText(drawWindow, TEXT_ALIGN_TOPLEFT, padding, bestTimeY, 0.7, secondaryColor, "Best time", -1, TEXT_ALIGN_TOPLEFT);
    int bestTimeMs = stats->BestTimeMsPerDifficulty[difficulty];
    if (!hasStats || cost < 0) snprintf(strBuf, sizeof(strBuf), "--");
    else if (bestTimeMs > 0) snprintf(strBuf, sizeof(strBuf), "%d:%02d.%03d", bestTimeMs / 60000, (bestTimeMs / 1000) % 60, bestTimeMs % 1000);
    else snprintf(strBuf, sizeof(strBuf), "Incomplete");
    menuDrawFittedText(drawWindow, drawWindow->Width - padding, bestTimeY, contentWidth - 80, 0.7, textColor, strBuf, TEXT_ALIGN_TOPRIGHT);

    Window_t starsWindow;
    windowCreateFrom(&starsWindow, drawWindow, padding, starsY, contentWidth, starsHeight, TEXT_ALIGN_TOPLEFT);
    windowFill(&starsWindow, RAIDS_MENU_TRACK_COLOR);
    windowBorder(&starsWindow, RAIDS_MENU_BORDER_COLOR, 0, 0, 0, 1);
    const float starSize = 24;
    const float starSpacing = 5;
    float starsWidth = RAIDS_DIFFICULTY_COUNT*starSize + (RAIDS_DIFFICULTY_COUNT - 1)*starSpacing;
    float starX = (contentWidth - starsWidth) * 0.5;
    int i;
    gfxSetupGifPaging(0);
    for (i = 0; i < RAIDS_DIFFICULTY_COUNT; ++i) {
      windowDrawSprite(&starsWindow, TEXT_ALIGN_TOPLEFT, starX + i*(starSize + starSpacing), (starsHeight - starSize) * 0.5, starSize, starSize, LEVELSELECT_STAR_SPRITE_ID, i <= difficulty ? 0x8000C0C0 : 0x80808080, TEXT_ALIGN_TOPLEFT);
    }
    gfxDoGifPaging();
  }

  Window_t travelWindow;
  windowCreateFrom(&travelWindow, drawWindow, padding, travelY, contentWidth, travelHeight, TEXT_ALIGN_TOPLEFT);
  windowFill(&travelWindow, RAIDS_MENU_TRACK_COLOR);
  char* status = "\x10 VISIT";
  u32 statusColor = textColor;
  if (!hasAccount) {
    status = "Loading account...";
    statusColor = secondaryColor;
  } else if (cost < 0) {
    status = "Inaccessible";
    statusColor = RAIDS_MENU_WARNING_COLOR;
  } else if (bankGetLevel() + 1 < extra->MinPlayerLevel) {
    snprintf(strBuf, sizeof(strBuf), "Requires Hero Level %d", extra->MinPlayerLevel);
    status = strBuf;
    statusColor = RAIDS_MENU_WARNING_COLOR;
  } else if (cost > bank->Account.Bolts) {
    status = "Not enough bolts";
    statusColor = RAIDS_MENU_WARNING_COLOR;
  } else if (!gameAmIHost()) {
    status = "Only the host can travel";
    statusColor = secondaryColor;
  }
  menuDrawFittedText(&travelWindow, contentWidth * 0.5, 20, contentWidth - 8, 0.9, statusColor, status, TEXT_ALIGN_TOPCENTER);

  char costBuf[32];
  if (cost < 0) snprintf(costBuf, sizeof(costBuf), "Cost: --");
  else if (cost == 0) snprintf(costBuf, sizeof(costBuf), "Cost: FREE");
  else snprintf(costBuf, sizeof(costBuf), "Cost: %'d", cost);
  if (hasAccount) snprintf(strBuf, sizeof(strBuf), "%s    Bolts: %'u", costBuf, bank->Account.Bolts);
  else snprintf(strBuf, sizeof(strBuf), "%s    Bolts: --", costBuf);
  menuDrawFittedText(&travelWindow, contentWidth * 0.5, 4, contentWidth - 8, 0.6, secondaryColor, strBuf, TEXT_ALIGN_TOPCENTER);
}

//--------------------------------------------------------------------------
void levelselectDrawFooter(Window_t* drawWindow)
{
  char strBuf[128];
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  snprintf(strBuf, sizeof(strBuf), "\x1D \x1C MAP    ");
  if (!levelselectDrawState.MapStats.Invalid && levelselectDrawState.MapStats.ChallengesCount > 0)
    strcat(strBuf, "\x11 CHALLENGES    ");
  if (levelselectDrawState.SelectedMapDef && levelselectDrawState.SelectedMapExtraData.MissionType == RAIDS_MISSION_RAID)
    strcat(strBuf, "\x1A \x1B STARS    ");
  strcat(strBuf, "\x13 REFRESH    \x12 CLOSE");
  menuDrawFooter(drawWindow, strBuf, 0.7, TEXT_ALIGN_MIDDLELEFT, RAIDS_MENU_PADDING);
}

//--------------------------------------------------------------------------
void levelselectDraw(void)
{
  const u32 borderColor = RAIDS_MENU_BORDER_COLOR;
  const float windowWidth = 480;
  const float windowHeight = 320;
  const float titleHeight = RAIDS_MENU_TITLE_HEIGHT;
  const float footerHeight = RAIDS_MENU_FOOTER_HEIGHT;
  const float mapListWidth = 184;
  const float bodyHeight = windowHeight - titleHeight - footerHeight;
  Window_t drawWindow;
  levelselectDrawState.NumPlanets = 0;

  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // setup draw state
  windowCreate(&drawWindow, SCREEN_WIDTH * 0.5, SCREEN_HEIGHT * 0.5, 0, 0, windowWidth, windowHeight, TEXT_ALIGN_MIDDLECENTER);

  // draw frame and title
  menuDrawFrame(&drawWindow, "Planet Select", titleHeight);

  // draw map list
  Window_t drawWindowMapList;
  windowCreateFrom(&drawWindowMapList, &drawWindow, 0, titleHeight, mapListWidth, bodyHeight, TEXT_ALIGN_TOPLEFT);
  levelselectDrawState.NumPlanets = levelselectDrawMapList(&drawWindowMapList, levelselectDrawState.SelectedIdx, &levelselectDrawState.SelectedMapDef);

  // check for selected map changed
  if (levelselectDrawState.SelectedMapDef) {
    CustomMapDef_t* def = levelselectDrawState.SelectedMapDef;
    if (strncmp(def->Filename, levelselectDrawState.SelectedMapFilename, sizeof(levelselectDrawState.SelectedMapFilename)) != 0) {
      safe_strcpy(levelselectDrawState.SelectedMapFilename, def->Filename, sizeof(levelselectDrawState.SelectedMapFilename));
      PATCH_INTEROP->ReadCustomMapExtraData(def->Filename, levelselectDrawState.SelectedMapExtraDataBuf, sizeof(levelselectDrawState.SelectedMapExtraDataBuf), CUSTOM_MODE_RAIDS);
      levelselectDrawState.MapStats.Invalid = 1;
      levelselectDrawState.MapStats.ChallengesCount = levelselectDrawState.SelectedMapExtraData.ChallengesCount;
      levelselectDrawState.MapStats.CollectiblesCount = levelselectDrawState.SelectedMapExtraData.CollectiblesCount;
      int missionType = levelselectDrawState.SelectedMapExtraData.MissionType;
      bankRequestMapStats(def->Filename, def->Name, &levelselectDrawState.MapStats, missionType);
    }
  }

  // draw item details
  Window_t drawWindowMapDetails;
  windowCreateFrom(&drawWindowMapDetails, &drawWindow, mapListWidth, titleHeight, windowWidth - mapListWidth, bodyHeight, TEXT_ALIGN_TOPLEFT);
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
    menuDrawBackdrop(&drawWindow);

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
    levelselectRefreshMaps();
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

  if (shouldCloseMenu()) {
    levelselectClose();
    return;
  }

  int forceOpen = !missionFailed && missionIsFailed() && gameAmIHost();
  if (MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_LEVELSELECT) {
    if (forceOpen) missionFailed = 1;
    levelselectHandleInput();
    return;
  }

  int canOpen = state->MenuOpen == RAIDS_CUSTOM_MENU_NONE && padGetButtonDown(0, PAD_UP) > 0;
  if (canOpen || forceOpen) {
    levelselectOpen();
    if (forceOpen && state->MenuOpen == RAIDS_CUSTOM_MENU_LEVELSELECT) missionFailed = 1;
  }
}

//--------------------------------------------------------------------------
void levelselectInit(void)
{
  levelselectDrawState.SelectedIdx = 0;
  levelselectDrawState.SelectedDifficulty = MapConfig.State ? MapConfig.State->DifficultyStars : 0;
}
