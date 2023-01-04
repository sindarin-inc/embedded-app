#pragma once

#include <optional>

#include "EinkView.hpp"

class BoxView : public EinkView {
public:
    BoxView(DisplaySystem &display, int cornerRadius = 0)
        : EinkView(display), strokeColor(display.fg), cornerRadius(cornerRadius) {}
    ~BoxView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        if (fillColor.has_value()) {
            display_.fillRoundRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                                   cornerRadius, fillColor.value());
        }
        if (strokeColor.has_value()) {
            display_.drawRoundRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                                   cornerRadius, strokeColor.value());
        }
    }

    std::optional<uint16_t> fillColor;
    std::optional<uint16_t> strokeColor;
    int cornerRadius;
};
