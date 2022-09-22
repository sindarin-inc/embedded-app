#include "MainMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"

// We should decide if we want to do this #if as !SIMULATOR or as EINKET013TT1
#if !SIMULATOR
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

void MainMenuScreen::render(DisplaySystem &display) {
    NUM_LINES_PER_MENU_PAGE = 5;
    if (beforeFirstLoad_) {
        numBookItems_ = numMenuItems_;
        bookItems_ = menuItems_;
        currBookSelection_ = currSelection_;
        currBookPage_ = currMenuPage_;
        beforeFirstLoad_ = false;
    }

    // uint16_t midX = display.width() / 2;
    // uint16_t midY = display.height() / 2;

    // In case someone changed the selection out from underneath us.
    int oldMenuPage = currMenuPage_;
    currMenuPage_ = currSelection_ / NUM_LINES_PER_MENU_PAGE;
    if (oldMenuPage != currMenuPage_) {
        firstLoad_ = true;
    }

    // Top Arrow
    // display.drawLine(midX, 42, midX - 10, 47, display.fg);
    // display.drawLine(midX, 42, midX + 10, 47, display.fg);

    // Bottom Arrow
    // display.drawLine(midX, midY * 2 - 40, midX - 10, midY * 2 - 45, display.fg);
    // display.drawLine(midX, midY * 2 - 40, midX + 10, midY * 2 - 45, display.fg);

    // Left Arrow
    // display.drawLine(25, midY, 35, midY - 10, display.fg);
    // display.drawLine(25, midY, 35, midY + 10, display.fg);

    // Right Arrow
    // display.drawLine(midX * 2 - 25, midY, midX * 2 - 35, midY - 10, display.fg);
    // display.drawLine(midX * 2 - 25, midY, midX * 2 - 35, midY + 10, display.fg);

    // Middle circle
    /*
        uint radius = display.height() / 256 * 80;
        display.fillCircle(midX, midY, radius, display.fg);
        display.fillRect(midX - radius * 0.8, midY - radius * 0.8, radius * 1.6, radius * 1.6,
                         display.bg);
    */
    // display.drawXBitmap((int16_t)((display.width() - MENU2_BLANK_WIDTH) / 2),
    //                     (int16_t)((display.height() - MENU2_BLANK_HEIGHT) / 2), MENU2_BLANK_BITS,
    //                     MENU2_BLANK_WIDTH, MENU2_BLANK_HEIGHT, display.fg);

    display.setFont(display.style.getFontFamily().mainMenuFont);
    auto itemTitle = getCurrItem().title;
    uint16_t width, height;

    display.getTextBounds(itemTitle.c_str(), &width, &height);
    // uint16_t screenWidth = display.width();
    // uint16_t screenHeight = display.height();

    // display.setRotation(0);
    //  display.drawSingleLineOfText(itemTitle.c_str(), (screenWidth / 2) - (width / 2),
    //                               (screenHeight / 2) - (display.getTextHeight()) / 2);

    // auto prevTitle = getItemAt((currSelection_ > 0) ? ((currSelection_ - 1) % numMenuItems_)
    //                                                : (numMenuItems_ - 1))
    //                    .title;
    // auto nextTitle = getItemAt((currSelection_ + 1) % numMenuItems_).title;

    // display.getTextBounds(prevTitle.c_str(), &width, &height);

    // display.drawSingleLineOfText(prevTitle.c_str(), (screenWidth / 2) - (width / 2), 80);

    // display.getTextBounds(nextTitle.c_str(), &width, &height);

    // display.drawSingleLineOfText(nextTitle.c_str(), (screenWidth / 2) - (width / 2),
    //                              (screenHeight - 80 - display.getTextHeight()));

    // display.setRotation(0);
    /*
    display.getTextBounds("Settings", &width, &height);

    display.drawSingleLineOfText("Settings", screenWidth / 2 - width / 2, 28);
*/

    drawArrowsAndSelections(display);

    display.setFont(display.style.getFontFamily().mainMenuFont);
    // display.drawXBitmap(display.width() /* - BOOK_WIDTH - 1*/, screenHeight / 2 - BOOK_WIDTH / 2,
    //                    BOOK_BITS, BOOK_WIDTH, BOOK_HEIGHT, display.fg);

    /*
        display.getTextBounds("Read", &width, &height);

        display.drawSingleLineOfText("Read", screenWidth / 2 - width / 2, screenHeight - 30 -
       height);
    */
    // Draw the icon for the opposite of the mode we're in. If you're reading books and press
    // this direction, it takes you to articles, and vice versa.
    // if (!inBooksMode_) {
    // display.drawXBitmap(-20, screenHeight / 2 - GEAR_HEIGHT / 2, GEAR_BITS, GEAR_WIDTH,
    // GEAR_HEIGHT,
    //                   display.fg);
    // } else {
    //     display.drawXBitmap(1, screenHeight / 2 - ARTICLES_HEIGHT / 2, ARTICLES_BITS,
    //                         ARTICLES_WIDTH, ARTICLES_HEIGHT, display.fg);
    // }

    // display.setRotation(0);

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        for (int i = currMenuPage_ * NUM_LINES_PER_MENU_PAGE;
             i < currMenuPage_ * NUM_LINES_PER_MENU_PAGE + NUM_LINES_PER_MENU_PAGE &&
             i < numMenuItems_;
             i++) {
            display.drawSingleLineOfText(getItemTitleAt(i), display.width() / 4,
                                         display.height() / 3 + (i % NUM_LINES_PER_MENU_PAGE) *
                                                                    (4 + display.getTextHeight()));
        }

        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }
    // Starting the process of doing this better eventually. Mimicking the lighting screen.
    // (But without the fast).
    // display.updateDisplay(firstLoad_ ? DisplaySystem::UpdateType::FULL
    //                                 : DisplaySystem::UpdateType::FAST);

    //
    // Duplicates the code above but only on the first load
    // TODO: This is a hack. We should be able to do this in a single pass??
    //
    // change the vcom to reduce ghosting
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

    for (int i = currMenuPage_ * NUM_LINES_PER_MENU_PAGE;
         i < currMenuPage_ * NUM_LINES_PER_MENU_PAGE + NUM_LINES_PER_MENU_PAGE && i < numMenuItems_;
         i++) {
        display.drawSingleLineOfText(getItemTitleAt(i), display.width() / 4,
                                     display.height() / 3 + (i % NUM_LINES_PER_MENU_PAGE) *
                                                                (4 + display.getTextHeight()));

        if (i == currSelection_) {
            display.fillRoundRect(
                display.width() / 4 - 15,
                display.height() / 3 +
                    (i % NUM_LINES_PER_MENU_PAGE) * (4 + display.getTextHeight()) + 2,
                display.getTextHeight() - 3, display.getTextHeight() - 3, 3, display.fg);
        }
    }

    /*
        display.drawXBitmap(display.width() - BOOK_WIDTH - 1, screenHeight / 2 - BOOK_HEIGHT / 2,
                            BOOK_BITS, BOOK_WIDTH, BOOK_HEIGHT, display.fg);



            display.getTextBounds("Read", &width, &height);

            display.drawSingleLineOfText("Read", screenWidth / 2 - width / 2, screenHeight - 30 -
           height);
        */
    // Draw the icon for the opposite of the mode we're in. If you're reading books and press
    // this direction, it takes you to articles, and vice versa.
    // if (!inBooksMode_) {
    /*
display.drawXBitmap(-20, screenHeight / 2 - GEAR_HEIGHT / 2, GEAR_BITS, GEAR_WIDTH, GEAR_HEIGHT,
                    display.fg);*/

    // } else {
    //    display.drawXBitmap(1, screenHeight / 2 - ARTICLES_HEIGHT / 2, ARTICLES_BITS,
    //                       ARTICLES_WIDTH, ARTICLES_HEIGHT, display.fg);
    //}

    drawArrowsAndSelections(display);
    display.updateDisplay(DisplaySystem::UpdateType::FAST);

