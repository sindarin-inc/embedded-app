#include "UI/BitmapScreen.hpp"

#include "Displays/DisplaySystem.hpp"

void BitmapScreen::render(DisplaySystem &display) {
    display.fillScreen(display.bg);

    // TODO: We should figure out what we really want for these 'full-screen' bitmaps
    // so that they can use the screen offset settings as well. Probably we want 'full-screen'
    // to mean the full width of the focus area, in which case we shouldn't hardcode 256 here.

    // By default, we draw the bitmap in the middle of the screen. TODO: add the screen offset if
    // we'd like. This is one step closer to that, however.
    display.drawXBitmap((display.width() - width_) / 2, (display.height() - height_) / 2, bitmap_,
                        width_, height_, display.fg);
    display.updateDisplay();
}

void BitmapScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::PREV:
    case RemoteCommand::NEXT:
    case RemoteCommand::BACK:
        back();
        break;
    default:
        break;
    }
}