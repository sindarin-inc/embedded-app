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
    // The natural line spacing handles the vertical padding
    // The horizontal padding adds one blank pixel column on either side of the rect
    display.drawDottedBox(-2, 0, display_.width() + 1, display_.height() - 1, 1, 2);
    this->renderer_.renderFirstPage(0, true);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void ResizeBoxScreen::processCommand(RemoteCommand command) {
    DisplayInset oldInsets = display_.getDisplayInsets();
    switch (command) {
    case RemoteCommand::LEFT:
        if (oldInsets.left > display_.getMinInsets().left - BOX_OFFSET_ADJUST_AMOUNT) {
            oldInsets.left -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        } else {
            oldInsets.left = display_.getMinInsets().left;
        }
        if (oldInsets.right < display_.getMinInsets().right + BOX_OFFSET_ADJUST_AMOUNT) {
            oldInsets.right -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::RIGHT:
        oldInsets.right += BOX_OFFSET_ADJUST_AMOUNT / 2;
        oldInsets.left += BOX_OFFSET_ADJUST_AMOUNT / 2;
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
        if (oldInsets.top > display_.getMinInsets().top - BOX_OFFSET_ADJUST_AMOUNT) {
            oldInsets.top -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        } else {
            oldInsets.top = display_.getMinInsets().top;
        }
        if (oldInsets.bottom < display_.getMinInsets().bottom + BOX_OFFSET_ADJUST_AMOUNT) {
            oldInsets.bottom -= BOX_OFFSET_ADJUST_AMOUNT / 2;
        } else {
            oldInsets.bottom = display_.getMinInsets().bottom;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        oldInsets.top += BOX_OFFSET_ADJUST_AMOUNT / 2;
        oldInsets.bottom += BOX_OFFSET_ADJUST_AMOUNT / 2;
        [[fallthrough]];
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        oldInsets.right += BOX_OFFSET_ADJUST_AMOUNT / 2;
        oldInsets.left += BOX_OFFSET_ADJUST_AMOUNT / 2;
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
