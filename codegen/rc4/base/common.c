#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/graphics.h>
#include "common.h"
#include "messageid.h"

#define OFFSET_TO_DZO_X(x) (x * (1080.0 / SCREEN_WIDTH));
#define OFFSET_TO_DZO_Y(y) (y * (1080.0 / SCREEN_HEIGHT));

//------------------------------------------------------------------------------
//------------------------------------ MISC ------------------------------------
//------------------------------------------------------------------------------
void helperAlign(float* pX, float* pY, float w, float h, enum TextAlign alignment)
{
  float x = 0, y = 0;
  if (pX) x = *pX;
  if (pY) y = *pY;

  switch (alignment)
  {
    case TEXT_ALIGN_TOPLEFT: break;
    case TEXT_ALIGN_TOPCENTER: x -= w * 0.5; break;
    case TEXT_ALIGN_TOPRIGHT: x -= w; break;
    case TEXT_ALIGN_MIDDLELEFT: y -= h * 0.5; break;
    case TEXT_ALIGN_MIDDLECENTER: x -= w * 0.5; y -= h * 0.5; break;
    case TEXT_ALIGN_MIDDLERIGHT: x -= w; y -= h * 0.5; break;
    case TEXT_ALIGN_BOTTOMLEFT: y -= h; break;
    case TEXT_ALIGN_BOTTOMCENTER: x -= w * 0.5; y -= h; break;
    case TEXT_ALIGN_BOTTOMRIGHT: x -= w; y -= h; break;
  }

  if (pX) *pX = x;
  if (pY) *pY = y;
}

//------------------------------------------------------------------------------
//------------------------------------ DRAW ------------------------------------
//------------------------------------------------------------------------------
void gfxHelperDrawBox(float anchorX, float anchorY, float offsetX, float offsetY, float w, float h, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  // pass to dzo
  if (dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS && isInGame()) {
    CustomDzoCommandDrawBox_t boxCmd;
    boxCmd.X = OFFSET_TO_DZO_X(offsetX);
    boxCmd.Y = OFFSET_TO_DZO_Y(offsetY);
    boxCmd.AnchorX = anchorX / SCREEN_WIDTH;
    boxCmd.AnchorY = anchorY / SCREEN_HEIGHT;
    boxCmd.W = w / SCREEN_HEIGHT;
    boxCmd.H = h / SCREEN_HEIGHT;
    boxCmd.Color = color;
    boxCmd.Alignment = alignment;
    boxCmd.Stretch = dzoDrawType == COMMON_DZO_DRAW_STRETCHED;
    PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_BOX, sizeof(boxCmd), &boxCmd);
  }

  // draw
  if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
    float x = anchorX + offsetX;
    float y = anchorY + offsetY;
    helperAlign(&x, &y, w, h, alignment);
    gfxPixelSpaceBox(x, y, w, h, color);
  }
}

//------------------------------------------------------------------------------
void gfxHelperDrawBox_WS(VECTOR worldPosition, float w, float h, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  if (!isInGame()) return;

  int x,y;
  if (gfxWorldSpaceToScreenSpace(worldPosition, &x, &y)) {
    // pass to dzo
    if (dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS) {
      CustomDzoCommandDrawWSBox_t boxCmd;
      vector_copy(boxCmd.Position, worldPosition);
      boxCmd.W = w / SCREEN_HEIGHT;
      boxCmd.H = h / SCREEN_HEIGHT;
      boxCmd.Color = color;
      boxCmd.Alignment = alignment;
      PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_WS_BOX, sizeof(boxCmd), &boxCmd);
    }

    // draw
    if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
      float fx = x, fy = y;
      helperAlign(&fx, &fy, w, h, alignment);
      gfxPixelSpaceBox(fx, fy, w, h, color);
    }
  }
}

//------------------------------------------------------------------------------
void gfxHelperDrawText(float anchorX, float anchorY, float offsetX, float offsetY, float scale, u32 color, char* str, int length, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  // pass to dzo
  if (dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS && isInGame()) {
    CustomDzoCommandDrawText_t textCmd;
    textCmd.X = OFFSET_TO_DZO_X(offsetX);
    textCmd.Y = OFFSET_TO_DZO_Y(offsetY);
    textCmd.Scale = scale;
    textCmd.Alignment = alignment;
    textCmd.Color = color;
    textCmd.AnchorX = anchorX / SCREEN_WIDTH;
    textCmd.AnchorY = anchorY / SCREEN_HEIGHT;
    strncpy(textCmd.Text, str, (length >= 0 && length < 64) ? length : 64);
    PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_TEXT, sizeof(textCmd), &textCmd);
  }

  // draw
  if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
    gfxScreenSpaceText(anchorX + offsetX, anchorY + offsetY, scale, scale, color, str, length, alignment);
  }
}

//------------------------------------------------------------------------------
void gfxHelperDrawText_WS(VECTOR worldPosition, float scale, u32 color, char* str, int length, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  if (!isInGame()) return;

  int x,y;
  if (gfxWorldSpaceToScreenSpace(worldPosition, &x, &y)) {
    // pass to dzo
    if (dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS) {
      CustomDzoCommandDrawWSText_t textCmd;
      vector_copy(textCmd.Position, worldPosition);
      textCmd.Scale = scale;
      textCmd.Alignment = alignment;
      textCmd.Color = color;
      strncpy(textCmd.Text, str, (length >= 0 && length < 64) ? length : 64);
      PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_WS_TEXT, sizeof(textCmd), &textCmd);
    }

    // draw
    if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
      gfxScreenSpaceText(x, y, scale, scale, color, str, length, alignment);
    }
  }
}

