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
      boxWidthOffset_((float)prefs.displayBoxWidthOffsetMm.get().value()),
      boxHeightOffset_((float)prefs.displayBoxHeightOffsetMm.get().value()), renderer_(display) {

    std::string message;
    for (int i = 0; i < 15; i++) {
        message +=
            "Press up arrow to increase vertical dimension, down arrow to decrease vertical "
            "dimension, left arrow to increase horizontal dimension, right arrow to decrease "
            "horizontal dimension. Press and hold left arrow when finished. Message repeats. ";
    }
    renderer_.setString(message);
}

void ResizeBoxScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPage(0);

    needsRerender = false;
}

void ResizeBoxScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        boxWidthOffset_ += BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        boxWidthOffset_ -= BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        boxHeightOffset_ += BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        boxHeightOffset_ -= BOX_OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        boxWidthOffset_ -= BOX_OFFSET_ADJUST_AMOUNT;
    case RemoteCommand::ENTER:
        prefs_.displayBoxWidthOffsetMm.set((int)boxWidthOffset_);
        prefs_.displayBoxHeightOffsetMm.set((int)boxHeightOffset_);
        log_d("Saving box width and height to preferences: (%.0f, %.0f)", boxWidthOffset_,
              boxHeightOffset_);
        backCallback();
        break;
    default:
        break;
    }

    log_d("boxWidth and boxHeight offsets are %f %f", boxWidthOffset_, boxHeightOffset_);

    log_d("setting new focus size to be %f %f", display_.getFocusWidth() + boxWidthOffset_,
          display_.getFocusHeight() + boxHeightOffset_);
    display_.setFocusOffset(boxWidthOffset_, boxHeightOffset_);
}
