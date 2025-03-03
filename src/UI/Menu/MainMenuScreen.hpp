#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Components/ClockBatteryView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/CarouselMenuScreen.hpp"

class MainMenuScreen : public CarouselMenuScreen {
public:
    MainMenuScreen(DisplaySystem &display, Battery &battery, PreferencesStore &prefs);

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;

private:
    ClockBatteryView clockBatteryView_;
};
