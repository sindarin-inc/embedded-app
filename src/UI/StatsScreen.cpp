#include "UI/StatsScreen.hpp"

#include "UI/Fonts/Tahoma8pt7b.h"
#include "sindarin-debug.h"

StatsScreen::StatsScreen(DisplaySystem &display, PreferencesStore &prefs)
    : display_(display), prefs_(prefs) {}

void StatsScreen::render(DisplaySystem &display) {
    auto wordCount = prefs_.wordCount.get().value_or(0);
    auto totalReadingTimeSeconds = prefs_.totalReadingTimeSeconds.get().value_or(0);

    // get time components
    auto secondsRead = totalReadingTimeSeconds % 60;
    auto totalMinutesRead = (totalReadingTimeSeconds - secondsRead) / 60;
    auto minutesRead = totalMinutesRead % 60;
    auto hoursRead = totalMinutesRead / 60;

    std::string wordsMessage = "Words Read: " + std::to_string(wordCount);
    std::string timeMessage = "Time Read: ";
    char formattedString[32];
    if (hoursRead > 0) {
        // NOLINTNEXTLINE(cert-err33-c) -- this is a safe, fixed length, format string
        snprintf(formattedString, sizeof(formattedString),
                 "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "", hoursRead, minutesRead, secondsRead);
        timeMessage += formattedString;
    } else {
        // NOLINTNEXTLINE(cert-err33-c) -- this is a safe, fixed length, format string
        snprintf(formattedString, sizeof(formattedString), "%02" PRIu32 ":%02" PRIu32 "",
                 minutesRead, secondsRead);
        timeMessage += formattedString;
    }

    display.setFont(&TAHOMA_REGULAR_FONT5PT7B);
    display.drawSingleLineOfText(
        wordsMessage,
        display.xAbsolute(display.xAbsoluteCenter() -
                          display.getTextWidth(wordsMessage.c_str()) / 2),
        display.yAbsolute(display.yAbsoluteCenter() - (display.getTextHeight() * 2 + 1) / 2));
    display.drawSingleLineOfText(timeMessage,
                                 display.xAbsolute(display.xAbsoluteCenter() -
                                                   display.getTextWidth(timeMessage.c_str()) / 2),
                                 display.yAbsolute(display.yAbsoluteCenter() -
                                                   (display.getTextHeight() * 2 + 1) / 2 +
                                                   display.getTextHeight() + 1));
    display.updateDisplay();
}

void StatsScreen::processCommand(RemoteCommand command) { back(); }
