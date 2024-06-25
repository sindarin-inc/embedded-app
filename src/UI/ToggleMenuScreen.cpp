#include "UI/ToggleMenuScreen.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Style.hpp"

#define MAX_WIDTH 150

auto ToggleMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 3;

    context.setViewFrame(leftArrowView_, {{0, 1}, {5, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);
    context.setViewFrame(upperLeftTextView_, {{10, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    const auto mainInset = 2;

    context.setSublayoutFrame(
        {{mainInset, mainInset}, {size.width - 2 * mainInset, size.height - 2 * mainInset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int place = 0;

            Rect offRect =
                context.setViewFrame(offTextView_, {{inset, y}, {size.width - 2 * inset - 19, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignRight);

            Rect onRect = context.setViewFrame(
                onTextView_, {{offRect.origin.x - 25, y}, {200, offRect.size.height}},
                LayoutOptionsSizeToFit);

            y += offRect.size.height;

            for (auto &view : menuItemViews_) {
                auto viewFrame = context.setViewFrame(*view, {{x + 7, y}, {size.width - 70, 0}},
                                                      LayoutOptionsSizeToFitVertical);
                if (place == currSelection_) {
                    context.setViewFrame(*selectedSelectionView_,
                                         {{x, y}, {size.width - 70, viewFrame.size.height + 1}});
                }

                // Only layout the bubble if it's visible (doing this here instead of draw since we
                // already have the place in the vector here)
                if (showToggleBubble_[place]) {
                    Rect alignWithRect = menuToggleValues_[place] ? onRect : offRect;
                    // Align the bubble horizontally with the on/off text and vertically witht
                    // the menu item text (with a little nudge)
                    context.setViewSizeInRect(*menuToggleSelectionViews_[place], {7, 7},
                                              {{alignWithRect.origin.x, y + 1},
                                               {alignWithRect.size.width, viewFrame.size.height}},
                                              LayoutOptionsAlignCenter);
                }

                y += viewFrame.size.height + 1;
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
        clockBatteryView_.updateTime();

        // Populate the menu items with the correct menus for the current subMenu
        menuItemViews_.clear();
        menuPageViews_.clear();
        menuToggleSelectionViews_.clear();

        // Need to do it this way to handle the dark mode / regular mode toggle correctly.
        selectedSelectionView_ = std::make_shared<BoxView>(display, 3);

        for (auto &menuItem : menuItems_) {
            log_d("Adding menu item %s", menuItem.title.c_str());
            auto menuItemView = std::make_shared<SingleLineTextView>(display, menuItem.title,
                                                                     myStyle_.navigationFont);
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
    case RemoteCommand::UP:
        if (prevSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        if (nextSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        menuToggleValues_[currSelection_] = !menuToggleValues_[currSelection_];
        setNeedsRerender();
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        back();
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}
