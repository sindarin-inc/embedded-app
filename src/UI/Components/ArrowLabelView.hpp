#pragma once

#include "ArrowView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Layout/View.hpp"

enum class ArrowSide {
    Left,
    Right,
};

class ArrowLabelView : public View {
public:
    ArrowLabelView(DisplaySystem &display, const std::string &text,
                   ArrowDirection direction = ArrowLeft, ArrowSide arrowSide = ArrowSide::Left)
        : arrowView_(display, direction), textView_(display, text), arrowSide_(arrowSide) {}

    auto layout(LayoutContext &context, Size size) -> Size override {
        int arrowHeight;

        if (arrowView_.direction == ArrowLeft || arrowView_.direction == ArrowRight) {
            arrowHeight = textView_.getFont()->lineHeight() - 3;
        } else {
            arrowHeight = textView_.getFont()->lineHeight() / 2 - 1;
        }
        auto arrowHorizontal =
            arrowView_.direction == ArrowLeft || arrowView_.direction == ArrowRight;

        if (arrowSide_ == ArrowSide::Left) {
            // On our 256x256 screen the text bounding box is even and the arrow size is odd and it
            // looks a little better to nudge the arrow down by 1px
            auto arrowRect = context.setViewSizeInRect(
                arrowView_, {0, arrowHeight}, {{0, 1}, {size.width, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);
            auto padding = arrowHorizontal ? arrowRect.size.width : arrowRect.size.height;
            auto backTextRect =
                context.setViewFrame(textView_, {{arrowRect.right() + padding, 0}, size},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);

            return {backTextRect.right(), std::max(arrowHeight, backTextRect.size.height)};
        } else {
            auto arrowRect = context.setViewSizeInRect(
                arrowView_, {0, arrowHeight}, {{0, 1}, {size.width, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                    LayoutOptionsAlignRight);
            auto padding = arrowHorizontal ? arrowRect.size.width : arrowRect.size.height;
            auto backTextRect =
                context.setViewFrame(textView_, {{0, 0}, {arrowRect.left() - padding, size.height}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                                         LayoutOptionsAlignRight);
            return {size.width - backTextRect.left(),
                    std::max(arrowHeight, backTextRect.size.height)};
        }
    }

    void drawInRect(Rect rect) override {
        arrowView_.draw();
        textView_.draw();
    }

    void setText(const std::string &text) {
        textView_.setText(text);
        setNeedsLayout();
    }

    void setArrowDirection(ArrowDirection direction) {
        arrowView_.direction = direction;
        setNeedsLayout();
    }

private:
    ArrowView arrowView_;
    SingleLineTextView textView_;
    ArrowSide arrowSide_;
};
