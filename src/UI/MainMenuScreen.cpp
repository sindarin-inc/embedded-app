#include "MainMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"

#define MAX_WIDTH 150

// We should decide if we want to do this #if as !SIMULATOR or as EINKET013TT1
#if !SIMULATOR
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

void MainMenuScreen::drawMenuItems(DisplaySystem &display, bool withBullet) {
    for (int i = currMenuPage_ * NUM_LINES_PER_MENU_PAGE;
         i < currMenuPage_ * NUM_LINES_PER_MENU_PAGE + NUM_LINES_PER_MENU_PAGE &&
         i < numMenuItems_ - 1;
         i++) {
        std::string itemTitle = getItemTitleAt(i);
        // If we still don't fit, shrink the string until we do.
        while (display.getTextWidth(itemTitle.c_str()) > MAX_WIDTH) {
            log_d("Item title %s still too long at smallest font size; truncating...",
                  itemTitle.c_str());
            if (itemTitle.length() > 4) {
                itemTitle = itemTitle.substr(0, itemTitle.length() - 4) + "...";
            } else {
                // Practially, this shouldn't ever happen, but I'm doing
                // the bounds check here just in case we do hit it some day.
                itemTitle = "too big!";
            }
        }
        display.drawSingleLineOfText(itemTitle, 25,
                                     display.height() / 3 + (i % NUM_LINES_PER_MENU_PAGE) *
                                                                (4 + display.getTextHeight()));

        if ((i == currSelection_) && (withBullet)) {
            display.fillRoundRect(
                12,
                display.height() / 3 +
                    (i % NUM_LINES_PER_MENU_PAGE) * (4 + display.getTextHeight()) + 3,
                display.getTextHeight() - 7, display.getTextHeight() - 7, 3, display.fg);
        }
    }
}

void MainMenuScreen::render(DisplaySystem &display) {
    NUM_LINES_PER_MENU_PAGE = 5;
    if (beforeFirstLoad_) {
        numBookItems_ = numMenuItems_;
        bookItems_ = menuItems_;
        currBookSelection_ = currSelection_;
        currBookPage_ = currMenuPage_;
        beforeFirstLoad_ = false;
    }

    // In case someone changed the selection out from underneath us.
    int oldMenuPage = currMenuPage_;
    currMenuPage_ = currSelection_ / NUM_LINES_PER_MENU_PAGE;
    if (oldMenuPage != currMenuPage_) {
        firstLoad_ = true;
    }

    drawArrowsAndSelections(display);

    display.setFont(display.style.getFontFamily().mainMenuFont);

    // Draw the icon for the opposite of the mode we're in. If you're reading books and press
    // this direction, it takes you to articles, and vice versa.
    //
    // Proposing to check this in as we may want to flip between books mode and articles mode
    // even if we only use words to describe it instead of icons.
    //
    // if (!inBooksMode_) {
    //   display.drawXBitmap(-20, screenHeight / 2 - GEAR_HEIGHT / 2, GEAR_BITS, GEAR_WIDTH,
    //   GEAR_HEIGHT, display.fg);
    // } else {
    //     display.drawXBitmap(1, screenHeight / 2 - ARTICLES_HEIGHT / 2, ARTICLES_BITS,
    //                         ARTICLES_WIDTH, ARTICLES_HEIGHT, display.fg);
    // }

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        drawMenuItems(display, false);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

    //
    // change the vcom to reduce ghosting for when we do the partial refresh
    //
#if EINKET013TT1
    int oldVcom = (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->getVcom();
    int newVcom = 0;
    if (display.getInversion()) {
        newVcom = oldVcom - 2000;
    } else {
        newVcom = oldVcom + 2000;
    }
    (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->setVcom(newVcom);
#endif

    drawMenuItems(display, true);

    // Draw the icon for the opposite of the mode we're in. If you're reading books and press
    // this direction, it takes you to articles, and vice versa.
    // See above comment for why this should remain for now.
    // if (!inBooksMode_) {
    //   display.drawXBitmap(-20, screenHeight / 2 - GEAR_HEIGHT / 2, GEAR_BITS, GEAR_WIDTH,
    //   GEAR_HEIGHT, display.fg);
    // } else {
    //    display.drawXBitmap(1, screenHeight / 2 - ARTICLES_HEIGHT / 2, ARTICLES_BITS,
    //                        ARTICLES_WIDTH, ARTICLES_HEIGHT, display.fg);
    //}

    drawArrowsAndSelections(display);
    display.updateDisplay(DisplaySystem::UpdateType::FAST);

#if EINKET013TT1
    (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->setVcom(oldVcom);
#endif

    display.setFont(display.style.getFontFamily().readableFont);
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
    // case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::STOP:

        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
        /*case RemoteCommand::CHANNEL_MINUS:

            // If we are going from Books to Articles in the menu, save off the current
            // place for the books, and swap in the place for the articles.
            // Otherwise, vice versa.
            // Going to keep this logic in here in case we want to flip between books and
            // articles.
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
            */

    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:

        // Settings are always the last item
        // REWORK THIS!

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