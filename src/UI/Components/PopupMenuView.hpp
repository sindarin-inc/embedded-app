#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
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

    void setAuxText(std::string auxText) { auxTextView_.setText(auxText); }

    auto layout(LayoutContext &context, Size size) -> Size override {
        const auto inset = 10;
        const auto spacing = 10;

        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());

        context.setViewFrame(popupMenuBackground_, {{0, 0}, size}, LayoutOptionsAlignCenter);

        context.setViewFrame(popupMenuForeground_, {{5, 5}, {size.width - 10, size.height - 10}},
                             LayoutOptionsAlignCenter);

        auto insetRect = context.setSublayoutFrame(
            {{5 + inset, 5 + inset}, {size.width - 10 - 2 * inset, size.height - 10 - 2 * inset}},
            LayoutOptionsAlignTop, [this](LayoutContext &context, Size size) -> Size {
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

        context.setViewFrame(clockView_, {{15, 10}, {size.width - 30, size.height - 20}},
                             LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                 LayoutOptionsAlignLeft);

        context.setViewFrame(auxTextView_, {{15, 10}, {size.width - 30, size.height - 20}},
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

    // Every popup has the clock on the lower left if they want it
    SingleLineTextView clockView_;

    // Aux text to show on the lower right
    SingleLineTextView auxTextView_;

    BoxView popupMenuBackground_;
    BoxView popupMenuForeground_;

    bool showClock_ = true;
};