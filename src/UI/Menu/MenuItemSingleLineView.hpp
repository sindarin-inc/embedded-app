#pragma once

#include "MenuItemSingleLine.hpp"
#include "MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class MenuItemSingleLineView : public MenuItemView {
public:
    MenuItemSingleLineView(DisplaySystem &display,
                           const std::shared_ptr<MenuItemSingleLine> &menuItem)
        : MenuItemView(display), titleView_(display) {
        titleView_.setText(menuItem->title());
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto titleRect =
            context.setViewFrame(titleView_, Rect::fromSize(size), LayoutOptionsSizeToFit);
        return titleRect.size;
    }

    void drawInRect(Rect rect) override { titleView_.draw(); }

protected:
    SingleLineTextView titleView_;
};
