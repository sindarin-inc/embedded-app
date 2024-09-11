#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/CarouselTitleLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/Screen.hpp"

using MenuItemFactory = std::function<std::vector<std::shared_ptr<NewMenuItem>>()>;
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

    void setFirstLoad() { firstLoad_ = true; }

    auto layout(LayoutContext &context, Size size) -> Size override = 0;
    void drawInRect(Rect rect) override = 0;
    void render(DisplaySystem &display) override = 0;

protected:
    // We give access to these to the subclasses so they can style the menuView_ and layout and draw
    // the title as needed.
    std::string currentTabTitle_;
    CarouselTitleLabelView titleView_;
    NewMenuView menuView_;
    bool firstLoad_{true};

private:
    std::vector<CarouselMenuEntry> menuTabs_;
    int currentTabIndex_{0};
    void moveSelection(int delta);
};
