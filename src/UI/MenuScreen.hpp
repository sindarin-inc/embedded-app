#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class MenuScreen : public Screen, public Menu {

public:
    MenuScreen(MenuStyleFamily myStyle, std::string menuName = "Default")
        : Menu(myStyle), menuName_(std::move(menuName)) {
        numMenuItems_ = 0;
    };

    MenuScreen() { numMenuItems_ = 0; };

    MenuScreen(MenuScreen const &) = delete;

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
};
