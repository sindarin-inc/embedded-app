#include "UI/ReadableScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <utility>

#include "Misc/Platform.hpp"
#include "UI/Bitmaps/Auto_Pilot_Icons.hpp"
#include "UI/Bitmaps/Home.hpp"
#include "UI/Bitmaps/Read_Gear.hpp"
#include "sindarin-debug.h"

// Two minutes per page currently
#define SLOWEST_PAGE_SPEED 120000
// Change speed of speed reading in increments of 2 seconds
#define PAGE_SPEED_INCREMENT 2000

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
    inBookmark_ = false;
    SFile bookmarkFile = fileSystem_.open(readable_.bookmarkFilePath().c_str(), FILE_READ, false);
    if (!bookmarkFile) {
        log_i("Failed to open bookmark file: %s", readable_.bookmarkFilePath().c_str());
        return;
    }

    LocalBookmark bookmark;
    while (ReadOneBookmark(bookmarkFile, bookmark)) {
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
    // create log directory in filesystem if it doesn't exist
    if (!fileSystem_.createDir("/bookmarks")) {
        log_e("could not create /bookmarks directory");
        return;
    }

    SFile bookmarkFile = fileSystem_.open(readable_.bookmarkFilePath().c_str(), FILE_APPEND, true);

    if (!bookmarkFile) {
        log_i("Failed to open bookmark file: %s", readable_.bookmarkFilePath().c_str());
        return;
    }

    std::string plusOrMinus = inBookmark_ ? "+ " : "- ";
    plusOrMinus += std::to_string(readable_.place) + " " + std::to_string(newPlace_) + "\n";
    bookmarkFile.write((const uint8_t *)plusOrMinus.c_str(), plusOrMinus.length());
    bookmarkFile.flush();
    bookmarkFile.close();

    prefs_.readableBookmarksChanged.set(readable_.shortenedUuid.c_str(), true);
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
    if (!prefs_.cesMode.get().value_or(false)) {
        display.drawXBitmap(screenWidth / 2 - READ_GEAR_WIDTH / 2,
                            screenHeight + display.getTextHeight() / 2 + 3, readGearBits,
                            READ_GEAR_WIDTH, READ_GEAR_HEIGHT, display.fg);
    }

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

    if (!prefs_.cesMode.get().value_or(false)) {
        // Up arrow - looks like a bookmark to indicate that's what you can do.
        display.drawXBitmap(screenWidth / 2 - HOME2_WIDTH / 2, -HOME2_HEIGHT - 3, home2bits,
                            HOME2_WIDTH, HOME2_HEIGHT, display.fg);

        if (pageSpeed_ == 0) {
            display.setRotation(1);
            // Right side show bookmark icon.
            display.drawLine(screenWidth / 2, -arrowHeight - 1, screenWidth / 2 - arrowWidth / 2,
                             -1, display.fg);
            display.drawLine(screenWidth / 2, -arrowHeight - 1, screenWidth / 2 + arrowWidth / 2,
                             -1, display.fg);

            display.drawLine(screenWidth / 2 - arrowWidth / 2, -1, screenWidth / 2 - arrowWidth / 2,
                             display.yAbsolute(0), display.fg);
            display.drawLine(screenWidth / 2 + arrowWidth / 2, -1, screenWidth / 2 + arrowWidth / 2,
                             display.yAbsolute(0), display.fg);

            display.setRotation(0);
        } else {
            display.drawXBitmap(screenWidth + 2, screenHeight / 2 - auto_plus_height / 2,
                                autoPlusBits, auto_plus_width, auto_plus_height, display.fg);
        }
    }

    // Right Arrow
    if (prefs_.cesMode.get().value_or(false)) {
        display.drawLine(screenWidth + arrowHeight + 2, screenHeight / 2, screenWidth + 2,
                         screenHeight / 2 + arrowWidth / 2, display.fg);
        display.drawLine(screenWidth + arrowHeight + 2, screenHeight / 2, screenWidth + 2,
                         screenHeight / 2 - arrowWidth / 2, display.fg);
    }
}

// Since this happens in a couple of places (animation as well as button press),
// we have a helper method to do the work.
void ReadableScreen::turnPage() {
    if (atEnd_) {
        showPopupMenu();
        popupMenuScreen_.preselectArchiveMenuItem();
    } else {
        readable_.placePrev = readable_.place;
        readable_.place = newPlace_;
        prefs_.readablePlace.set(readable_.shortenedUuid.c_str(), readable_.place);
        prefs_.readablePlaceChanged.set(readable_.shortenedUuid.c_str(), true);
        speedLinesToSkip_ = 0;
    }

    uint32_t wc = prefs_.wordCount.get().value_or(0);
    wc = wc + renderer_.getCurrentPageWordCount();
    prefs_.wordCount.set(wc);
}

