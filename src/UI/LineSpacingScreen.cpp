#include "UI/LineSpacingScreen.hpp"

#include "sindarin-debug.h"

#define MIN_LINE_SPACING (-2)
#define MAX_LINE_SPACING 5

LineSpacingScreen::LineSpacingScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), renderer_(display), prefs_(prefs),
      lineSpacing_(prefs.lineSpacing.get().value()) {

    std::string message;
    for (int i = 0; i < 200; i++) {
        message += "Press up arrow to increase line spacing, down arrow to decrease line spacing. "
                   "Press center button when finished. Message repeats. ";
    }
    auto stream = new StdStringStream(message);
    renderer_.setStream(stream);
}

void LineSpacingScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    this->renderer_.renderPage(0);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

    needsRerender = false;
}

void LineSpacingScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
        // Intentionally left out for now
        break;
    case RemoteCommand::PREV:
        lineSpacing_++;
        if (lineSpacing_ > MAX_LINE_SPACING) {
            lineSpacing_ = MAX_LINE_SPACING;
        }

        display_.setLineSpacing(lineSpacing_);
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        lineSpacing_--;
        if (lineSpacing_ < MIN_LINE_SPACING) {
            lineSpacing_ = MIN_LINE_SPACING;
        }
        display_.setLineSpacing(lineSpacing_);
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        // If CHANNEL_MINUS is implemented above,
        // undo that here, since you get both PREV and BACK on a longpress
    case RemoteCommand::ENTER:
        prefs_.lineSpacing.set(lineSpacing_);
        log_d("Saving lineSpacing to preferences: (%d)", lineSpacing_);
        backCallback();
        break;
    default:
        break;
    }

    log_d("setting new lineSpacing to be %d", display_.getLineSpacing());
}
