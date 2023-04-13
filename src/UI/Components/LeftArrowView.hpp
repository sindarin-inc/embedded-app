#pragma once

#include <utility>

#include "EinkView.hpp"
#include "UI/Layout/View.hpp"

// A view that draws a left arrow
class LeftArrowView : public EinkView {
public:
    LeftArrowView(DisplaySystem &display) : EinkView(display) {}
    ~LeftArrowView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        display_.drawLine(rect.size.width + rect.origin.x, rect.origin.y, rect.origin.x,
                          rect.size.height / 2 + rect.origin.y, display_.fg);
        display_.drawLine(rect.origin.x, rect.size.height / 2 + rect.origin.y,
                          rect.origin.x + rect.size.width, rect.origin.y + rect.size.height,
                          display_.fg);
    }
};