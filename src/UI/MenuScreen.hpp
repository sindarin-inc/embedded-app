#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class MenuScreen : public Screen, public Menu {

public:
    MenuScreen(Power &power, MenuStyleFamily myStyle) : Menu(myStyle), power_(power) {
        numMenuItems_ = 0;
    };

    MenuScreen(Power &power) : power_(power) { numMenuItems_ = 0; };

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void setFirstLoad() { firstLoad_ = true; }

    void setExtraText(std::string extra) { extraTextBelowMenu_ = std::move(extra); }

    void drawMenuItems(DisplaySystem &display, bool withBullet, bool withLastItem = true);

    void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                              int duration = 2000, bool rerenderAfter = true);

protected:
    void drawArrowsAndSelections(DisplaySystem &display);
    bool firstLoad_ = true;

    std::string upperLeftText_ = "Back";
    std::string lowerRightText_ = "Select";

    std::string extraTextBelowMenu_ = "";

    Power &power_;
};
