#include "LineMenuScreen.hpp"

auto LineMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 2;
    const auto spacing = 10;

    auto insetRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width - 2 * inset, size.height - 2 * inset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            log_i("Archive menu layout: %zu views in size %dpx x %d", menuItemViews_.size(),
                  size.width, size.height);

            for (auto &view : menuItemViews_) {
                auto viewFrame = context.setViewFrame(*view, {{x, y}, size},
                                                      LayoutOptionsSizeToFitVertical |
                                                          LayoutOptionsAlignCenterVertical);
                y += viewFrame.size.height + spacing;
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

    if (menuItemViews_.size() <= 0) {
        log_w("No views to draw in LineMenuScreen");
    }

    setNeedsLayout();
    setFrame({{0, 0}, {display.width(), display.height()}});

    if (firstLoad_) {
        PROFILE_START(LineMenuScreenLayoutAndDraw);
        draw();
        PROFILE_END_LOG_US(LineMenuScreenLayoutAndDraw);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

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
        needsRerender = true;
        break;
    case RemoteCommand::ANIMATION:
        needsRerender = false;
        break;
    default:
        break;
    }
}
