#include "UI/MenuScreen.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Style.hpp"

#define MAX_WIDTH 150

// All of this needs to get fixed up for the ttf fonts to look right
auto MenuScreen::layout(LayoutContext &context, Size size) -> Size {

    context.setViewFrame(backLabelView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    context.setViewFrame(clockBatteryView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);
    return size;
}

void MenuScreen::requireRestart() { restartOnBack_ = true; }

void MenuScreen::drawInRect(Rect rect) {
    backLabelView_.draw();
    clockBatteryView_.draw();
}

void MenuScreen::drawMenuItems(DisplaySystem &display, bool withBullet, bool withLastItem) {
    display.setFont(myStyle_.itemFont);
    int additionalYOffset = 0;
    int iteratorEnd = (withLastItem) ? numMenuItems_ : numMenuItems_ - 1;
    int xOffsetForMenu = display.defaultUIInsets().left + myStyle_.xOffsetForList;
    int yOffsetForMenu = display.defaultUIInsets().top + myStyle_.yOffsetForList;

    // If we have less than a page worth, we need to add some extra padding at the top to center
    // the items. For each item less than the numnber of items, we add half the height of the
    // text. Bias a bit to the top by subtracting 5.
    if (getNumItems() < myStyle_.numberOfItemsPerPage) {
        additionalYOffset = (myStyle_.numberOfItemsPerPage - getNumItems() - 1) *
                            (display.getLineHeightPlusSpacing() - display.getLineSpacing() + 3) / 2;
    }

    for (int i = currMenuPage_ * myStyle_.numberOfItemsPerPage;
         (i < (currMenuPage_ * myStyle_.numberOfItemsPerPage + myStyle_.numberOfItemsPerPage)) &&
         (i < iteratorEnd);
         i++) {
        std::string itemTitle = getItemTitleAt(i);
        // If we still don't fit, shrink the string until we do.
        while (display.getTextWidth(itemTitle) > MAX_WIDTH) {
            if (itemTitle.length() > 4) {
                itemTitle = Utf8Substr(itemTitle, 0, itemTitle.length() - 4) + "...";
            } else {
                // Practically, this shouldn't ever happen, but I'm doing
                // the bounds check here just in case we do hit it some day.
                itemTitle = "too big!";
            }
        }
        display.drawSingleLineOfText(
            itemTitle, xOffsetForMenu + 5,
            yOffsetForMenu + additionalYOffset - 1 +
                (i % myStyle_.numberOfItemsPerPage) *
                    (display.getLineHeightPlusSpacing() - display.getLineSpacing() + 1));

        // xoffset for main menu is 12, for settings menu is 23
        // yoffset for main menu is some crazy thing
        // for settings menu some other crazy thing
        // Switch to Resolution-based here. 5x5 is a good size for the paging bubbles
        // on a 256 x 256 screen, but higher resolution screens will need larger bubbles
        //
        // This probably should change to a layout item that is Vertically Center Aligned
        // with the menu item. That's the only way to get it to look right on small screens
        // and large screens.
        // Copied from MenuItemView -- why arent't these MenuItemViews?
        auto height = display.getLineHeightPlusSpacing();
        // Good default for 1 bit smallest font
        auto indicatorWidth = (height + 1) / 3;
        // Mirroring our previous defaults but now at least it's dynamic
        // TODO: Make this a MenuItemView instead
        auto indicatorWidthAdjustment = indicatorWidth * 8 / 5;

        if ((i == currSelection_) && (withBullet)) {
            display.fillRoundRect(
                xOffsetForMenu - indicatorWidthAdjustment,
                yOffsetForMenu + additionalYOffset +
                    (i % myStyle_.numberOfItemsPerPage) *
                        (display.getLineHeightPlusSpacing() - display.getLineSpacing() + 1) +
                    indicatorWidth * 4 / 5,
                indicatorWidth, indicatorWidth, indicatorWidth * 5 / 3, display.fg);
        }
    }

    display.setFont(display.style.getFontFamily().readableFont);
    int extraTextWidth = display.getTextWidth(extraTextBelowMenu_);
    display.drawSingleLineOfText(extraTextBelowMenu_, (display.width() - extraTextWidth) / 2,
                                 display.height() - 61);
}

void MenuScreen::showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                      int duration, bool rerenderAfter) {
    display.setFont(display.style.getFontFamily().settingsFont);
    display.drawSingleLineOfCenteredText(message);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    Delay(duration);
    if (rerenderAfter) {
        setNeedsRerender();
        this->setFirstLoad();
        this->render(display);
    }
}

void MenuScreen::render(DisplaySystem &display) {
    log_i("Menuscreen render (selected = %s)", getCurrItem().title.c_str());

    setNeedsLayout();

    setFrame(defaultFrameRect());

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        clockBatteryView_.updateTime();
        drawMenuItems(display, false);
#if CONFIG_DISPLAY_EINKET013TT1
        draw();
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
#endif
    }

    drawMenuItems(display, true);
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);

    firstLoad_ = false;
}

void MenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        if (prevSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        if (nextSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        back();
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}
