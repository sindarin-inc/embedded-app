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

        if (dotted && fillColor.has_value() && strokeColor.has_value()) {
            display_.drawDottedBox(rect.origin.x, rect.origin.y, rect.origin.x + rect.size.width,
                                   rect.origin.y + rect.size.height, 1, 2);
            // No need to draw anything else if it's dotted.
            return;
        }

        if (fillColor.has_value()) {
            if (cornerRadius > 0) {
                display_.fillRoundRect(rect.origin.x, rect.origin.y, rect.size.width,
                                       rect.size.height, cornerRadius, fillColor.value());
            } else {
                display_.fillRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                                  fillColor.value());
            }
        }
        if (strokeColor.has_value()) {
            if (cornerRadius > 0) {
                display_.drawRoundRect(rect.origin.x, rect.origin.y, rect.size.width,
                                       rect.size.height, cornerRadius, strokeColor.value());
            } else {
                display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                                  strokeColor.value());
            }
        }
    }

    std::optional<uint16_t> fillColor;
    std::optional<uint16_t> strokeColor;
    int cornerRadius;
    bool dotted = false;
};
