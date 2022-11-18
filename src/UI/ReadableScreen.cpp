#include "UI/ReadableScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <utility>

#include "Misc/Platform.hpp"
#include "UI/Bitmaps/BenIconHacking.hpp"
#include "sindarin-debug.h"

// Two minutes per page currently
#define SLOWEST_PAGE_SPEED 120000
// Change speed of speed reading in increments of 2 seconds
#define PAGE_SPEED_INCREMENT 2000

// Simply reads from the bookmark file for this Readable and returns whether it was
// a turn on or turn off bookmark.
auto ReadableScreen::readOneBookmark(File &file, Bookmark &bookmark) -> bool {
    // The first character in the line is a + or -, read that first
    if (!file.available()) {
        return false;
    }

    char firstChar = file.read();
    if (firstChar == '+') {
        bookmark.status = BOOKMARK_ON;
    } else if (firstChar == '-') {
        bookmark.status = BOOKMARK_OFF;
    } else {
        // Got an unexpected character
        return false;
    }

    // Next will be a space.
    char secondChar = file.read();
    if (secondChar != ' ') {
        return false;
    }

    String firstThing = file.readStringUntil(' ');
    bookmark.firstPlace = firstThing.toInt();

    String secondThing = file.readStringUntil('\n');
    bookmark.lastPlace = secondThing.toInt();
    return true;
}

// Once we have a real database, this will all be replaced.
// For now, since we assume that there won't be TOO many bookmarks, we simply
// start from the first bookmark action (on or off) that the user did and replay
// every bookmark action to determine if the current page is bookmarked.
//
// We assume if the midpoint of the current page is within a range of a previously
// defined bookmark, that counts. Of course, if font changes or page size changes,
// there could be cases where you have as single bookmark that shows up on multiple
// pages, but I think that's fine for now.
//
void ReadableScreen::updateBookmarkStatus() {
    std::string bookmarkPath = "/bookmarks/";
    bookmarkPath += readable_.uuid;
    bookmarkPath += "-bookmark.txt";

    inBookmark_ = false;

    File bookmarkFile = fileSystem_.open(bookmarkPath.c_str(), FILE_READ, false);
    if (!bookmarkFile) {
        log_e("Failed to open bookmark file: %s is the filename", bookmarkPath.c_str());
        return;
    }
    Bookmark bookmark;
    while (readOneBookmark(bookmarkFile, bookmark)) {
        // A decent heuristic for now. If the midpoint of the bookmark is within
        // the current page range, then turn the bookmark on for this page.
        uint32_t averagePlace =
            (bookmark.lastPlace - bookmark.firstPlace) / 2 + bookmark.firstPlace;
        if ((averagePlace >= readable_.place) && (averagePlace <= newPlace_)) {
            if (bookmark.status == BOOKMARK_ON) {
                inBookmark_ = true;
            } else {
                inBookmark_ = false;
            }
        }
    }
    bookmarkFile.close();
}

void ReadableScreen::writeOutBookmarkUpdate() {
    std::string bookmarkPath = "/bookmarks/";
    bookmarkPath += readable_.uuid;
    bookmarkPath += "-bookmark.txt";

    // create log directory in filesystem if it doesn't exist
    if (!fileSystem_.createDir("/bookmarks")) {
        log_e("could not create /bookmarks directory");
        return;
    }

    File bookmarkFile = fileSystem_.open(bookmarkPath.c_str(), FILE_APPEND, true);

    if (!bookmarkFile) {
        log_e("Failed to open bookmark file: %s is the filename", bookmarkPath.c_str());
        return;
    }

    std::string plusOrMinus = inBookmark_ ? "+ " : "- ";
    plusOrMinus += std::to_string(readable_.place) + " " + std::to_string(newPlace_) + "\n";
    bookmarkFile.write((const uint8_t *)plusOrMinus.c_str(), plusOrMinus.length());
    bookmarkFile.flush();
    bookmarkFile.close();
}

