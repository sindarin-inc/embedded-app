#include "UI/MenuScreen.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Style.hpp"

#define MAX_WIDTH 150

void MenuScreen::drawMenuItems(DisplaySystem &display, bool withBullet, bool withLastItem) {
    display.setFont(myStyle_.itemFont);
    int iteratorEnd = (withLastItem) ? numMenuItems_ : numMenuItems_ - 1;
    for (int i = currMenuPage_ * myStyle_.numberOfItemsPerPage;
         (i < (currMenuPage_ * myStyle_.numberOfItemsPerPage + myStyle_.numberOfItemsPerPage)) &&
         (i < iteratorEnd);
         i++) {
        std::string itemTitle = getItemTitleAt(i);
        // If we still don't fit, shrink the string until we do.
        while (display.getTextWidth(itemTitle.c_str()) > MAX_WIDTH) {
            if (itemTitle.length() > 4) {
                itemTitle = itemTitle.substr(0, itemTitle.length() - 4) + "...";
            } else {
                // Practically, this shouldn't ever happen, but I'm doing
                // the bounds check here just in case we do hit it some day.
                itemTitle = "too big!";
            }
        }
        display.drawSingleLineOfText(
            itemTitle, myStyle_.xOffsetForList,
            myStyle_.yOffsetForList + (i % myStyle_.numberOfItemsPerPage) *
                                          (display.getTextHeight() / 5 + display.getTextHeight()));

        // xoffset for main menu is 12, for settings menu is 23
        // yoffset for main menu is some crazy thing
        // for settings menu some other crazy thing
        if ((i == currSelection_) && (withBullet)) {
            display.fillRoundRect(myStyle_.xOffsetForList - display.getTextHeight() / 4 - 2,
                                  myStyle_.yOffsetForList +
                                      (i % myStyle_.numberOfItemsPerPage) *
                                          (display.getTextHeight() / 5 + display.getTextHeight()) +
                                      4,
                                  display.getTextHeight() - 7, display.getTextHeight() - 7,
                                  display.getTextHeight() / 5, display.fg);
        }
    }

    int oldTextHeight = display.getTextHeight();
    display.setFont(display.style.getFontFamily().readableFont);
    display.drawSingleLineOfText(extraTextBelowMenu_.c_str(), myStyle_.xOffsetForList + 10,
                                 (1 + myStyle_.numberOfItemsPerPage) * oldTextHeight + 50);
}

void MenuScreen::drawArrowsAndSelections(DisplaySystem &display) {

    display.setFont(myStyle_.navigationFont);
    // Upper left arrow for Settings
    display.drawLine(
        display.xAbsolute(myStyle_.xOffsetForArrows), display.yAbsolute(myStyle_.yOffsetForArrows),
        display.xAbsolute(myStyle_.xOffsetForArrows + myStyle_.arrowWidth),
        display.yAbsolute(myStyle_.yOffsetForArrows - myStyle_.arrowHeight / 2), display.fg);
    display.drawLine(
        display.xAbsolute(myStyle_.xOffsetForArrows), display.yAbsolute(myStyle_.yOffsetForArrows),
        display.xAbsolute(myStyle_.xOffsetForArrows + myStyle_.arrowWidth),
        display.yAbsolute(myStyle_.yOffsetForArrows + myStyle_.arrowHeight / 2), display.fg);

    // Lower right arrow for "Read" or flip between "Books and Articles"
    display.drawLine(display.xAbsolute(display.xAbsoluteMax() - myStyle_.xOffsetForArrows),
                     display.yAbsolute(display.yAbsoluteMax() - myStyle_.yOffsetForArrows),
                     display.xAbsolute(display.xAbsoluteMax() -
                                       (myStyle_.xOffsetForArrows + myStyle_.arrowWidth)),
                     display.yAbsolute(display.yAbsoluteMax() -
                                       (myStyle_.yOffsetForArrows - myStyle_.arrowHeight / 2)),
                     display.fg);
    display.drawLine(display.xAbsolute(display.xAbsoluteMax() - myStyle_.xOffsetForArrows),
                     display.yAbsolute(display.yAbsoluteMax() - myStyle_.yOffsetForArrows),
                     display.xAbsolute(display.yAbsoluteMax() -
                                       (myStyle_.xOffsetForArrows + myStyle_.arrowWidth)),
                     display.yAbsolute(display.yAbsoluteMax() -
                                       (myStyle_.yOffsetForArrows + myStyle_.arrowHeight / 2)),
                     display.fg);

    display.drawSingleLineOfText(
        upperLeftText_.c_str(),
        display.xAbsolute(myStyle_.xOffsetForArrows + myStyle_.arrowWidth +
                          myStyle_.arrowTextSpacing),
        display.yAbsolute(myStyle_.yOffsetForArrows - display.getTextHeight() / 2));

    display.drawSingleLineOfText(
        lowerRightText_.c_str(),
        display.xAbsolute(display.xAbsoluteMax() -
                          (myStyle_.xOffsetForArrows + myStyle_.arrowWidth +
                           myStyle_.arrowTextSpacing +
                           display.getTextWidth(lowerRightText_.c_str()))),
        display.yAbsolute(display.yAbsoluteMax() -
                          (myStyle_.yOffsetForArrows + display.getTextHeight() / 2)));
}

void MenuScreen::showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                      int duration, bool rerenderAfter) {
    display.drawSingleLineOfCenteredText(message);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    delay(duration);
    if (rerenderAfter) {
        this->needsRerender = true;
        this->setFirstLoad();
        this->render(display);
    }
}

void MenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    log_i("Menuscreen render (selected = %s)", getCurrItem().title.c_str());

    drawArrowsAndSelections(display);

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        drawMenuItems(display, false);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

    drawMenuItems(display, true);
    drawArrowsAndSelections(display);
    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    firstLoad_ = false;
    needsRerender = false;
}

void MenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        if (prevSelection()) {
            firstLoad_ = true;
        }
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        if (nextSelection()) {
            firstLoad_ = true;
        }
        needsRerender = true;
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        if (backCallback != nullptr) {
            firstLoad_ = true;
            backCallback();
        }
        break;
    case RemoteCommand::ANIMATION:
        needsRerender = false;
        break;
    default:
        break;
    }
}
