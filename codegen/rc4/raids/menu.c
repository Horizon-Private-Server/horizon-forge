#include <libdl/stdio.h>
#include <libdl/string.h>
#include "menu.h"

//--------------------------------------------------------------------------
float menuDrawFittedText(Window_t* window, float x, float y, float width, float scale, u32 color, char* text, enum TextAlign alignment)
{
  if (width <= 0) return 0;

  char buf[128];
  snprintf(buf, sizeof(buf), "%s", text);
  if (gfxGetFontWidth(buf, -1, scale) > width) {
    float dotsWidth = gfxGetFontWidth("...", -1, scale);
    int length = strlen(buf);
    while (length > 0 && (length > (int)sizeof(buf) - 4 || gfxGetFontWidth(buf, -1, scale) + dotsWidth > width)) buf[--length] = 0;
    if (dotsWidth <= width) strcat(buf, "...");
  }
  windowDrawText(window, TEXT_ALIGN_TOPLEFT, x, y, scale, color, buf, -1, alignment);
  return gfxGetFontWidth(buf, -1, scale);
}

//--------------------------------------------------------------------------
void menuDrawFrame(Window_t* window, char* title, float titleHeight)
{
  windowFill(window, RAIDS_MENU_BACKGROUND_COLOR);
  menuDrawFittedText(window, window->Width * 0.5, titleHeight * 0.5, window->Width - 2*RAIDS_MENU_PADDING, 1.1, RAIDS_MENU_TEXT_COLOR, title, TEXT_ALIGN_MIDDLECENTER);
  windowDrawBox(window, TEXT_ALIGN_TOPLEFT, RAIDS_MENU_PADDING, titleHeight - 1, window->Width - 2*RAIDS_MENU_PADDING, 1, RAIDS_MENU_TRACK_COLOR, TEXT_ALIGN_TOPLEFT);
  windowBorder(window, RAIDS_MENU_BORDER_COLOR, 1, 1, 1, 1);
}

//--------------------------------------------------------------------------
void menuDrawFooter(Window_t* window, char* buttons, float scale, enum TextAlign alignment, float padding)
{
  float x = padding;
  if (alignment == TEXT_ALIGN_MIDDLECENTER) x = window->Width * 0.5;
  else if (alignment == TEXT_ALIGN_MIDDLERIGHT) x = window->Width - padding;

  windowFill(window, RAIDS_MENU_FOOTER_COLOR);
  menuDrawFittedText(window, x, window->Height * 0.5, window->Width - 2*padding, scale, RAIDS_MENU_TEXT_COLOR, buttons, alignment);
}

//--------------------------------------------------------------------------
void menuDrawPopupFrame(Window_t* window, char* title, u32 titleColor, char* buttons)
{
  const float padding = RAIDS_MENU_POPUP_PADDING;
  windowDrawBox(window, TEXT_ALIGN_TOPLEFT, 3, 3, window->Width, window->Height, RAIDS_MENU_BACKDROP_COLOR, TEXT_ALIGN_TOPLEFT);
  windowFill(window, RAIDS_MENU_POPUP_COLOR);
  windowDrawBox(window, TEXT_ALIGN_TOPLEFT, 0, 0, window->Width, 2, RAIDS_MENU_BORDER_COLOR, TEXT_ALIGN_TOPLEFT);
  menuDrawFittedText(window, window->Width * 0.5, 8, window->Width - 2*padding, 0.95, titleColor, title, TEXT_ALIGN_TOPCENTER);
  windowDrawBox(window, TEXT_ALIGN_TOPLEFT, padding, RAIDS_MENU_POPUP_TITLE_HEIGHT, window->Width - 2*padding, 1, RAIDS_MENU_TRACK_COLOR, TEXT_ALIGN_TOPLEFT);

  Window_t footerWindow;
  windowCreateFrom(&footerWindow, window, 0, 0, window->Width, RAIDS_MENU_POPUP_FOOTER_HEIGHT, TEXT_ALIGN_BOTTOMCENTER);
  menuDrawFooter(&footerWindow, buttons, 0.7, TEXT_ALIGN_MIDDLECENTER, padding);
  windowBorder(window, RAIDS_MENU_BORDER_COLOR, 1, 1, 1, 1);
}

//--------------------------------------------------------------------------
void menuDrawBackdrop(Window_t* window)
{
  windowFill(window, RAIDS_MENU_BACKDROP_COLOR);
}

//--------------------------------------------------------------------------
void menuDrawPopup(Window_t* parent, char* title, char* body, char* buttons, float width, float height)
{
  const float padding = RAIDS_MENU_POPUP_PADDING;
  const float bodyY = RAIDS_MENU_POPUP_TITLE_HEIGHT + padding;
  Window_t window;
  Window_t bodyWindow;
  menuDrawBackdrop(parent);
  windowCreateFrom(&window, parent, 0, 0, width, height, TEXT_ALIGN_MIDDLECENTER);
  menuDrawPopupFrame(&window, title, RAIDS_MENU_TEXT_COLOR, buttons);
  windowCreateFrom(&bodyWindow, &window, padding, bodyY, width - 2*padding, height - bodyY - RAIDS_MENU_POPUP_FOOTER_HEIGHT - padding, TEXT_ALIGN_TOPLEFT);
  if (body)
    windowDrawTextWindow(&bodyWindow, TEXT_ALIGN_TOPLEFT, 0, 0, 0.7, RAIDS_MENU_TEXT_COLOR, body, -1, TEXT_ALIGN_TOPLEFT);
}

//--------------------------------------------------------------------------
void menuDrawProgressBar(Window_t* window, float x, float y, float width, double progress)
{
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  windowDrawBox(window, TEXT_ALIGN_TOPLEFT, x, y, width, RAIDS_MENU_PROGRESS_HEIGHT, RAIDS_MENU_TRACK_COLOR, TEXT_ALIGN_TOPLEFT);
  if (progress > 0)
    windowDrawBox(window, TEXT_ALIGN_TOPLEFT, x, y, width * progress, RAIDS_MENU_PROGRESS_HEIGHT, RAIDS_MENU_PROGRESS_COLOR, TEXT_ALIGN_TOPLEFT);
}
