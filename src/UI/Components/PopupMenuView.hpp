#pragma once

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Layout/View.hpp"

class PopupMenuView : public View, public Menu {
public:
    PopupMenuView(DisplaySystem &display)
        : display_(display), popupMenuBackground_(display, 5), popupMenuForeground_(display, 5),
          footer1TextView_(display, ""), footer2TextView_(display, ""),
          footer3TextView_(display, "") {}

    ~PopupMenuView() override = default;

    void setFooter1Text(std::string text) {
        footer1TextView_.setText(std::move(text));
        setNeedsLayout();
    }

    void setFooter2Text(std::string text) {
        footer2TextView_.setText(std::move(text));
        setNeedsLayout();
    }

    void setFooter3Text(std::string text) {
        footer3TextView_.setText(std::move(text));
        setNeedsLayout();
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto fullFrame = Rect::fromSize(size);

        context.setViewFrame(popupMenuBackground_, fullFrame);
        // Border is inset by 5 pixels from the total size
        auto foregroundFrame = context.setViewFrame(popupMenuForeground_, fullFrame.inset(5));
        auto contentFrame = foregroundFrame.inset(10);
        auto y = contentFrame.origin.y;
        auto x = 0;

        for (auto &view : menuItemViews) {
            if (!view->isHidden()) {
                auto viewFrame =
                    context.setViewFrame(*view, {{contentFrame.origin.x, y}, contentFrame.size},
                                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop);
                y += viewFrame.size.height;
                x = std::max(x, viewFrame.size.width);
            }
        }

        // Nudge down the footers
        if (!footer1TextView_.getText().empty() || !footer2TextView_.getText().empty()) {
            y += 5;
        }

        auto footerRect = Rect({{contentFrame.origin.x, y}, {contentFrame.size.width, 0}});
        if (!footer1TextView_.getText().empty()) {
            auto footerTopRect =
                context.setViewFrame(footer1TextView_, footerRect,
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += footerTopRect.size.height;
            footerRect.origin.y = y;
        }
        if (!footer2TextView_.getText().empty()) {
            auto footerBottomRect =
                context.setViewFrame(footer2TextView_, footerRect,
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += footerBottomRect.size.height;
            footerRect.origin.y = y;
        }
        if (!footer3TextView_.getText().empty()) {
            auto footerBottomBottomRect =
                context.setViewFrame(footer3TextView_, footerRect,
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += footerBottomBottomRect.size.height;
        }

        // Add bottom padding
        y += 5;

        // Make room for foregroundFrame box
        y += 5;

        // To be visually pleasing, the popup menu should take about 150% of the width
        // of the widest text -- this allows for the buttons as well
        // return {maxWidthOfText * 110 / 100, y};
        // 152 / 100 makes it exactly what it was before (106).
        // However, on other displays with larger resoltuions, the popup menu still seems a bit too
        // big. Future work to figure this out.
        return {152 * x / 100, y};
    }

    void drawInRect(Rect rect) override {
        popupMenuBackground_.fillColor = display_.bg;
        popupMenuBackground_.strokeColor = display_.bg;
        popupMenuForeground_.strokeColor = display_.fg;

        popupMenuBackground_.draw();
        popupMenuForeground_.draw();

        for (auto &view : menuItemViews) {
            if (!view->isHidden()) {
                view->draw();
            }
        }

        footer1TextView_.draw();
        footer2TextView_.draw();
        footer3TextView_.draw();
    }

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews;

protected:
    DisplaySystem &display_;

    BoxView popupMenuBackground_;
    BoxView popupMenuForeground_;

    // Two lines of footer to put stuff like the clock on
    SingleLineTextView footer1TextView_;
    SingleLineTextView footer2TextView_;
    SingleLineTextView footer3TextView_;
};
