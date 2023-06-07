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
                 bool multiLine = false)
        : EinkView(display), textView_(display, menuItem.title, font),
          subtextView_(display, menuItem.subtitle, static_cast<Font *>(&fontTahomaRegularFonT5PT7B),
                       true),
          textMultiView_(display, menuItem.title, 2, 0, font), multiLine_(multiLine),
          indicator_(display, 4) {
        if (!menuItem.subtitle.empty()) {
            hasSubtext_ = true;
        }
        log_i("initializing menu item view with title: %s", menuItem.title.c_str());
    }
    ~MenuItemView() override = default;

    // Extra height on top and bottom of the indicator relative to the size to fit the text.
    void setIndicatorPadding(int padding) { indicatorPadding_ = padding; }

    // Extra vertical spacing at the bottom of this view.
    void setVerticalPadding(int padding) { verticalPadding_ = padding; }

    // Whether or not the indicator should be a circle or the full height of the text (plus
    // indicatorPadding)
    void setCircleIndicator(bool circle) { circleIndicator_ = circle; }

    // Whether or not the subtitle should be right aligned on the same line as the title
    void setRightSubtext(bool rightSubtext) { rightSubtext_ = rightSubtext; }
    auto layout(LayoutContext &context, Size size) -> Size override {
        int x = 0;
        int y = 0;
        int indicatorWidth = 5;

        auto indicatorWidthPlus = indicatorWidth + 8;
        auto indicatorHeightPadding = indicatorPadding_;
        auto rightSubtextWidth = rightSubtext_ ? 24 : 0;
        Rect textRect, subtextRect;

        if (multiLine_) {
            textRect = context.setViewFrame(
                textMultiView_,
                {{indicatorWidthPlus, y},
                 {size.width - indicatorWidthPlus - rightSubtextWidth, size.height}},
                LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        } else {
            textRect = context.setViewFrame(
                textView_,
                {{indicatorWidthPlus, y},
                 {size.width - indicatorWidthPlus - rightSubtextWidth, size.height}},
                LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        }

        auto indicatorHeight =
            circleIndicator_ ? indicatorWidth : textRect.size.height + 2 * indicatorHeightPadding;

        if (hasSubtext_ && rightSubtext_) {
            subtextRect = context.setViewFrame(subtextView_,
                                               {{indicatorWidthPlus + textRect.size.width + 2, y},
                                                {rightSubtextWidth, size.height}},
                                               LayoutOptionsSizeToFitVertical |
                                                   LayoutOptionsAlignCenterVertical);

            context.setViewSizeInRect(indicator_, {indicatorWidth, indicatorHeight},
                                      Rect{{x, y + 1}, {indicatorWidth, textRect.size.height}},
                                      LayoutOptionsAlignCenterVertical);
        } else if (hasSubtext_ && !rightSubtext_) {
            subtextRect = context.setViewFrame(
                subtextView_,
                {{indicatorWidthPlus + 15, y + textRect.size.height + 2},
                 {size.width - indicatorWidthPlus - 15, size.height}},
                LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

            context.setViewFrame(indicator_,
                                 {{x, y + indicatorHeightPadding + 2},
                                  {indicatorWidth, textRect.size.height + subtextRect.size.height +
                                                       2 * indicatorHeightPadding}});
        } else {
            context.setViewSizeInRect(indicator_, {indicatorWidth, indicatorHeight},
                                      Rect{{x, y + 1}, {indicatorWidth, textRect.size.height}},
                                      LayoutOptionsAlignCenterVertical);
        }

        y += textRect.size.height + verticalPadding_;
        if (!rightSubtext_ && hasSubtext_) {
            y += subtextRect.size.height;
        }
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
        if (multiLine_) {
            textMultiView_.draw();
        } else {
            textView_.draw();
        }
        if (hasSubtext_) {
            subtextView_.draw();
        }
    }

    void setSelected(bool selected) {
        selected_ = selected;
        setNeedsLayout();
    }

protected:
    SingleLineTextView textView_;
    SingleLineTextView subtextView_;
    MultiLineTextView textMultiView_;
    bool multiLine_ = false;
    BoxView indicator_;
    int indicatorPadding_ = 3;
    int verticalPadding_ = 2;
    bool circleIndicator_ = false;
    bool selected_ = false;
    bool hasSubtext_ = false;
    bool rightSubtext_ = false;
};
