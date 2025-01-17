#pragma once

#include "MenuItemTOC.hpp"
#include "MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class MenuItemTOCView : public MenuItemView {
public:
    MenuItemTOCView(DisplaySystem &display, const std::shared_ptr<MenuItemTOC> &menuItem)
        : MenuItemView(display), titleView_(display), placeView_(display) {
        titleView_.setText(menuItem->title());
        placeView_.setText(menuItem->place());
    }

    auto layout(solui::LayoutContext &context, solui::Size size) -> solui::Size override {
        auto placeRect =
            context.setViewFrame(placeView_, solui::Rect::fromSize(size),
                                 solui::LayoutOptionsSizeToFit | solui::LayoutOptionsAlignRight);
        auto titleRect = context.setViewFrame(
            titleView_, solui::Rect::fromSize(size).inset(0, placeRect.size.width + 3, 0, 0),
            solui::LayoutOptionsSizeToFit);
        return {size.width, std::max(titleRect.size.height, placeRect.size.height)};
    }

    void drawInRect(solui::Rect rect) override {
        titleView_.draw();
        placeView_.draw();
    }

protected:
    SingleLineTextView titleView_;
    SingleLineTextView placeView_;
};
