#pragma once

#include "MenuItemView.hpp"
#include "UI/Bitmaps/CheckMark.xbm"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuItemCheckable.hpp"

class MenuItemCheckableView : public MenuItemView {
public:
    MenuItemCheckableView(DisplaySystem &display,
                          const std::shared_ptr<MenuItemCheckable> &menuItem)
        : MenuItemView(display), titleView_(display),
          checkmarkView_(display, CHECK_MARK_BITS, CheckMark_width, CheckMark_height),
          menuItem_(menuItem) {
        titleView_.setText(menuItem->title());
        checkmarkView_.setHidden(false);
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto checkmarkRect = context.setViewFrame(checkmarkView_, Rect::fromSize(size).inset(0, 4),
                                                  LayoutOptionsSizeToFit | LayoutOptionsAlignRight);
        auto titleRect = context.setViewFrame(
            titleView_, Rect::fromSize(size).inset(0, checkmarkRect.size.width + 3, 0, 0),
            LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

        return {size.width, std::max({titleRect.size.height, checkmarkRect.size.height})};
    }

    void drawInRect(Rect rect) override { titleView_.draw(); }

    void drawFastOnlyElements() override {
        if (menuItem_->isChecked()) {
            checkmarkView_.draw();
        }
    }

protected:
    SingleLineTextView titleView_;
    IconView checkmarkView_;
    std::shared_ptr<MenuItemCheckable> menuItem_;
};
