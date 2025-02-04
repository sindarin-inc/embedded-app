#pragma once

#include "MenuItemArticle.hpp"
#include "MenuItemView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

class MenuItemArticleView : public MenuItemView {
public:
    MenuItemArticleView(DisplaySystem &display, const std::shared_ptr<MenuItemArticle> &menuItem)
        : MenuItemView(display), titleView_(display, 2) /*, sourceView_(display) */ {
        titleView_.setText(menuItem->title());
        // sourceView_.setText(menuItem->source(), true);
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        auto titleRect =
            context.setViewFrame(titleView_, SolRect::fromSize(size), LayoutOptionsSizeToFit);
        // Some day, we may want to include this, but before we do, we'll need to do some processing
        // on the source text to make it look right. Right now it's EMAIL_ARTICLE or POCKET_ARTICLE
        // and we'll need probably need give more interesting info.
        // auto sourceRect =
        //     context.setViewFrame(sourceView_,
        //                          {{display_.getFont()->lineHeight() * 2/ 3, titleRect.size.height
        //                          },
        //                           {size.width, size.height}},
        //                          LayoutOptionsSizeToFit);
        // return {std::max(titleRect.size.width, sourceRect.size.width),
        // titleRect.size.height + sourceRect.size.height};
        return titleRect.size;
    }

    void drawInRect(SolRect rect) override {
        titleView_.draw();
        // sourceView_.draw();
    }

protected:
    MultiLineTextView titleView_;
    // SingleLineTextView sourceView_;
};
