#include "UI/ReadableScreen.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

// Simply reads from the bookmark file for this Readable and returns whether it was
// a turn on or turn off bookmark.
auto ReadableScreen::readOneBookmark(File &file, Bookmark &bookmark) -> int {
    // The first character in the line is a + or -, read that first
    if (!file.available()) {
        return 0;
    }

    char firstChar = file.read();
    if (firstChar == '+') {
        bookmark.status = BOOKMARK_ON;
    } else if (firstChar == '-') {
        bookmark.status = BOOKMARK_OFF;
    } else {
        // Got an unexpected character
        return 0;
    }

    // Next will be a space.
    char secondChar = file.read();
    if (secondChar != ' ') {
        return 0;
    }

    String firstThing = file.readStringUntil(' ');
    bookmark.firstPlace = firstThing.toInt();

    String secondThing = file.readStringUntil('\n');
    bookmark.lastPlace = secondThing.toInt();
    return 1;
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
    std::string bookmarkPath = "/logs/";
    bookmarkPath += readable_.id;
    bookmarkPath += "-bookmark.txt";
    log_d("Attemping to open file from updateBookmarkStatus %s", bookmarkPath.c_str());
    File bookmarkFile = bookmarkFileSystem_.open(bookmarkPath.c_str(), FILE_READ, false);
    if (!bookmarkFile) {
        log_e("Failed to open bookmark file: %s is the filename", bookmarkPath.c_str());
    } else {
        Bookmark bookmark;
        while (readOneBookmark(bookmarkFile, bookmark)) {
            log_d("Found a bookmark with the following vals %d %d %d", bookmark.status,
                  bookmark.firstPlace, bookmark.lastPlace);
            // A decent heuristic for now. If the midpoint of the current page is within
            // a bookmark range, then apply the change.
            u_int32_t averagePlace = (newPlace_ - readable_.place) / 2 + readable_.place;
            if ((averagePlace >= bookmark.firstPlace) && (averagePlace <= bookmark.lastPlace)) {
                if (bookmark.status == BOOKMARK_ON) {
                    inBookmark_ = true;
                } else {
                    inBookmark_ = false;
                }
            }
        }
        bookmarkFile.close();
    }
}

void ReadableScreen::writeOutBookmarkUpdate() {
    std::string bookmarkPath = "/logs/";
    bookmarkPath += readable_.id;
    bookmarkPath += "-bookmark.txt";
    log_d("Attemping to open file from writeOutBookmarkUpdate %s", bookmarkPath.c_str());
    File bookmarkFile = bookmarkFileSystem_.open(bookmarkPath.c_str(), FILE_APPEND, true);
    log_d("Got the open done");
    if (!bookmarkFile) {
        log_e("Failed to open bookmark file: %s is the filename", bookmarkPath.c_str());
    } else {
        std::string plusOrMinus = inBookmark_ ? "+ " : "- ";
        plusOrMinus += std::to_string(readable_.place) + " " + std::to_string(newPlace_) + "\n";
        bookmarkFile.write((const uint8_t *)plusOrMinus.c_str(), plusOrMinus.length());
        bookmarkFile.flush();
        bookmarkFile.close();
    }
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

    bool oldInBookmark = inBookmark_;
    newPlace_ = renderer_.renderPage(readable_.place, inBookmark_);
    updateBookmarkStatus();
    // This is clunky for now in that a page that you go back to that has had a bookmark added
    // previously will have to render twice. This is because for now we need to know the next
    // place in the file to know if we are in a bookmark or not.
    // It'd be a little better to draw the bookmark in this file, perhaps as AU4. But that's
    // out of scope for now.
    if (oldInBookmark != inBookmark_) {
        newPlace_ = renderer_.renderPage(readable_.place, inBookmark_);
    }
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
        // The render command will check to see if this needs to be flipped.
        inBookmark_ = false;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:

        readable_.placePrev = readable_.place;
        readable_.place = newPlace_;
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        // The render command will check to see if this needs to be flipped.
        inBookmark_ = false;
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