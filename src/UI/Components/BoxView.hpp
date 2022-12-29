#pragma once

#include <optional>

#include "EinkView.hpp"

// Absurdly simple view that just draws a box
class BoxView : public EinkView {
public:
    BoxView(DisplaySystem &display) : EinkView(display), strokeColor(display.fg) {}
    ~BoxView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        if (fillColor.has_value()) {
            display_.fillRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                              fillColor.value());
        }
        if (strokeColor.has_value()) {
            display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                              strokeColor.value());
        }
    }

    std::optional<uint16_t> fillColor;
    std::optional<uint16_t> strokeColor;
};
