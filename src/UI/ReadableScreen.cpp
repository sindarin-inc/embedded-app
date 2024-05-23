#include "UI/ReadableScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <utility>

#include "Misc/Platform.hpp"
#include "UI/Bitmaps/Auto_Pilot_Icons.hpp"
#include "UI/ReadableHookScreens.hpp"
#include "sindarin-debug.h"

// Two minutes per page currently
#define SLOWEST_PAGE_SPEED 120000
// Change speed of speed reading in increments of 2 seconds
#define PAGE_SPEED_INCREMENT 2000

auto ReadableScreen::layout(LayoutContext &context, Size size) -> Size {
    int arrowWidth = 24;
    int arrowHeight = 8;
    int sidePadding = -15;
    int topBottomPadding = -10;
    auto arrowContainer = Rect::fromSize(size).inset(sidePadding, topBottomPadding);
    context.setViewSizeInRect(leftArrowView_, {arrowHeight, arrowWidth}, arrowContainer,
                              LayoutOptionsAlignLeft | LayoutOptionsAlignCenterVertical);
    context.setViewSizeInRect(rightArrowView_, {arrowHeight, arrowWidth}, arrowContainer,
                              LayoutOptionsAlignRight | LayoutOptionsAlignCenterVertical);
    context.setViewSizeInRect(upArrowView_, {arrowWidth, arrowHeight}, arrowContainer,
                              LayoutOptionsAlignTop | LayoutOptionsAlignCenterHorizontal);
    context.setViewSizeInRect(downArrowView_, {arrowWidth, arrowHeight}, arrowContainer,
                              LayoutOptionsAlignBottom | LayoutOptionsAlignCenterHorizontal);
    return size;
}

void ReadableScreen::drawArrows(bool eraseArrow = false) {
    switch (lastArrowPress_) {
    case REMOTE_LEFT:
        leftArrowView_.color = eraseArrow ? display_.bg : display_.fg;
        leftArrowView_.draw();
        break;
    case REMOTE_RIGHT:
    case REMOTE_CENTER: {
        // Because the user can repurpose the center button and right button, we need to check
        // if they did that before showing the right arrow.
        bool skipDisplay = false;

        if (prefs_.fastBookmarkRight.get().value_or(false) && (lastArrowPress_ == REMOTE_RIGHT)) {
            skipDisplay = true;
        }
        if (prefs_.fastBookmarkCenter.get().value_or(false) && (lastArrowPress_ == REMOTE_CENTER)) {
            skipDisplay = true;
        }
        if (!skipDisplay) {
            rightArrowView_.color = eraseArrow ? display_.bg : display_.fg;
            rightArrowView_.draw();
        }
        break;
    }
    case REMOTE_UP:
        if (!prefs_.cesMode.get().value_or(false)) {
            upArrowView_.color = eraseArrow ? display_.bg : display_.fg;
            upArrowView_.draw();
        }
        break;
    case REMOTE_DOWN:
        if (!prefs_.cesMode.get().value_or(false)) {
            downArrowView_.color = eraseArrow ? display_.bg : display_.fg;
            downArrowView_.draw();
        }
        break;
    default:
        break;
    }
}

void ReadableScreen::drawInRect(Rect rect) { drawArrows(); }

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
    SFile bookmarkFile = fileSystem_.open(readable_->bookmarkFilePath(), FILE_READ, false);
    if (!bookmarkFile) {
        log_i("Failed to open bookmark file: %s", readable_->bookmarkFilePath().c_str());
        return;
    }

    LocalBookmark bookmark;
    while (ReadOneBookmark(bookmarkFile, bookmark)) {
        // A decent heuristic for now. If the midpoint of the bookmark is within
        // the current page range, then turn the bookmark on for this page.

        // Ensure that the current chapter matches the place in the bookmark
        auto isEpub = bookmark.firstPlace->isFor(DocType::EPUB);
        if (isEpub) {
            if (std::dynamic_pointer_cast<EPubPlace>(bookmark.firstPlace)->getHRef() !=
                std::dynamic_pointer_cast<EPubPlace>(readable_->place)->getHRef()) {
                continue;
            }
        }

        uint32_t currentPlace = readable_->place->getOffset();
        uint32_t bookmarkAveragePlace;
        uint16_t pageLength = renderer_.getTotalLengthOfLines();
        if (isEpub && (std::dynamic_pointer_cast<EPubPlace>(bookmark.firstPlace)->getHRef() ==
                       std::dynamic_pointer_cast<EPubPlace>(bookmark.lastPlace)->getHRef())) {
            bookmarkAveragePlace =
                bookmark.firstPlace->getOffset() +
                (bookmark.lastPlace->getOffset() - bookmark.firstPlace->getOffset()) / 2;
        } else {
            bookmarkAveragePlace = bookmark.firstPlace->getOffset() + (pageLength / 2);
        }

        if (bookmarkAveragePlace >= currentPlace &&
            bookmarkAveragePlace <= (currentPlace + pageLength)) {
            if (bookmark.status == BOOKMARK_ON) {
                inBookmark_ = true;
            } else {
                inBookmark_ = false;
            }
        }
    }
    bookmarkFile.close();
}

