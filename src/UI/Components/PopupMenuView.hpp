#pragma once

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Layout/View.hpp"

class PopupMenuView : public View {
public:
    PopupMenuView(DisplaySystem &display)
        : View(), display_(display), popupMenuBackground_(display, 5),
          popupMenuForeground_(display, 5), clockView_(display, ""), auxTextView_(display, "") {}

    ~PopupMenuView() override = default;

    void populateMenuItems(std::vector<std::shared_ptr<MenuItemView>> menuItems,
                           std::vector<int> menuItemsOrder) {
        menuItemViews_ = std::move(menuItems);
        menuItemsOrder_ = std::move(menuItemsOrder);
        popupMenuBackground_.fillColor = display_.bg;
        popupMenuBackground_.strokeColor = display_.bg;
        popupMenuForeground_.strokeColor = display_.fg;
    }

    void setAuxText(std::string auxText) {
        auxTextView_.setText(std::move(auxText));
        setNeedsLayout();
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto fullFrame = Rect::fromSize(size);

        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());

        context.setViewFrame(popupMenuBackground_, fullFrame, LayoutOptionsAlignCenter);

        // Border is inset by 5 pixels from the total size
        auto foregroundFrame = context.setViewFrame(popupMenuForeground_, fullFrame.inset(5),
                                                    LayoutOptionsAlignCenter);

        // Internal content sublayout is further inset
        auto contentFrame = context.setSublayoutFrame(
            foregroundFrame.inset(10), LayoutOptionsAlignTop,
            [this](LayoutContext &context, Size size) -> Size {
                int x = 0;
                int y = 0;

                for (auto &index : menuItemsOrder_) {
                    if (!menuItemViews_[index]->isHidden()) {
                        auto viewFrame =
                            context.setViewFrame(*menuItemViews_[index], {{x, y}, size},
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignTop);
                        y += viewFrame.size.height;
                    }
                }

                return {size.width, y};
            });

        // Inset clock and aux horizontally to match the content but with only 5px on the bottom
        context.setViewFrame(clockView_, foregroundFrame.inset(10, 5),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                 LayoutOptionsAlignLeft);

        context.setViewFrame(auxTextView_, foregroundFrame.inset(10, 5),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                 LayoutOptionsAlignRight);

        return size;
    }

    void drawInRect(Rect rect) override {
        popupMenuBackground_.draw();
        popupMenuForeground_.draw();

        for (auto &index : menuItemsOrder_) {
            if (!menuItemViews_[index]->isHidden()) {
                menuItemViews_[index]->draw();
            }
        }

        if (showClock_) {
            clockView_.draw();
        }

        auxTextView_.draw();
    }

protected:
    DisplaySystem &display_;

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    std::vector<int> menuItemsOrder_;

    BoxView popupMenuBackground_;
    BoxView popupMenuForeground_;

    // Every popup has the clock on the lower left if they want it
    SingleLineTextView clockView_;

    // Aux text to show on the lower right
    SingleLineTextView auxTextView_;

    bool showClock_ = true;
};
