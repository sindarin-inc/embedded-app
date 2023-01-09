#pragma once

#include <optional>

#include "EinkView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/MenuScreen.hpp"

class MenuItemView : public EinkView {
public:
    MenuItemView(DisplaySystem &display, MenuItem &menuItem)
        : EinkView(display), textView_(display, std::move(menuItem.title)), indicator_(display, 4),
          menuItem_(menuItem) {}
    ~MenuItemView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        int x = 0;
        int y = 0;
        int indicatorWidth = 5;

        auto indicatorWidthPlus = indicatorWidth + 8;
        auto indicatorHeightPadding = 4;
        auto textRect = context.setViewFrame(
            textView_, {{indicatorWidthPlus, y}, {size.width - indicatorWidthPlus, size.height}},
            LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        auto indicatorRect = context.setViewFrame(
            indicator_, {{x, y - indicatorHeightPadding},
                         {indicatorWidth, textRect.size.height + 2 * indicatorHeightPadding}});

        y += textRect.size.height;
        log_d("MenuItemView layout: %d x %d (%d vs %d)", size.width, y, textRect.size.height,
              indicatorRect.size.height);
        return {size.width, y};
    }

    void drawInRect(Rect rect) override {
        log_d("Drawing menu item: %s %d", menuItem_.title.c_str(), selected_);

        if (selected_) {
            indicator_.fillColor = display_.fg;
            indicator_.strokeColor.reset();
        } else {
            indicator_.fillColor.reset();
            indicator_.strokeColor.reset();
        }

        indicator_.draw();
        textView_.draw();
    }

    void setSelected(bool selected) {
        selected_ = selected;
        setNeedsLayout();
    }

protected:
    SingleLineTextView textView_;
    BoxView indicator_;
    MenuItem &menuItem_;
    bool selected_ = false;
};
