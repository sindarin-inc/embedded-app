#pragma once

#include "NewMenuItemSingleLine.hpp"
#include "NewMenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class NewMenuItemSingleLineView : public NewMenuItemView {
public:
    NewMenuItemSingleLineView(DisplaySystem &display,
                              const std::shared_ptr<NewMenuItemSingleLine> &menuItem)
        : NewMenuItemView(display), titleView_(display) {
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
