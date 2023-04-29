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
        : View(), display_(display), popupMenuBackground_(display, 5),
          popupMenuForeground_(display, 5), clockView_(display, ""), auxTextView_(display, "") {}

    ~PopupMenuView() override = default;

    void setAuxText(std::string auxText) {
        auxTextView_.setText(std::move(auxText));
        setNeedsLayout();
    }

    auto showAltView() -> bool { return showAltView_; }

    void addAltView(std::shared_ptr<View> view) { altViews_.push_back(view); }

    void setAltViewIndex(uint32_t index) {
        uint32_t oldIndex = altViewIndex_;
        if (index < altViews_.size()) {
            altViewIndex_ = index;
        }
        if ((oldIndex != index) && (showAltView_)) {
            setNeedsLayout();
        }
    }

    void setShowAltView(bool showAltView) {
        bool oldShowAltView = showAltView_;
        showAltView_ = showAltView;
        if (showAltView_ != oldShowAltView) {
            setNeedsLayout();
        }
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto fullFrame = Rect::fromSize(size);

        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());

        context.setViewFrame(popupMenuBackground_, fullFrame, LayoutOptionsAlignCenter);

        // Border is inset by 5 pixels from the total size
        auto foregroundFrame = context.setViewFrame(popupMenuForeground_, fullFrame.inset(5),
                                                    LayoutOptionsAlignCenter);

        if (!showAltView_) {
            // Internal content sublayout is further inset
            auto contentFrame = context.setSublayoutFrame(
                foregroundFrame.inset(10), LayoutOptionsAlignTop,
                [this](LayoutContext &context, Size size) -> Size {
                    int x = 0;
                    int y = 0;

                    for (auto &view : menuItemViews) {
                        if (!view->isHidden()) {
                            auto viewFrame = context.setViewFrame(*view, {{x, y}, size},
                                                                  LayoutOptionsSizeToFit |
                                                                      LayoutOptionsAlignTop);
                            y += viewFrame.size.height;
                        }
                    }

                    return {size.width, y};
                });

            // Inset clock and aux horizontally to match the content but with 5px less padding on
            // the bottom
            context.setViewFrame(clockView_, contentFrame.inset(0, -5),
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                     LayoutOptionsAlignLeft);

            context.setViewFrame(auxTextView_, contentFrame.inset(0, -5),
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                     LayoutOptionsAlignRight);
        } else {
            context.setViewFrame(*altViews_[altViewIndex_], foregroundFrame.inset(10),
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignTop);
        }

        return size;
    }

    void drawInRect(Rect rect) override {
        popupMenuBackground_.fillColor = display_.bg;
        popupMenuBackground_.strokeColor = display_.bg;
        popupMenuForeground_.strokeColor = display_.fg;

        // Always draw the background and foreground
        popupMenuBackground_.draw();
        popupMenuForeground_.draw();

        if (!showAltView_) {
            // By default, draw the menu items
            // but if there are other views stored in this view for various
            // functionality, optionally draw those instead
            for (auto &view : menuItemViews) {
                if (!view->isHidden()) {
                    view->draw();
                }
            }

            if (showClock_) {
                clockView_.draw();
            }

            auxTextView_.draw();
        } else {
            altViews_[altViewIndex_]->draw();
        }
    }

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews;

protected:
    DisplaySystem &display_;

    BoxView popupMenuBackground_;
    BoxView popupMenuForeground_;

    // Every popup has the clock on the lower left if they want it
    SingleLineTextView clockView_;

    // Aux text to show on the lower right
    SingleLineTextView auxTextView_;

    bool showClock_ = true;

    bool showAltView_ = false;
    std::vector<std::shared_ptr<View>> altViews_;
    uint32_t altViewIndex_ = 0;
};
