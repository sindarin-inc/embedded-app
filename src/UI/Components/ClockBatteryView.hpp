#pragma once

#include <utility>

#include "BoxView.hpp"
#include "EinkView.hpp"
#include "IconView.hpp"
#include "Misc/Battery.hpp"
#include "Misc/SolClock.hpp"
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
        // We'll lay it out even if we don't draw it.
        auto clockRect = context.setViewFrame(
            clockView_, {{0, 0}, size}, LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);
        if (!batteryLow_ && battery_.batteryLow()) {
            batteryLow_ = true;
        }
        if (batteryLow_) {
            auto iconRect =
                context.setViewFrame(batteryIconView_, {{clockRect.size.width + 10, 0}, size},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);

            return {clockRect.size.width + iconRect.size.width + 10,
                    std::max(clockRect.size.height, iconRect.size.height)};
        } else {
            return {clockRect.size.width, clockRect.size.height};
        }
    }

    void drawInRect(Rect rect) override {
        switch (battery_.getBatteryPercentageInTime()) {
        case 100:
            batteryIconView_.setIcon(BATTERY_100_BITS, battery_100_width, battery_100_height);
            break;
        case 90:
            batteryIconView_.setIcon(BATTERY_90_BITS, battery_90_width, battery_90_height);
            break;
        case 80:
            batteryIconView_.setIcon(BATTERY_80_BITS, battery_80_width, battery_80_height);
            break;
        case 70:
            batteryIconView_.setIcon(BATTERY_70_BITS, battery_70_width, battery_70_height);
            break;
        case 60:
            batteryIconView_.setIcon(BATTERY_60_BITS, battery_60_width, battery_60_height);
            break;
        case 50:
            batteryIconView_.setIcon(BATTERY_50_BITS, battery_50_width, battery_50_height);
            break;
        case 40:
            batteryIconView_.setIcon(BATTERY_40_BITS, battery_40_width, battery_40_height);
            break;
        case 30:
            batteryIconView_.setIcon(BATTERY_30_BITS, battery_30_width, battery_30_height);
            break;
        case 20:
            batteryIconView_.setIcon(BATTERY_20_BITS, battery_20_width, battery_20_height);
            break;
        case 10:
            batteryIconView_.setIcon(BATTERY_10_BITS, battery_10_width, battery_10_height);
            break;
        case 0:
            batteryIconView_.setIcon(BATTERY_0_BITS, battery_0_width, battery_0_height);
            break;
        }

        // Only show the battery and clock if the battery is low.
        // We can probbaly do better than this.
        if (batteryLow_) {
            batteryIconView_.draw();
        }
        if (SolClock::getInstance().isValidTime()) {
            clockView_.draw();
        }
    }

protected:
    IconView batteryIconView_;
    SingleLineTextView clockView_;
    Battery &battery_;
    // When the battery is right on the boundary, sometimes it'll report low on the first check
    // but then later be above the threshold which looks weird when it goes back and forth.
    // So in a session, if we see the battery low, we remember that we saw it low so we always
    // draw it in that session.
    bool batteryLow_ = false;
};