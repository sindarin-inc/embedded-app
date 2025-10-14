#include "UI/OffsetAdjustScreen.hpp"

#include <memory>
#include <utility>

#include "Displays/TextInsets.hpp"
#include "UI/Fonts.hpp"
#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 5

OffsetAdjustScreen::OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       Telemetry &telemetry)
    : EinkView(display), display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs),
                FontHandle{FontFromPrefs(prefs.font.get().value_or(PREFS_SOL_SMALL))}),
      prefs_(prefs), telemetry_(telemetry), rendererRef_(renderer_), outerBoxView_(display, 5),
      backgroundBoxView_(display, 5), explanationView_(display, 5) {
    std::string message;
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

    rendererRef_.setStream(stream);
    rendererRef_.setLineSpacing(prefs.lineSpacing.get().value_or(0));
    setupInstructions();
}

OffsetAdjustScreen::OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       Telemetry &telemetry, Renderer &renderer,
                                       std::shared_ptr<Place> place)
    : EinkView(display), display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs),
                FontHandle{FontFromPrefs(prefs.font.get().value_or(PREFS_SOL_SMALL))}),
      place_(std::move(place)), prefs_(prefs), telemetry_(telemetry), rendererRef_(renderer),
      outerBoxView_(display, 5), backgroundBoxView_(display, 5), explanationView_(display, 5) {
    setupInstructions();
}

auto OffsetAdjustScreen::setupInstructions() -> void {
    std::string instructions = "\ue003  Move up\n";
    instructions += "\ue004  Move down\n";
    instructions += "\ue001   Move left\n";
    instructions += "\ue002   Move right\n";
    instructions += "\ue000  Done";

    explanationView_.setText(instructions);
    explanationView_.dynamicWidth = true;

    setNeedsLayout();
}

auto OffsetAdjustScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto explanationFrame = context.setViewFrame(explanationView_, SolRect::fromSize(size),
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
    auto outerBoxViewFrame = context.setViewFrame(outerBoxView_, explanationFrame.inset(-5));
    context.setViewFrame(backgroundBoxView_, outerBoxViewFrame.inset(-8));

    return size;
}

void OffsetAdjustScreen::drawInRect(SolRect rect) {
    outerBoxView_.fillColor = display_.bg;
    outerBoxView_.strokeColor = display_.fg;
    backgroundBoxView_.fillColor = display_.bg;
    backgroundBoxView_.strokeColor = display_.bg;

    backgroundBoxView_.draw();
    outerBoxView_.draw();
    explanationView_.draw();
}

void OffsetAdjustScreen::render(DisplaySystem &display) {
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

void OffsetAdjustScreen::processCommand(RemoteCommand command) {
    DisplayInset insets = GetTextInsetFromPrefs(display_, prefs_);
    switch (command) {
    case RemoteCommand::LEFT:
        if (insets.left - OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().left) {
            insets.left -= OFFSET_ADJUST_AMOUNT;
            insets.right += OFFSET_ADJUST_AMOUNT;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::RIGHT:
        if (insets.right - OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().right) {
            insets.right -= OFFSET_ADJUST_AMOUNT;
            insets.left += OFFSET_ADJUST_AMOUNT;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::UP:
        if (insets.top - OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().top) {
            insets.top -= OFFSET_ADJUST_AMOUNT;
            insets.bottom += OFFSET_ADJUST_AMOUNT;
            setNeedsRerender();
        }
        break;
    case RemoteCommand::DOWN:
        if (insets.bottom - OFFSET_ADJUST_AMOUNT >= display_.minTextInsets().bottom) {
            insets.bottom -= OFFSET_ADJUST_AMOUNT;
            insets.top += OFFSET_ADJUST_AMOUNT;
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
        insets.left += OFFSET_ADJUST_AMOUNT;
        insets.right -= OFFSET_ADJUST_AMOUNT;
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
