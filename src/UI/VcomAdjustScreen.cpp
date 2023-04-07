#if EINKET013TT1
#include "UI/VcomAdjustScreen.hpp"

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Fonts/LucidaGrande10pt7b.h"
#include "UI/Fonts/LucidaGrande5pt7b.h"
#include "sindarin-debug.h"

#define STEP_IN_MV 50
#define MIN_VCOM_IN_MV (-4500)
#define MAX_VCOM_IN_MV (-3500)

VcomAdjustScreen::VcomAdjustScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), renderer_(display), prefs_(prefs) {

    auto *driver = (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver();
    vCom = driver->getVcom();
}

void VcomAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    display.setFont(&LUCIDA_GRANDE10PT7B);

    auto vcomStr = StringFormat("%dmV", vCom);

    int vcomStrWidth = display.getTextWidth(vcomStr.c_str());
    int vcomStrHeight = display.getTextHeight();

    // This doesn't seem like the right interface.
    // We should use the DisplaySystem.drawSingleLineOfText here
    display.setCursor(display.width() / 2 - vcomStrWidth / 2,
                      display.height() / 2 - vcomStrHeight / 2);
    display.println(vcomStr.c_str());

    display.updateDisplay();

    display.setFont(&LUCIDA_GRANDE5PT7B);
    needsRerender = false;
}

void VcomAdjustScreen::processCommand(RemoteCommand command) {
    auto *driver = (static_cast<DisplayEinkET013TT1 *>(&display_))->getDriver();
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::ENTER:
        prefs_.vcom.set(vCom);
        log_d("Saving vcom to preferences: (%d)", vCom);
        backCallback();
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        vCom += STEP_IN_MV;
        if (vCom > MAX_VCOM_IN_MV) {
            vCom = MAX_VCOM_IN_MV;
        }
        driver->setVcom(vCom);
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        vCom -= STEP_IN_MV;
        if (vCom < MIN_VCOM_IN_MV) {
            vCom = MIN_VCOM_IN_MV;
        }
        driver->setVcom(vCom);
        needsRerender = true;
        break;
    default:
        break;
    }
}

#endif
