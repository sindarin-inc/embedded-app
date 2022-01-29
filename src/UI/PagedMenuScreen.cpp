#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "config.h"

#include "Bitmaps/Menu_Blank.hpp"
#include "Bitmaps/Menu_Blank_Bidirectional.hpp"
#include "Fonts/LucidaGrande10pt7b.h"
#include "Fonts/LucidaGrande14pt7b.h"
#include "Fonts/LucidaGrande5pt7b.h"
#include "Fonts/LucidaGrande6pt7b.h"

#define MAX_WIDTH 152

// BC: I'm reducing the number of fonts to 2 here as I feel 14 is too big and 5 is too
// small when I look at it in glass
const GFXfont *shrinkingFonts[] = {/*&LucidaGrande14pt7b,*/
                                   &LUCIDA_GRANDE10PT7B, &LUCIDA_GRANDE6PT7B
                                   /*,&LucidaGrande5pt7b */};

void PagedMenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    log_i("PagedMenuScreen render (selected = %s)", getCurrItem().title.c_str());
    auto autoUpdate = display.getAutoUpdate();
    display.setAutoUpdate(false);
    // Blank out screen
    display.fillScreen(display.bg);
    // Draw menu surroundings
    const unsigned char *menuBits;
    unsigned int menuWidth;
    unsigned int menuHeight;

    // Allows us to have arrows on both left and right side of the menu for
    // when a PagedMenu is used as a sub-menu (as in the case of being called)
    // from a PopupMenu from a readable.
    if (!isBidirectional_) {
        menuBits = MENU_BLANK_BITS;
        menuWidth = MENU_BLANK_WIDTH;
        menuHeight = MENU_BLANK_HEIGHT;
    } else {
        menuBits = MENU_BLANK_BIDIRECTIONAL_BITS;
        menuWidth = MENU_BLANK_BIDIRECTIONAL_WIDTH;
        menuHeight = MENU_BLANK_BIDIRECTIONAL_HEIGHT;
    }

    display.drawXBitmap(display.localXOffset + (display.width() - menuWidth) / 2,
                        display.localYOffset + (display.height() - menuHeight) / 2, menuBits,
                        menuWidth, menuHeight, display.fg);

    String itemTitle = getCurrItem().title;
    // Cache / replace old font; then try descending size fonts until we find one that fits our
    // string in the desired space.
    auto prevFont = display.getFont();
    for (int i = 0; i < sizeof(shrinkingFonts); ++i) {
        display.setFont(shrinkingFonts[i]);
        if (display.getTextWidth(itemTitle.c_str()) < MAX_WIDTH) {
            break;
        }
        log_d("Font size %d was too big for menu item title; stepping down!", i);
    }
    // If we still don't fit, shrink the string until we do.
    while (display.getTextWidth(itemTitle.c_str()) > MAX_WIDTH) {
        log_d("Item title %s still too long at smallest font size; truncating...",
              itemTitle.c_str());
        itemTitle = itemTitle.substring(0, itemTitle.length() - 4) + "...";
    }
    // Now draw!
    uint16_t width = 0, height = 0;
    String batteryStatus = this->power_.getBatteryLifeString();
    display.getTextBounds(itemTitle.c_str(), &width, &height);
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();
    display.drawText(itemTitle.c_str(), display.localXOffset + (screenWidth / 2) - (width / 2),
                     display.localYOffset + (screenHeight / 2) + (height / 2) - 3, screenWidth,
                     screenHeight);
    display.setFont(&LUCIDA_GRANDE5PT7B);
    display.getTextBounds(batteryStatus.c_str(), &width, &height);
    // Arbitrarily put the battery text off of the bottom of the screen 20 pixels
    // This is just so we always have it in our prototype, not that it'll be
    // the right place (or icon) long term
    // Not taking into account the rednerer x and y offsets here on purpose
    display.drawText(batteryStatus.c_str(), (screenWidth / 2) - (width / 2), screenHeight - 20,
                     screenWidth, screenHeight);
    display.updateDisplay();
    display.setFont(prevFont);
    display.setAutoUpdate(autoUpdate);
    needsRerender = false;
}

// Returns 1 if the selection was updated, 0 otherwise
auto PagedMenuScreen::nextSelection() -> int {
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    return numMenuItems_ > 1 ? 1 : 0;
}

// Returns 1 if the selection was updated, 0 otherwise
auto PagedMenuScreen::prevSelection() -> int {
    currSelection_ = (currSelection_ - 1) % numMenuItems_;
    return numMenuItems_ > 1 ? 1 : 0;
}