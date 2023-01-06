#include "LineMenuScreen.hpp"

#define SPACING 10

auto LineMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int inset = 2;
    auto insetRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width - inset, size.height - inset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int viewCount = menuItemViews_.size();
            int boxHeight = (size.height - (viewCount - 1) * SPACING) / viewCount;
            Size boxSize = {size.width, boxHeight};
            log_i("Archive menu layout: %d views, %dpx height", viewCount, boxHeight);

            for (auto &view : menuItemViews_) {
                auto viewFrame = context.setViewFrame(
                    *view, {{x, y}, boxSize}, LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
                y += viewFrame.size.height + SPACING;
            }

            return {size.width, y};
        });

    return {size.width, insetRect.size.height + 2 * inset};
};

void LineMenuScreen::drawInRect(Rect rect) {
    for (auto &view : menuItemViews_) {
        (*view).draw();
    }
}

void LineMenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    if (firstLoad_) {
        menuItemViews_.clear();
        for (auto &menuItem : menuItems_) {
            auto menuItemView = std::make_shared<MenuItemView>(display, menuItem);
            menuItemViews_.push_back(menuItemView);
        }
    }

    setFrame({{0, 0}, {display.width(), display.height()}});
    PROFILE_START(LineMenuScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(LineMenuScreenLayoutAndDraw);

    display.updateDisplay(firstLoad_ ? DisplaySystem::UpdateType::FULL
                                     : DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    std::shared_ptr<MenuItemView> selectedMenuItemView = menuItemViews_[getCurrSelection()];
    for (auto &menuItemView : menuItemViews_) {
        if (menuItemView && menuItemView == selectedMenuItemView) {
            menuItemView->setSelected(true);
        } else {
            menuItemView->setSelected(false);
        }
    }

    // Redraw with a different vcom setting for the menu item bullet
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    firstLoad_ = false;
    needsRerender = false;
}

void LineMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        if (prevSelection()) {
            firstLoad_ = true;
        }
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        if (nextSelection()) {
            firstLoad_ = true;
        }
        needsRerender = true;
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
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
        needsRerender = false;
        break;
    default:
        break;
    }
}
