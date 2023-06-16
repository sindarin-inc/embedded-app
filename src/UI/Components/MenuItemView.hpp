#pragma once

#include <optional>

#include "EinkView.hpp"
#include "Misc/Style.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/MenuScreen.hpp"

class MenuItemView : public EinkView {
public:
    // Always use the smallest font for subtitles
    MenuItemView(DisplaySystem &display, const MenuItem &menuItem,
                 Font *font = static_cast<Font *>(&fontTahomaRegularFonT5PT7B),
                 bool multiLine = false, bool subtitle = true)
        : EinkView(display), indicator_(display, 4) {
        if (multiLine) {
            textMultiView_ =
                std::make_unique<MultiLineTextView>(display, menuItem.title, 2, 0, font);
        } else {
            textView_ = std::make_unique<SingleLineTextView>(display, menuItem.title, font);
        }
        if (!menuItem.subtitle.empty() && subtitle) {
            subtextView_ = std::make_unique<SingleLineTextView>(
                display, menuItem.subtitle, static_cast<Font *>(&fontTahomaRegularFonT5PT7B), true);
        }
        log_i("initializing menu item view with title: %s", menuItem.title.c_str());
    }
    ~MenuItemView() override = default;

    // Whether or not the indicator should be a circle or the full height of the text (plus
    // indicatorExtraVerticalHeight_)
    void setCircleIndicator(bool circle) { circleIndicator_ = circle; }

    void setRightText(const std::string &rightText) {
        if (rightTextView_ == nullptr) {
            rightTextView_ = std::make_unique<SingleLineTextView>(
                display_, rightText, static_cast<Font *>(&fontTahomaRegularFonT5PT7B), true);
        } else {
            rightTextView_->setText(rightText);
        }
        setNeedsLayout();
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        int x = 0;
        int y = 0;
        int right = size.width;
        int indicatorRightPadding = 15;
        int indicatorWidth = 5;
        auto indicatorWidthPlus = indicatorWidth + 8;
        Rect textRect, subtextRect;

        // Layout the right text view if necessary
        if (rightTextView_ != nullptr && !rightTextView_->isHidden()) {
            auto rightTextRect =
                context.setViewFrame(*rightTextView_, Rect::fromSize(size),
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                                         LayoutOptionsAlignRight);
            // Use a column of at least 24px wide, but larger if necessary
            right = size.width - std::max(24, rightTextRect.size.width);
        }

        // Layout the main text item
        if (textMultiView_ != nullptr) {
            textRect = context.setViewFrame(
                *textMultiView_,
                {{indicatorWidthPlus, y}, {right - x - indicatorWidthPlus, size.height}},
                LayoutOptionsSizeToFitVertical);
        } else {
            textRect = context.setViewFrame(
                *textView_,
                {{indicatorWidthPlus, y}, {right - x - indicatorWidthPlus, size.height}},
                LayoutOptionsSizeToFitVertical);
        }

        y += textRect.size.height;

        // Layout the subtext if necessary
        if (subtextView_ != nullptr && !subtextView_->isHidden()) {
            // Tiny bit of padding in-between
            y += 2;
            subtextRect = context.setViewFrame(
                *subtextView_,
                {{indicatorWidthPlus + indicatorRightPadding, y},
                 {right - x - indicatorWidthPlus - indicatorRightPadding, size.height}},
                LayoutOptionsSizeToFitVertical);
            y += subtextRect.size.height;
        }

        // Layout the indicator
        int indicatorHeight;
        int indicatorYNudge = 0;
        if (circleIndicator_) {
            indicatorHeight = indicatorWidth;
            indicatorYNudge = 1;
        } else {
            // Extends above and below the size by indicatorExtraVerticalHeight_
            indicatorHeight = size.height + 2 * indicatorExtraVerticalHeight_;
            // Ben says nudge the indicator down 2px but only when there is subtext
            if (subtextView_ != nullptr) {
                indicatorYNudge = 2;
            }
        }
        context.setViewSizeInRect(indicator_, {indicatorWidth, indicatorHeight},
                                  Rect{{x, indicatorYNudge}, {indicatorWidth, size.height}},
                                  LayoutOptionsAlignCenterVertical);

        return {size.width, y};
    }

    void drawInRect(Rect rect) override {
        if (selected_) {
            indicator_.fillColor = display_.fg;
            indicator_.strokeColor.reset();
        } else {
            indicator_.fillColor.reset();
            indicator_.strokeColor.reset();
        }

        indicator_.draw();
        if (textMultiView_ != nullptr) {
            textMultiView_->draw();
        } else {
            textView_->draw();
        }
        if (subtextView_ != nullptr && !subtextView_->isHidden()) {
            subtextView_->draw();
        }
        if (rightTextView_ != nullptr && !rightTextView_->isHidden()) {
            rightTextView_->draw();
        }
    }

    void setSelected(bool selected) {
        selected_ = selected;
        setNeedsLayout();
    }

protected:
    std::unique_ptr<SingleLineTextView> textView_;
    std::unique_ptr<MultiLineTextView> textMultiView_;
    std::unique_ptr<SingleLineTextView> subtextView_;
    std::unique_ptr<SingleLineTextView> rightTextView_;
    BoxView indicator_;
    // Extra height on top and bottom of the indicator relative to the size to fit the text.
    // By default, the tall indicator extends 3px above and below the size that fits
    // Unsused if circleIndicator_ is true
    int indicatorExtraVerticalHeight_ = 3;
    bool circleIndicator_ = false;
    bool selected_ = false;
};
