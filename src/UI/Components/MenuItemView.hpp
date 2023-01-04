#pragma once

#include <optional>

#include "EinkView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/MenuScreen.hpp"

class MenuItemView : public EinkView {
public:
    MenuItemView(DisplaySystem &display, std::string text, MenuItem &menuItem,
                 std::function<void()> callback = {})
        : EinkView(display), selected_(false), textView_(display, std::move(text)),
          indicator_(display, 4), menuItem_(menuItem), callback(std::move(callback)) {}
    ~MenuItemView() override = default;
    std::function<void()> callback;

    auto layout(LayoutContext &context, Size size) -> Size override {
        int x = 0;
        int y = 0;
        int indicatorWidth = 5;

        auto indicatorRect = context.setViewFrame(indicator_, {x, y, indicatorWidth, size.height});
        auto indicatorWidthPlus = indicatorRect.size.width + 8;
        auto textRect = context.setViewFrame(
            textView_, {indicatorWidthPlus, y, size.width - indicatorWidthPlus, size.height},
            LayoutOptionsSizeToFitVertical);

        y += std::max(textRect.size.height, indicatorRect.size.height) + 2;

        return {size.width, y};
    }

    void drawInRect(Rect rect) override {

        if (selected_) {
            indicator_.fillColor = display_.fg;
            indicator_.draw();
        }

        textView_.draw();
    }

    void setSelected(bool selected) {
        selected_ = selected;

        setNeedsLayout();
    }

protected:
    SingleLineTextView textView_;
    BoxView indicator_;
    MenuItem *menuItem_;
    bool selected_ = false;
};
