#pragma once

#include "MenuItemBook.hpp"
#include "MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class MenuItemBookView : public MenuItemView {
public:
    MenuItemBookView(DisplaySystem &display, const std::shared_ptr<MenuItemBook> &menuItem)
        : MenuItemView(display), titleView_(display), authorView_(display) {
        titleView_.setText(menuItem->title());
        // Make the author text all caps
        authorView_.setText(std::string(menuItem->author()), true);
    }

    auto layout(SolUI::LayoutContext &context, SolUI::Size size) -> SolUI::Size override {
        auto titleRect = context.setViewFrame(titleView_, SolUI::Rect::fromSize(size),
                                              SolUI::LayoutOptionsSizeToFit);

        SolUI::Rect authorRect = SolUI::Rect::zero();
        if (!authorView_.getText().empty()) {
            // Yes, we are indenting the author view by the height of the title view. This looks
            // better and scales nicely to bigger displays. It looks funny I know.
            authorRect = context.setViewFrame(
                authorView_,
                {{titleRect.size.height * 2 / 3, titleRect.size.height}, {size.width, size.height}},
                SolUI::LayoutOptionsSizeToFit);
        }
        return {std::max(titleRect.size.width, authorRect.size.width),
                titleRect.size.height + authorRect.size.height};
    }

    void drawInRect(SolUI::Rect rect) override {
        titleView_.draw();
        authorView_.draw();
    }

protected:
    SingleLineTextView titleView_;
    SingleLineTextView authorView_;
};
