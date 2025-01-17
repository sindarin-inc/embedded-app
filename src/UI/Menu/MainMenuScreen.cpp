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

auto MainMenuScreen::layout(SolUI::LayoutContext &context, SolUI::Size size) -> SolUI::Size {
    context.setViewFrame(titleView_, {{0, 0}, {size.width, 0}},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, SolUI::Rect::fromSize(size),
                         SolUI::LayoutOptionsSizeToFitVertical |
                             SolUI::LayoutOptionsAlignCenterVertical);

    context.setViewFrame(clockBatteryView_, SolUI::Rect::fromSize(size),
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignBottom |
                             SolUI::LayoutOptionsAlignCenterHorizontal);

    return size;
}

void MainMenuScreen::drawInRect(SolUI::Rect rect) {
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
