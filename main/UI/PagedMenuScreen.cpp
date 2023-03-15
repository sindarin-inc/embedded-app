#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Bitmaps/Menu_Blank.hpp"
#include "Bitmaps/Menu_Blank_Bidirectional.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#define MAX_WIDTH 152

void PagedMenuScreen::render(DisplaySystem &display) {

    uint16_t midX = display.width() / 2;
    uint16_t midY = display.height() / 2;

    // Top Arrow
    display.drawLine(midX, 42, midX - 10, 47, display.fg);
    display.drawLine(midX, 42, midX + 10, 47, display.fg);

    // Bottom Arrow
    display.drawLine(midX, midY * 2 - 40, midX - 10, midY * 2 - 45, display.fg);
    display.drawLine(midX, midY * 2 - 40, midX + 10, midY * 2 - 45, display.fg);

    // Left Arrow
    display.drawLine(10, midY, 15, midY - 10, display.fg);
    display.drawLine(10, midY, 15, midY + 10, display.fg);

    // Right Arrow
    display.drawLine(midX * 2 - 10, midY, midX * 2 - 15, midY - 10, display.fg);
    display.drawLine(midX * 2 - 10, midY, midX * 2 - 15, midY + 10, display.fg);

    display.setFont(display.style.getFontFamily().mainMenuFont);
    auto itemTitle = getCurrItem().title;
    uint16_t width, height;

    display.getTextBounds(itemTitle.c_str(), &width, &height);
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    display.setRotation(0);
    display.drawSingleLineOfText(itemTitle.c_str(), (screenWidth / 2) - (width / 2),
                                 (screenHeight / 2) - (display.getTextHeight()) / 2);

    auto prevTitle = getItemAt((currSelection_ > 0) ? ((currSelection_ - 1) % numMenuItems_)
                                                    : (numMenuItems_ - 1))
                         .title;
    auto nextTitle = getItemAt((currSelection_ + 1) % numMenuItems_).title;

    display.getTextBounds(prevTitle.c_str(), &width, &height);

    display.getTextBounds(nextTitle.c_str(), &width, &height);

    display.setFont(display.style.getFontFamily().readableFont);

    display.getTextBounds("Settings", &width, &height);

    display.drawSingleLineOfText("Settings", screenWidth / 2 - width / 2, 28);

    display.getTextBounds("Read", &width, &height);

    display.drawSingleLineOfText("Read", screenWidth / 2 - width / 2, screenHeight - 30 - height);

    display.updateDisplay();

    needsRerender = false;
}

auto PagedMenuScreen::nextSelection(bool skipLast) -> bool {
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    if (currSelection_ == numMenuItems_ - 2) {
        currSelection_ = 0;
    }
    needsRerender = true;
    return false;
}

auto PagedMenuScreen::prevSelection(bool skipLast) -> bool {
    currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
    if (currSelection_ == numMenuItems_ - 1) {
        // If there are 0 readables, this breaks the menu
        currSelection_ = numMenuItems_ - 3;
    }
    needsRerender = true;
    return false;
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
        // Settings are always the last item
        currSelection_ = numMenuItems_ - 1;
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }

        prevSelection();
        needsRerender = true;

        break;

    case RemoteCommand::NEXT:
    case RemoteCommand::ENTER:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;

    case RemoteCommand::CHANNEL_PLUS:
        nextSelection();
        needsRerender = true;

        break;

    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        prevSelection();
        needsRerender = true;
        break;
    default:
        break;
    }
}
