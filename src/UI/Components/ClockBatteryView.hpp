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
    ClockBatteryView(Battery &battery, DisplaySystem &display, PreferencesStore &prefs)
        : EinkView(display),
          batteryIconView_(display, BATTERY_0_BITS, battery_0_width, battery_0_height),
          clockView_(display, ""), battery_(battery), prefs_(prefs) {}
    ~ClockBatteryView() override = default;

    auto batteryLow() -> bool { return battery_.getBatteryPercentage() <= 37.0; }

    auto updateTime() {
        SolClock &theClock = SolClock::getInstance();
        clockView_.setText(theClock.getTimeString());
        setNeedsLayout();
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        SolRect clockRect = SolRect::zero();
        SolRect batteryRect = SolRect::zero();

        int x = 0;
        if (SolClock::getInstance().isValidTime()) {
            clockRect =
                context.setViewFrame(clockView_, {{0, 0}, size},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);
            x += clockRect.size.width;
        }

        if (!batteryLow_ && batteryLow()) {
            batteryLow_ = true;
        }

        if (batteryLow_ || prefs_.alwaysShowBattery.get().value_or(false)) {
            // Add a little padding between the clock and the battery if there is a clock
            if (x != 0) {
                x += 10;
            }
            batteryRect =
                context.setViewFrame(batteryIconView_, {{x, 0}, size},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);
            x += batteryRect.size.width;
        }

        return {x, std::max(clockRect.size.height, batteryRect.size.height)};
    }

    void drawInRect(SolRect rect) override {

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

        // Only show the battery and clock if the battery is low.
        // We can probbaly do better than this.
        if (batteryLow_ || prefs_.alwaysShowBattery.get().value_or(false)) {
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

    PreferencesStore &prefs_;
};