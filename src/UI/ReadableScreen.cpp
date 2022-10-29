#include "UI/ReadableScreen.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "UI/Bitmaps/BenIconHacking.hpp"
#include "sindarin-debug.h"

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

    // Left Arrow
    display.drawLine(-arrowHeight - 8, screenHeight / 2, -8, screenHeight / 2 - arrowWidth / 2,
                     display.fg);
    display.drawLine(-arrowHeight - 8, screenHeight / 2, -8, screenHeight / 2 + arrowWidth / 2,
                     display.fg);

    // Up arrow - looks like a bookmark to indicate that's what you can do.
    display.drawXBitmap(screenWidth / 2 - HOME2_WIDTH / 2, -HOME2_HEIGHT - 3, homE2Bits,
                        HOME2_WIDTH, HOME2_HEIGHT, display.fg);

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

    // Right Arrow
    /*
    display.drawLine(screenWidth + arrowHeight + 2, screenHeight / 2, screenWidth + 2,
                     screenHeight / 2 + arrowWidth / 2, display.fg);
    display.drawLine(screenWidth + arrowHeight + 2, screenHeight / 2, screenWidth + 2,
                     screenHeight / 2 - arrowWidth / 2, display.fg);
                     */
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

        std::string message =
            "Welcome to reading with Sol! This is a one page tutorial on what each button does "
            "while reading.\n\n\nUp: Main Menu\n\nDown: Reading Settings\n\nLeft: Previous "
            "Page\n\nRight: Bookmark Page\n\nCenter: Next Page\n\n\n"

            "That's it! Press any button... ";

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

    newPlace_ = renderer_.renderPage(readable_.place);
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

    log_d("%lu is the new place in the text", newPlace_);

    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

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
        readable_.place = renderer_.getPreviousPlace();
        prefs_.readablePlace.set(readable_.placemarkUuid.c_str(), readable_.place);
        needsRerender = true;
        break;
    case RemoteCommand::ENTER:
        readable_.placePrev = readable_.place;
        readable_.place = newPlace_;
        prefs_.readablePlace.set(readable_.placemarkUuid.c_str(), readable_.place);
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        setCurrentScreen(popup_);
        break;
    case RemoteCommand::CHANNEL_PLUS:
        inBookmark_ = !inBookmark_;
        writeOutBookmarkUpdate();
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::PREV:
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
