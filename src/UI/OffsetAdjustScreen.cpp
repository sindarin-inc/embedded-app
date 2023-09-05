#include "UI/OffsetAdjustScreen.hpp"

#include <memory>

#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 5

OffsetAdjustScreen::OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), renderer_(display), prefs_(prefs) {

    std::string message;
    // This needs to be pretty big for smaller fonts
    message += "Press arrows to move box around to your liking. Then "
               "press \ue000 (center) when done! Message repeats.\n\n";
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
    DisplayRect currRect = display_.getBoundingBox();
    display.drawDottedBox(-2, -2, currRect.width + 4, currRect.height + 4, 1, 2);
    this->renderer_.renderPage(0, 0, true);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void OffsetAdjustScreen::processCommand(RemoteCommand command) {
    DisplayRect oldRect = display_.getBoundingBox();
    switch (command) {
    case RemoteCommand::LEFT:
        oldRect.x -= OFFSET_ADJUST_AMOUNT;
        setNeedsRerender();
        break;
    case RemoteCommand::RIGHT:
        oldRect.x += OFFSET_ADJUST_AMOUNT;
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
        oldRect.y -= OFFSET_ADJUST_AMOUNT;
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        oldRect.y += OFFSET_ADJUST_AMOUNT;
        setNeedsRerender();
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
        back();
        break;
    default:
        break;
    }

    display_.setBoundingBox(oldRect);
}
