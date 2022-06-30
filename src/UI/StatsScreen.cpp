#include "UI/StatsScreen.hpp"

#include "UI/Fonts/Tahoma8pt7b.h"
#include "sindarin-debug.h"

StatsScreen::StatsScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), prefs_(prefs) {}

void StatsScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    auto wordCount = prefs_.wordCount.get().value();
    auto totalReadingTimeSeconds = prefs_.totalReadingTimeSeconds.get().value();

    // get time components
    auto secondsRead = totalReadingTimeSeconds % 60;
    auto totalMinutesRead = (totalReadingTimeSeconds - secondsRead) / 60;
    auto minutesRead = totalMinutesRead % 60;
    auto hoursRead = totalMinutesRead / 60;

    std::string message = "Words Read: " + std::to_string(wordCount);
    message += "\nTime Read: ";
    char formattedString[32];
    if (hoursRead > 0) {
        sprintf(formattedString, "%02d:%02d:%02d", hoursRead, minutesRead, secondsRead);
        message += formattedString;
    } else {
        sprintf(formattedString, "%02d:%02d", minutesRead, secondsRead);
        message += formattedString;
    }

    display.setFont(&TAHOMA_REGULAR_FONT8PT7B);
    display.drawTextInBox(message, 0, 0, display.width(), display.height());
    needsRerender = false;
}

void StatsScreen::processCommand(RemoteCommand command) { backCallback(); }
