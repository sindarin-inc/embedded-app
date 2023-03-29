#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "Misc/MenuStyle.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Bitmaps/Battery_Bolt.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class MainMenuScreen : public MenuScreen, public EinkView {
public:
    MainMenuScreen(Power &power, DisplaySystem &display, MenuStyleFamily myStyle)
        : MenuScreen(power, myStyle), EinkView(display), batteryTextView_(display, ""),
          topBarSelectionView_(display, 3),
          batteryIconView_(display, boltBits, bolt_width, bolt_height), clockView_(display, ""){};
    MainMenuScreen(MainMenuScreen const &) = delete;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void addSubMenuScreen(MenuScreen *newSubScreen);

protected:
    void processCommand(RemoteCommand command) override;

    std::vector<MenuScreen *> subMenus_;

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    std::vector<std::shared_ptr<BoxView>> menuPageViews_;
    std::vector<std::shared_ptr<SingleLineTextView>> topBarViews_;
    SingleLineTextView batteryTextView_;
    BoxView topBarSelectionView_;
    IconView batteryIconView_;
    SingleLineTextView clockView_;

    int currentSelection_ = 0;
};

#endif // MAIN_MENU_SCREEN_H