#if EINKET013TT1
    (static_cast<DisplayEinkET013TT1 *>(&display))->getDriver()->setVcom(oldVcom);
#endif

    // display.updateDisplay();
    display.setFont(display.style.getFontFamily().readableFont);
    needsRerender = false;

    firstLoad_ = false;
}

/*
void PagedMenuScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    log_i("PagedMenuScreen render (selected = %s)", getCurrItem().title.c_str());

    // Blank out screen
    display.fillScreen(display.bg);
    // Draw menu surroundings
    const unsigned char *menuBits;
    unsigned int menuWidth;
    unsigned int menuHeight;

    // Allows us to have arrows on both left and right side of the menu for
    // when a PagedMenu is used as a sub-menu (as in the case of being called)
    // from a PopupMenu from a readable.
    if (!isBidirectional_) {
        menuBits = MENU_BLANK_BITS;
        menuWidth = MENU_BLANK_WIDTH;
        menuHeight = MENU_BLANK_HEIGHT;
    } else {
        menuBits = MENU_BLANK_BIDIRECTIONAL_BITS;
        menuWidth = MENU_BLANK_BIDIRECTIONAL_WIDTH;
        menuHeight = MENU_BLANK_BIDIRECTIONAL_HEIGHT;
    }

    display.drawXBitmap((int16_t)((display.width() - menuWidth) / 2),
                        (int16_t)((display.height() - menuHeight) / 2), menuBits,
                        (int16_t)menuWidth, (int16_t)menuHeight, display.fg);

    auto itemTitle = getCurrItem().title;

    display.setFont(display.style.getFontFamily().mainMenuFont);

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

    // Now draw!
    uint16_t width = 0, height = 0;
    auto batteryStatus = this->power_.getBatteryLifeString();
    uint16_t leftTemp;
#if EINKET013TT1
    auto *driver = ((DisplayEinkET013TT1 *)&display)->getDriver();
    // TODO: Add UI for right screen temp also
    leftTemp = driver->readTemperature(ScreenSide::Left);
#else
    leftTemp = 22;
#endif
    batteryStatus += StringFormat(" %dC", leftTemp);

    display.getTextBounds(itemTitle.c_str(), &width, &height);
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();
    display.drawSingleLineOfText(itemTitle.c_str(), (screenWidth / 2) - (width / 2),
                                 (screenHeight / 2) - (display.getTextHeight()) + 6);
    display.setFont(display.style.getFontFamily().readableFont);

    display.getTextBounds(batteryStatus.c_str(), &width, &height);
    // Arbitrarily put the battery text off of the bottom of the screen 20 pixels
    // This is just so we always have it in our prototype, not that it'll be
    // the right place (or icon) long term
    // Not taking into account the rednerer x and y offsets here on purpose
    display.gfxClass()->setCursor((display.gfxClass()->width() / 2) - (width / 2),
                                  display.gfxClass()->height() - 20);
    display.println(batteryStatus.c_str());

    display.updateDisplay();
    display.setFont(display.style.getFontFamily().readableFont);

    needsRerender = false;
}
*/
/*
void MainMenuScreen::nextSelection() {
    currSelection_ = (currSelection_ + 1) % numMenuItems_;

        if (currSelection_ == numMenuItems_ - 2) {
            currSelection_ = 0;
        }

    needsRerender = true;
}

void MainMenuScreen::prevSelection() {
    currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
      if (currSelection_ == numMenuItems_ - 1) {
          // If there are 0 readables, this breaks the menu
          currSelection_ = numMenuItems_ - 3;
      }
    needsRerender = true;
}

*/
//
// For a PagedMenu, we actually want to progress through
// the options via the left and right buttons and use
// the down button (CHANNEL_PLUS) to select the text.
//
// This is more consistent with the behavior of turning pages
// and using down to go to the popup menu and up to go back.
//
// While we are using the TurnTouch, we should make the commands
// more consistent (left and right versus up and down).
//
// Up is back
// Right is next, Left is previous
// Down is select
//
void MainMenuScreen::processCommand(RemoteCommand command) {
    int oldCurr = 0;

    switch (command) {
    case RemoteCommand::PREV:

        prevSelection();
        needsRerender = true;
        break;

    case RemoteCommand::NEXT:
        nextSelection();
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