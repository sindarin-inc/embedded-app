#include "NewMainMenuScreen.hpp"

NewMainMenuScreen::NewMainMenuScreen(DisplaySystem &display) : CarouselMenuScreen(display) {
    titleView_.setText(currentTabTitle_);
    menuView_.padding = 5;
    menuView_.itemsPerPage = 4;
    menuView_.indicatorWidth = 5;
    menuView_.indicatorPadding = 5;
    menuView_.pagingIndicatorWidth = 5;
    menuView_.pagingIndicatorPadding = 5;
    menuView_.setIndicatorType(NewMenuView::IndicatorType::Line);
}

auto NewMainMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(titleView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return size;
}

void NewMainMenuScreen::drawInRect(Rect rect) {
    titleView_.draw();
    menuView_.draw();
}

// Implement Screen method
void NewMainMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawIndicator();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
