#include "UI/ResizeBoxScreen.hpp"

#include "UI/Fonts/Fonts.hpp"
#include "sindarin-debug.h"

#define BOX_OFFSET_ADJUST_AMOUNT 10

// We will want a more intuitive way to do this in the final system,
// but since it's so dependent on what the PageTurner ends up being,
// I didn't want to over-engineer it here.
ResizeBoxScreen::ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs,
                                 Telemetry &telemetry)
    : EinkView(display), display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs),
                FontFromPrefs(prefs.font.get().value_or(PREFS_SOL_SMALL))),
      prefs_(prefs), telemetry_(telemetry), rendererRef_(renderer_), outerBoxView_(display, 5),
      backgroundBoxView_(display, 5), explanationView_(display, 5) {
    std::string message;
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

    rendererRef_.setStream(stream);
    rendererRef_.setLineSpacing(prefs.lineSpacing.get().value_or(0));

    setupInstructions();
}

ResizeBoxScreen::ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs,
                                 Telemetry &telemetry, Renderer &renderer,
                                 std::shared_ptr<Place> place)
    : EinkView(display), display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs),
                FontFromPrefs(prefs.font.get().value_or(PREFS_SOL_SMALL))),
      place_(std::move(place)), prefs_(prefs), telemetry_(telemetry), rendererRef_(renderer),
      outerBoxView_(display, 5), backgroundBoxView_(display, 5), explanationView_(display, 5) {
    setupInstructions();
}

auto ResizeBoxScreen::setupInstructions() -> void {
    std::string instructions = "\ue003  Stretch vertical\n";
    instructions += "\ue004  Shrink vertical\n";
    instructions += "\ue002   Stretch horizontal\n";
    instructions += "\ue001   Shrink horizontal\n";
    instructions += "\ue000  Done";

    explanationView_.setText(instructions);
    explanationView_.dynamicWidth = true;

    setNeedsLayout();
}

auto ResizeBoxScreen::layout(LayoutContext &context, Size size) -> Size {
    auto explanationFrame = context.setViewFrame(explanationView_, Rect::fromSize(size),
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
    auto outerBoxViewFrame = context.setViewFrame(outerBoxView_, explanationFrame.inset(-5));
    context.setViewFrame(backgroundBoxView_, outerBoxViewFrame.inset(-8));

    return size;
}

void ResizeBoxScreen::drawInRect(Rect rect) {
    outerBoxView_.fillColor = display_.bg;
    outerBoxView_.strokeColor = display_.fg;
    backgroundBoxView_.fillColor = display_.bg;
    backgroundBoxView_.strokeColor = display_.bg;

    backgroundBoxView_.draw();
    outerBoxView_.draw();
    explanationView_.draw();
}

void ResizeBoxScreen::render(DisplaySystem &display) {
    // Get the bounding box
    auto textBox = GetTextRectFromPrefs(display, prefs_);

    setFrame(defaultFrameRect());

    rendererRef_.setRect(textBox);
    if (place_ == nullptr) {
        rendererRef_.renderFirstPage(0, true);
    } else {
        rendererRef_.renderPage(place_, 0);
    }

    draw();

    // The natural line spacing handles the vertical padding
    // The horizontal padding adds one blank pixel column on either side of the rect
    display.drawDottedBox(textBox.x - 2, textBox.y, textBox.x + textBox.width + 1,
                          textBox.y + textBox.height - 1, 1, 2);

    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
}

void ResizeBoxScreen::processCommand(RemoteCommand command) {
    DisplayInset insets = GetTextInsetFromPrefs(display_, prefs_);
    DisplayRect rect = GetTextRectFromPrefs(display_, prefs_);
    switch (command) {
    case RemoteCommand::RIGHT:
        if (insets.left - BOX_OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().left) {
            insets.left -= BOX_OFFSET_ADJUST_AMOUNT / 2;
            setNeedsRerender();
        }
        if (insets.right - BOX_OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().right) {
            insets.right -= BOX_OFFSET_ADJUST_AMOUNT / 2;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::LEFT:
        if (rect.width - BOX_OFFSET_ADJUST_AMOUNT >= display_.minTextBoxWidth()) {
            insets.right += BOX_OFFSET_ADJUST_AMOUNT / 2;
            insets.left += BOX_OFFSET_ADJUST_AMOUNT / 2;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::UP:
        if (insets.top - BOX_OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().top) {
            insets.top -= BOX_OFFSET_ADJUST_AMOUNT / 2;
            setNeedsRerender();
        }
        if (insets.bottom - BOX_OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().bottom) {
            insets.bottom -= BOX_OFFSET_ADJUST_AMOUNT / 2;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::DOWN:
        if (rect.height - BOX_OFFSET_ADJUST_AMOUNT >= display_.minTextBoxHeight()) {
            insets.top += BOX_OFFSET_ADJUST_AMOUNT / 2;
            insets.bottom += BOX_OFFSET_ADJUST_AMOUNT / 2;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::BACK:
        // The Turn Touch also sends a left keypress when long-pressing back so let's undo that
        // keystroke before saving.
        // TODO: This obviously doesn't do the right thing using the dedicated back button in the
        // app but since we're using TurnTouch for demos, I think it's ok for now. In the future, it
        // might be good to pass more data to `process_command` including which remote is being used
        // so that we can have remote-specific workarounds if needed.
        insets.right += BOX_OFFSET_ADJUST_AMOUNT / 2;
        insets.left += BOX_OFFSET_ADJUST_AMOUNT / 2;
        [[fallthrough]];
    case RemoteCommand::ENTER:
        telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"in_l", insets.left},
                                                                {"in_r", insets.right},
                                                                {"in_t", insets.top},
                                                                {"in_b", insets.bottom}});
        SetTextInset(prefs_, insets);
        back();
        return;
    default:
        break;
    }

    SetTextInset(prefs_, insets);
}
