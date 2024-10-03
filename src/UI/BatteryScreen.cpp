#include "UI/BatteryScreen.hpp"

#if CONFIG_DISPLAY_EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#include "UI/Fonts/Fonts.hpp"
#include "sindarin-debug.h"

BatteryScreen::BatteryScreen(DisplaySystem &display, PreferencesStore &prefs, Battery &battery)
    : display_(display), prefs_(prefs), battery_(battery), leftTemp_(99), rightTemp_(99) {

#if CONFIG_DISPLAY_EINKET013TT1
    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display))->getDriver();
    leftTemp_ = driver->readTemperature(ScreenSide::Left);
    rightTemp_ = driver->readTemperature(ScreenSide::Right);
#else

#endif
}

void BatteryScreen::render(DisplaySystem &display) {
    uint16_t width = 0, height = 0;
    display.setFont(static_cast<Font *>(&fontFace0));
    auto batteryStatus = battery_.getBatteryLifeString();

    auto ptBatteryPct = PreferencesStore::lastPageTurnerBattery;
    auto ptBuild = prefs_.lastPageTurnerBuild.get().value_or("Unknown build");
    auto ptBatteryStatus = StringFormat("Page Turner: %d%% (%s)", ptBatteryPct, ptBuild.c_str());
    width = display.getTextWidth(ptBatteryStatus);
    height = display.getTextHeight(ptBatteryStatus);
    display.drawSingleLineOfText(ptBatteryStatus, (display.width() - width) / 2,
                                 (display.height() + height) / 2 + 2);

    batteryStatus += StringFormat("  L: %dC  R: %dC", leftTemp_, rightTemp_);

    width = display.getTextWidth(batteryStatus);
    height = display.getTextHeight(batteryStatus);

    display.drawSingleLineOfText(batteryStatus, (display.width() - width) / 2,
                                 (display.height() - height) / 2 - 2);

    display.updateDisplay();
}

void BatteryScreen::processCommand(RemoteCommand command) { back(); }
