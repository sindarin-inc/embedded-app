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
    MenuItemView(DisplaySystem &display) : EinkView(display), indicator_(display, 4) {}

    //! Convenience method to initialize and then set the menuItem
    MenuItemView(DisplaySystem &display, const MenuItem &menuItem,
                 Font *font = static_cast<Font *>(&fontFace0), bool multiLine = false,
                 bool subtitle = true)
        : MenuItemView(display) {
        setMenuItem(menuItem, font, multiLine, subtitle);
    }

    ~MenuItemView() override = default;

    void setMenuItem(const MenuItem &menuItem, Font *font = static_cast<Font *>(&fontFace0),
                     bool multiLine = false, bool subtitle = true) {
        if (textView_ != nullptr) {
            textView_->setHidden(true);
        }
        if (textMultiView_ != nullptr) {
            textMultiView_->setHidden(true);
        }
        if (subtextView_ != nullptr) {
            subtextView_->setHidden(true);
        }
        if (multiLine) {
            if (textMultiView_ == nullptr) {
                textMultiView_ = std::make_unique<MultiLineTextView>(display_, 2, 0);
            }
            textMultiView_->setText(menuItem.title);
            textMultiView_->setFont(font);
            textMultiView_->setHidden(false);
        } else {
            if (textView_ == nullptr) {
                textView_ = std::make_unique<SingleLineTextView>(display_);
            }
            textView_->setText(menuItem.title);
            textView_->setFont(font);
            textView_->setHidden(false);
        }
        if (!menuItem.subtitle.empty() && subtitle) {
            if (subtextView_ == nullptr) {
                subtextView_ = std::make_unique<SingleLineTextView>(
                    display_, menuItem.subtitle, static_cast<Font *>(&fontFace0), true);
            } else {
                subtextView_->setText(menuItem.subtitle, true);
                subtextView_->setHidden(false);
            }
        }

        setNeedsLayout();
    }

    // Whether or not the indicator should be a circle or the full height of the text (plus
    // indicatorExtraVerticalHeight_)
    void setCircleIndicator(bool circle) { circleIndicator_ = circle; }

    void setRightText(const std::string &rightText) {
        if (rightTextView_ == nullptr) {
            rightTextView_ = std::make_unique<SingleLineTextView>(
                display_, rightText, static_cast<Font *>(&fontFace0), true);
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
        Rect textRect{}, subtextRect{};

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
        if (textMultiView_ != nullptr && !textMultiView_->isHidden()) {
            textRect = context.setViewFrame(
                *textMultiView_, {{indicatorWidthPlus, y}, {right - x - indicatorWidthPlus, 0}},
                LayoutOptionsSizeToFitVertical);
        } else {
            textRect = context.setViewFrame(
                *textView_, {{indicatorWidthPlus, y}, {right - x - indicatorWidthPlus, 0}},
                LayoutOptionsSizeToFitVertical);
        }

        y += textRect.size.height;

        // Layout the subtext if necessary
        if (subtextView_ != nullptr && !subtextView_->isHidden()) {
            // Tiny bit of padding in-between
            y += 2;
            subtextRect =
                context.setViewFrame(*subtextView_,
                                     {{indicatorWidthPlus + indicatorRightPadding, y},
                                      {right - x - indicatorWidthPlus - indicatorRightPadding, 0}},
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
            if (subtextView_ != nullptr && !subtextView_->isHidden()) {
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
        if (textMultiView_ != nullptr && !textMultiView_->isHidden()) {
            textMultiView_->draw();
        }
        if (textView_ != nullptr && !textView_->isHidden()) {
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
        if (selected_ == selected) {
            return;
        }
        selected_ = selected;
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
