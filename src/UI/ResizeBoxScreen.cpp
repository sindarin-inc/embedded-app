#include "UI/ResizeBoxScreen.hpp"

#include "sindarin-debug.h"

#define BOX_OFFSET_ADJUST_AMOUNT 10

// We will want a more intuitive way to do this in the final system,
// but since it's so dependent on what the PageTurner ends up being,
// I didn't want to over-engineer it here.
ResizeBoxScreen::ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs,
                                 Telemetry &telemetry)
    : display_(display), renderer_(display), prefs_(prefs), telemetry_(telemetry) {

    std::string message;
    message += "Press up to increase vertical dimension, down to decrease vertical "
               "dimension, left to increase horizontal dimension, right to decrease "
               "horizontal dimension. Press \ue000 (center) when finished.\n\n";
    message +=
        "In a secluded tower lived a wizard named Elinor, obsessed with reading. Her passion lay "
        "not in spells but in stories. One ancient tome enchanted her; its runes sparkled with "
        "hidden wisdom. Night after night, she read, the runes dancing around her. Unbeknownst to "
        "her, they were weaving a spell, granting her unparalleled wisdom. Elinor's library became "
        "legendary, each book a magical gateway, all thanks to her love for reading. "
        "Elinor soon discovered her wisdom had limits; the books ran out. Venturing outside, she "
        "sought new stories. She stumbled upon a village terrorized by a dragon. Wisdom in hand, "
        "she orchestrated a plan. The dragon left, villagers hailed her as a hero. But Elinor "
        "found no joy; her wisdom couldn't decipher the dragon's untold tale.";
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
}

void ResizeBoxScreen::render(DisplaySystem &display) {
    DisplayRect currRect = display_.getBoundingBox();
    // The natural line spacing handles the vertical padding
    // The horizontal padding adds one blank pixel column on either side of the rect
    display.drawDottedBox(-2, 0, currRect.width + 1, currRect.height - 1, 1, 2);
    this->renderer_.renderFirstPage(0, true);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void ResizeBoxScreen::processCommand(RemoteCommand command) {
    DisplayRect oldRect = display_.getBoundingBox();
    switch (command) {
    case RemoteCommand::LEFT:
        oldRect.width += BOX_OFFSET_ADJUST_AMOUNT;
        // Don't move the y if we're at the minimum height
        oldRect.x -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        setNeedsRerender();
        break;
    case RemoteCommand::RIGHT: {
        // We apply the rect's bounding box in DisplaySystem, but we need to make sure to not adjust
        // the X if we're already at the minimum
        auto targetWidth = std::max(DisplaySystem::MIN_BOUNDING_BOX.width,
                                    oldRect.width - BOX_OFFSET_ADJUST_AMOUNT);
        auto subtractWidth = oldRect.width - targetWidth;
        oldRect.width -= subtractWidth;
        oldRect.x += subtractWidth / 2;
        setNeedsRerender();
        break;
    }
    case RemoteCommand::UP:
        oldRect.height += BOX_OFFSET_ADJUST_AMOUNT;
        oldRect.y -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN: {
        // We apply the rect's bounding box in DisplaySystem, but we need to make sure to not adjust
        // the Y if we're already at the minimum
        auto targetHeight = std::max(DisplaySystem::MIN_BOUNDING_BOX.height,
                                     oldRect.height - BOX_OFFSET_ADJUST_AMOUNT);
        auto subtractHeight = oldRect.height - targetHeight;
        oldRect.height -= subtractHeight;
        oldRect.y += subtractHeight / 2;
        setNeedsRerender();
        break;
    }
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

        telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                 {{"bb_x", display_.getBoundingBox().x},
                                  {"bb_y", display_.getBoundingBox().y},
                                  {"bb_w", display_.getBoundingBox().width},
                                  {"bb_h", display_.getBoundingBox().height}});

        back();
        break;
    default:
        break;
    }

    log_d("New bounding box is: (%s)", display_.getBoundingBox().toString().c_str());

    display_.setBoundingBox(oldRect);
}