auto ReadableScreen::checkAtEnd() -> bool { return (newPlace_ == 0); }

void ReadableScreen::render(DisplaySystem &display) {
    // Rendering the popup is a special case. We first re-draw the text onto the background (since
    // it would have been cleared at the end of the last updateDisplay), then we render the popup
    // over it, then we draw that as a partial refresh. Then we only render the popup (no text) and
    // update again as AU4. That causes the text to be slightly faded out, but the popup menu to be
    // very clear.
    // NOTE: We might consider in the future, not always clearing the display (`fillScreen(bg);`)
    // after updateDisplay. Then we could simply draw over existing screen contents without
    // rerendering it. But that would be a pretty global change, so we'll leave it for now.
    bool renderPopupBackground = currentScreen_ == &popupMenuScreen_ && renderPopup_;

    if (currentScreen_ != this && !renderPopupBackground) {
        currentScreen_->render(display);
        return;
    }

    prefs_.lastReadableId.set(readable_.uuid);

    display.setFont(display.style.getFontFamily().readableFont);

    // If it's the first read of the book, show the help screen.
    // Then ignore the next press of the button
    if (!prefs_.firstRead.get().value_or(false)) {

        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message =
            "Welcome to reading with Sol! This is a one page tutorial on what each button does "
            "while reading.\n\n\nUp: Main Menu\n\nDown: Reading Settings\n\nLeft: Previous "
            "Page\n\nRight: Bookmark Page\n\nCenter: Next Page\n\n\nThat's it! Press any button... "
            "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

        auto streamFirstTime = std::make_shared<StdStringStream>(message);
        firstTimeRenderer_.setStream(streamFirstTime);

        prefs_.firstRead.set(true);
        ignoreNextPress_ = true;
        firstTimeRenderer_.renderPage(0);
        // renderIcons(display);
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
        }
        return;
    }

    if (!prefs_.firstAutopilotRead.get().value_or(false) && (pageSpeed_ != 0)) {
        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message =
            "Welcome to the beta of Autopilot mode! Here is what each button does "
            "while in autopilot mode.\n\n\nUp: Main Menu\n\nDown: Reading "
            "Settings\n\nLeft: Slow Down "
            "Page\n\nRight: Speed Up\n\nCenter: Pause/unpause\n\n\nPress any button "
            "to "
            "start reading... \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

        auto streamFirstTime = std::make_shared<StdStringStream>(message);
        firstTimeRenderer_.setStream(streamFirstTime);

        prefs_.firstAutopilotRead.set(true);
        ignoreNextPress_ = true;
        firstTimeRenderer_.renderPage(0);
        // renderIcons(display);
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
        }
        return;
    }

    if (pageSpeed_ == 0) {
        // Regular Reading
        newPlace_ = renderer_.renderPage(readable_.place);
        atEnd_ = checkAtEnd();
    } else {
        // Autopilot Reading
        if (speedLinesToSkip_ >= renderer_.getNumRenderedLines()) {
            turnPage();
        } else {
            speedLinesToSkip_++;
        }

        newPlace_ = renderer_.renderPage(readable_.place, speedLinesToSkip_);
        atEnd_ = checkAtEnd();

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

        setNeedsRerender();
    }

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
        // renderIcons(display);
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

    log_d("%lu is the new place in the text %s", newPlace_, atEnd_ ? "(at end)" : "");

    if (renderPopupBackground) {
        // Initial popup render (will use PARTIAL and not clear out the text)
        popupMenuScreen_.render(display);
        // Render again (This time it will use AU4 and render without the text)
        popupMenuScreen_.render(display);
    } else if ((pageSpeed_ != 0) && (speedLinesToSkip_ > 0)) {
        if (autoPilotForceFull_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
            autoPilotForceFull_ = false;
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4);
        }
    } else {
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
        }
    }

    // remember the time this page was rendered
    readingStartTime_ = GetRunTimeMs();

    if (pageSpeed_ != 0) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        if (!autoPilotPause_) {
            xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        }
    }

    if (prefs_.emcMode.get().value_or(false)) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Change page every 30 seconds
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(30000), 0);
        turnPage();
    }

    renderPopup_ = false;
}

