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
                 const GFXfont *font = &TAHOMA_REGULAR_FONT5PT7B, bool multiLine = false)
        : EinkView(display), textView_(display, menuItem.title, font),
          subtextView_(display, menuItem.subtitle, &TAHOMA_REGULAR_FONT5PT7B, true),
          textMultiView_(display, menuItem.title, 2, 0, font), multiLine_(multiLine),
          indicator_(display, 4), menuItem_(menuItem) {
        if (!menuItem.subtitle.empty()) {
            hasSubtext_ = true;
        }
        log_i("initializing menu item view with title: %s", menuItem.title.c_str());
    }
    ~MenuItemView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        int x = 0;
        int y = 0;
        int indicatorWidth = 5;

        auto indicatorWidthPlus = indicatorWidth + 8;
        auto indicatorHeightPadding = 3;

        Rect textRect;

        if (multiLine_) {
            textRect = context.setViewFrame(
                textMultiView_,
                {{indicatorWidthPlus, y}, {size.width - indicatorWidthPlus, size.height}},
                LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        } else {
            textRect = context.setViewFrame(
                textView_,
                {{indicatorWidthPlus, y}, {size.width - indicatorWidthPlus, size.height}},
                LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        }

        int subtextRectHeight = 0;
        if (hasSubtext_) {
            auto subtextRect =
                context.setViewFrame(subtextView_,
                                     {{indicatorWidthPlus + 15, y + textRect.size.height + 2},
                                      {size.width - indicatorWidthPlus - 15, size.height}},
                                     LayoutOptionsSizeToFitVertical |
                                         LayoutOptionsAlignCenterVertical | LayoutOptionsAlignLeft);
            context.setViewFrame(indicator_,
                                 {{x, y + indicatorHeightPadding + 2},
                                  {indicatorWidth, textRect.size.height + subtextRect.size.height +
                                                       2 * indicatorHeightPadding}});
            subtextRectHeight = subtextRect.size.height;

        } else {
            context.setViewFrame(
                indicator_, {{x, y - indicatorHeightPadding},
                             {indicatorWidth, textRect.size.height + 2 * indicatorHeightPadding}});
        }
        y += textRect.size.height + subtextRectHeight + 2;
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
    const MenuItem &menuItem_;
    bool selected_ = false;
    bool hasSubtext_ = false;
};
