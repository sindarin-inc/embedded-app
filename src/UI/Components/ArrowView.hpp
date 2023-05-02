#pragma once

#include <utility>

#include "EinkView.hpp"
#include "UI/Layout/View.hpp"

enum ArrowDirection { ArrowLeft, ArrowRight, ArrowUp, ArrowDown };

// A view that draws a left arrow
class ArrowView : public EinkView {
public:
    ArrowView(DisplaySystem &display, ArrowDirection direction = ArrowLeft)
        : EinkView(display), arrowDirection_(direction), color_(display.fg) {}
    ~ArrowView() override = default;

    ArrowDirection arrowDirection_ = ArrowLeft;
    uint16_t color_;

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        switch (arrowDirection_) {
        case ArrowLeft:
            // Draw Left Arrow with the following two drawLine calls
            display_.drawLine(rect.size.width + rect.origin.x, rect.origin.y, rect.origin.x,
                              rect.size.height / 2 + rect.origin.y, color_);
            display_.drawLine(rect.origin.x, rect.size.height / 2 + rect.origin.y,
                              rect.origin.x + rect.size.width, rect.origin.y + rect.size.height,
                              color_);
            break;
        case ArrowRight:
            /* Draw Right arrow with the following two drawLine calls*/
            display_.drawLine(rect.origin.x, rect.origin.y, rect.size.width + rect.origin.x,
                              rect.size.height / 2 + rect.origin.y, color_);
            display_.drawLine(rect.size.width + rect.origin.x, rect.size.height / 2 + rect.origin.y,
                              rect.origin.x, rect.size.height + rect.origin.y, color_);
            break;
        case ArrowUp:
            /* Draw Up arrow with the following two drawLine calls*/
            display_.drawLine(rect.origin.x, rect.size.height + rect.origin.y,
                              rect.size.width / 2 + rect.origin.x, rect.origin.y, color_);
            display_.drawLine(rect.size.width / 2 + rect.origin.x, rect.origin.y,
                              rect.size.width + rect.origin.x, rect.size.height + rect.origin.y,
                              color_);
            break;
        case ArrowDown:
            /* Draw Down arrow with the following two drawLine calls*/
            display_.drawLine(rect.origin.x, rect.origin.y, rect.size.width / 2 + rect.origin.x,
                              rect.size.height + rect.origin.y, color_);
            display_.drawLine(rect.size.width / 2 + rect.origin.x, rect.size.height + rect.origin.y,
                              rect.size.width + rect.origin.x, rect.origin.y, color_);
            break;
        default:
            break;
        }
    }
};