void ReadableScreen::renderIcons(DisplaySystem &display) {
    int screenWidth = display.width();
    int screenHeight = display.height();
    int arrowWidth = 8;
    int arrowHeight = 4;

    // Use a gear for the bottom icon.
    // the Y calcuation is a bit of a hack, but at least it takes into acccount line spacing
    // differences. This is one place that will benefit from more descriptive getTextHeight
    // methods.
    display.drawXBitmap(screenWidth / 2 - READ_GEAR_WIDTH / 2,
                        screenHeight + display.getTextHeight() / 2 + 3, readGearBits,
                        READ_GEAR_WIDTH, READ_GEAR_HEIGHT, display.fg);

    if (pageSpeed_ == 0) {
        // Left Arrow
        display.drawLine(-arrowHeight - 8, screenHeight / 2, -8, screenHeight / 2 - arrowWidth / 2,
                         display.fg);
        display.drawLine(-arrowHeight - 8, screenHeight / 2, -8, screenHeight / 2 + arrowWidth / 2,
                         display.fg);
    } else {
        display.drawXBitmap(-auto_minus_width - 8, screenHeight / 2 - auto_minus_height / 2 - 5,
                            autoMinusBits, auto_minus_width, auto_minus_height, display.fg);
    }

    // Up arrow - looks like a bookmark to indicate that's what you can do.
    display.drawXBitmap(screenWidth / 2 - HOME2_WIDTH / 2, -HOME2_HEIGHT - 3, homE2Bits,
                        HOME2_WIDTH, HOME2_HEIGHT, display.fg);

    if (pageSpeed_ == 0) {
        display.setRotation(1);
        // Right side show bookmark icon.
        display.drawLine(screenWidth / 2, -arrowHeight - 1, screenWidth / 2 - arrowWidth / 2, -1,
                         display.fg);
        display.drawLine(screenWidth / 2, -arrowHeight - 1, screenWidth / 2 + arrowWidth / 2, -1,
                         display.fg);

        display.drawLine(screenWidth / 2 - arrowWidth / 2, -1, screenWidth / 2 - arrowWidth / 2,
                         display.yAbsolute(0), display.fg);
        display.drawLine(screenWidth / 2 + arrowWidth / 2, -1, screenWidth / 2 + arrowWidth / 2,
                         display.yAbsolute(0), display.fg);

        display.setRotation(0);
    } else {
        display.drawXBitmap(screenWidth + 2, screenHeight / 2 - auto_plus_height / 2, autoPlusBits,
                            auto_plus_width, auto_plus_height, display.fg);
    }

    // Right Arrow
    /*
    display.drawLine(screenWidth + arrowHeight + 2, screenHeight / 2, screenWidth + 2,
                     screenHeight / 2 + arrowWidth / 2, display.fg);
    display.drawLine(screenWidth + arrowHeight + 2, screenHeight / 2, screenWidth + 2,
                     screenHeight / 2 - arrowWidth / 2, display.fg);
                     */
}

