#include "ToggleCompressedReadablesScreen.hpp"

#include "Communications/SindarinAPI.hpp"
#include "sindarin-debug.h"

void ToggleCompressedReadablesScreen::render(DisplaySystem &display) {
    log_i("compressed readables RENDER");
    if (!needsRerender) {
        return;
    }

    if (prefs_.gzipReadables.get().value_or(false)) {
        display.drawSingleLineOfCenteredText("Compressed Readables ON");
    } else {
        display.drawSingleLineOfCenteredText("Compressed Readables OFF");
    }

    display.updateDisplay(DisplaySystem::UpdateType::FULL);
    needsRerender = false;
}

void ToggleCompressedReadablesScreen::processCommand(RemoteCommand command) { backCallback(); }

void ToggleCompressedReadablesScreen::begin() {
    if (prefs_.gzipReadables.get().value_or(false)) {
        prefs_.gzipReadables.set(false);
    } else {
        prefs_.gzipReadables.set(true);
    }

    render(display_);
}
