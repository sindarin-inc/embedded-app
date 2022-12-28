#pragma once

#include "EinkView.hpp"

// Absurdly simple view that just draws a box
class BoxView : public EinkView {
public:
    BoxView(DisplaySystem &display) : EinkView(display) {}
    ~BoxView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                          display_.fg);
    }
};