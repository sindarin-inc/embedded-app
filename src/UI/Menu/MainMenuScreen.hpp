#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Components/ClockBatteryView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/CarouselMenuScreen.hpp"

class MainMenuScreen : public CarouselMenuScreen {
public:
    MainMenuScreen(DisplaySystem &display, Battery &battery);

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

private:
    ClockBatteryView clockBatteryView_;
};
