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

    auto layout(SolUI::LayoutContext &context, SolUI::Size size) -> SolUI::Size override {
        auto checkmarkRect =
            context.setViewFrame(checkmarkView_, SolUI::Rect::fromSize(size).inset(0, 4),
                                 SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignRight);
        auto titleRect = context.setViewFrame(
            titleView_, SolUI::Rect::fromSize(size).inset(0, checkmarkRect.size.width + 3, 0, 0),
            SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

        return {size.width, std::max({titleRect.size.height, checkmarkRect.size.height})};
    }

    void drawInRect(SolUI::Rect rect) override { titleView_.draw(); }

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
