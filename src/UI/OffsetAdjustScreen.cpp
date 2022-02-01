#include "UI/OffsetAdjustScreen.hpp"

#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 3

OffsetAdjustScreen::OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       Power &power)
    : display_(display), prefs_(prefs), power_(power), xOffset(prefs.localXOffset.get().value()),
      yOffset(prefs.localYOffset.get().value()), renderer_(display) {

    std::string message;
    for (int i = 0; i < 15; i++) {
        message += "Press arrows to move box around to your liking. Then "
                   "long-press the left button when done! Message repeats. ";
    }
    renderer_.setString(message);
}

void OffsetAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPageWithBox(0);

    needsRerender = false;
}

void OffsetAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        xOffset -= OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        xOffset += OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        yOffset -= OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        yOffset += OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        xOffset += OFFSET_ADJUST_AMOUNT;
    case RemoteCommand::ENTER:
        prefs_.localXOffset.set(xOffset);
        prefs_.localYOffset.set(yOffset);
        log_d("Saving x and y offset to preferences: (%d, %d)", xOffset, yOffset);
        backCallback();
        break;
    default:
        break;
    }
    display_.localXOffset = xOffset;
    display_.localYOffset = yOffset;
}
