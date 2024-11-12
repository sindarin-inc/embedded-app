#include "NewMainMenuScreen.hpp"

NewMainMenuScreen::NewMainMenuScreen(DisplaySystem &display, Battery &battery)
    : CarouselMenuScreen(display), clockBatteryView_(battery, display) {
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
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    context.setViewFrame(clockBatteryView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);

    return size;
}

void NewMainMenuScreen::drawInRect(Rect rect) {
    titleView_.draw();
    menuView_.draw();
    clockBatteryView_.draw();
}

void NewMainMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());

    if (firstLoad_) {
        clockBatteryView_.updateTime();
    }

    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
