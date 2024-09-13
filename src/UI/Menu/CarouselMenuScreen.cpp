#include "CarouselMenuScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/MsgPopup.hpp"

CarouselMenuScreen::CarouselMenuScreen(DisplaySystem &display)
    : EinkView(display), titleView_(display, ""), menuView_(display) {}

void CarouselMenuScreen::addTab(const std::string &tabName,
                                const MenuItemFactory &menuItemFactory) {
    menuTabs_.emplace_back(tabName, menuItemFactory);
}

void CarouselMenuScreen::setTabToTitle(const std::string &tabTitle) {
    for (int i = 0; i < menuTabs_.size(); i++) {
        if (std::get<0>(menuTabs_[i]) == tabTitle) {
            setCurrentTabIndex(i);
            return;
        }
    }
}

void CarouselMenuScreen::setCurrentTabIndex(int index) {
    currentTabIndex_ = index;
    populateMenuView();
}

void CarouselMenuScreen::populateMenuView() {
    menuView_.resetMenuItems();
    currentTabTitle_ = std::get<0>(menuTabs_[currentTabIndex_]);
    auto &currentTabFactory = std::get<1>(menuTabs_[currentTabIndex_]);

    // Does this somehow keep extra references around to the shared_ptrs?
    // The system menu doesn't seem to be cleaning up when going to another tab.
    auto menuItems = currentTabFactory();

    for (int i = 0; i < menuItems->size(); i++) {
        menuView_.addMenuItem(menuItems->at(i));
    }

    // Make sure this reference is deleted.
    menuItems = nullptr;
    titleView_.setText(currentTabTitle_);
}

void CarouselMenuScreen::showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                              int duration, bool rerenderAfter) {
    MsgPopup popup(display, message);
    popup.render(display);
    if (rerenderAfter) {
        Delay(duration);
        setNeedsRerender();
        this->render(display);
    }
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
        currentTabIndex_ = (currentTabIndex_ - 1 + menuTabs_.size()) % menuTabs_.size();
        populateMenuView();
        firstLoad_ = true;
        setNeedsRerender();
        break;

    case RemoteCommand::RIGHT:
        currentTabIndex_ = (currentTabIndex_ + 1) % menuTabs_.size();
        populateMenuView();
        firstLoad_ = true;
        setNeedsRerender();
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

void CarouselMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}
