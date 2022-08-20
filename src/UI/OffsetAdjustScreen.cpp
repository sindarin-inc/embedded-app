#include "UI/OffsetAdjustScreen.hpp"

#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 3

OffsetAdjustScreen::OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {

    std::string message;
    // This needs to be pretty big for smaller fonts
    for (int i = 0; i < 30; i++) {
        message += "Press arrows to move box around to your liking. Then "
                   "long-press the left button when done! Message repeats.\n\n";
    }
    auto stream = new StdStringStream(message);
    renderer_.setStream(stream);
}

void OffsetAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPage(0);

    needsRerender = false;
}

void OffsetAdjustScreen::processCommand(RemoteCommand command) {
    DisplayRect oldRect = display_.getBoundingBox();
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        oldRect.x -= OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        oldRect.x += OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
        oldRect.y -= OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        oldRect.y += OFFSET_ADJUST_AMOUNT;
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        oldRect.x += OFFSET_ADJUST_AMOUNT;
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

    display_.setBoundingBox(oldRect);
}
