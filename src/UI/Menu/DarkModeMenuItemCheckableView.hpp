#pragma once

#include "MenuItemView.hpp"
#include "UI/Bitmaps/CheckMark.xbm"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/DarkModeMenuItemCheckable.hpp"

class DarkModeMenuItemCheckableView : public MenuItemView {
public:
    DarkModeMenuItemCheckableView(DisplaySystem &display,
                                  const std::shared_ptr<DarkModeMenuItemCheckable> &menuItem)
        : MenuItemView(display), titleView_(display),
          checkmarkView_(display, CHECK_MARK_BITS, CheckMark_width, CheckMark_height),
          arrowView_(display, "", ArrowRight), menuItem_(menuItem) {
        titleView_.setText(menuItem->title());
        checkmarkView_.setHidden(false);
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        auto checkmarkRect =
            context.setViewFrame(checkmarkView_, SolRect::fromSize(size).inset(0, 4),
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignRight);
        auto titleRect = context.setViewFrame(
            titleView_, SolRect::fromSize(size).inset(0, checkmarkRect.size.width + 3, 0, 0),
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

        // Arrow is to the immediate right of the title
        context.setViewFrame(arrowView_,
                             SolRect::fromSize(size).inset(titleRect.size.width + 3, 0, 0, 0),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

        return {size.width, std::max({titleRect.size.height, checkmarkRect.size.height})};
    }

    void drawInRect(SolRect rect) override {
        titleView_.draw();
        if (menuItem_->hasRightButtonCallback()) {
            arrowView_.draw();
        }
    }

    void drawFastOnlyElements() override {
        if (menuItem_->isChecked()) {
            checkmarkView_.draw();
        }
    }

protected:
    SingleLineTextView titleView_;
    IconView checkmarkView_;
    ArrowLabelView arrowView_;
    std::shared_ptr<DarkModeMenuItemCheckable> menuItem_;
};
