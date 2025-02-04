#pragma once

#include "ArrowView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Layout/View.hpp"

class CarouselTitleLabelView : public View {
public:
    CarouselTitleLabelView(DisplaySystem &display, const std::string &text)
        : leftArrowView_(display, ArrowLeft), rightArrowView_(display, ArrowRight),
          textView_(display, text), display_(display) {}

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        int arrowHeight;

        arrowHeight = textView_.getFont()->lineHeight() - 3;
        // This will have to be re-evaluated on other displays to make sure it looks ok, but
        // at least it's proportional.
        auto paddingBetweenArrows = display_.width() / 5;
        // Because arrowHeight is 2xArrowWidth.
        auto boxWidth = paddingBetweenArrows + arrowHeight + 1;

        context.setViewSizeInRect(
            leftArrowView_, {0, arrowHeight}, {{0, 1}, {size.width, size.height}},
            LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignLeft);

        context.setViewFrame(textView_, {{1, 0}, {size.width, size.height}},
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

        context.setViewSizeInRect(
            rightArrowView_, {0, arrowHeight}, {{0, 1}, {size.width, size.height}},
            LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight);

        return {boxWidth, arrowHeight};
    }

    void drawInRect(SolRect rect) override {
        leftArrowView_.draw();
        textView_.draw();
        rightArrowView_.draw();
    }

    void setText(const std::string &text) {
        textView_.setText(text);
        setNeedsLayout();
    }

private:
    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
    SingleLineTextView textView_;

    DisplaySystem &display_;
};
