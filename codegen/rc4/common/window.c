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
#include "window.h"
#include "common.h"
#include "maputils.h"

//--------------------------------------------------------------------------
void windowReset(Window_t *out)
{
	out->Position[0] = 0;
	out->Position[1] = 0;
}

//--------------------------------------------------------------------------
void windowMove(Window_t *out, float x, float y)
{
	out->Position[0] += x;
	out->Position[1] += y;
}

//--------------------------------------------------------------------------
void windowSetScreen(Window_t *window, int localPlayerIndex)
{
	window->LocalPlayerIndex = localPlayerIndex;
}

//--------------------------------------------------------------------------
void windowGetScale(Window_t *window, float *w, float *h)
{
  int localCount = playerGetNumLocals();
  *w = localCount > 2 ? 0.5 : 1;
  *h = localCount > 1 ? 0.5 : 1;
}

//--------------------------------------------------------------------------
void windowResolveScreen(Window_t* out, Window_t *window)
{
  memcpy(out, window, sizeof(Window_t));

  float w, h;
  windowGetScale(window, &w, &h);
  
	transformToSplitscreenPixelCoordinates(window->LocalPlayerIndex, &out->AnchorPoint[0], &out->AnchorPoint[1]);
  out->Width *= w;
  out->Height *= h;

	float x = window->WindowPoint[0] + window->Position[0];
	float y = window->WindowPoint[1] + window->Position[1];
	transformToSplitscreenPixelCoordinates(window->LocalPlayerIndex, &x, &y);
	helperRealign(&x, &y, out->Width, out->Height, window->AnchorAlign, out->AnchorAlign);
  
	out->WindowPoint[0] = x;
	out->WindowPoint[1] = y;
	out->Position[0] = 0;
	out->Position[1] = 0;
}

//--------------------------------------------------------------------------
void windowResolve(Window_t *window, float *x, float *y, float offsetX, float offsetY, enum TextAlign alignment)
{
	*x = window->WindowPoint[0] + window->Position[0] + offsetX;
	*y = window->WindowPoint[1] + window->Position[1] + offsetY;
	helperRealign(x, y, window->Width, window->Height, window->AnchorAlign, alignment);
}