auto ReadableScreen::getScreenCurrentCharOffset() -> uint32_t {
    auto epubFile = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
    auto epubPlace = std::dynamic_pointer_cast<EPubPlace>(readable_->place);
    if ((epubFile != nullptr) && (epubPlace != nullptr)) {
        return epubFile->findCharOffsetAtFileOffset(epubPlace->getHRef(), epubPlace->getOffset());
    } else {
        return 0;
    }
}

void ReadableScreen::writeOutBookmarkUpdate() {
    // create log directory in filesystem if it doesn't exist
    if (!fileSystem_.createDir("/bookmarks")) {
        log_e("could not create /bookmarks directory");
        return;
    }

    SFile bookmarkFile = fileSystem_.open(readable_->bookmarkFilePath(), FILE_APPEND, true);

    if (!bookmarkFile) {
        log_i("Failed to open bookmark file: %s", readable_->bookmarkFilePath().c_str());
        return;
    }

    std::string plusOrMinus = inBookmark_ ? "+ " : "- ";
    plusOrMinus += readable_->place->str() + " " + newPlace_->str() + "\n";
    bookmarkFile.write(reinterpret_cast<const uint8_t *>(plusOrMinus.c_str()),
                       plusOrMinus.length());
    bookmarkFile.flush();
    bookmarkFile.close();

    prefs_.readableBookmarksChanged.set(readable_->shortenedUuid.c_str(), true);
}

// Since this happens in a couple of places (animation as well as button press),
// we have a helper method to do the work.
void ReadableScreen::turnPage() {
    if (atEnd_) {
        if (prefs_.emcMode.get().value_or(false)) {
            log_d("Looping back to the beginning of Readable for EMC testing mode");
            // Loop back to the beginning
            readable_->placePrev->setOffset(0);
            readable_->place->setOffset(0);
            return;
        }
        showPopupMenu();
        popupMenuScreen_.preselectArchiveMenuItem();
    } else {
        readable_->placePrev = readable_->place;
        readable_->place = newPlace_;
        prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
        prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
        prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                                SolClock::getInstance().getEpochTime());
        speedLinesToSkip_ = 0;
    }

    uint32_t wc = prefs_.wordCount.get().value_or(0);
    wc = wc + renderer_.getCurrentPageWordCount();
    prefs_.wordCount.set(wc);
}

auto ReadableScreen::checkAtEnd() -> bool { return renderer_.atEndOfFile(); }

