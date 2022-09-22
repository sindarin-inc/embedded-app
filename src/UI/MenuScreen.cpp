#include "UI/MenuScreen.hpp"

#include <utility>

#include "Misc/Style.hpp"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

void MenuScreen::drawArrowsAndSelections(DisplaySystem &display) {

    display.setFont(display.style.getFontFamily().readableFont);
    // Upper left arrow for Settings
    display.drawLine(display.xAbsolute(xOffsetForArrows), display.yAbsolute(yOffsetForArrows),
                     display.xAbsolute(xOffsetForArrows + arrowWidth),
                     display.yAbsolute(yOffsetForArrows - arrowHeight / 2), display.fg);
    display.drawLine(display.xAbsolute(xOffsetForArrows), display.yAbsolute(yOffsetForArrows),
                     display.xAbsolute(xOffsetForArrows + arrowWidth),
                     display.yAbsolute(yOffsetForArrows + arrowHeight / 2), display.fg);

    // Lower right arrow for "Read" or flip between "Books and Articles"
    display.drawLine(
        display.xAbsolute(display.xAbsoluteMax() - xOffsetForArrows),
        display.yAbsolute(display.yAbsoluteMax() - yOffsetForArrows),
        display.xAbsolute(display.xAbsoluteMax() - (xOffsetForArrows + arrowWidth)),
        display.yAbsolute(display.yAbsoluteMax() - (yOffsetForArrows - arrowHeight / 2)),
        display.fg);
    display.drawLine(
        display.xAbsolute(display.xAbsoluteMax() - xOffsetForArrows),
        display.yAbsolute(display.yAbsoluteMax() - yOffsetForArrows),
        display.xAbsolute(display.yAbsoluteMax() - (xOffsetForArrows + arrowWidth)),
        display.yAbsolute(display.yAbsoluteMax() - (yOffsetForArrows + arrowHeight / 2)),
        display.fg);

    display.drawSingleLineOfText(
        upperLeftText_, display.xAbsolute(xOffsetForArrows + arrowWidth + arrowTextSpacing),
        display.yAbsolute(yOffsetForArrows - display.getTextHeight() / 2));

    display.drawSingleLineOfText(
        lowerRightText_,
        display.xAbsolute(display.xAbsoluteMax() -
                          (xOffsetForArrows + arrowWidth + arrowTextSpacing +
                           display.getTextWidth(lowerRightText_))),
        display.yAbsolute(display.yAbsoluteMax() -
                          (yOffsetForArrows + display.getTextHeight() / 2)));
}

void MenuScreen::addMenuItem(const MenuItem &newItem) {
    menuItems_.push_back(newItem);
    numMenuItems_++;
}

void MenuScreen::resetMenuItems() { numMenuItems_ = 0; }

auto MenuScreen::getNumItems() -> int { return numMenuItems_; }

void MenuScreen::setCurrSelection(int newSelection) { currSelection_ = newSelection; }

auto MenuScreen::getCurrSelection() -> int { return currSelection_; }

auto MenuScreen::getCurrItem() -> MenuItem { return menuItems_[currSelection_]; }

void MenuScreen::nextSelection() {
    int oldCurMenuPage = currMenuPage_;
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    currMenuPage_ = currSelection_ / NUM_LINES_PER_MENU_PAGE;
    // This makes sure we do a full refresh when the page changes
    if (oldCurMenuPage != currMenuPage_) {
        log_d("Page changed, refreshing");
        firstLoad_ = true;
        needsRerender = true;
    }
}

void MenuScreen::prevSelection() {
    int oldCurMenuPage = currMenuPage_;
    // Wrap back around to the last item
    currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
    currMenuPage_ = currSelection_ / NUM_LINES_PER_MENU_PAGE;
    // This makes sure we do a full refresh when the page changes
    if (oldCurMenuPage != currMenuPage_) {
        firstLoad_ = true;
        needsRerender = true;
    }
}

// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
auto MenuScreen::getItemTitleAt(int index) -> std::string {
    std::string all;
    assert(index < numMenuItems_);
    // Add the indicator for where we are in the menu based on the current selection
    if (index == currSelection_) {
        all = " ";
    } else {
        all = " ";
    }
    all = all + " " + menuItems_[index].title;
    return all;
}

auto MenuScreen::getItemAt(int index) -> MenuItem {
    assert(index < numMenuItems_);
    return menuItems_[index];
}

void MenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    log_i("Menuscreen render (selected = %s)", getCurrItem().title.c_str());

    auto batLife = power_.getBatteryLifeString();
    // TODO: Just rendering one gigantic screen limits our layout options here.
    // For example, it would be nice to have menu items take up multiple lines (e.g. Pocket articles
    // with long names)
    // std::string totalToDisplay = menuString + std::string("\n\n") + batLife;

    display.setFont(display.style.getFontFamily().settingsFont);

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        drawArrowsAndSelections(display);
        for (int i = currMenuPage_ * NUM_LINES_PER_MENU_PAGE;
             i < currMenuPage_ * NUM_LINES_PER_MENU_PAGE + NUM_LINES_PER_MENU_PAGE &&
             i < numMenuItems_;
             i++) {
            display.drawSingleLineOfText(getItemTitleAt(i), 30,
                                         (i % NUM_LINES_PER_MENU_PAGE) * display.getTextHeight() +
                                             30);
        }

        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }
    // Starting the process of doing this better eventually. Mimicking the lighting screen.
    // (But without the fast).
    // display.updateDisplay(firstLoad_ ? DisplaySystem::UpdateType::FULL
    //                                 : DisplaySystem::UpdateType::FAST);

    //
    // Duplicates the code above but only on the first load
    // TODO: This is a hack. We should be able to do this in a single pass??
    //
    // change the vcom to reduce ghosting
#if EINKET013TT1
    int oldVcom = (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->getVcom();
    int newVcom = 0;
    if (display.getInversion()) {
        newVcom = oldVcom - 2000;
    } else {
        newVcom = oldVcom + 2000;
    }
    (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->setVcom(newVcom);
#endif

    for (int i = currMenuPage_ * NUM_LINES_PER_MENU_PAGE;
         i < currMenuPage_ * NUM_LINES_PER_MENU_PAGE + NUM_LINES_PER_MENU_PAGE && i < numMenuItems_;
         i++) {
        display.drawSingleLineOfText(getItemTitleAt(i), 30,
                                     (i % NUM_LINES_PER_MENU_PAGE) * display.getTextHeight() + 50);

        if (i == currSelection_) {
            display.fillRoundRect(
                2 + 20, (i % NUM_LINES_PER_MENU_PAGE) * display.getTextHeight() + 2 + 50,
                display.getTextHeight() - 3, display.getTextHeight() - 3, 3, display.fg);
        }
    }
    drawArrowsAndSelections(display);
    display.updateDisplay(DisplaySystem::UpdateType::FAST);

#if EINKET013TT1
    (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->setVcom(oldVcom);
#endif
    firstLoad_ = false;
    needsRerender = false;
}

void MenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        prevSelection();
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        nextSelection();
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
    default:
        break;
    }
}
