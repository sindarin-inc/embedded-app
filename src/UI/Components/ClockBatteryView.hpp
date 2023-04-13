#pragma once

#include <utility>

#include "EinkView.hpp"
#include "IconView.hpp"
#include "Misc/Battery.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/Style.hpp"
#include "SingleLineTextView.hpp"
#include "UI/Bitmaps/Battery_Bolt.hpp"
#include "UI/Layout/View.hpp"

// A view that draws a lockup of the battery and time
class ClockBatteryView : public EinkView {
public:
    ClockBatteryView(Battery &battery, DisplaySystem &display)
        : EinkView(display), batteryTextView_(display, ""),
          batteryIconView_(display, boltBits, bolt_width, bolt_height), clockView_(display, ""),
          battery_(battery) {}
    ~ClockBatteryView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {

        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());

        batteryTextView_.setText(battery_.getBatteryPercentageOnlyString());

        int x = 0;
        int y = 0;
        auto clockRect = context.setViewFrame(clockView_, {{x, y}, size}, LayoutOptionsSizeToFit);
        x += clockRect.size.width + 20;
        auto iconRect = context.setViewFrame(batteryIconView_, {{x, y}, {size.width, size.height}},
                                             LayoutOptionsSizeToFit);
        x += iconRect.size.width + 3;
        auto textRect = context.setViewFrame(batteryTextView_, {{x, y}, {size.width, size.height}},
                                             LayoutOptionsSizeToFit);
        x += textRect.size.width;
        return {x, std::max(iconRect.size.height, textRect.size.height)};
    }

    void drawInRect(Rect rect) override {
        batteryIconView_.draw();
        batteryTextView_.draw();
        clockView_.draw();
    }

protected:
    SingleLineTextView batteryTextView_;
    IconView batteryIconView_;
    SingleLineTextView clockView_;
    Battery &battery_;
};