void ReadableScreen::render(DisplaySystem &display) {

    // Rendering the popup is a special case. We first re-draw the text onto the background
    // (since it would have been cleared at the end of the last updateDisplay), then we render
    // the popup over it, then we draw that as a partial refresh. Then we only render the popup
    // (no text) and update again as AU4. That causes the text to be slightly faded out, but the
    // popup menu to be very clear. NOTE: We might consider in the future, not always clearing
    // the display (`fillScreen(bg);`) after updateDisplay. Then we could simply draw over
    // existing screen contents without rerendering it. But that would be a pretty global
    // change, so we'll leave it for now.
    bool renderPopupBackground = currentScreen_ == &popupMenuScreen_ && renderPopup_;

    bool renderTransitionArrows = prefs_.pageTransitionArrows.get().value_or(false);

    if (currentScreen_ != this && !renderPopupBackground) {
        currentScreen_->render(display);
        return;
    }

    if (!hookScreens_.empty()) {
        auto &hookScreen = hookScreens_[0];
        hookScreen->setNeedsRerender();
        setCurrentScreen(*hookScreen);
        hookScreen->setBackCallback([this, hookScreen]() {
            this->setNeedsRerender();
            setCurrentScreen(*this);
            this->hookScreens_.erase(this->hookScreens_.begin());
        });
        hookScreen->render(display);
        return;
    }

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    if (renderTransitionArrows) {
        if (lastArrowPress_ != REMOTE_NONE) {
            draw();
            display.updateDisplay(DisplaySystem::UpdateType::FAST);
        }
    }

    prefs_.lastReadableId.set(readable_->uuid.c_str());
    telemetry_.logMetric(TelemetryField::METRIC_READABLE_PLACE, readable_->shortenedUuid.c_str(),
                         readable_->telemetryPlacemarkMetrics());

    display.setFont(display.style.getFontFamily().readableFont);

    bool emcMode = prefs_.emcMode.get().value_or(false);

    // If it's the first read of the book, show the help screen.
    // Then ignore the next press of the button
    if (!emcMode && !prefs_.firstRead.get().value_or(false)) {

        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message =
            "Welcome to reading with Sol! This is a one page tutorial on what each button does "
            "while reading.\n\n\n\ue003  or  \ue004   Popup Menu\n\n\ue001   "
            "Previous "
            "Page\n\n\ue000  or  \uE002  Next Page\n\n\nThat's "
            "it! Press "
            "any button... "
            "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

        auto streamFirstTime = std::make_shared<StdStringStream>(message);
        renderer_.setStream(streamFirstTime);

        prefs_.firstRead.set(true);
        showingFirstTimeView_ = true;
        renderer_.renderFirstPage();

        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
        }
        return;
    }

    if (!emcMode && !prefs_.firstAutopilotRead.get().value_or(false) && (pageSpeed_ != 0)) {
        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message =
            "Welcome to the beta of Autopilot mode! Here is what each button does "
            "while in autopilot mode.\n\n\nUp or Down: Popup Menu\n\nLeft: Slow Down "
            "\n\nRight: Speed Up\n\nCenter: Pause/unpause\n\n\nPress any button "
            "to start reading... \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

        auto streamFirstTime = std::make_shared<StdStringStream>(message);
        renderer_.setStream(streamFirstTime);

        prefs_.firstAutopilotRead.set(true);
        showingFirstTimeView_ = true;
        renderer_.renderFirstPage();

        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL);
        }
        return;
    }

    uint8_t darkPicturePercent = 0;

    if (pageSpeed_ == 0) {
        // Regular Reading
        bool retrieveMenuParameters = true;
        do {
            std::tie(newPlace_, darkPicturePercent) =
                renderer_.renderPage(readable_->place, 0, false, retrieveMenuParameters);
            if (retrieveMenuParameters) {
                const MenuParameters &menuParameters = renderer_.getMenuParameters();
                if (!menuParameters.empty()) {
                    // Note that we can't guarantee the order in which the hooks
                    // will be parsed out and executed. So if you have multiple
                    // hooks in one place, just keep that in mind.
                    log_d("Menu Parameters:");
                    for (auto &param : menuParameters) {
                        log_d("Creating hook screen %s: %s", param.first.c_str(),
                              param.second.c_str());
                        hookScreens_.push_back(ReadableHookScreens::getNewHookScreen(
                            param.first, display_, prefs_, telemetry_));
                    }
                    retrieveMenuParameters = false;
                    continue;
                }
            }

            break;
        } while (true);

        // log_w("Char Offset: %" PRIu32, getScreenCurrentCharOffset());
        atEnd_ = checkAtEnd();
    } else {
        // Autopilot Reading
        if (speedLinesToSkip_ >= renderer_.getNumRenderedLines()) {
            turnPage();
        } else {
            speedLinesToSkip_++;
        }

        std::tie(newPlace_, darkPicturePercent) =
            renderer_.renderPage(readable_->place, speedLinesToSkip_);
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
        int leftSide = display.xAbsolute(display.xAbsoluteMax() - 25);
        log_d("Drawing bookmark!");
        display.fillRect(leftSide + 1, display.yAbsolute(0), 13, 55, display.fg);
        display.fillTriangle(leftSide + 1, display.yAbsolute(55), leftSide + 13,
                             display.yAbsolute(55), leftSide + 7, display.yAbsolute(50),
                             display.bg);
    }

    // Only show the autopilot icon on page turns or if speed was changed -- let it fade out
    // with the rest of the text
    if ((pageSpeed_ != 0) && (speedLinesToSkip_ == 0)) {
        int screenWidth = display.width();
        display.drawXBitmap(screenWidth + 2, 0, autoPilotBits, auto_pilot_width, auto_pilot_height,
                            display.fg);
    }

    log_d("%s is the new place in the text %s", newPlace_->str().c_str(), atEnd_ ? "(at end)" : "");

    if (renderPopupBackground) {
        // Initial popup render (will use PARTIAL and not clear out the text)
        popupMenuScreen_.render(display);
        // Render again (This time it will use AU4 and render without the text)
        popupMenuScreen_.render(display);
    } else if ((pageSpeed_ != 0) && (speedLinesToSkip_ > 0)) {
        if (autoPilotForceFull_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
            autoPilotForceFull_ = false;
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
    } else {
        if (renderTransitionArrows) {
            draw();
        }
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL,
                                  (renderTransitionArrows) ? false : true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL,
                                  (renderTransitionArrows) ? false : true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
        if (renderTransitionArrows) {
            drawArrows(true);
            display.updateDisplay(DisplaySystem::UpdateType::ERASE, false,
                                  display.darkPictureVComAdj(darkPicturePercent));
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

    if (emcMode) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Change page every 30 seconds
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(30000), 0);
        turnPage();
    }

    renderPopup_ = false;

    // Reset the lastArrowPress so we don't show it again until another key is pressed (for
    // autopilot mostly)
    lastArrowPress_ = REMOTE_NONE;
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
    case RemoteCommand::LEFT:
        numUps_ = 0;
        processCommandNormal(command);
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        numUps_ = 0;
        turnPage();
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        numUps_ = 0;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        numUps_++;
        if (numUps_ > 3) {
            numUps_ = 0;
            prefs_.cesMode.set(false);
            setNeedsRerender();
            display_.clearDisplay();
            display_.clearFrameBuffer();
            display_.clearDontChangeRefreshType();
            back();
        }
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}

