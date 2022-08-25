#include "UI/MenuScreen.hpp"

#include <utility>

#include "Misc/Style.hpp"

#define NUM_LINES_PER_MENU_PAGE 12

void MenuScreen::addMenuItem(const MenuItem &newItem) {
    menuItems_.push_back(newItem);
    numMenuItems_++;
}

void MenuScreen::resetMenuItems() { numMenuItems_ = 0; }

auto MenuScreen::getNumItems() -> int { return numMenuItems_; }

void MenuScreen::setCurrSelection(int newSelection) { currSelection_ = newSelection; }

auto MenuScreen::getCurrSelection() -> int { return currSelection_; }

auto MenuScreen::getCurrItem() -> MenuItem { return menuItems_[currSelection_]; }

// Returns 1 if the selection was updated, 0 otherwise
void MenuScreen::nextSelection() {
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    curMenuPage_ = currSelection_ / NUM_LINES_PER_MENU_PAGE;
}

// Returns 1 if the selection was updated, 0 otherwise
void MenuScreen::prevSelection() {
    // Wrap back around to the last item
    currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
    curMenuPage_ = currSelection_ / NUM_LINES_PER_MENU_PAGE;
}

// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
auto MenuScreen::getItemTitleAt(int index) -> std::string {
    std::string all;
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

    for (int i = curMenuPage_ * NUM_LINES_PER_MENU_PAGE;
         i < curMenuPage_ * NUM_LINES_PER_MENU_PAGE + NUM_LINES_PER_MENU_PAGE && i < numMenuItems_;
         i++) {
        display.drawSingleLineOfText(getItemTitleAt(i), 0,
                                     (i % NUM_LINES_PER_MENU_PAGE) * display.getTextHeight());
    }

    // Starting the process of doing this better eventually. Mimicking the lighting screen.
    // (But without the fast).
    display.updateDisplay(firstLoad_ ? DisplaySystem::UpdateType::FULL
                                     : DisplaySystem::UpdateType::FULL);
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
            backCallback();
        }
        break;
    default:
        break;
    }
}
