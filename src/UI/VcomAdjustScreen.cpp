#if CONFIG_DISPLAY_EINKET013TT1
#include "UI/VcomAdjustScreen.hpp"

#include <StringUtil/StringUtil.hpp>

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Displays/TextInsets.hpp"
#include "UI/Fonts.hpp"
#include "sindarin-debug.h"
#define STEP_IN_MV 50
#define MIN_VCOM_IN_MV (-4500)
#define MAX_VCOM_IN_MV (-3500)

VcomAdjustScreen::VcomAdjustScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs), FontHandle{&fontFace0}),
      prefs_(prefs) {

    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display))->getDriver();
    vCom = driver->getVcom();
}

void VcomAdjustScreen::render(DisplaySystem &display) {
    auto vcomStr = StringFormat("%dmV", vCom);

    display.drawSingleLineOfCenteredText(vcomStr, false, static_cast<Font *>(&fontFace2));

    display.updateDisplay();
}

void VcomAdjustScreen::processCommand(RemoteCommand command) {
    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display_))->getDriver();
    switch (command) {
    case RemoteCommand::LEFT:
    case RemoteCommand::ENTER:
        prefs_.vcom.set(vCom);
        log_d("Saving vcom to preferences: (%d)", vCom);
        setNeedsRerender();
        back();
        return;
    case RemoteCommand::DOWN:
        vCom += STEP_IN_MV;
        if (vCom > MAX_VCOM_IN_MV) {
            vCom = MAX_VCOM_IN_MV;
        }
        driver->setVcom(vCom);
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
        vCom -= STEP_IN_MV;
        if (vCom < MIN_VCOM_IN_MV) {
            vCom = MIN_VCOM_IN_MV;
        }
        driver->setVcom(vCom);
        setNeedsRerender();
        break;
    default:
        break;
    }
}

#endif
