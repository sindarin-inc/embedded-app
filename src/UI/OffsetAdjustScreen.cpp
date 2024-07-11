#include "UI/OffsetAdjustScreen.hpp"

#include <memory>

#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 5

OffsetAdjustScreen::OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       Telemetry &telemetry)
    : display_(display), renderer_(display), prefs_(prefs), telemetry_(telemetry) {

    std::string message;
    // This needs to be pretty big for smaller fonts
    message += "Press arrows to move box around to your liking. Then "
               "press \ue000 (center) when done!\n\n";
    message +=
        "Beneath a rugged exterior, Pirate Captain O'Malley harbored a love for poetry. Between "
        "skirmishes and treasure hunts, he'd pen verses inspired by the sea's beauty. A stolen, "
        "enchanted quill from a captured merchant vessel soon revealed its magic, making his poems "
        "come to life. The images he wrote danced in the air, telling tales of love, adventure, "
        "and the sea. O'Malley's crew, once fierce and merciless, became entranced by the art of "
        "words, turning their ship into a floating haven of poetry and wonder. "
        "However, the enchanted quill's magic drew the ire of Poseidon, god of the sea. Waves "
        "turned treacherous, testing the pirate crew's newfound love for poetry. Frustrated, "
        "O'Malley used the quill to write a poem addressing Poseidon. The god appeared, "
        "challenging O'Malley to a duel of verses. Poseidon's poetry conjured storms; O'Malley's "
        "verses evoked calm seas.";
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
}

void OffsetAdjustScreen::render(DisplaySystem &display) {
    // The natural line spacing handles the vertical padding
    // The horizontal padding adds one blank pixel column on either side of the rect
    display.drawDottedBox(-2, 0, display_.width() + 1, display_.height() - 1, 1, 2);
    this->renderer_.renderFirstPage(0, true);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void OffsetAdjustScreen::processCommand(RemoteCommand command) {
    DisplayInset oldInsets = display_.getDisplayInsets();
    switch (command) {
    case RemoteCommand::LEFT:
        if (oldInsets.left > display_.getMinInsets().left - OFFSET_ADJUST_AMOUNT) {
            oldInsets.left -= OFFSET_ADJUST_AMOUNT;
            oldInsets.right += OFFSET_ADJUST_AMOUNT;
        } else {
            oldInsets.right += (oldInsets.left - display_.getMinInsets().left);
            oldInsets.left = display_.getMinInsets().left;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::RIGHT:
        if (oldInsets.right > display_.getMinInsets().right - OFFSET_ADJUST_AMOUNT) {
            oldInsets.right -= OFFSET_ADJUST_AMOUNT;
            oldInsets.left += OFFSET_ADJUST_AMOUNT;
        } else {
            oldInsets.left += (oldInsets.right - display_.getMinInsets().right);
            oldInsets.right = display_.getMinInsets().right;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
        if (oldInsets.top > display_.getMinInsets().top - OFFSET_ADJUST_AMOUNT) {
            oldInsets.top -= OFFSET_ADJUST_AMOUNT;
            oldInsets.bottom += OFFSET_ADJUST_AMOUNT;
        } else {
            oldInsets.bottom += (oldInsets.top - display_.getMinInsets().top);
            oldInsets.top = display_.getMinInsets().top;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        if (oldInsets.bottom > display_.getMinInsets().bottom - OFFSET_ADJUST_AMOUNT) {
            oldInsets.bottom -= OFFSET_ADJUST_AMOUNT;
            oldInsets.top += OFFSET_ADJUST_AMOUNT;
        } else {
            oldInsets.top += (oldInsets.bottom - display_.getMinInsets().bottom);
            oldInsets.bottom = display_.getMinInsets().bottom;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        oldInsets.left += OFFSET_ADJUST_AMOUNT;
        oldInsets.right -= OFFSET_ADJUST_AMOUNT;
        display_.setTemporaryInsets(oldInsets);
        [[fallthrough]];
    case RemoteCommand::ENTER:
        display_.storeInsets();
        telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                 {{"in_t", display_.getDisplayInsets().top},
                                  {"in_b", display_.getDisplayInsets().bottom},
                                  {"in_l", display_.getDisplayInsets().left},
                                  {"in_r", display_.getDisplayInsets().right}});

        back();
        break;
    default:
        break;
    }

    display_.setTemporaryInsets(oldInsets);
}
