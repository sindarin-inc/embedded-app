#include "UI/ResizeBoxScreen.hpp"

#include "sindarin-debug.h"

// This is in mm for now, which isn't the best, but easier than
// adding all the Display accessors to do it via pixels.
// For our displays, 0.273mm is about 3 pixels.
#define BOX_OFFSET_ADJUST_AMOUNT 0.273

// We will want a more intuitive way to do this in the final system,
// but since it's so dependent on what the PageTurner ends up being,
// I didn't want to over-engineer it here.
ResizeBoxScreen::ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), prefs_(prefs), power_(power),
      boxWidthOffset(prefs.displayBoxWidthOffset.get().value()),
      boxHeightOffset(prefs.displayBoxHeightOffset.get().value()), renderer_(display) {

    std::string message;
    for (int i = 0; i < 5; i++) {
        message +=
            "Press up arrow to increase vertical dimension, down arrow to decrease vertical "
            "dimension, left arrow to increase horizontal dimension, right arrow to decrease "
            "horizontal dimension. Press and hold left arrow when finished. Message repeats.";
    }
    renderer_.setString(message);
}

void ResizeBoxScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPageWithBox(0);

    needsRerender = false;
}

void ResizeBoxScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        boxWidthOffset += BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        boxWidthOffset -= BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        boxHeightOffset += BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        boxHeightOffset -= BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        boxWidthOffset -= BOX_OFFSET_ADJUST_AMOUNT;
    case RemoteCommand::ENTER:
        prefs_.displayBoxWidthOffset.set(boxWidthOffset);
        prefs_.displayBoxHeightOffset.set(boxHeightOffset);
        log_d("Saving box width and height to preferences: (%d, %d)", boxWidthOffset,
              boxHeightOffset);
        backCallback();
        break;
    default:
        break;
    }

    log_d("boxWidth and boxHeight offsets are %f %f", boxWidthOffset, boxHeightOffset);

    log_d("setting new focus size to be %f %f", display_.getFocusWidth() + boxWidthOffset,
          display_.getFocusHeight() + boxHeightOffset);
    display_.setFocusOffset(boxWidthOffset, boxHeightOffset);
}
