#pragma once

#include "EinkView.hpp"

// Absurdly simple view that just draws a box
class ArrowView : public EinkView {
public:
    ArrowView(DisplaySystem &display) : EinkView(display) {}
    ~ArrowView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        // Arrow view likes to be 5x10 but is responsive to handle any size
        return {5, 10};
    }

    void drawInRect(Rect rect) override {
        auto x = rect.origin.x;
        auto y = rect.origin.y;
        display_.drawLine(x, y, x + rect.size.width, y + rect.size.height / 2, display_.fg);
        display_.drawLine(x + rect.size.width, y + rect.size.height / 2, x, y + rect.size.height,
                          display_.fg);
    }
};
