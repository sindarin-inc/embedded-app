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
#include "UI/Components/BoxView.hpp"
#include "UI/Components/ClockBatteryView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/LeftArrowView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class MenuScreen : public Screen, public Menu, public EinkView {

public:
    MenuScreen(Battery &battery, DisplaySystem &display, MenuStyleFamily myStyle,
               std::string menuName = "Default", std::string upperLeftText = "Main Menu")
        : Menu(myStyle, std::move(menuName)), EinkView(display),
          upperLeftText_(std::move(upperLeftText)), clockBatteryView_(battery, display),
          upperLeftTextView_(display, upperLeftText_), leftArrowView_(display) {
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

    auto getFirstLoad() -> bool { return firstLoad_; }

protected:
    bool firstLoad_ = true;

    std::string upperLeftText_ = "Main Menu";

    std::string extraTextBelowMenu_ = "";

    ClockBatteryView clockBatteryView_;

    SingleLineTextView upperLeftTextView_;

    LeftArrowView leftArrowView_;
};
