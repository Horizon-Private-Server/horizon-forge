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
#include "common.h"

extern struct RaidsState State;

LevelselectDrawState_t levelselectDrawState = {
  .SelectedIdx = 0,
  .SelectedDifficulty = 0,
  .NumPlanets = 0
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
  if (hasPendingWorldHop()) return;
  if (!gameAmIHost()) return;
  if (!drawState) return;
  if (!drawState->SelectedMapFilename[0]) return;

#if !DEBUG
  RaidsPlayerBank_t* localBank = bankGetLocalBank();
  int cost = drawState->SelectedMapExtraData.Cost[drawState->SelectedDifficulty];
  if (cost > localBank->Account.Bolts) return;
#else
  int cost = 0;
#endif

  // hop
  if (MapConfig.BeginWorldHopFunc)
    MapConfig.BeginWorldHopFunc(drawState->SelectedMapFilename, drawState->SelectedDifficulty, cost, 5 * TIME_SECOND);
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
void levelselectDrawFooter(LevelselectDrawState_t* drawState)
{
  u32 textColor = 0x80FFFFFF;
  char strBuf[128];

  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // draw footer text
  strBuf[0] = 0;
  if (!state->OnHubWorld) strcat(strBuf, "\x11 RETURN TO HUB    ");
  strcat(strBuf, "\x13 REFRESH    ");
  strcat(strBuf, "\x1A \x1B STARS    ");
  strcat(strBuf, "\x12 CLOSE");
  gfxHelperDrawText(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, -LEVELSELECT_DRAW_FULL_W/2 + 5, LEVELSELECT_DRAW_FULL_H/2 - 5, 0.8, textColor, strBuf, -1, TEXT_ALIGN_BOTTOMLEFT, COMMON_DZO_DRAW_NORMAL);
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
  float xOff = -LEVELSELECT_DRAW_FULL_W/2.0 + 5;
  float yOff = -LEVELSELECT_DRAW_FULL_H/2.0;
  float borderSizeH = LEVELSELECT_DRAW_FRAME_BORDER_W * SCREEN_RATIO_INV;
  float borderSizeV = LEVELSELECT_DRAW_FRAME_BORDER_W;
  int i;
  int totalMapDefCount = 0;
  char strBuf[128];
  levelselectDrawState.NumPlanets = 0;

  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  // draw frame
  gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, 0, LEVELSELECT_DRAW_FULL_W, LEVELSELECT_DRAW_FULL_H, bgColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

  // draw title text
  gfxHelperDrawText(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, yOff, 1.3, textColor, "Planet Select", -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
  yOff += LEVELSELECT_TITLE_H;

  // draw map list
  if (PATCH_INTEROP && PATCH_INTEROP->GetCustomMapDefCount && PATCH_INTEROP->GetCustomMapDef) {
    if (PATCH_INTEROP->GetCustomMapDefCount) totalMapDefCount = PATCH_INTEROP->GetCustomMapDefCount();

    i = 0;
    if (levelselectDrawState.SelectedIdx > LEVELSELECT_MAPLIST_ITEM_COUNT)
      i = levelselectDrawState.SelectedIdx - LEVELSELECT_MAPLIST_ITEM_COUNT;

    for (; i < totalMapDefCount; ++i) {
      CustomMapDef_t* def = PATCH_INTEROP->GetCustomMapDef(i);
      if (!def) continue;
      if (def->HideFromMapList == 1) continue;
      if (def->ForcedCustomModeId != CUSTOM_MODE_RAIDS) continue;
      if (!(def->CustomModeExtraDataMask & (1<<CUSTOM_MODE_RAIDS))) continue;
      if (strncmp(def->Filename, RAIDS_HUB_MAPFILENAME, 10) == 0) continue;

      // draw selection line
      if (levelselectDrawState.NumPlanets == levelselectDrawState.SelectedIdx) {
        if (strncmp(def->Filename, levelselectDrawState.SelectedMapFilename, sizeof(levelselectDrawState.SelectedMapFilename)) != 0) {
          strncpy(levelselectDrawState.SelectedMapFilename, def->Filename, sizeof(levelselectDrawState.SelectedMapFilename));
          PATCH_INTEROP->ReadCustomMapExtraData(def->Filename, &levelselectDrawState.SelectedMapExtraData, sizeof(levelselectDrawState.SelectedMapExtraData), CUSTOM_MODE_RAIDS);
        }
        gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, xOff-5, yOff, LEVELSELECT_MAPLIST_W, LEVELSELECT_MAPLIST_ITEM_H, selectColor, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
      }

      // draw map name
      snprintf(strBuf, sizeof(strBuf), "%s (V%d)", def->Name, def->Version);
      gfxHelperDrawText(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, xOff, yOff + LEVELSELECT_MAPLIST_ITEM_H/2.0, 0.8, textColor, strBuf, -1, TEXT_ALIGN_MIDDLELEFT, COMMON_DZO_DRAW_NORMAL);
      yOff += LEVELSELECT_MAPLIST_ITEM_H;
      ++levelselectDrawState.NumPlanets;
    }
  }

  // draw selected planet
  xOff = 5;
  yOff = -LEVELSELECT_DRAW_FULL_H/2.0 + LEVELSELECT_TITLE_H;
  if (levelselectDrawState.NumPlanets > 0 && levelselectDrawState.SelectedMapFilename[0]) {

    // text bgs
    gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, yOff, LEVELSELECT_MAPINFO_W, 14, 0x70000040, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, yOff+14, LEVELSELECT_MAPINFO_W, LEVELSELECT_MAPDESC_H, 0x60101010, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, yOff+14+LEVELSELECT_MAPDESC_H, LEVELSELECT_MAPINFO_W, 25, 0x30404040, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);

    // author
    snprintf(strBuf, sizeof(strBuf), "Author: %s", levelselectDrawState.SelectedMapExtraData.Author);
    gfxHelperDrawText(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, xOff, yOff, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPLEFT, COMMON_DZO_DRAW_NORMAL);
    yOff += 14;

    // description
    gfxHelperDrawTextWindow(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, yOff, LEVELSELECT_MAPINFO_W, LEVELSELECT_MAPDESC_H, 5, 5, 0.7, textColor, levelselectDrawState.SelectedMapExtraData.Description, -1, TEXT_ALIGN_TOPLEFT, FONT_WINDOW_FLAGS_NO_SCISSOR, COMMON_DZO_DRAW_NORMAL);
    yOff += LEVELSELECT_MAPDESC_H;

    // star select
    gfxSetupGifPaging(0);
    xOff = (LEVELSELECT_MAPINFO_W/2.0) - ((RAIDS_DIFFICULTY_COUNT/2)*LEVELSELECT_STAR_W);
    for (i = 0; i < RAIDS_DIFFICULTY_COUNT; ++i) {
      gfxHelperDrawSprite(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, xOff, yOff + 12, 24, 24, 32, 32, LEVELSELECT_STAR_SPRITE_ID, i <= levelselectDrawState.SelectedDifficulty ? starActiveColor : starInactiveColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
      xOff += LEVELSELECT_STAR_W;
    }
    gfxDoGifPaging();
    yOff += LEVELSELECT_STAR_H;
    xOff = 5;

    // interact text
    RaidsPlayerBank_t* localBank = bankGetLocalBank();
    char selectChar = gameAmIHost() ? '\x10' : '\x08';
    int cost = levelselectDrawState.SelectedMapExtraData.Cost[levelselectDrawState.SelectedDifficulty];
    int canAfford = cost <= localBank->Account.Bolts;
    if (cost > 0) snprintf(strBuf, sizeof(strBuf), "%c VISIT %c%'d", canAfford ? selectChar : '\x0E', canAfford ? '\x0A' : '\x0E', cost);
    else snprintf(strBuf, sizeof(strBuf), "%c VISIT\x0A FREE", selectChar);
    gfxHelperDrawText(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, LEVELSELECT_MAPINFO_W/2.0, yOff, 0.8, textColor, strBuf, -1, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
  }

  // draw footer
  levelselectDrawFooter(&levelselectDrawState);

  // draw frame borders
  gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, LEVELSELECT_DRAW_FULL_W/2, 0, borderSizeH, LEVELSELECT_DRAW_FULL_H + borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, -LEVELSELECT_DRAW_FULL_W/2, 0, borderSizeH, LEVELSELECT_DRAW_FULL_H + borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, LEVELSELECT_DRAW_FULL_H/2, LEVELSELECT_DRAW_FULL_W + borderSizeH, borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);
  gfxHelperDrawBox(LEVELSELECT_DRAW_CENTER_X, LEVELSELECT_DRAW_CENTER_Y, 0, -LEVELSELECT_DRAW_FULL_H/2, LEVELSELECT_DRAW_FULL_W + borderSizeH, borderSizeV, borderColor, TEXT_ALIGN_MIDDLECENTER, COMMON_DZO_DRAW_NORMAL);

}

//--------------------------------------------------------------------------
void levelselectHandleInput(void)
{
  struct RaidsState* state = MapConfig.State;
  if (!state) return;

  RaidsPlayerBank_t* bank = bankGetLocalBank();
  if (!bank) return;

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
  } else if (gameAmIHost() && !state->OnHubWorld && MapConfig.BeginWorldHopFunc && padGetButtonDown(0, PAD_CIRCLE) > 0) {           // TO HUB
    MapConfig.BeginWorldHopFunc(RAIDS_HUB_MAPFILENAME, 0, 0, 5 * TIME_SECOND);
    levelselectClose();
  } else if (gameAmIHost() && padGetButtonDown(0, PAD_CROSS) > 0) {                             // TRAVEL
    levelselectGo(&levelselectDrawState);
  }

#if DEBUG
  else if (gameAmIHost() && MapConfig.BeginWorldHopFunc && padGetButtonDown(0, PAD_CIRCLE) > 0) {           // TO HUB
    MapConfig.BeginWorldHopFunc(RAIDS_HUB_MAPFILENAME, 0, 0, 5 * TIME_SECOND);
    levelselectClose();
  }
#endif

  // clamp selected index
  if (levelselectDrawState.SelectedIdx >= levelselectDrawState.NumPlanets) {
    levelselectDrawState.SelectedIdx = levelselectDrawState.NumPlanets - 1;
  }
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
