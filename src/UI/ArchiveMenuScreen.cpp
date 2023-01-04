#include "ArchiveMenuScreen.hpp"

auto ArchiveMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int inset = 2;
    auto insetRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width - inset, size.height - inset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;

            // Assume equal sizes for now
            int viewCount = views_.size();
            int dimension = vertical_ ? size.height : size.width;

            Size boxSize;
            if (vertical_) {
                int boxHeight = (dimension - (viewCount - 1) * spacing_) / viewCount;
                boxSize = {size.width, boxHeight};
            } else {
                int boxWidth = (dimension - (viewCount - 1) * spacing_) / viewCount;
                boxSize = {boxWidth, size.height};
            }
            int x = 0;
            int y = 0;

            for (auto &view : views_) {
                context.setViewFrame(*view, {{x, y}, boxSize},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                         LayoutOptionsDefaultSize);
                if (vertical_) {
                    y += boxSize.height + spacing_;
                } else {
                    x += boxSize.width + spacing_;
                }
            }

            return size;
            auto menuRect = context.setViewFrame(this->menuView_, {{x, y}, {size.width, 15}},
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

            y += menuRect.size.height + 2;

            return {size.width, y};
        });

    return {size.width, insetRect.size.height};
};

void ArchiveMenuScreen::drawInRect(Rect rect) { menuView_.draw(); }

void ArchiveMenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    menuItemViews_.clear();
    for (auto &view : menuItems_) {
        auto menuItem = std::make_shared<MenuItemView>(display, view.title, view.callback);
        menuItemViews_.push_back(menuItem);
    }
    StackView temp(display, menuItemViews_, 18, true);
    menuView_ = std::move(temp);
    needsRerender = true;

    setFrame({{0, 0}, {display.width(), display.height()}});
    PROFILE_START(ArchiveMenuScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(ArchiveMenuScreenLayoutAndDraw);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    firstLoad = false;
    needsRerender = true;

    std::shared_ptr<EinkView> view = menuItemViews_[getCurrSelection()];
    std::shared_ptr<MenuItemView> selectedMenuItem = std::dynamic_pointer_cast<MenuItemView>(view);
    for (auto &view : menuItemViews_) {
        std::shared_ptr<MenuItemView> menuItem = std::dynamic_pointer_cast<MenuItemView>(view);
        if (menuItem && menuItem == selectedMenuItem) {
            menuItem->setSelected(true);
        } else {
            menuItem->setSelected(false);
        }
    }

    // Redraw with a different vcom setting for the menu item bullet
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
}
