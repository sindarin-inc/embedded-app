#pragma once

#include "NewMenuItem.hpp"
#include "UI/Components/EinkView.hpp"

// Abstract base class for a menu item view (menu row view)
class NewMenuItemView : public EinkView {
public:
    NewMenuItemView(DisplaySystem &display) : EinkView(display) {}

    // Subclasses can implement to change appearance when selected
    virtual void setSelected(bool selected) {}
};
