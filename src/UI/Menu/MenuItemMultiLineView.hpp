#pragma once

#include "MenuItemMultiLine.hpp"
#include "MenuItemView.hpp"
#include "UI/Components/MultiLineTextView.hpp"

class MenuItemMultiLineView : public MenuItemView {
public:
    MenuItemMultiLineView(DisplaySystem &display,
                          const std::shared_ptr<MenuItemMultiLine> &menuItem)
        : MenuItemView(display), textView_(display, menuItem->maxLines()) {
        textView_.setText(menuItem->text());
    }

    auto layout(solui::LayoutContext &context, solui::Size size) -> solui::Size override {
        auto textRect = context.setViewFrame(textView_, solui::Rect::fromSize(size),
                                             solui::LayoutOptionsSizeToFitVertical);

        return textRect.size;
    }

    void drawInRect(solui::Rect rect) override { textView_.draw(); }

protected:
    MultiLineTextView textView_;
};
