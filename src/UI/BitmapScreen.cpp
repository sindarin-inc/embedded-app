#include "UI/BitmapScreen.hpp"

#include "config.h"

#include "Displays/DisplaySystem.hpp"

void BitmapScreen::render(DisplaySystem &display) {
    // Ideally this would just use dynamic_cast plus the null check,
    // but it appears ESP-IDF doesn't necessarily play nice yet with RTTI enabled,
    // so we will hack it with #ifdef for now... -cojo
    if (!needsRerender) {
        return;
    }
    display.fillScreen(display.bg);

    // TODO: We should figure out what we really want for these 'full-screen' bitmaps
    // so that they can use the screen offset settings as well. Probably we want 'full-screen'
    // to mean the full width of the focus area, in which case we shouldn't hardcode 256 here.

    // By default, we draw the bitmap in the middle of the screen. TODO: add the screen offset if
    // we'd like. This is one step closer to that, however.
    display.drawXBitmap((display.width() - width_) / 2, (display.height() - height_) / 2, bitmap_,
                        width_, height_, display.fg);

    needsRerender = false;
}

void BitmapScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::PREV:
    case RemoteCommand::NEXT:
    case RemoteCommand::BACK:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}