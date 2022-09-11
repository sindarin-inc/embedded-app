#include "UI/ReadableScreen.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

void ReadableScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }
    if (!needsRerender) {
        return;
    }
    newPlace_ = renderer_.renderPage(readable_.place);
    log_d("%lu is the new place in the text", newPlace_);

    uint32_t wc = prefs_.wordCount.get().value();
    wc = wc + renderer_.getCurrentPageWordCount();
    prefs_.wordCount.set(wc);

    // remember the time this page was rendered
    readingStartTime_ = GetRunTimeMs();

    needsRerender = false;
}

void ReadableScreen::incrementReadingTime() {
    // increment total reading time
    auto endMillis = GetRunTimeMs();
    auto thisPageReadingTimeSeconds =
        (endMillis > readingStartTime_) ? (endMillis - readingStartTime_) / 1000 : 0;
    auto totalReadingTimeSeconds = prefs_.totalReadingTimeSeconds.get().value();
    totalReadingTimeSeconds += thisPageReadingTimeSeconds;
    prefs_.totalReadingTimeSeconds.set(totalReadingTimeSeconds);
}

void ReadableScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    auto back = [this]() {
        if (backCallback != nullptr) {
            needsRerender = true;
            backCallback();
        }
    };

    switch (command) {
    case RemoteCommand::PREV:
        readable_.place = renderer_.getPreviousPlace();
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
    case RemoteCommand::ENTER:
        readable_.placePrev = readable_.place;
        readable_.place = newPlace_;
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        needsRerender = true;
        break;

    case RemoteCommand::CHANNEL_PLUS:
        setCurrentScreen(popup_);
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        back();
        break;
    case RemoteCommand::NOP_BUTTON:
        needsRerender = true;
        break;
    default:
        break;
    }

    if (needsRerender) {
        incrementReadingTime();
    }
}