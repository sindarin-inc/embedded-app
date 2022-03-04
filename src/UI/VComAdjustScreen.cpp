#include "UI/VComAdjustScreen.hpp"

#include "Misc/StringUtil.hpp"
#include "UI/Fonts/LucidaGrande10pt7b.h"
#include "UI/Fonts/LucidaGrande5pt7b.h"
#include "sindarin-debug.h"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"

#define STEP_IN_MV 50
#define MIN_VCOM_IN_MV -4500
#define MAX_VCOM_IN_MV -3500

VComAdjustScreen::VComAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), prefs_(prefs), power_(power), renderer_(display) {

    auto *driver = (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver();
    vCom = driver->getVcom();
}

void VComAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    auto *driver = (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver();
    driver->setVcom(vCom);

    display.setFont(&LUCIDA_GRANDE10PT7B);

    int vcomStrWidth = display.getTextWidth(StringFormat("%d", vCom).c_str());
    int vcomStrHeight = display.getTextHeight();

    display.setCursor(display.width() / 2 - vcomStrWidth / 2,
                      display.height() / 2 - vcomStrHeight / 2);
    display.println(StringFormat("%d", vCom).c_str());

    display.updateDisplay();

    display.setFont(&LUCIDA_GRANDE5PT7B);
    needsRerender = false;
}

void VComAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::ENTER:
        prefs_.vCom.set(vCom);
        log_d("Saving vcom to preferences: (%d)", vCom);
        backCallback();
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        vCom += STEP_IN_MV;
        if (vCom > MAX_VCOM_IN_MV) {
            vCom = MAX_VCOM_IN_MV;
        }
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        vCom -= STEP_IN_MV;
        if (vCom < MIN_VCOM_IN_MV) {
            vCom = MIN_VCOM_IN_MV;
        }
        needsRerender = true;
        break;
    default:
        break;
    }
}

#else

VComAdjustScreen::VComAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), prefs_(prefs), power_(power), renderer_(display) {}

void VComAdjustScreen::render(DisplaySystem &display) {}

void VComAdjustScreen::processCommand(RemoteCommand command) {}

#endif
