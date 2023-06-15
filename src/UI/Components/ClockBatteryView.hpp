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
        : EinkView(display),
          batteryIconView_(display, BATTERY_0_BITS, battery_0_width, battery_0_height),
          clockView_(display, ""), battery_(battery) {}
    ~ClockBatteryView() override = default;

    auto updateTime() {
        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());
        setNeedsLayout();
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
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
            batteryIconView_.setIcon(BATTERY_100_BITS, battery_100_width, battery_100_height);
        } else if (percent > 87.0) {
            batteryIconView_.setIcon(BATTERY_90_BITS, battery_90_width, battery_90_height);
        } else if (percent > 80.0) {
            batteryIconView_.setIcon(BATTERY_80_BITS, battery_80_width, battery_80_height);
        } else if (percent > 70.0) {
            batteryIconView_.setIcon(BATTERY_70_BITS, battery_70_width, battery_70_height);
        } else if (percent > 60.0) {
            batteryIconView_.setIcon(BATTERY_60_BITS, battery_60_width, battery_60_height);
        } else if (percent > 53.0) {
            batteryIconView_.setIcon(BATTERY_50_BITS, battery_50_width, battery_50_height);
        } else if (percent > 48.0) {
            batteryIconView_.setIcon(BATTERY_40_BITS, battery_40_width, battery_40_height);
        } else if (percent > 42.0) {
            batteryIconView_.setIcon(BATTERY_30_BITS, battery_30_width, battery_30_height);
        } else if (percent > 37.0) {
            batteryIconView_.setIcon(BATTERY_20_BITS, battery_20_width, battery_20_height);
        } else if (percent > 25.0) {
            batteryIconView_.setIcon(BATTERY_10_BITS, battery_10_width, battery_10_height);
        } else {
            batteryIconView_.setIcon(BATTERY_0_BITS, battery_0_width, battery_0_height);
        }

        batteryIconView_.draw();
        clockView_.draw();
    }

protected:
    IconView batteryIconView_;
    SingleLineTextView clockView_;
    Battery &battery_;
};