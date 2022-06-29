#include "UI/StatsScreen.hpp"

#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma7pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"
#include "UI/Fonts/Tahoma9pt7b.h"
#include "sindarin-debug.h"

StatsScreen::StatsScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
    : display_(display), renderer_(display), prefs_(prefs), power_(power) {}

void StatsScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    //std::string message = messages_[currentScreen_];
    auto wordCount = prefs_.wordCount.get().value();
    std::string message = "Words Read: " + std::to_string(wordCount);

    auto stream = new StdStringStream(message);
    display.setFont(&TAHOMA_REGULAR_FONT8PT7B);

    renderer_.setStream(stream);
    renderer_.setHorizontalCenter(true);
    renderer_.setVerticalCenter(true);

    this->renderer_.renderPage(0);

    needsRerender = false;
}

void StatsScreen::processCommand(RemoteCommand command) {
    backCallback();
}