void ReadableScreen::incrementReadingTime() {
    // increment total reading time
    auto endMillis = GetRunTimeMs();
    auto thisPageReadingTimeSeconds =
        (endMillis > readingStartTime_) ? (endMillis - readingStartTime_) / 1000 : 0;
    auto totalReadingTimeSeconds = prefs_.totalReadingTimeSeconds.get().value_or(0);
    totalReadingTimeSeconds += thisPageReadingTimeSeconds;
    prefs_.totalReadingTimeSeconds.set(totalReadingTimeSeconds);
}

void ReadableScreen::showPopupMenu() {
    renderPopup_ = true;
    popupMenuScreen_.setUpPopupMenu();
    setCurrentScreen(popupMenuScreen_);
}

void ReadableScreen::hidePopupMenu() { setCurrentScreen(*this); }

void ReadableScreen::processCommandDemoMode(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        numUps_ = 0;
        processCommandNormal(command);
        break;
    case RemoteCommand::PLAY:
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
        numUps_ = 0;
        turnPage();
        setNeedsRerender();
        break;
    case RemoteCommand::NEXT:
        numUps_ = 0;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::PREV:
        numUps_++;
        if (numUps_ > 3) {
            numUps_ = 0;
            prefs_.cesMode.set(false);
            setNeedsRerender();
            back();
        }
        break;

    case RemoteCommand::NOP_BUTTON:
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}

void ReadableScreen::processCommandAutoPilot(RemoteCommand command) {

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        pageSpeed_ = std::min(pageSpeed_ + PAGE_SPEED_INCREMENT, SLOWEST_PAGE_SPEED);
        prefs_.autoPilotSpeed.set(pageSpeed_);
        log_d("Slowing down: New Page Speed is %u", pageSpeed_);
        showAutopilotMinus_ = true;
        break;
    case RemoteCommand::PLAY:
    case RemoteCommand::ENTER:
        // If we aren't pause, pause.
        // If we are paused, start the timer again.
        log_d("Pausing or unpausing auto pilot");
        if (!autoPilotPause_) {
            xTimerStop(DisplaySystem::animationTimer, 0);
        } else {
            xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        }
        autoPilotPause_ = !autoPilotPause_;
        break;

    case RemoteCommand::NEXT:
    case RemoteCommand::BACK:
    case RemoteCommand::PREV:
        // When entering the popup menu from autopilot, go back up to two line so that coming
        // out the user can not be behind.
        if (speedLinesToSkip_ > 0) {
            speedLinesToSkip_--;
        }
        if (speedLinesToSkip_ > 0) {
            speedLinesToSkip_--;
        }
        xTimerStop(DisplaySystem::animationTimer, 0);
        processCommandNormal(command);
        break;
    case RemoteCommand::CHANNEL_PLUS:
        pageSpeed_ = std::max(pageSpeed_ - PAGE_SPEED_INCREMENT, 1);
        log_d("Speeding up: New Page Speed is %u", pageSpeed_);
        prefs_.autoPilotSpeed.set(pageSpeed_);
        showAutopilotPlus_ = true;
        break;

    case RemoteCommand::NOP_BUTTON:
    case RemoteCommand::ANIMATION:
    default:
        break;
    }

    setNeedsRerender();
}

void ReadableScreen::processCommandNormal(RemoteCommand command) {

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        readable_.place = renderer_.getPreviousPlace();
        prefs_.readablePlace.set(readable_.shortenedUuid.c_str(), readable_.place);
        prefs_.readablePlaceChanged.set(readable_.shortenedUuid.c_str(), true);
        break;
    case RemoteCommand::PLAY:
    case RemoteCommand::ENTER:
        if (prefs_.fastBookmarkCenter.get().value_or(false)) {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        } else {
            turnPage();
        }
        break;
    case RemoteCommand::NEXT:
        showPopupMenu();
        popupMenuScreen_.preselectLastItem();
        break;
    case RemoteCommand::CHANNEL_PLUS:
        if (prefs_.fastBookmarkRight.get().value_or(false)) {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        } else {
            turnPage();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::PREV:
        showPopupMenu();
        popupMenuScreen_.preselectFirstItem();
        break;

    case RemoteCommand::NOP_BUTTON:
    case RemoteCommand::ANIMATION:
    default:
        break;
    }

    setNeedsRerender();
}

void ReadableScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    if (ignoreNextPress_) {
        ignoreNextPress_ = false;
        setNeedsRerender();
        return;
    }

    // Similarly, for demo mode, we want a simple set of commands.
    if (prefs_.cesMode.get().value_or(false)) {
        processCommandDemoMode(command);
        return;
    }

    // Then we move on to autopilot mode and regular mode.
    if (pageSpeed_ != 0) {
        processCommandAutoPilot(command);
    } else {
        processCommandNormal(command);
    }

    if (needsRerender_) {
        incrementReadingTime();
    }
}
