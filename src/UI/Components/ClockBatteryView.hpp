#pragma once

#include <utility>

#include "EinkView.hpp"
#include "IconView.hpp"
#include "Misc/Battery.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/Style.hpp"
#include "SingleLineTextView.hpp"
#include "UI/Bitmaps/Battery_0.hpp"
#include "UI/Bitmaps/Battery_10.hpp"
#include "UI/Bitmaps/Battery_100.hpp"
#include "UI/Bitmaps/Battery_20.hpp"
#include "UI/Bitmaps/Battery_30.hpp"
#include "UI/Bitmaps/Battery_40.hpp"
#include "UI/Bitmaps/Battery_50.hpp"
#include "UI/Bitmaps/Battery_60.hpp"
#include "UI/Bitmaps/Battery_70.hpp"
#include "UI/Bitmaps/Battery_80.hpp"
#include "UI/Bitmaps/Battery_90.hpp"
#include "UI/Layout/View.hpp"

// A view that draws a lockup of the battery and time
class ClockBatteryView : public EinkView {
public:
    ClockBatteryView(Battery &battery, DisplaySystem &display)
        : EinkView(display), batteryIconView_(display, BATTERY_0_BITS, sizeof(BATTERY_0_BITS)),
          clockView_(display, ""), battery_(battery) {}
    ~ClockBatteryView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {

        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());

        int x = 0;
        int y = 0;
        auto clockRect = context.setViewFrame(clockView_, {{x, y}, size}, LayoutOptionsSizeToFit);
        x += clockRect.size.width + 20;
        y += 1;

        auto iconRect = context.setViewFrame(batteryIconView_, {{x, y}, {size.width, size.height}},
                                             LayoutOptionsSizeToFit);

        context.setViewFrame(batteryIconView_, iconRect, LayoutOptionsSizeToFit);

        x += iconRect.size.width;
        return {x, iconRect.size.height};
    }

    void drawInRect(Rect rect) override {

        auto percent = battery_.getBatteryPercentage();

        if (percent > 95.0) {
            batteryIconView_.setPBMIcon(BATTERY_100_BITS, sizeof(BATTERY_100_BITS));
        } else if (percent > 87.0) {
            batteryIconView_.setPBMIcon(BATTERY_90_BITS, sizeof(BATTERY_90_BITS));
        } else if (percent > 80.0) {
            batteryIconView_.setPBMIcon(BATTERY_80_BITS, sizeof(BATTERY_80_BITS));
        } else if (percent > 70.0) {
            batteryIconView_.setPBMIcon(BATTERY_70_BITS, sizeof(BATTERY_70_BITS));
        } else if (percent > 60.0) {
            batteryIconView_.setPBMIcon(BATTERY_60_BITS, sizeof(BATTERY_60_BITS));
        } else if (percent > 53.0) {
            batteryIconView_.setPBMIcon(BATTERY_50_BITS, sizeof(BATTERY_50_BITS));
        } else if (percent > 48.0) {
            batteryIconView_.setPBMIcon(BATTERY_40_BITS, sizeof(BATTERY_40_BITS));
        } else if (percent > 42.0) {
            batteryIconView_.setPBMIcon(BATTERY_30_BITS, sizeof(BATTERY_30_BITS));
        } else if (percent > 37.0) {
            batteryIconView_.setPBMIcon(BATTERY_20_BITS, sizeof(BATTERY_20_BITS));
        } else if (percent > 25.0) {
            batteryIconView_.setPBMIcon(BATTERY_10_BITS, sizeof(BATTERY_10_BITS));
        } else {
            batteryIconView_.setPBMIcon(BATTERY_0_BITS, sizeof(BATTERY_0_BITS));
        }

        batteryIconView_.draw();
        clockView_.draw();
    }

protected:
    IconView batteryIconView_;
    SingleLineTextView clockView_;
    Battery &battery_;
};