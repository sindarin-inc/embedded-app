#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Bitmaps/Menu_Blank.hpp"
#include "Bitmaps/Menu_Blank_Bidirectional.hpp"
#include "Fonts/LucidaGrande10pt7b.h"
#include "Fonts/LucidaGrande14pt7b.h"
#include "Fonts/LucidaGrande5pt7b.h"
#include "Fonts/LucidaGrande6pt7b.h"
#include "Misc/StringUtil.hpp"
#if !SIMULATOR
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#define MAX_WIDTH 152

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

    display.drawXBitmap((int16_t)((display.width() - menuWidth) / 2),
                        (int16_t)((display.height() - menuHeight) / 2), menuBits,
                        (int16_t)menuWidth, (int16_t)menuHeight, display.fg);

    auto itemTitle = getCurrItem().title;
    // Cache / replace old font; then try descending size fonts until we find one that fits our
    // string in the desired space.
    auto prevFont = display.getFont();

    display.setFont(&LUCIDA_GRANDE10PT7B);

    // If we still don't fit, shrink the string until we do.
    while (display.getTextWidth(itemTitle.c_str()) > MAX_WIDTH) {
        log_d("Item title %s still too long at smallest font size; truncating...",
              itemTitle.c_str());
        if (itemTitle.length() > 4) {
            itemTitle = itemTitle.substr(0, itemTitle.length() - 4) + "...";
        } else {
            // Practially, this shouldn't ever happen, but I'm doing
            // the bounds check here just in case we do hit it some day.
            itemTitle = "too big!";
        }
    }

    // Now draw!
    uint16_t width = 0, height = 0;
    auto batteryStatus = this->power_.getBatteryLifeString();
    uint16_t temp;
#if !SIMULATOR
    auto *driver = ((DisplayEinkET013TT1 *)&display)->getDriver();
    temp = driver->readTemperature();
#else
    temp = 22;
#endif
    batteryStatus += StringFormat(" %dC", temp);

    // Get the letters in there that go below the base of the standard character
    if (maxHeightForThisFont_ == 0) {
        display.getTextBounds("Hi J g y p q Q j!!", &width, &maxHeightForThisFont_);
    }
    display.getTextBounds(itemTitle.c_str(), &width, &height);
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();
    display.drawText(itemTitle.c_str(), (screenWidth / 2) - (width / 2),
                     (screenHeight / 2) + (maxHeightForThisFont_ / 2) - 4, screenWidth,
                     screenHeight);
    display.setFont(&LUCIDA_GRANDE5PT7B);

    display.getTextBounds(batteryStatus.c_str(), &width, &height);
    // Arbitrarily put the battery text off of the bottom of the screen 20 pixels
    // This is just so we always have it in our prototype, not that it'll be
    // the right place (or icon) long term
    // Not taking into account the rednerer x and y offsets here on purpose
    display.gfxClass()->setCursor((display.gfxClass()->width() / 2) - (width / 2),
                                  display.gfxClass()->height() - 20);
    display.println(batteryStatus.c_str());

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
    if (currSelection_ == 0) {
        currSelection_ = numMenuItems_ - 1;
    } else {
        currSelection_ = (currSelection_ - 1) % numMenuItems_;
    }
    return numMenuItems_ > 1 ? 1 : 0;
}

//
// For a PagedMenu, we actually want to progress through
// the options via the left and right buttons and use
// the down button (CHANNEL_PLUS) to select the text.
//
// This is more consistent with the behavior of turning pages
// and using down to go to the popup menu and up to go back.
//
// While we are using the TurnTouch, we should make the commands
// more consistent (left and right versus up and down).
//
// Up is back
// Right is next, Left is previous
// Down is select
//
void PagedMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        // Discussion for BC and JB around the right way to change the inversion
        // settting here. Should processCommand take a display? That seems clunky.
        // But unlike the Readable, we don't have a clear path to the display here.
        // Also need to get the prefs. Maybe it needs to be a callback. Not critical
        // as this is a nice to have to invert the display here. So maybe we don't
        // care at all.
        //
        // SOMEHOWgetDisplay()->flipInversion();
        // SOMEHOWGETprefs_.invertSetting.set(SOMEHOWgetDisplay()->getInversion());
        // needsRerender = true;

        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    case RemoteCommand::NEXT:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
        needsRerender = nextSelection();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        needsRerender = prevSelection();
        break;
    default:
        break;
    }
}