#include "CarouselMenuScreen.hpp"

#include "Misc/Platform.hpp"

CarouselMenuScreen::CarouselMenuScreen(DisplaySystem &display)
    : EinkView(display), titleView_(display, ""), menuView_(display) {}

void CarouselMenuScreen::addTab(const std::string &tabName, const MenuItemFactory &menuItemFactory,
                                int itemsPerPage) {
    menuTabs_.emplace_back(tabName, menuItemFactory, itemsPerPage, "");
}

void CarouselMenuScreen::setTabWithName(const std::string &tabName, int rowIndex) {
    for (int i = 0; i < menuTabs_.size(); i++) {
        if (menuTabs_[i].getTabName() == tabName) {
            setCurrentTabIndex(i);
            if (rowIndex != -1) {
                menuTabs_[i].setSelectedItemId(menuView_.getIDfromRowIndex(rowIndex));
                menuView_.setSelectedByMenuItemID(menuTabs_[i].getSelectedItemId());
            }
            return;
        }
    }
}

void CarouselMenuScreen::setTabWithName(const std::string &tabName, const std::string &id) {
    for (int i = 0; i < menuTabs_.size(); i++) {
        if (menuTabs_[i].getTabName() == tabName) {
            setCurrentTabIndex(i);
            menuTabs_[i].setSelectedItemId(id);
            menuView_.setSelectedByMenuItemID(menuTabs_[i].getSelectedItemId());
            return;
        }
    }
}

auto CarouselMenuScreen::isMenuIDInTab(const std::string &tabName, const std::string &id) -> bool {
    for (auto &menuTab : menuTabs_) {
        if (menuTab.getTabName() == tabName) {
            return menuView_.isIDInMenu(id);
        }
    }
    return false;
}

void CarouselMenuScreen::regenerateCurrentTab() { setCurrentTabIndex(getCurrentTabIndex()); }

// Note, MainMenuScreen and PopupMenuScreen rely on the fact that this will populate
// even if the passed in index is the same as the currentTab.
void CarouselMenuScreen::setCurrentTabIndex(int index) {
    currentTabIndex_ = index;
    populateMenuView();
    // The current tab can become empty if the user deletes the tutorial readable and there
    // are no other items in the tab. This really only happens on the Bible tab if there
    // are no bible readables but the tutorial readable is still there.
    if (menuView_.getItemCount() == 0) {
        // delete the current tab
        menuTabs_.erase(menuTabs_.begin() + currentTabIndex_);
        // this will set the current tab to the next tab from what it was previously.
        populateMenuView();
    }
}

void CarouselMenuScreen::populateMenuView() {
    menuView_.resetMenuItems();
    auto &currentTab = menuTabs_[currentTabIndex_];

    auto menuItems = currentTab.getMenuFactory()();
    menuView_.itemsPerPage = currentTab.getMaxItemsPerPage();

    for (auto &i : *menuItems) {
        menuView_.addMenuItem(i);
    }

    menuView_.setSelectedByMenuItemID(currentTab.getSelectedItemId());
    titleView_.setText(currentTab.getTabName());
    firstLoad_ = true;
    setNeedsRerender();
}

void CarouselMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        setCurrentTabIndex((currentTabIndex_ - 1 + menuTabs_.size()) % menuTabs_.size());
        break;

    case RemoteCommand::RIGHT:
        setCurrentTabIndex((currentTabIndex_ + 1 + menuTabs_.size()) % menuTabs_.size());
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

void CarouselMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    menuTabs_[currentTabIndex_].setSelectedItemId(menuView_.getSelectedMenuItemID());
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}
