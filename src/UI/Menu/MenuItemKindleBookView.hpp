#pragma once

#include "MenuItemBook.hpp"
#include "MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class MenuItemKindleBookView : public MenuItemView {
public:
    MenuItemKindleBookView(DisplaySystem &display, const std::shared_ptr<MenuItemKindleBook> &menuItem)
        : MenuItemView(display), titleView_(display), authorView_(display), progressView_(display) {
        titleView_.setText(menuItem->title());
        // Make the author text all caps
        authorView_.setText(std::string(menuItem->author()), true);
        
        // Add progress if available
        if (menuItem->progressPercent() > 0) {
            progressView_.setText(StringFormat("%d%%", menuItem->progressPercent()));
        } else {
            progressView_.setText("");
        }
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
        
        SolRect progressRect = SolRect::zero();
        if (!progressView_.getText().empty()) {
            // Place progress info on the right side
            progressRect = context.setViewFrame(
                progressView_,
                {{size.width - 50, titleRect.origin.y}, {50, titleRect.size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignRight);
        }
        
        return {std::max(titleRect.size.width, authorRect.size.width),
                titleRect.size.height + authorRect.size.height};
    }

    void drawInRect(SolRect rect) override {
        titleView_.draw();
        authorView_.draw();
        progressView_.draw();
    }

protected:
    SingleLineTextView titleView_;
    SingleLineTextView authorView_;
    SingleLineTextView progressView_;
};