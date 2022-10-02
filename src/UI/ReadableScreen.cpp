#include "UI/ReadableScreen.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

void ReadableScreen::updateBookmarkStatus() { inBookmark_ = false; }

void ReadableScreen::writeOutBookmarkUpdate() {
    std::string plusOrMinus = inBookmark_ ? "+ " : "- ";
    plusOrMinus += std::to_string(readable_.place) + " " + std::to_string(newPlace_) + "\n";
    bookmarkFile_.write((const uint8_t *)plusOrMinus.c_str(), plusOrMinus.length());
    bookmarkFile_.flush();
}

void ReadableScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }
    if (!needsRerender) {
        return;
    }

    display.setFont(display.style.getFontFamily().readableFont);

    newPlace_ = renderer_.renderPage(readable_.place, inBookmark_);
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
    case RemoteCommand::CHANNEL_MINUS:
        readable_.place = renderer_.getPreviousPlace();
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        updateBookmarkStatus();
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:

        readable_.placePrev = readable_.place;
        readable_.place = newPlace_;
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        updateBookmarkStatus();
        needsRerender = true;
        break;

    case RemoteCommand::NEXT:
        setCurrentScreen(popup_);
        break;
    case RemoteCommand::PREV:
        inBookmark_ = !inBookmark_;
        writeOutBookmarkUpdate();
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
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