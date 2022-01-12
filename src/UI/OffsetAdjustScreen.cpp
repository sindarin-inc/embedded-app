#include "UI/OffsetAdjustScreen.hpp"

#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 3

void OffsetAdjustScreen::render() {
    if (!needs_rerender) {
        return;
    }

    std::string message;
    for (int i = 0; i < 5; i++) {
        message += "Press arrows to move box around to your liking. Then "
                   "long-press the left button when done! Message repeats. ";
    }

    this->renderer.set_string(message);
    this->renderer.render_page_with_box(0);

    needs_rerender = false;
}

void OffsetAdjustScreen::process_command(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        xOffset -= OFFSET_ADJUST_AMOUNT;
        needs_rerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        xOffset += OFFSET_ADJUST_AMOUNT;
        needs_rerender = true;
        break;
    case RemoteCommand::PREV:
        yOffset -= OFFSET_ADJUST_AMOUNT;
        needs_rerender = true;
        break;
    case RemoteCommand::NEXT:
        yOffset += OFFSET_ADJUST_AMOUNT;
        needs_rerender = true;
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
        prefs.localXOffset.set(xOffset);
        prefs.localYOffset.set(yOffset);
        log_d("Saving x and y offset to preferences: (%d, %d)", xOffset, yOffset);
        back_callback();
        break;
    default:
        break;
    }
    renderer.local_x_offset = xOffset;
    renderer.local_y_offset = yOffset;
}