void ReadableScreen::processCommandAutoPilot(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
        pageSpeed_ = std::min(pageSpeed_ + PAGE_SPEED_INCREMENT, SLOWEST_PAGE_SPEED);
        prefs_.autoPilotSpeed.set(pageSpeed_);
        log_d("Slowing down: New Page Speed is %u", pageSpeed_);
        break;
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

    case RemoteCommand::DOWN:
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
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
    case RemoteCommand::RIGHT:
        pageSpeed_ = std::max(pageSpeed_ - PAGE_SPEED_INCREMENT, 1);
        log_d("Speeding up: New Page Speed is %u", pageSpeed_);
        prefs_.autoPilotSpeed.set(pageSpeed_);
        break;

    case RemoteCommand::ANIMATION:
    default:
        break;
    }

    setNeedsRerender();
}

void ReadableScreen::processCommandNormal(RemoteCommand command) {
    // Go ahead and begin powering on the screen since in all cases we trigger a re-render
    display_.beginPowerOn();

    switch (command) {
    case RemoteCommand::LEFT:
        readable_->place = renderer_.getPreviousPlace();
        prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
        prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
        prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                                SolClock::getInstance().getEpochTime());
        break;
    case RemoteCommand::ENTER:
        if (prefs_.fastBookmarkCenter.get().value_or(false)) {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        } else {
            turnPage();
        }
        break;
    case RemoteCommand::DOWN:
        showPopupMenu();
        popupMenuScreen_.preselectLastItem();
        break;
    case RemoteCommand::RIGHT:
        if (prefs_.fastBookmarkRight.get().value_or(false)) {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        } else {
            turnPage();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        showPopupMenu();
        popupMenuScreen_.preselectFirstItem();
        break;

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

    if (showingFirstTimeView_) {
        showingFirstTimeView_ = false;
        renderer_.setStream(stream_);
        setNeedsRerender();
        return;
    }

    switch (command) {
    case RemoteCommand::LEFT:
        lastArrowPress_ = REMOTE_LEFT;
        break;
    case RemoteCommand::RIGHT:
        lastArrowPress_ = REMOTE_RIGHT;
        break;
    case RemoteCommand::ENTER:
        lastArrowPress_ = REMOTE_CENTER;
        break;
    case RemoteCommand::DOWN:
        lastArrowPress_ = REMOTE_DOWN;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        lastArrowPress_ = REMOTE_UP;
        break;
    default:
        break;
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
