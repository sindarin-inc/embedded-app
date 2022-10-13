#include "MainMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

// This largely mirrors the superclass implementation, but with enough
// changes that we need to do our own thing here.
void MainMenuScreen::render(DisplaySystem &display) {
#ifdef SWAP_ARTICLES_BOOKS
    if (beforeFirstLoad_) {
        numBookItems_ = numMenuItems_;
        bookItems_ = menuItems_;
        currBookSelection_ = currSelection_;
        currBookPage_ = currMenuPage_;
        beforeFirstLoad_ = false;
    }
#endif

    // In case someone changed the selection out from underneath us.
    int oldMenuPage = currMenuPage_;
    currMenuPage_ = currSelection_ / myStyle_.numberOfItemsPerPage;
    if (oldMenuPage != currMenuPage_) {
        firstLoad_ = true;
    }

    drawArrowsAndSelections(display);

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        drawMenuItems(display, false, false);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

#if EINKET013TT1
    saveAndUpdateVCOM(display);
#endif

    drawMenuItems(display, true, false);
    drawArrowsAndSelections(display);
    display.updateDisplay(DisplaySystem::UpdateType::FAST);

#if EINKET013TT1
    revertVCOM(display);
#endif

    needsRerender = false;
    firstLoad_ = false;
}

void MainMenuScreen::processCommand(RemoteCommand command) {
    int oldCurr = 0;

    switch (command) {
    case RemoteCommand::PREV:

        prevSelection(true);
        needsRerender = true;
        break;

    case RemoteCommand::NEXT:
        nextSelection(true);
        needsRerender = true;

        break;

    // Long press down on the TT should count as enter since we don't have the
    // 5th button
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::STOP:

        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
#ifdef SWAP_ARTICLES_BOOKS
    case RemoteCommand::CHANNEL_MINUS:

        // If we are going from Books to Articles in the menu, save off the current
        // place for the books, and swap in the place for the articles.
        // Otherwise, vice versa.
        // Going to keep this logic in here and commented out
        // in case we want to flip between books and articles
        // in the future.
        if (inBooksMode_) {
            numBookItems_ = numMenuItems_;
            bookItems_ = menuItems_;
            currBookSelection_ = currSelection_;
            currBookPage_ = currMenuPage_;

            numMenuItems_ = numArticleItems_;
            menuItems_ = articleItems_;
            currSelection_ = currArticleSelection_;
            currMenuPage_ = currArticlePage_;
        } else {
            numArticleItems_ = numMenuItems_;
            articleItems_ = menuItems_;
            currArticleSelection_ = currSelection_;
            currArticlePage_ = currMenuPage_;

            numMenuItems_ = numBookItems_;
            menuItems_ = bookItems_;
            currSelection_ = currBookSelection_;
            currMenuPage_ = currBookPage_;
        }
        inBooksMode_ = !inBooksMode_;
        firstLoad_ = true;

        break;
#endif

    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        //
        // Settings are always the last item
        // Eventually this will be a
        // container screen to remove this awkward interaction
        //
        oldCurr = currSelection_;
        currSelection_ = numMenuItems_ - 1;

        if (getCurrItem().callback != nullptr) {
            auto fn = getCurrItem().callback;
            // Make sure we return to the right place.
            currSelection_ = oldCurr;
            fn();
        }

        break;
    default:
        break;
    }
}