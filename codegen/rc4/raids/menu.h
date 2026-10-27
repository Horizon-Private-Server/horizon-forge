#ifndef RAIDS_MENU_H
#define RAIDS_MENU_H

#include "window.h"

#define RAIDS_MENU_BACKGROUND_COLOR     (0x70000000)
#define RAIDS_MENU_PANEL_COLOR          (0x78101010)
#define RAIDS_MENU_POPUP_COLOR          (0x80101010)
#define RAIDS_MENU_BORDER_COLOR         (0x80000020)
#define RAIDS_MENU_FOOTER_COLOR         (0x80000000)
#define RAIDS_MENU_TEXT_COLOR           (0x80FFFFFF)
#define RAIDS_MENU_SECONDARY_COLOR      (0x80B0B0B0)
#define RAIDS_MENU_WARNING_COLOR        (0x804040D0)
#define RAIDS_MENU_TRACK_COLOR          (0x80303030)
#define RAIDS_MENU_PROGRESS_COLOR       (0x80B0B050)
#define RAIDS_MENU_BACKDROP_COLOR       (0x60000000)

#define RAIDS_MENU_PADDING              (8)
#define RAIDS_MENU_TITLE_HEIGHT         (24)
#define RAIDS_MENU_FOOTER_HEIGHT        (16)
#define RAIDS_MENU_POPUP_PADDING        (12)
#define RAIDS_MENU_POPUP_TITLE_HEIGHT   (28)
#define RAIDS_MENU_POPUP_FOOTER_HEIGHT  (20)
#define RAIDS_MENU_PROGRESS_HEIGHT      (6)

// Drawing helpers do not change the supplied window or handle input/state.
void menuDrawFrame(Window_t* window, char* title, float titleHeight);
// Use a middle-left, middle-center, or middle-right alignment for controls.
void menuDrawFooter(Window_t* window, char* buttons, float scale, enum TextAlign alignment, float padding);
void menuDrawPopupFrame(Window_t* window, char* title, u32 titleColor, char* buttons);
void menuDrawPopup(Window_t* parent, char* title, char* body, char* buttons, float width, float height);
void menuDrawBackdrop(Window_t* window);
float menuDrawFittedText(Window_t* window, float x, float y, float width, float scale, u32 color, char* text, enum TextAlign alignment);
void menuDrawProgressBar(Window_t* window, float x, float y, float width, double progress);

#endif // RAIDS_MENU_H
