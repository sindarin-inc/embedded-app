#include "UI/BitmapScreen.hpp"

#include "config.h"

#include "Displays/DisplayEinkET013TT1.hpp"
#include "UI/Bitmaps/Calibration.hpp"

void BitmapScreen::render() {
    // Ideally this would just use dynamic_cast plus the null check,
    // but it appears ESP-IDF doesn't necessarily play nice yet with RTTI enabled,
    // so we will hack it with #ifdef for now... -cojo
    if (!needs_rerender) {
        return;
    }
#ifdef EINKET013TT1
    DisplayEinkET013TT1 *eink = (DisplayEinkET013TT1 *)renderer.get_display();
    if (eink != nullptr) {
        eink->fillScreen(eink->bg);
        eink->drawXBitmap(0, 0, bitmap, 256, 256, eink->fg);
    }
#endif
    needs_rerender = false;
}

void BitmapScreen::process_command(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::PREV:
    case RemoteCommand::NEXT:
    case RemoteCommand::BACK:
        if (back_callback != nullptr) {
            back_callback();
        }
        break;
    default:
        break;
    }
}