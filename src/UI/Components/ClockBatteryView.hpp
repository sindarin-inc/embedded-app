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
#include "UI/Bitmaps/Battery_Bolt.hpp"
#include "UI/Layout/View.hpp"

// A view that draws a lockup of the battery and time
class ClockBatteryView : public EinkView {
public:
    ClockBatteryView(Battery &battery, DisplaySystem &display)
        : EinkView(display),
          batteryIcon0View_(display, battery_0_bits, battery_0_width, battery_0_height),
          batteryIcon10View_(display, battery_10_bits, battery_10_width, battery_10_height),
          batteryIcon20View_(display, battery_20_bits, battery_20_width, battery_20_height),
          batteryIcon30View_(display, battery_30_bits, battery_30_width, battery_30_height),
          batteryIcon40View_(display, battery_40_bits, battery_40_width, battery_40_height),
          batteryIcon50View_(display, battery_50_bits, battery_50_width, battery_50_height),
          batteryIcon60View_(display, battery_60_bits, battery_60_width, battery_60_height),
          batteryIcon70View_(display, battery_70_bits, battery_70_width, battery_70_height),
          batteryIcon80View_(display, battery_80_bits, battery_80_width, battery_80_height),
          batteryIcon90View_(display, battery_90_bits, battery_90_width, battery_90_height),
          batteryIcon100View_(display, battery_100_bits, battery_100_width, battery_100_height),
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

        auto iconRect = context.setViewFrame(batteryIcon0View_, {{x, y}, {size.width, size.height}},
                                             LayoutOptionsSizeToFit);

        context.setViewFrame(batteryIcon10View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);

        context.setViewFrame(batteryIcon20View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);

        context.setViewFrame(batteryIcon30View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);

        context.setViewFrame(batteryIcon40View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);
        context.setViewFrame(batteryIcon50View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);
        context.setViewFrame(batteryIcon60View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);
        context.setViewFrame(batteryIcon70View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);
        context.setViewFrame(batteryIcon80View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);
        context.setViewFrame(batteryIcon90View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);
        context.setViewFrame(batteryIcon100View_, {{x, y}, {size.width, size.height}},
                             LayoutOptionsSizeToFit);

        x += iconRect.size.width;
        return {x, iconRect.size.height};
    }

    void drawInRect(Rect rect) override {

        auto percent = battery_.getBatteryPercentage();

        if (percent > 95.0) {
            batteryIcon100View_.draw();
        } else if (percent > 87.0) {
            batteryIcon90View_.draw();
        } else if (percent > 80.0) {
            batteryIcon80View_.draw();
        } else if (percent > 70.0) {
            batteryIcon70View_.draw();
        } else if (percent > 60.0) {
            batteryIcon60View_.draw();
        } else if (percent > 53.0) {
            batteryIcon50View_.draw();
        } else if (percent > 48.0) {
            batteryIcon40View_.draw();
        } else if (percent > 42.0) {
            batteryIcon30View_.draw();
        } else if (percent > 37.0) {
            batteryIcon20View_.draw();
        } else if (percent > 25.0) {
            batteryIcon10View_.draw();
        } else {
            batteryIcon0View_.draw();
        }
        clockView_.draw();
    }

protected:
    IconView batteryIcon0View_;
    IconView batteryIcon10View_;
    IconView batteryIcon20View_;
    IconView batteryIcon30View_;
    IconView batteryIcon40View_;
    IconView batteryIcon50View_;
    IconView batteryIcon60View_;
    IconView batteryIcon70View_;
    IconView batteryIcon80View_;
    IconView batteryIcon90View_;
    IconView batteryIcon100View_;
    SingleLineTextView clockView_;
    Battery &battery_;
};