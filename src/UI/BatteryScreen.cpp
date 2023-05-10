#include "UI/BatteryScreen.hpp"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#include "sindarin-debug.h"

void BatteryScreen::render(DisplaySystem &display) {
    uint16_t width = 0, height = 0;
    display.setFont(display.style.getFontFamily().readableFont);
    auto batteryStatus = battery_.getBatteryLifeString();
    uint16_t leftTemp;
#if EINKET013TT1
    auto *driver = (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver();
    // TODO: Add UI for right screen temp also
    leftTemp = driver->readTemperature(ScreenSide::Left);
#else
    leftTemp = 99;
#endif
    batteryStatus += StringFormat(" %dC", leftTemp);

    width = display.getTextWidth(batteryStatus.c_str());
    height = display.getTextHeight(batteryStatus.c_str());

    display.drawSingleLineOfText(batteryStatus.c_str(),
                                 display.xAbsolute(display.xAbsoluteCenter() - width / 2),
                                 display.yAbsolute(display.yAbsoluteCenter() - height / 2 - 2));

    auto ptBatteryPct = PreferencesStore::lastPageTurnerBattery;
    auto ptBuild = prefs_.lastPageTurnerBuild.get().value_or("Unknown build");
    auto ptBatteryStatus = StringFormat("Page Turner: %d%% (%s)", ptBatteryPct, ptBuild.c_str());
    width = display.getTextWidth(ptBatteryStatus.c_str());
    height = display.getTextHeight(ptBatteryStatus.c_str());
    display.drawSingleLineOfText(ptBatteryStatus.c_str(),
                                 display.xAbsolute(display.xAbsoluteCenter() - width / 2),
                                 display.yAbsolute(display.yAbsoluteCenter() + height / 2 + 2));

    display.updateDisplay();
}

void BatteryScreen::processCommand(RemoteCommand command) { back(); }
