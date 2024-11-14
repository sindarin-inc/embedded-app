#pragma once

#include "MenuItem.hpp"
#include "UI/Components/EinkView.hpp"

// Abstract base class for a menu item view (menu row view)
class MenuItemView : public EinkView {
public:
    MenuItemView(DisplaySystem &display) : EinkView(display) {}

    // Subclasses can implement to change appearance when selected
    virtual void setSelected(bool selected) {}

    // This can be overridden to draw things that should only be drawn like the indicator is
    // (e.g., the checkmark)
    virtual void drawFastOnlyElements(){};
};
