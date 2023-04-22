#include "UI/ToggleMenuScreen.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Style.hpp"

#define MAX_WIDTH 150

void ToggleMenuScreen::requireRestart() { restartOnBack_ = true; }

auto ToggleMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 3;

    context.setViewFrame(leftArrowView_, {{0, 1}, {5, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);
    context.setViewFrame(upperLeftTextView_, {{10, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    context.setSublayoutFrame(
        {{inset, inset}, {size.width - 2 * inset, 50 - 2 * inset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignRight,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 35;

            context.setViewFrame(onTextView_, {{x, y}, size}, LayoutOptionsSizeToFit);
            x += 25;
            context.setViewFrame(offTextView_, {{x, y}, size}, LayoutOptionsSizeToFit);
            x += 40;

            return {x, size.height};
        });

    const auto mainInset = 2;
    const auto mainSpacing = 10;

    context.setSublayoutFrame(
        {{mainInset, mainInset}, {size.width - 2 * mainInset, size.height - 2 * mainInset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int viewCount = menuItemViews_.size();
            int boxHeight = (size.height - (viewCount - 1) * mainSpacing) / viewCount + 6;
            Size boxSize = {size.width, boxHeight};
            int place = 0;

            for (auto &view : menuItemViews_) {
                auto viewFrame =
                    context.setViewFrame(*view, {{x, y}, boxSize}, LayoutOptionsSizeToFit);
                if (place == currSelection_) {
                    context.setViewFrame(*selectedSelectionView_,
                                         {{x, y}, {boxSize.width - 70, boxSize.height}},
                                         LayoutOptionsSizeToFit);
                }

                if (menuToggleValues_[place] == true) {
                    context.setViewFrame(*menuToggleSelectionViews_[place],
                                         {{x + 139, y + 3}, {7, 7}}, LayoutOptionsSizeToFit);
                } else {
                    context.setViewFrame(*menuToggleSelectionViews_[place],
                                         {{x + 167, y + 3}, {7, 7}}, LayoutOptionsSizeToFit);
                }

                y += viewFrame.size.height + 2;
                place++;
            }

            return {size.width, y};
        });

    context.setSublayoutFrame(
        // The -5 is to leave room for the paging circles at the right hand side of the view
        {{size.width - mainInset - 5, mainInset}, {10, size.height - 2 * mainInset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            Size boxSize = {5, 5};
            for (auto &view : menuPageViews_) {
                auto viewFrame = context.setViewFrame(
                    *view, {{x, y}, boxSize}, LayoutOptionsSizeToFit | LayoutOptionsAlignRight);
                y += viewFrame.size.height + 1;
            }

            return {size.width, y};
        });

    context.setViewFrame(clockBatteryView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);

    return size;
}

void ToggleMenuScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
    onTextView_.draw();
    offTextView_.draw();
    for (auto &view : menuItemViews_) {
        (*view).draw();
    }

    if (drawBubbles_) {
        for (auto &view : menuToggleSelectionViews_) {
            (*view).draw();
        }
    }

    if (menuPageViews_.size() > 1) {
        for (auto &view : menuPageViews_) {
            (*view).draw();
        }
    }

    if (drawBubbles_) {
        selectedSelectionView_->draw();
    }

    clockBatteryView_.draw();
}

// This largely mirrors the superclass implementation, but with enough
// changes that we need to do our own thing here.
void ToggleMenuScreen::render(DisplaySystem &display) {
    setNeedsLayout();

    // In case this changes for whatever reason?
    headerTextView_.setText(menuName_);

    // Take up the whole screen
    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        // Populate the menu items with the correct menus for the current subMenu
        menuItemViews_.clear();
        menuPageViews_.clear();
        menuToggleSelectionViews_.clear();

        // Need to do it this way to handle the dark mode / regular mode toggle correctly.
        selectedSelectionView_ = std::make_shared<BoxView>(display, 3);

        for (auto &menuItem : menuItems_) {
            log_d("Adding menu item %s", menuItem.title.c_str());
            auto menuItemView =
                std::make_shared<MenuItemView>(display, menuItem, myStyle_.navigationFont, true);
            menuItemViews_.push_back(menuItemView);
            menuToggleSelectionViews_.push_back(std::make_shared<BoxView>(display, 3));
            menuToggleSelectionViews_[menuToggleSelectionViews_.size() - 1]->fillColor =
                display_.fg;
        }

        draw();
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    drawBubbles_ = true;
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    drawBubbles_ = false;
    firstLoad_ = false;
}

void ToggleMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        if (prevSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::NEXT:
        if (nextSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
        menuToggleValues_[currSelection_] = !menuToggleValues_[currSelection_];
        setNeedsRerender();
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        if (backCallback != nullptr) {
            firstLoad_ = true;
            backCallback();
        }
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}
