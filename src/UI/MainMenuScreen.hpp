#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "Misc/Battery.hpp"
#include "Misc/MenuStyle.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Bitmaps/Battery_Bolt.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/ClockBatteryView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"
class MainMenuScreen : public MenuScreen {
public:
    MainMenuScreen(Battery &battery, DisplaySystem &display, MenuStyleFamily myStyle)
        : MenuScreen(battery, display, myStyle), topBarSelectionView_(display, 3){};
    MainMenuScreen(MainMenuScreen const &) = delete;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void addSubMenu(Menu *newSubMenu);

    void setTabToLast() { currentSelection_ = subMenus_.size() - 1; }

protected:
    void processCommand(RemoteCommand command) override;

    std::vector<Menu *> subMenus_;

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    std::vector<std::shared_ptr<BoxView>> menuPageViews_;
    std::vector<std::shared_ptr<SingleLineTextView>> topBarViews_;
    BoxView topBarSelectionView_;

    int currentSelection_ = 0;
};

#endif // MAIN_MENU_SCREEN_H