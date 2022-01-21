#include "UI/MenuScreen.hpp"

#include <utility>

#define NUM_LINES_PER_MENU_PAGE 8

void MenuScreen::addMenuItem(const MenuItem &newItem) {
    menuItems_[numMenuItems_] = newItem;
    numMenuItems_++;
}

void MenuScreen::resetMenuItems() { numMenuItems_ = 0; }

auto MenuScreen::getNumItems() -> int { return numMenuItems_; }

void MenuScreen::setCurrSelection(int newSelection) { currSelection_ = newSelection; }

auto MenuScreen::getCurrSelection() -> int { return currSelection_; }

auto MenuScreen::getCurrItem() -> MenuItem { return menuItems_[currSelection_]; }

// Returns 1 if the selection was updated, 0 otherwise
auto MenuScreen::nextSelection() -> int {
    int oldSel = currSelection_;
    currSelection_++;
    // Don't go further than the last item in the menu
    if (currSelection_ >= numMenuItems_) {
        currSelection_ = numMenuItems_ - 1;
    }
    auto diff = currSelection_ - oldSel;
    if (diff) {
        // Hit a page boundary -- update the page!
        if (getCurrSelection() % NUM_LINES_PER_MENU_PAGE == 0) {
            curMenuPage_++;
        }
    }
    return diff;
}

// Returns 1 if the selection was updated, 0 otherwise
auto MenuScreen::prevSelection() -> int {
    int oldSel = currSelection_;
    currSelection_--;
    // Don't go before the first menu item
    if (currSelection_ < 0) {
        currSelection_ = 0;
    }
    auto diff = oldSel - currSelection_;
    if (diff) {
        // Hit a page boundary -- update the page!
        if ((getCurrSelection() % NUM_LINES_PER_MENU_PAGE) == NUM_LINES_PER_MENU_PAGE - 1) {
            curMenuPage_--;
        }
    };
    return diff;
}

// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
auto MenuScreen::getItemTitleAt(int index) -> String {
    String all;
    assert(index < numMenuItems_);
    // Add the indicator for where we are in the menu based on the current selection
    if (index == currSelection_) {
        all = " ->";
    } else {
        all = "   ";
    }
    all = all + " " + menuItems_[index].title;
    return all;
}

auto MenuScreen::getItemAt(int index) -> MenuItem {
    assert(index < numMenuItems_);
    return menuItems_[index];
}

auto MenuScreen::getWholeMenuAsString() -> String {
    return getMenuRangeAsString(0, numMenuItems_ - 1);
}

auto MenuScreen::getMenuRangeAsString(int startItem, int endItem) -> String {
    int i;
    String complete = "";
    // If the client gives us an end_item past the end of the range, just go to the
    // end only
    for (i = startItem - 1; i < endItem && i < numMenuItems_; i++) {
        complete = complete + getItemTitleAt(i);
        complete = complete + "\n";
    }
    return complete;
}

auto MenuScreen::getPaginatedMenuString() -> String {
    String menuString = getMenuRangeAsString(curMenuPage_ * NUM_LINES_PER_MENU_PAGE + 1,
                                             curMenuPage_ * NUM_LINES_PER_MENU_PAGE + 5);
    return menuString;
}

void MenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    log_i("Menuscreen render (selected = %s)", getCurrItem().title.c_str());
    String menuString = getPaginatedMenuString();
    String batLife = power_.getBatteryLifeString();
    String totalToDisplay = menuString + String("\n\n") + batLife;
    display.drawTextInBox(totalToDisplay, display.xMin + display.localXOffset,
                          display.yMin + display.localYOffset, display.xMax + display.localXOffset,
                          display.yMax + display.localYOffset);
    needsRerender = false;
}

void MenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        needsRerender = prevSelection();
        break;
    case RemoteCommand::NEXT:
        needsRerender = nextSelection();
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
            backCallback();
        }
        break;
    default:
        break;
    }
}
