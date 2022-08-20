#include "UI/ResizeBoxScreen.hpp"

#include "sindarin-debug.h"

#define BOX_OFFSET_ADJUST_AMOUNT 4

// We will want a more intuitive way to do this in the final system,
// but since it's so dependent on what the PageTurner ends up being,
// I didn't want to over-engineer it here.
ResizeBoxScreen::ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {

    std::string message;
    for (int i = 0; i < 15; i++) {
        message +=
            "Press up arrow to increase vertical dimension, down arrow to decrease vertical "
            "dimension, left arrow to increase horizontal dimension, right arrow to decrease "
            "horizontal dimension. Press and hold left arrow when finished. Message repeats.\n\n";
    }
    auto stream = new StdStringStream(message);
    renderer_.setStream(stream);
}

void ResizeBoxScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPage(0);

    needsRerender = false;
}

void ResizeBoxScreen::processCommand(RemoteCommand command) {
    DisplayRect oldRect = display_.getBoundingBox();
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        oldRect.width += BOX_OFFSET_ADJUST_AMOUNT;
        oldRect.x -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        oldRect.width -= BOX_OFFSET_ADJUST_AMOUNT;
        oldRect.x += BOX_OFFSET_ADJUST_AMOUNT / 2;
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        oldRect.height += BOX_OFFSET_ADJUST_AMOUNT;
        oldRect.y -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        oldRect.height -= BOX_OFFSET_ADJUST_AMOUNT;
        oldRect.y += BOX_OFFSET_ADJUST_AMOUNT / 2;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        oldRect.width -= BOX_OFFSET_ADJUST_AMOUNT;
        oldRect.x += BOX_OFFSET_ADJUST_AMOUNT / 2;
        display_.setBoundingBox(oldRect);
        [[fallthrough]];
    case RemoteCommand::ENTER:
        prefs_.rectX.set(display_.getBoundingBox().x);
        prefs_.rectY.set(display_.getBoundingBox().y);
        prefs_.rectW.set(display_.getBoundingBox().width);
        prefs_.rectH.set(display_.getBoundingBox().height);
        log_d("Saving bounding box to preferences: (%s)",
              display_.getBoundingBox().toString().c_str());
        backCallback();
        break;
    default:
        break;
    }

    log_d("New bounding box is: (%s)", display_.getBoundingBox().toString().c_str());

    display_.setBoundingBox(oldRect);
}
