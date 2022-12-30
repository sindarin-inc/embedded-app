#include "ArchiveMenuScreen.hpp"

auto ArchiveMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int inset = 2;
    auto insetRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width - inset, size.height - inset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;

            auto menuRect = context.setViewFrame(this->menuView_, {{x, y}, {size.width, 15}},
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignLeft |
                                                     LayoutOptionsAlignCenterVertical);

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

    setFrame({{0, 0}, {display.width(), display.height()}});
    PROFILE_START(ArchiveMenuScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(ArchiveMenuScreenLayoutAndDraw);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    firstLoad = false;
    needsRerender = true;

    std::shared_ptr<EinkView> view = menuView_.viewAtIndex(0);
    std::shared_ptr<MenuItemView> selectedMenuItem = std::dynamic_pointer_cast<MenuItemView>(view);
    if (selectedMenuItem) {
        selectedMenuItem->setSelected(true);
    }

    // Redraw with a different vcom setting for the menu item bullet
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
}
