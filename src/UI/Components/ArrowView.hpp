#pragma once

#include <utility>

#include "EinkView.hpp"
#include "UI/Layout/View.hpp"

enum ArrowDirection { ArrowLeft, ArrowRight, ArrowUp, ArrowDown };

// A view that draws an arrow
class ArrowView : public EinkView {
public:
    ArrowView(DisplaySystem &display, ArrowDirection direction = ArrowLeft)
        : EinkView(display), arrowDirection(direction), color(display.fg) {}
    ~ArrowView() override = default;

    ArrowDirection arrowDirection = ArrowLeft;
    uint16_t color;

    auto layout(LayoutContext &context, Size size) -> Size override { return size; }

    void drawInRect(Rect rect) override {
        // have to set it again here in case the color changed since the the constructor.
        color = display_.fg;

        switch (arrowDirection) {
        // All the minus ones here are because the width + origin is 1 past the end.
        case ArrowLeft:
            // Draw Left Arrow with the following two drawLine calls
            display_.drawLine(rect.size.width + rect.origin.x - 1, rect.origin.y, rect.origin.x,
                              (rect.size.height + 1) / 2 + rect.origin.y - 1, color);
            display_.drawLine(rect.origin.x, (rect.size.height + 1) / 2 + rect.origin.y - 1,
                              rect.origin.x + rect.size.width - 1,
                              rect.origin.y + rect.size.height - 1, color);
            break;
        case ArrowRight:
            /* Draw Right arrow with the following two drawLine calls*/
            display_.drawLine(rect.origin.x, rect.origin.y, rect.size.width + rect.origin.x - 1,
                              (rect.size.height + 1) / 2 + rect.origin.y - 1, color);
            display_.drawLine(rect.size.width + rect.origin.x - 1,
                              (rect.size.height + 1) / 2 + rect.origin.y - 1, rect.origin.x,
                              rect.size.height + rect.origin.y - 1, color);
            break;
        case ArrowUp:
            /* Draw Up arrow with the following two drawLine calls*/
            display_.drawLine(rect.origin.x, rect.size.height + rect.origin.y - 1,
                              (rect.size.width + 1) / 2 + rect.origin.x - 1, rect.origin.y, color);
            display_.drawLine((rect.size.width + 1) / 2 + rect.origin.x - 1, rect.origin.y,
                              rect.size.width + rect.origin.x - 1,
                              rect.size.height + rect.origin.y - 1, color);
            break;
        case ArrowDown:
            /* Draw Down arrow with the following two drawLine calls*/
            display_.drawLine(rect.origin.x, rect.origin.y,
                              (rect.size.width + 1) / 2 + rect.origin.x - 1,
                              rect.size.height + rect.origin.y - 1, color);
            display_.drawLine((rect.size.width + 1) / 2 + rect.origin.x - 1,
                              rect.size.height + rect.origin.y - 1,
                              rect.size.width + rect.origin.x - 1, rect.origin.y, color);
            break;
        default:
            break;
        }
    }
};
