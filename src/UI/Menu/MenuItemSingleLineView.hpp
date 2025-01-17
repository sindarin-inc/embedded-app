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

    auto layout(solui::LayoutContext &context, solui::Size size) -> solui::Size override {
        auto titleRect = context.setViewFrame(titleView_, solui::Rect::fromSize(size),
                                              solui::LayoutOptionsSizeToFit);
        return titleRect.size;
    }

    void drawInRect(solui::Rect rect) override { titleView_.draw(); }

protected:
    SingleLineTextView titleView_;
};
