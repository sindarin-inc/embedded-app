#include "CarouselMenuScreen.hpp"

CarouselMenuScreen::CarouselMenuScreen(DisplaySystem &display)
    : EinkView(display), titleView_(display, ""), menuView_(display) {}

void CarouselMenuScreen::addTab(const std::string &tabName,
                                const MenuItemFactory &menuItemFactory) {
    menuTabs_.emplace_back(tabName, menuItemFactory);
}

void CarouselMenuScreen::setCurrentTabIndex(int index) { currentTabIndex_ = index; }

void CarouselMenuScreen::populateMenuView() {
    menuView_.resetMenuItems();
    currentTabTitle_ = std::get<0>(menuTabs_[currentTabIndex_]);
    auto &currentTabFactory = std::get<1>(menuTabs_[currentTabIndex_]);

    std::vector<std::shared_ptr<NewMenuItem>> menuItems = currentTabFactory();
    for (const auto &menuitem : menuItems) {
        menuView_.addMenuItem(menuitem);
    }

    titleView_.setText(currentTabTitle_);
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
