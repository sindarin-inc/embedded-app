#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/PowerManager.hpp"
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
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ToggleMenuScreen : public MenuScreen, public EinkView {

    // constructor to create a toggle menu screen
public:
    ToggleMenuScreen(Battery &battery, DisplaySystem &display, MenuStyleFamily myStyle,
                     std::string menuName = "Default")
        : MenuScreen(myStyle, menuName), EinkView(display), headerTextView_(display, ""),
          onTextView_(display, "ON"), offTextView_(display, "OFF"),
          selectedSelectionView_(display, 3), batteryTextView_(display, ""),
          batteryIconView_(display, boltBits, bolt_width, bolt_height), clockView_(display, ""),
          battery_(battery){};

    auto layout(LayoutContext &context, Size size) -> Size;
    void drawInRect(Rect rect);

    void render(DisplaySystem &display);

    void processCommand(RemoteCommand command);

    void setToggleValue(int index, bool value) { menuToggleValues_[index] = value; }

    void toggleToggleValue(int index) { menuToggleValues_[index] = !menuToggleValues_[index]; }

    void setToggleLastItem(bool value) { menuToggleValues_.push_back(value); }

protected:
    void drawArrowsAndSelections(DisplaySystem &display);

    SingleLineTextView headerTextView_;
    SingleLineTextView onTextView_;
    SingleLineTextView offTextView_;
    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    std::vector<std::shared_ptr<BoxView>> menuPageViews_;
    BoxView selectedSelectionView_;
    std::vector<bool> menuToggleValues_;
    std::vector<std::shared_ptr<BoxView>> menuToggleSelectionViews_;
    SingleLineTextView batteryTextView_;
    IconView batteryIconView_;
    SingleLineTextView clockView_;

    Battery &battery_;
    bool drawBubbles_ = false;
};