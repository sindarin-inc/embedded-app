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

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        auto textRect = context.setViewFrame(textView_, SolRect::fromSize(size),
                                             LayoutOptionsSizeToFitVertical);

        return textRect.size;
    }

    void drawInRect(SolRect rect) override { textView_.draw(); }

protected:
    MultiLineTextView textView_;
};
