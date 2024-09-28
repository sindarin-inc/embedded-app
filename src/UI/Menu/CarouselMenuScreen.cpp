#include "CarouselMenuScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/MsgPopup.hpp"

CarouselMenuScreen::CarouselMenuScreen(DisplaySystem &display)
    : EinkView(display), titleView_(display, ""), menuView_(display) {}

void CarouselMenuScreen::addTab(const std::string &tabName,
                                const MenuItemFactory &menuItemFactory) {
    menuTabs_.emplace_back(tabName, menuItemFactory, 0);
}

void CarouselMenuScreen::setTabToTitle(const std::string &tabTitle) {
    for (int i = 0; i < menuTabs_.size(); i++) {
        if (std::get<0>(menuTabs_[i]) == tabTitle) {
            setCurrentTabIndex(i);
            return;
        }
    }
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
    auto &currentTabFactory = std::get<1>(menuTabs_[currentTabIndex_]);

    // Does this somehow keep extra references around to the shared_ptrs?
    // The system menu doesn't seem to be cleaning up when going to another tab.
    auto menuItems = currentTabFactory();

    for (auto &i : *menuItems) {
        menuView_.addMenuItem(i);
    }

    menuView_.setSelectedIndex(std::get<2>(menuTabs_[currentTabIndex_]));

    titleView_.setText(getCurrentTabTitle());
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
    std::get<2>(menuTabs_[currentTabIndex_]) = menuView_.getSelectedIndex();
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}
