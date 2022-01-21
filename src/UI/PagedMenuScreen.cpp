#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "config.h"

#include "Bitmaps/Menu_Blank.hpp"
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

void PagedMenuScreen::render() {
    if (!needsRerender) {
        return;
    }
    log_i("PagedMenuScreen render (selected = %s)", getCurrItem().title.c_str());
    auto autoUpdate = display_.getAutoUpdate();
    display_.setAutoUpdate(false);
    // Blank out screen
    display_.fillScreen(display_.bg);
    // Draw menu surroundings
    display_.drawXBitmap(display_.localXOffset, display_.localYOffset, MENU_BLANK_BITS, 256, 256,
                         display_.fg);
    String itemTitle = getCurrItem().title;
    // Cache / replace old font; then try descending size fonts until we find one that fits our
    // string in the desired space.
    auto prevFont = display_.getFont();
    for (int i = 0; i < sizeof(shrinkingFonts); ++i) {
        display_.setFont(shrinkingFonts[i]);
        if (display_.getTextWidth(itemTitle.c_str()) < MAX_WIDTH) {
            break;
        }
        log_d("Font size %d was too big for menu item title; stepping down!", i);
    }
    // If we still don't fit, shrink the string until we do.
    while (display_.getTextWidth(itemTitle.c_str()) > MAX_WIDTH) {
        log_d("Item title %s still too long at smallest font size; truncating...",
              itemTitle.c_str());
        itemTitle = itemTitle.substring(0, itemTitle.length() - 4) + "...";
    }
    // Now draw!
    uint16_t width = 0, height = 0;
    String batteryStatus = this->power_.getBatteryLifeString();
    display_.getTextBounds(itemTitle.c_str(), &width, &height);
    uint16_t screenWidth = display_.width();
    uint16_t screenHeight = display_.height();
    display_.drawText(itemTitle.c_str(), display_.localXOffset + (screenWidth / 2) - (width / 2),
                      display_.localYOffset + (screenHeight / 2) + (height / 2), screenWidth,
                      screenHeight);
    display_.setFont(&LUCIDA_GRANDE5PT7B);
    display_.getTextBounds(batteryStatus.c_str(), &width, &height);
    // Arbitrarily put the battery text off of the bottom of the screen 20 pixels
    // This is just so we always have it in our prototype, not that it'll be
    // the right place (or icon) long term
    // Not taking into account the rednerer x and y offsets here on purpose
    display_.drawText(batteryStatus.c_str(), (screenWidth / 2) - (width / 2), screenHeight - 20,
                      screenWidth, screenHeight);
    display_.updateDisplay();
    display_.setFont(prevFont);
    display_.setAutoUpdate(autoUpdate);
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