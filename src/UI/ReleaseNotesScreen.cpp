#include "UI/ReleaseNotesScreen.hpp"

#include "sindarin-debug.h"

ReleaseNotesScreen::ReleaseNotesScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       Firmware &firmware)
    : display_(display), renderer_(display), prefs_(prefs), firmware_(firmware) {

    SpiramString message = SpiRamStringFormat(
        "New firmware %s is now available! Here's what's new:\n\n", firmware.version.c_str());
    message += firmware.releaseNotes;
    auto strinstream = SpiramStringStream(message);

    auto stream = std::make_shared<StdStringStream>(strinstream);
    renderer_.setStream(stream);
}

void ReleaseNotesScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPage(0);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

    needsRerender = false;
}

void ReleaseNotesScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
        // Intentionally left out for now
        break;
    case RemoteCommand::PREV:
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // If CHANNEL_MINUS is implemented above,
        // undo that here, since you get both PREV and BACK on a longpress
    case RemoteCommand::ENTER:
        break;
    default:
        break;
    }
}
