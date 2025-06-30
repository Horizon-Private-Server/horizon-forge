#ifndef RAIDS_DRAW_H
#define RAIDS_DRAW_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>
#include <libdl/graphics.h>

typedef struct
{
  POINT AnchorPoint;
  POINT WindowPoint;
  POINT Position;
  float Width;
  float Height;
  enum TextAlign AnchorAlign;
} Window_t;

void windowReset(Window_t* out);
void windowMove(Window_t* out, float x, float y);
void windowCrop(Window_t* out, float left, float top, float right, float bottom);
void windowResolve(float* x, float *y, Window_t* window, float offsetX, float offsetY, enum TextAlign alignment);
void windowDrawSprite(Window_t* window, enum TextAlign windowAnchor, float offsetX, float offsetY, float width, float height, int spriteId, int spriteDimW, int spriteDimH, u32 color, enum TextAlign alignment);
void windowDrawBox(Window_t* window, enum TextAlign windowAnchor, float offsetX, float offsetY, float width, float height, u32 color, enum TextAlign alignment);
void windowFill(Window_t* window, u32 color);
void windowBorder(Window_t* window, u32 color, float left, float top, float right, float bottom);
float windowDrawText(Window_t* window, enum TextAlign windowAnchor, float offsetX, float offsetY, float scale, u32 color, char* str, int length, enum TextAlign alignment);
void windowDrawTextWindow(Window_t* window, enum TextAlign windowAnchor, float offsetX, float offsetY, float scale, u32 color, char* str, int length, enum TextAlign alignment);
int windowHasArea(Window_t* window);
void windowCreateFrom(Window_t* out, Window_t* window, float winOffsetX, float winOffsetY, float winWidth, float winHeight, enum TextAlign winAlignment);
void windowCreate(Window_t* out, float anchorX, float anchorY, float offsetX, float offsetY, float width, float height, enum TextAlign anchorAlignment);
void windowDrawDialog(Window_t* drawWindow, char* titleStr, char* subtitleStr, char* bodyStr, char* buttonStr);

#endif // RAIDS_DRAW_H
