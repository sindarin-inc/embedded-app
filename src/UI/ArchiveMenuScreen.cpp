#include "ArchiveMenuScreen.hpp"

#define SPACING 18

auto ArchiveMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int inset = 2;
    auto insetRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width - inset, size.height - inset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int viewCount = menuItemViews_.size();
            int boxHeight = (size.height - (viewCount - 1) * SPACING) / viewCount;
            Size boxSize = {size.width, boxHeight};

            for (auto &view : menuItemViews_) {
                context.setViewFrame(*view, {{x, y}, boxSize},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                         LayoutOptionsDefaultSize);
                y += boxSize.height + SPACING;
            }

            return size;
        });

    return {size.width, insetRect.size.height + 2 * inset};
};

void ArchiveMenuScreen::drawInRect(Rect rect) {
    for (auto &view : menuItemViews_) {
        (*view).draw();
    }
}

void ArchiveMenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    menuItemViews_.clear();
    for (auto &menuItem : menuItems_) {
        auto menuItemView = std::make_shared<MenuItemView>(display, menuItem);
        menuItemViews_.push_back(menuItemView);
    }

    setFrame({{0, 0}, {display.width(), display.height()}});
    PROFILE_START(ArchiveMenuScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(ArchiveMenuScreenLayoutAndDraw);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    firstLoad = false;
    needsRerender = true;

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
}
