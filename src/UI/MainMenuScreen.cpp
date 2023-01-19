#include "MainMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "UI/Bitmaps/BenIconHacking.hpp"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

void MainMenuScreen::addSubMenuScreen(const MenuScreen &newSubScreen) {
    subMenus_.push_back(newSubScreen);
    numSubMenus_++;
}

auto MainMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 3;
    const auto spacing = 5;

    auto insetRectTop = context.setSublayoutFrame(
        {{inset, inset}, {size.width - 2 * inset, /*size.height*/ 20 - 2 * inset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignCenterHorizontal,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int viewCount = topBarViews_.size();
            // int boxHeight = (size.height - (viewCount - 1) * spacing) / viewCount;
            int boxWidth = (size.width - (viewCount - 1) * spacing) / viewCount;
            Size boxSize = {boxWidth, size.height};

            log_d("boxSize: %d, %d", boxSize.width, boxSize.height);
            int place = 0;

            for (auto &view : topBarViews_) {
                auto viewFrame = context.setViewFrame(*view, {{x, y}, boxSize},
                                                      LayoutOptionsSizeToFit |
                                                          LayoutOptionsAlignCenterHorizontal |
                                                          LayoutOptionsAlignCenterVertical);
                if (place == currentSelection_) {
                    context.setViewFrame(*topBarSelectionView_, {{x, y}, boxSize},
                                         LayoutOptionsSizeToFit |
                                             LayoutOptionsAlignCenterHorizontal |
                                             LayoutOptionsAlignCenterVertical);
                }
                x += boxWidth + spacing;
                place++;
            };

            return {x, size.height};
        });

    context.setViewFrame(
        *clockView_, {{inset, size.height - 20}, {size.width - 2 * inset, 20 - 2 * inset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignBottom);

    auto insetRectBottomRight = context.setSublayoutFrame(
        {{inset - spacing * 2, size.height - 20},
         {size.width - 2 * inset, /*size.height*/ 20 - 2 * inset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignBottom | LayoutOptionsAlignRight,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;

            context.setViewFrame(*batteryIconView_, {{x, size.height - 20}, {size.width, 20}},
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignRight |
                                     LayoutOptionsAlignBottom);

            x += 24;
            context.setViewFrame(*batteryTextView_, {{x, size.height - 20}, {size.width, 20}},
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignRight |
                                     LayoutOptionsAlignBottom);

            return {x, size.height};
        });

    log_d("size height is %d", size.height);

    return {size.width, insetRectTop.size.height + 2 * inset};
};

void MainMenuScreen::drawInRect(Rect rect) {
    for (auto &view : topBarViews_) {
        (*view).draw();
    }
    topBarSelectionView_->draw();
    batteryIconView_->draw();
    batteryTextView_->draw();
    clockView_->draw();
}

// This largely mirrors the superclass implementation, but with enough
// changes that we need to do our own thing here.
void MainMenuScreen::render(DisplaySystem &display) {
#ifdef SWAP_ARTICLES_BOOKS
    if (beforeFirstLoad_) {
        numBookItems_ = numMenuItems_;
        bookItems_ = menuItems_;
        currBookSelection_ = currSelection_;
        currBookPage_ = currMenuPage_;
        beforeFirstLoad_ = false;
    }
#endif

    if (!needsRerender) {
        return;
    }

    // In case someone changed the selection out from underneath us.
    int oldMenuPage = currMenuPage_;
    currMenuPage_ = currSelection_ / myStyle_.numberOfItemsPerPage;
    if (oldMenuPage != currMenuPage_) {
        firstLoad_ = true;
    }

    // drawArrowsAndSelections(display);

    setNeedsLayout();
    // Givnig the top bar a fixed height of 20 pixels and the entire width of the display.
    setFrame({{0, 0}, {display.width(), display.height()}});

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {

        topBarViews_.clear();
        topBarSelectionView_ = std::make_shared<BoxView>(display, 3);
        batteryIconView_ = std::make_shared<IconView>(display, bolt_bits, bolt_width, bolt_height);
        batteryTextView_ =
            std::make_shared<SingleLineTextView>(display, power_.getBatteryPercentageOnlyString());
        clockView_ = std::make_shared<SingleLineTextView>(display, "3:33pm");
        std::vector<std::string> topBarItems = {"Books", "Articles", "Bible", "Settings"};
        if (subMenus_.size() == 3) {
            topBarItems = {"Books", "Articles", "Settings"};
        }
        for (int i = 0; i < subMenus_.size(); i++) {
            auto topBarView = std::make_shared<SingleLineTextView>(display, topBarItems[i]);
            topBarViews_.push_back(topBarView);
        }

        subMenus_[currentSelection_].drawMenuItems(display, false);
        draw();
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    subMenus_[currentSelection_].drawMenuItems(display, true);
    draw();

    // drawArrowsAndSelections(display);
    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    needsRerender = false;
    firstLoad_ = false;
}

void MainMenuScreen::processCommand(RemoteCommand command) {
    int oldCurr = 0;

    switch (command) {
    case RemoteCommand::PREV:
        subMenus_[currentSelection_].processCommand(command);
        needsRerender = true;
        break;

    case RemoteCommand::NEXT:
        subMenus_[currentSelection_].processCommand(command);
        needsRerender = true;
        break;

    // Long press down on the TT should count as enter since we don't have the
    // 5th button
    case RemoteCommand::ENTER:
    case RemoteCommand::STOP:
        subMenus_[currentSelection_].processCommand(command);
        break;

    case RemoteCommand::CHANNEL_PLUS:
        currentSelection_++;
        currentSelection_ = currentSelection_ % topBarViews_.size();
        firstLoad_ = true;
        needsRerender = true;
        // This will switch between the menus
        break;

#ifdef SWAP_ARTICLES_BOOKS
    case RemoteCommand::CHANNEL_MINUS:

        // If we are going from Books to Articles in the menu, save off the current
        // place for the books, and swap in the place for the articles.
        // Otherwise, vice versa.
        // Going to keep this logic in here and commented out
        // in case we want to flip between books and articles
        // in the future.
        if (inBooksMode_) {
            numBookItems_ = numMenuItems_;
            bookItems_ = menuItems_;
            currBookSelection_ = currSelection_;
            currBookPage_ = currMenuPage_;

            numMenuItems_ = numArticleItems_;
            menuItems_ = articleItems_;
            currSelection_ = currArticleSelection_;
            currMenuPage_ = currArticlePage_;
        } else {
            numArticleItems_ = numMenuItems_;
            articleItems_ = menuItems_;
            currArticleSelection_ = currSelection_;
            currArticlePage_ = currMenuPage_;

            numMenuItems_ = numBookItems_;
            menuItems_ = bookItems_;
            currSelection_ = currBookSelection_;
            currMenuPage_ = currBookPage_;
        }
        inBooksMode_ = !inBooksMode_;
        firstLoad_ = true;

        break;
#endif

    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        currentSelection_--;
        currentSelection_ = currentSelection_ % topBarViews_.size();
        firstLoad_ = true;
        needsRerender = true;
        //
        // Settings are always the last item
        // Eventually this will be a
        // container screen to remove this awkward interaction
        //
        // oldCurr = currSelection_;
        // currSelection_ = numMenuItems_ - 1;

        // if (getCurrItem().callback != nullptr) {
        //     auto fn = getCurrItem().callback;
        // Make sure we return to the right place.
        //    currSelection_ = oldCurr;
        //    fn();
        //}
        // This will switch between the menus
        break;
    case RemoteCommand::ANIMATION:
        needsRerender = false;
        break;
    default:
        break;
    }
}