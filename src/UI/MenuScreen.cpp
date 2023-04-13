#include "UI/MenuScreen.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Style.hpp"

#define MAX_WIDTH 150

auto MenuScreen::layout(LayoutContext &context, Size size) -> Size {

    context.setViewFrame(leftArrowView_, {{0, 1}, {5, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);
    context.setViewFrame(upperLeftTextView_, {{10, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    context.setViewFrame(clockBatteryView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);
    return size;
}

void MenuScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
    clockBatteryView_.draw();
}

void MenuScreen::drawMenuItems(DisplaySystem &display, bool withBullet, bool withLastItem) {
    display.setFont(myStyle_.itemFont);
    int additionalYOffset = 0;
    int iteratorEnd = (withLastItem) ? numMenuItems_ : numMenuItems_ - 1;
    int xOffsetForMenu = display.xAbsolute(display.defaultXPadding) + myStyle_.xOffsetForList;
    int yOffsetForMenu = display.yAbsolute(display.defaultYPadding) + myStyle_.yOffsetForList;

    // If we have less than a page worth, we need to add some extra padding at the top to center
    // the items. For each item less than the numnber of items, we add half the height of the
    // text. Bias a bit to the top by subtracting 5.
    if (getNumItems() < myStyle_.numberOfItemsPerPage) {
        additionalYOffset = (myStyle_.numberOfItemsPerPage - getNumItems() - 1) *
                            (display.getTextHeight() - display.getLineSpacing() + 3) / 2;
    }

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
            itemTitle, xOffsetForMenu,
            yOffsetForMenu + additionalYOffset +
                (i % myStyle_.numberOfItemsPerPage) *
                    (display.getTextHeight() - display.getLineSpacing() + 3));

        // xoffset for main menu is 12, for settings menu is 23
        // yoffset for main menu is some crazy thing
        // for settings menu some other crazy thing
        if ((i == currSelection_) && (withBullet)) {
            display.fillRoundRect(xOffsetForMenu - display.getTextHeight() / 4 - 2,
                                  yOffsetForMenu + additionalYOffset +
                                      (i % myStyle_.numberOfItemsPerPage) *
                                          (display.getTextHeight() - display.getLineSpacing() + 3) +
                                      4,
                                  5, 5, 3, display.fg);
        }
    }

    display.setFont(display.style.getFontFamily().readableFont);
    int extraTextWidth = display.getTextWidth(extraTextBelowMenu_.c_str());
    display.drawSingleLineOfText(extraTextBelowMenu_.c_str(),
                                 display.xAbsolute(display.xAbsoluteCenter() - extraTextWidth / 2),
                                 display.yAbsolute(display.yAbsoluteMax() - 60));
}

void MenuScreen::showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                      int duration, bool rerenderAfter) {
    display.setFont(display.style.getFontFamily().settingsFont);
    display.drawSingleLineOfCenteredText(message);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    Delay(duration);
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

    setNeedsLayout();

    // For the main menu, we want to use the entire screen and ignore the user settings for reading
    // box.
    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        drawMenuItems(display, false);
        draw();
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

    drawMenuItems(display, true);
    draw();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    firstLoad_ = false;
    needsRerender = false;
}

void MenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        if (prevSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        if (nextSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
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
