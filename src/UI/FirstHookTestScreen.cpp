#include "UI/FirstHookTestScreen.hpp"

#include <memory>

#include "UI/Fonts.hpp"
#include "sindarin-debug.h"

#define OFFSET_ADJUST_AMOUNT 5

FirstHookTestScreen::FirstHookTestScreen(DisplaySystem &display, PreferencesStore &prefs,
                                         Telemetry &telemetry)
    : display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs), static_cast<Font *>(&fontFace0)),
      prefs_(prefs), telemetry_(telemetry) {

    std::string message;
    // This needs to be pretty big for smaller fonts
    message += "It seems that you've found a multivac image in the text.\n\n And this also "
               "demonstrates the ability for us to hook out of a readable into ";
    message += "a totally different screen. This is a test of the first hook. Press any ";
    message += "button to get back to your reading!";
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
}

void FirstHookTestScreen::render(DisplaySystem &display) {
    // The natural line spacing handles the vertical padding
    // The horizontal padding adds one blank pixel column on either side of the rect
    display.drawDottedBox(-2, 0, display_.width() + 1, display_.height() - 1, 1, 2);
    this->renderer_.renderFirstPage(0, true);
    display.updateDisplay(DisplaySystem::UpdateType::FULL);
}

void FirstHookTestScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
    case RemoteCommand::RIGHT:
    case RemoteCommand::UP:
    case RemoteCommand::DOWN:
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
        back();
        break;
    default:
        break;
    }
}