//--------------------------------------------------------------------------
void windowDrawSprite(Window_t *window, enum TextAlign windowAnchor, float offsetX, float offsetY, float width, float height, int spriteId, u32 color, enum TextAlign alignment)
{
	if (!windowHasArea(window))
		return;

	u64 spriteFrameTex = gfxGetFrameTex(spriteId);
	int spriteTexW = gfxGetTexWidth(spriteFrameTex);
	int spriteTexH = gfxGetTexHeight(spriteFrameTex);

	float x, y, w, h;
  Window_t drawWindow;
  windowResolveScreen(&drawWindow, window);
	windowResolve(&drawWindow, &x, &y, offsetX, offsetY, windowAnchor);
  windowGetScale(&drawWindow, &w, &h);
	gfxHelperDrawSprite(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, width*w, height*h, spriteTexW, spriteTexH, spriteId, color, alignment, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void windowDrawBox(Window_t *window, enum TextAlign windowAnchor, float offsetX, float offsetY, float width, float height, u32 color, enum TextAlign alignment)
{
	if (!windowHasArea(window))
		return;

	float x, y, w, h;
  Window_t drawWindow;
  windowResolveScreen(&drawWindow, window);
	windowResolve(&drawWindow, &x, &y, 0, 0, windowAnchor);
  windowGetScale(&drawWindow, &w, &h);
	gfxHelperDrawBox(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x+offsetX, y+offsetY, width*w, height*h, color, alignment, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void windowFill(Window_t *window, u32 color)
{
	if (!windowHasArea(window))
		return;

	float x, y, w, h;
  Window_t drawWindow;
  windowResolveScreen(&drawWindow, window);
	windowResolve(&drawWindow, &x, &y, 0, 0, window->AnchorAlign);
  windowGetScale(&drawWindow, &w, &h);
	gfxHelperDrawBox(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, drawWindow.Width - drawWindow.Position[0], drawWindow.Height - drawWindow.Position[1], color, drawWindow.AnchorAlign, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
void windowBorder(Window_t *window, u32 color, float left, float top, float right, float bottom)
{
	if (!windowHasArea(window))
		return;

	float x, y;
  Window_t drawWindow;
  windowResolveScreen(&drawWindow, window);

	if (left > 0)
	{
		windowResolve(&drawWindow, &x, &y, 0, 0, TEXT_ALIGN_MIDDLELEFT);
		gfxHelperDrawBox(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, left, drawWindow.Height - drawWindow.Position[1], color, TEXT_ALIGN_MIDDLELEFT, COMMON_DZO_DRAW_NORMAL);
	}

	if (right > 0)
	{
		windowResolve(&drawWindow, &x, &y, 0, 0, TEXT_ALIGN_MIDDLERIGHT);
		gfxHelperDrawBox(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, right, drawWindow.Height - drawWindow.Position[1], color, TEXT_ALIGN_MIDDLERIGHT, COMMON_DZO_DRAW_NORMAL);
	}

	if (top > 0)
	{
		windowResolve(&drawWindow, &x, &y, 0, 0, TEXT_ALIGN_TOPCENTER);
		gfxHelperDrawBox(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, drawWindow.Width - drawWindow.Position[0], top * SCREEN_RATIO_INV, color, TEXT_ALIGN_TOPCENTER, COMMON_DZO_DRAW_NORMAL);
	}

	if (bottom > 0)
	{
		windowResolve(&drawWindow, &x, &y, 0, 0, TEXT_ALIGN_BOTTOMCENTER);
		gfxHelperDrawBox(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, drawWindow.Width - drawWindow.Position[0], bottom * SCREEN_RATIO_INV, color, TEXT_ALIGN_BOTTOMCENTER, COMMON_DZO_DRAW_NORMAL);
	}
}

//--------------------------------------------------------------------------
void windowDrawTextWindow(Window_t *window, enum TextAlign windowAnchor, float offsetX, float offsetY, float scale, u32 color, char *str, int length, enum TextAlign alignment)
{
	if (!windowHasArea(window))
		return;

	float x, y, w, h;
  Window_t drawWindow;
  windowResolveScreen(&drawWindow, window);
	windowResolve(&drawWindow, &x, &y, 0, 0, windowAnchor);
  windowGetScale(&drawWindow, &w, &h);

  float s = scale * minf(w, h);
	gfxHelperDrawTextWindow(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x, y, drawWindow.Width - drawWindow.Position[0], drawWindow.Height - drawWindow.Position[1], offsetX, offsetY, s, color, str, -1, alignment, FONT_WINDOW_FLAGS_NO_SCISSOR, COMMON_DZO_DRAW_NORMAL);
}

//--------------------------------------------------------------------------
float windowDrawText(Window_t *window, enum TextAlign windowAnchor, float offsetX, float offsetY, float scale, u32 color, char *str, int length, enum TextAlign alignment)
{
	if (!windowHasArea(window))
		return 0;

	float x, y, w, h;
  Window_t drawWindow;
  windowResolveScreen(&drawWindow, window);
	windowResolve(&drawWindow, &x, &y, 0, 0, windowAnchor);
  windowGetScale(&drawWindow, &w, &h);
  
  float s = scale * minf(w, h);
	gfxHelperDrawText(drawWindow.AnchorPoint[0], drawWindow.AnchorPoint[1], x+offsetX, y+offsetY, s, color, str, length, alignment, COMMON_DZO_DRAW_NORMAL);
	return gfxGetFontWidth(str, length, s);
}

//--------------------------------------------------------------------------
int windowHasArea(Window_t *window)
{
	return window->Width > window->Position[0] && window->Height > window->Position[1];
}

//--------------------------------------------------------------------------
void windowCreateFrom(Window_t *out, Window_t *window, float winOffsetX, float winOffsetY, float winWidth, float winHeight, enum TextAlign winAlignment)
{
	memcpy(out, window, sizeof(Window_t));
	float x = window->WindowPoint[0] + window->Position[0];
	float y = window->WindowPoint[1] + window->Position[1];

	windowResolve(window, &x, &y, winOffsetX, winOffsetY, winAlignment);
	out->Width = winWidth;
	out->Height = winHeight;
	out->WindowPoint[0] = x;
	out->WindowPoint[1] = y;
	out->Position[0] = 0;
	out->Position[1] = 0;
	out->AnchorAlign = winAlignment;
	out->LocalPlayerIndex = window->LocalPlayerIndex;

	// helperAlign(&out->WindowPoint[0], &out->WindowPoint[1], winWidth, winHeight, out->AnchorAlign);
}

//--------------------------------------------------------------------------
void windowCreate(Window_t *out, float anchorX, float anchorY, float offsetX, float offsetY, float width, float height, enum TextAlign anchorAlignment)
{
	out->AnchorPoint[0] = anchorX;
	out->AnchorPoint[1] = anchorY;
	out->WindowPoint[0] = offsetX;
	out->WindowPoint[1] = offsetY;
	out->Position[0] = 0;
	out->Position[1] = 0;
	out->Width = width;
	out->Height = height;
	out->AnchorAlign = anchorAlignment;
	out->LocalPlayerIndex = 0;
}
