#pragma once

#include "NewMenuItemMultiLine.hpp"
#include "NewMenuItemView.hpp"
#include "UI/Components/MultiLineTextView.hpp"

class NewMenuItemMultiLineView : public NewMenuItemView {
public:
    NewMenuItemMultiLineView(DisplaySystem &display,
                             const std::shared_ptr<NewMenuItemMultiLine> &menuItem)
        : NewMenuItemView(display), textView_(display, menuItem->maxLines()) {
        textView_.setText(menuItem->text());
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto textRect =
            context.setViewFrame(textView_, Rect::fromSize(size), LayoutOptionsSizeToFitVertical);

        return textRect.size;
    }

    void drawInRect(Rect rect) override { textView_.draw(); }

protected:
    MultiLineTextView textView_;
};
