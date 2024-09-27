#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "UI/Components/CarouselTitleLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/Screen.hpp"

using MenuItemVector = std::vector<std::shared_ptr<NewMenuItem>>;
using MenuItemSharedPtrVector = std::shared_ptr<MenuItemVector>;
using MenuItemFactory = std::function<MenuItemSharedPtrVector()>;
using CarouselMenuEntry = std::tuple<std::string, MenuItemFactory>;

// An abstract base class for a carousel menu screen. It handles the
// adding of tabs and the selection of items but the subclasses must
// implement the layout and draw methods.
class CarouselMenuScreen : public Screen, public EinkView {
public:
    CarouselMenuScreen(DisplaySystem &display);

    // Add a tab to the carousel menu. The tab name is the text that will be displayed on top in
    // angle brackets. The second argument is a lambda that is a menu factory that will create the
    // menu items for that tab when it is selected.
    void addTab(const std::string &tabName, const MenuItemFactory &menuItemFactory);

    void populateMenuView();

    void processCommand(RemoteCommand command) override;

    void setCurrentTabIndex(int index);

    auto getCurrentTabIndex() const -> int { return currentTabIndex_; }

    void setTabToTitle(const std::string &tabTitle);

    auto getNumTabs() const -> int { return menuTabs_.size(); }

    auto layout(LayoutContext &context, Size size) -> Size override = 0;
    void drawInRect(Rect rect) override = 0;
    void render(DisplaySystem &display) override = 0;

    void regenerateCurrentTab();

    auto getCurrentTabTitle() const -> std::string {
        return std::get<0>(menuTabs_[currentTabIndex_]);
    }

protected:
    CarouselTitleLabelView titleView_;
    NewMenuView menuView_;

private:
    std::vector<CarouselMenuEntry> menuTabs_;
    int currentTabIndex_{0};
    void moveSelection(int delta);
};