//------------------------------------------------------------------------------
void gfxHelperDrawTextWindow(float anchorX, float anchorY, float offsetX, float offsetY, float width, float height, float textOffsetX, float textOffsetY, float scale, u32 color, char* str, int length, enum TextAlign alignment, enum FontWindowFlags flags, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  float fx = anchorX + offsetX;
  float fy = anchorY + offsetY;
  helperAlign(&fx, &fy, width, height, alignment);

  struct FontWindow fontWindow = {
    .windowLeft = fx,
    .windowRight = fx + width,
    .windowTop = fy,
    .windowBottom = fy + height,
    .textX = fx + textOffsetX,
    .textY = fy + textOffsetY,
    .maxWidth = width,
    .maxHeight = height,
    .lineSpacing = 16 * scale,
    .flags = flags,
    .shadowOffsetX = 1,
    .shadowOffsetY = 1
  };

  // pass to dzo
  if (0 && dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS && isInGame()) {
    CustomDzoCommandDrawTextWindow_t textCmd;
    textCmd.X = OFFSET_TO_DZO_X(offsetX);
    textCmd.Y = OFFSET_TO_DZO_Y(offsetY);
    textCmd.TextX = OFFSET_TO_DZO_X(textOffsetX);
    textCmd.TextY = OFFSET_TO_DZO_Y(textOffsetY);
    textCmd.Scale = scale;
    textCmd.Alignment = alignment;
    textCmd.Flags = flags;
    textCmd.Color = color;
    textCmd.AnchorX = anchorX / SCREEN_WIDTH;
    textCmd.AnchorY = anchorY / SCREEN_HEIGHT;
    strncpy(textCmd.Text, str, (length >= 0 && length < 256) ? length : 256);
    //PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_TEXT_WINDOW, sizeof(textCmd), &textCmd);
  }

  // draw
  if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
    gfxScreenSpaceTextWindow(&fontWindow, scale, scale, color, str, length, 0x80000000);
  }
}

//------------------------------------------------------------------------------
void gfxHelperDrawSprite(float anchorX, float anchorY, float offsetX, float offsetY, float w, float h, int texWidth, int texHeight, int texId, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  // pass to dzo
  if (dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS && isInGame()) {
    CustomDzoCommandDrawSprite_t spriteCmd;
    spriteCmd.X = OFFSET_TO_DZO_X(offsetX);
    spriteCmd.Y = OFFSET_TO_DZO_Y(offsetY);
    spriteCmd.Alignment = alignment;
    spriteCmd.AnchorX = anchorX / SCREEN_WIDTH;
    spriteCmd.AnchorY = anchorY / SCREEN_HEIGHT;
    spriteCmd.W = w / SCREEN_HEIGHT;
    spriteCmd.H = h / SCREEN_HEIGHT;
    spriteCmd.SpriteId = texId;
    spriteCmd.CustomSpriteId = 0;
    spriteCmd.Stretch = dzoDrawType == COMMON_DZO_DRAW_STRETCHED;
    spriteCmd.Color = color;
    PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_SPRITE, sizeof(spriteCmd), &spriteCmd);
  }

  // draw
  if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
    float x = anchorX + offsetX;
    float y = anchorY + offsetY;
    helperAlign(&x, &y, w, h, alignment);
    gfxDrawSprite(x, y, w, h, 0, 0, texWidth, texHeight, color, gfxGetFrameTex(texId));
  }
}

//------------------------------------------------------------------------------
void gfxHelperDrawSprite_WS(VECTOR worldPosition, float w, float h, int texWidth, int texHeight, int texId, u32 color, enum TextAlign alignment, enum COMMON_DZO_DRAW_TYPE dzoDrawType)
{
  if (!isInGame()) return;

  int x,y;
  if (gfxWorldSpaceToScreenSpace(worldPosition, &x, &y)) {

    // pass to dzo
    if (dzoDrawType > 0 && PATCH_DZO_INTEROP_FUNCS) {
      CustomDzoCommandDrawWSSprite_t spriteCmd;
      vector_copy(spriteCmd.Position, worldPosition);
      spriteCmd.Alignment = alignment;
      spriteCmd.W = w / SCREEN_HEIGHT;
      spriteCmd.H = h / SCREEN_HEIGHT;
      spriteCmd.SpriteId = texId;
      spriteCmd.CustomSpriteId = 0;
      spriteCmd.Color = color;
      PATCH_DZO_INTEROP_FUNCS->SendCustomCommandToClient(CUSTOM_DZO_CMD_ID_DRAW_WS_SPRITE, sizeof(spriteCmd), &spriteCmd);
    }

    // draw
    if (dzoDrawType != COMMON_DZO_DRAW_ONLY) {
      float fx = x, fy = y;
      helperAlign(&fx, &fy, w, h, alignment);
      gfxDrawSprite(fx, fy, w, h, 0, 0, texWidth, texHeight, color, gfxGetFrameTex(texId));
    }
  }
}
