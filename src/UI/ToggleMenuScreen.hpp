#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ToggleMenuScreen : public MenuScreen {

    // constructor to create a toggle menu screen
public:
    ToggleMenuScreen(Battery &battery, DisplaySystem &display, MenuStyleFamily myStyle,
                     std::string menuName = "Default")
        : MenuScreen(battery, display, myStyle, std::move(menuName)), headerTextView_(display, ""),
          onTextView_(display, "ON"), offTextView_(display, "OFF"),
          selectedSelectionView_(nullptr){};

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void requireRestart();

    auto getRequireRestart() -> bool { return restartOnBack_; }

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void setToggleValue(int index, bool value) { menuToggleValues_[index] = value; }

    void toggleToggleValue(int index) { menuToggleValues_[index] = !menuToggleValues_[index]; }

    void setToggleLastItem(bool value, bool showToggle = true) {
        menuToggleValues_.push_back(value);
        showToggleBubble_.push_back(showToggle);
    }

protected:
    SingleLineTextView headerTextView_;
    SingleLineTextView onTextView_;
    SingleLineTextView offTextView_;
    std::vector<std::shared_ptr<SingleLineTextView>> menuItemViews_;
    std::vector<std::shared_ptr<BoxView>> menuPageViews_;
    std::shared_ptr<BoxView> selectedSelectionView_;
    std::vector<bool> menuToggleValues_;
    std::vector<bool> showToggleBubble_;
    std::vector<std::shared_ptr<BoxView>> menuToggleSelectionViews_;
    bool drawBubbles_ = false;

    bool restartOnBack_ = false;
};