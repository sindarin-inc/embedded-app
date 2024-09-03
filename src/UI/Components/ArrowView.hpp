#pragma once

#include <utility>

#include "EinkView.hpp"
#include "UI/Layout/View.hpp"

enum ArrowDirection { ArrowLeft, ArrowRight, ArrowUp, ArrowDown };

#define MAKE_ODD(x) ((((x) % 2 == 1) ? (x) : ((x)-1)))

// A view that draws an arrow
class ArrowView : public EinkView {
public:
    ArrowView(DisplaySystem &display, ArrowDirection direction = ArrowLeft)
        : EinkView(display), direction(direction), color(display.fg) {}
    ~ArrowView() override = default;

    ArrowDirection direction = ArrowLeft;
    uint16_t color;

    auto layout(LayoutContext &context, Size size) -> Size override {
        bool heightConstrained;
        bool horizontal = direction == ArrowLeft || direction == ArrowRight;

        // Figure out whether we are width or height constrained

        // If size and width aren't set, use a default size but raise a warning
        if (size.width == 0 && size.height == 0) {
            log_w("ArrowView has no size set, using default size");
            return (horizontal ? Size({5, 9}) : Size({9, 5}));
        }
        // If both are set, then we are constrained based on the aspect ratio fitting in the box
        else if (size.width > 0 && size.height > 0) {
            if (horizontal) {
                heightConstrained = size.width * 2 > size.height;
            } else {
                heightConstrained = size.height * 2 > size.width;
            }
        }
        // If width is set and not height, then we are width constrained
        else if (size.width > 0 && size.height == 0) {
            heightConstrained = false;
        }
        // If height is set and not width, then we are height constrained
        else if (size.width == 0 && size.height > 0) {
            heightConstrained = true;
        } else {
            assert(false && "ArrowView logic bug");
        }

        if (horizontal) {
            if (heightConstrained) {
                auto width = MAKE_ODD((size.height + 1) / 2);
                auto height = width * 2 - 1;
                return {width, height};
            } else {
                auto width = MAKE_ODD(size.width);
                auto height = width * 2 - 1;
                return {width, height};
            }
        } else {
            if (heightConstrained) {
                auto height = MAKE_ODD(size.height);
                auto width = height * 2 - 1;
                return {width, height};
            } else {
                auto height = MAKE_ODD((size.width + 1) / 2);
                auto width = height * 2 - 1;
                return {width, height};
            }
        }
    }

    void drawInRect(Rect rect) override {
        // have to set it again here in case the color changed since the the constructor.
        color = display_.fg;

        switch (direction) {
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
