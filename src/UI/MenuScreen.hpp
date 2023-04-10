#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/PowerManager.hpp"
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
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class MenuScreen : public Screen, public Menu, public EinkView {

public:
    MenuScreen(Battery &battery, DisplaySystem &display, MenuStyleFamily myStyle,
               std::string menuName = "Default")
        : Menu(myStyle), EinkView(display), menuName_(std::move(menuName)),
          batteryTextView_(display, ""),
          batteryIconView_(display, boltBits, bolt_width, bolt_height), clockView_(display, ""),
          battery_(battery), clockBatteryView_(battery, display),
          upperLeftTextView_(display, upperLeftText_) {
        numMenuItems_ = 0;
    };

    MenuScreen(MenuScreen const &) = delete;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void setFirstLoad() { firstLoad_ = true; }

    void setExtraText(std::string extra) { extraTextBelowMenu_ = std::move(extra); }

    void drawMenuItems(DisplaySystem &display, bool withBullet, bool withLastItem = true);

    void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                              int duration = 2000, bool rerenderAfter = true);

    void populateMenuItems(std::vector<MenuItem> &menuItems);

    auto getMenuName() -> std::string { return menuName_; }

    auto getFirstLoad() -> bool { return firstLoad_; }

protected:
    void drawArrowsAndSelections(DisplaySystem &display);
    bool firstLoad_ = true;

    std::string upperLeftText_ = "Main Menu";
    std::string lowerRightText_ = "Select";

    std::string extraTextBelowMenu_ = "";

    std::string menuName_ = "Default";

    SingleLineTextView batteryTextView_;
    IconView batteryIconView_;
    SingleLineTextView clockView_;
    Battery &battery_;

    ClockBatteryView clockBatteryView_;

    SingleLineTextView upperLeftTextView_;
};
