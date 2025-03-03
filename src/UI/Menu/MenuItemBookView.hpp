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

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        auto titleRect =
            context.setViewFrame(titleView_, SolRect::fromSize(size), LayoutOptionsSizeToFit);

        SolRect authorRect = SolRect::zero();
        if (!authorView_.getText().empty()) {
            // Yes, we are indenting the author view by the height of the title view. This looks
            // better and scales nicely to bigger displays. It looks funny I know.
            authorRect = context.setViewFrame(
                authorView_,
                {{titleRect.size.height * 2 / 3, titleRect.size.height}, {size.width, size.height}},
                LayoutOptionsSizeToFit);
        }
        return {std::max(titleRect.size.width, authorRect.size.width),
                titleRect.size.height + authorRect.size.height};
    }

    void drawInRect(SolRect rect) override {
        titleView_.draw();
        authorView_.draw();
    }

protected:
    SingleLineTextView titleView_;
    SingleLineTextView authorView_;
};
