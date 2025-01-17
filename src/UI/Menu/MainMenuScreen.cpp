#include "MainMenuScreen.hpp"

MainMenuScreen::MainMenuScreen(DisplaySystem &display, Battery &battery, PreferencesStore &prefs)
    : CarouselMenuScreen(display), clockBatteryView_(battery, display, prefs) {
    menuView_.padding = 5;
    menuView_.itemsPerPage = 4;
    menuView_.indicatorWidth = 5;
    menuView_.indicatorPadding = 5;
    menuView_.pagingIndicatorWidth = 5;
    menuView_.pagingIndicatorPadding = 5;
    menuView_.setIndicatorType(MenuView::IndicatorType::Line);
}

auto MainMenuScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    context.setViewFrame(titleView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    context.setViewFrame(clockBatteryView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);

    return size;
}

void MainMenuScreen::drawInRect(SolRect rect) {
    titleView_.draw();
    menuView_.draw();
    clockBatteryView_.draw();
}

void MainMenuScreen::render(DisplaySystem &display) {
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