// Since this happens in a couple of places (animation as well as button press),
// we have a helper method to do the work.
void ReadableScreen::turnPage() {
    readable_.placePrev = readable_.place;
    readable_.place = newPlace_;
    prefs_.readablePlace.set(readable_.placemarkUuid.c_str(), readable_.place);
    prefs_.readablePlaceChanged.set(readable_.placemarkUuid.c_str(), true);
    speedLinesToSkip_ = 0;
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

    // If it's the first read of the book, show the help screen.
    // Then ignore the next press of the button
    if (prefs_.firstRead.get().value() == 0) {

        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message =
            "Welcome to reading with Sol! This is a one page tutorial on what each button does "
            "while reading.\n\n\nUp: Main Menu\n\nDown: Reading Settings\n\nLeft: Previous "
            "Page\n\nRight: Bookmark Page\n\nCenter: Next Page\n\n\nThat's it! Press any button... "
            "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

        auto streamFirstTime = new StdStringStream(message);
        firstTimeRenderer_.setStream(streamFirstTime);

        prefs_.firstRead.set(1);
        ignoreNextPress_ = true;
        firstTimeRenderer_.renderPage(0);
        renderIcons(display);
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        needsRerender = false;
        delete streamFirstTime;
        return;
    }

    if ((prefs_.firstAutopilotRead.get().value() == 0) && (pageSpeed_ != 0)) {
        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message =
            "Welcome to the beta of Autopilot mode! Here is what each button does "
            "while in autopilot mode.\n\n\nUp: Main Menu\n\nDown: Reading "
            "Settings\n\nLeft: Slow Down "
            "Page\n\nRight: Speed Up\n\nCenter: Turn off autopilot mode\n\n\nPress any button to "
            "start reading... \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

        auto streamFirstTime = new StdStringStream(message);
        firstTimeRenderer_.setStream(streamFirstTime);

        prefs_.firstAutopilotRead.set(1);
        ignoreNextPress_ = true;
        firstTimeRenderer_.renderPage(0);
        renderIcons(display);
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        needsRerender = false;
        delete streamFirstTime;
        return;
    }

    if (pageSpeed_ == 0) {
        newPlace_ = renderer_.renderPage(readable_.place);
    } else {
        if (speedLinesToSkip_ >= renderer_.getNumRenderedLines()) {
            // time to turn the page
            turnPage();
        } else {
            speedLinesToSkip_++;
        }

        newPlace_ = renderer_.renderPage(readable_.place, speedLinesToSkip_);

        // If the line is blank, we want to skip it. Also, skip multiple newlines in a row.
        while ((renderer_.getLengthOfLine(speedLinesToSkip_) == 1) &&
               speedLinesToSkip_ < renderer_.getNumRenderedLines()) {
            speedLinesToSkip_++;
        }

        // Only if we didn't end on a newline. Othwerise wait for next render to turn page.
        if (speedLinesToSkip_ != renderer_.getNumRenderedLines()) {
            nextInterval_ = pageSpeed_ * renderer_.getLengthOfLine(speedLinesToSkip_) /
                            renderer_.getTotalLengthOfLines();
            if (nextInterval_ == 0) {
                nextInterval_ = 1;
            }
        }

        // Give the user extra time for the first line.
        if (speedLinesToSkip_ == 0) {
            nextInterval_ *= 2;
        }

        log_d("Setting next interval: %lu", nextInterval_);

        needsRerender = true;
    }

    // Future work
    // Sync this to cloud and use on mobile.
    updateBookmarkStatus();

    if (inBookmark_) {
        log_d("Drawing bookmark!");
        display.fillRect(display.width() + 1, display.yAbsolute(0), 13, 55, display.fg);
        display.fillTriangle(display.width() + 1, display.yAbsolute(55), display.width() + 13,
                             display.yAbsolute(55), display.width() + 7, display.yAbsolute(50),
                             display.bg);
    }

    // Show the home screen and gear icon on the first page you read as a reminder. Then drop them
    // after that.
    if (!shownIcons_) {
        renderIcons(display);
        shownIcons_ = true;
    }

    // Only show the autopilot icon on page turns or if speed was changed -- let it fade out with
    // the rest of the text
    if (((pageSpeed_ != 0) && (speedLinesToSkip_ == 0)) || (showAutopilotMinus_) ||
        (showAutopilotPlus_)) {
        int screenWidth = display.width();
        int screenHeight = display.height();
        display.drawXBitmap(screenWidth + 2, 0, autoPilotBits, auto_pilot_width, auto_pilot_height,
                            display.fg);

        if (showAutopilotMinus_) {
            display.drawXBitmap(screenWidth + 2, screenHeight / 2 - auto_minus_height / 2,
                                autoMinusBits, auto_minus_width, auto_minus_height, display.fg);
            showAutopilotMinus_ = false;
        }

        if (showAutopilotPlus_) {
            display.drawXBitmap(screenWidth + 2, screenHeight / 2 - auto_plus_height / 2,
                                autoPlusBits, auto_plus_width, auto_plus_height, display.fg);
            showAutopilotPlus_ = false;
        }
    }

    log_d("%lu is the new place in the text", newPlace_);

    if ((pageSpeed_ != 0) && (speedLinesToSkip_ > 0)) {
        display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4);
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    uint32_t wc = prefs_.wordCount.get().value();
    wc = wc + renderer_.getCurrentPageWordCount();
    prefs_.wordCount.set(wc);

    // remember the time this page was rendered
    readingStartTime_ = GetRunTimeMs();

    if (pageSpeed_ == 0) {
        needsRerender = false;
    } else {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
    }
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

    if (ignoreNextPress_) {
        ignoreNextPress_ = false;
        needsRerender = true;
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
        if (pageSpeed_ == 0) {
            readable_.place = renderer_.getPreviousPlace();
            prefs_.readablePlace.set(readable_.placemarkUuid.c_str(), readable_.place);
            prefs_.readablePlaceChanged.set(readable_.placemarkUuid.c_str(), true);
            needsRerender = true;
        } else {
            pageSpeed_ = std::min(pageSpeed_ + PAGE_SPEED_INCREMENT, SLOWEST_PAGE_SPEED);
            prefs_.autoPilotSpeed.set(pageSpeed_);
            log_d("Slowing down: New Page Speed is %u", pageSpeed_);
            showAutopilotMinus_ = true;
            needsRerender = false;
        }
        break;
    case RemoteCommand::PLAY: // To allow the PT to long press east to change page without an enter
                              // button
    case RemoteCommand::ENTER:
        if (pageSpeed_ == 0) {
            turnPage();
            needsRerender = true;
        } else {
            pageSpeed_ = 0;
            prefs_.autoPilotSpeed.set(0);
            log_d("Stopping timer and turning off AutoPilot");
            xTimerStop(DisplaySystem::animationTimer, 0);
        }

        break;
    case RemoteCommand::NEXT:
        if (pageSpeed_ != 0) {
            xTimerStop(DisplaySystem::animationTimer, 0);
        }
        setCurrentScreen(popup_);
        break;
    case RemoteCommand::CHANNEL_PLUS:
        if (pageSpeed_ != 0) {
            pageSpeed_ = std::max(pageSpeed_ - PAGE_SPEED_INCREMENT, 1);
            log_d("Speeding up: New Page Speed is %u", pageSpeed_);
            prefs_.autoPilotSpeed.set(pageSpeed_);
            showAutopilotPlus_ = true;
            needsRerender = false;
        } else {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        }
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::PREV:
        if (pageSpeed_ != 0) {
            xTimerStop(DisplaySystem::animationTimer, 0);
        }
        back();
        break;
    case RemoteCommand::NOP_BUTTON:
        needsRerender = true;
        break;
    case RemoteCommand::ANIMATION:
        needsRerender = true;
        break;
    default:
        break;
    }

    if (needsRerender) {
        incrementReadingTime();
    }
}
