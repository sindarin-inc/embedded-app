#include "UI/BitmapScreen.hpp"

#include "config.h"

#include "Displays/DisplaySystem.hpp"

void BitmapScreen::render() {
    // Ideally this would just use dynamic_cast plus the null check,
    // but it appears ESP-IDF doesn't necessarily play nice yet with RTTI enabled,
    // so we will hack it with #ifdef for now... -cojo
    if (!needsRerender) {
        return;
    }
    display_.fillScreen(display_.bg);
    // TODO: We should figure out what we really want for these 'full-screen' bitmaps
    // so that they can use the screen offset settings as well. Probably we want 'full-screen'
    // to mean the full width of the focus area, in which case we shouldn't hardcode 256 here.
    display_.drawXBitmap(0, 0, bitmap_, 256, 256, display_.fg);
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