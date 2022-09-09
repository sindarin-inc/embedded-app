#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Bitmaps/Menu_Blank.hpp"
#include "Bitmaps/Menu_Blank_Bidirectional.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#if !SIMULATOR
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#define MAX_WIDTH 152

void PagedMenuScreen::render(DisplaySystem &display) {

    uint16_t midX = display.width() / 2;
    uint16_t midY = display.height() / 2;

    // Top Arrow
    display.drawLine(midX, 42, midX - 10, 47, display.fg);
    display.drawLine(midX, 42, midX + 10, 47, display.fg);

    // Bottom Arrow
    display.drawLine(midX, midY * 2 - 40, midX - 10, midY * 2 - 45, display.fg);
    display.drawLine(midX, midY * 2 - 40, midX + 10, midY * 2 - 45, display.fg);

    // Left Arrow
    display.drawLine(10, midY, 15, midY - 10, display.fg);
    display.drawLine(10, midY, 15, midY + 10, display.fg);

    // Right Arrow
    display.drawLine(midX * 2 - 10, midY, midX * 2 - 15, midY - 10, display.fg);
    display.drawLine(midX * 2 - 10, midY, midX * 2 - 15, midY + 10, display.fg);

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
    uint16_t screenWidth = display.width();
    uint16_t screenHeight = display.height();

    display.setRotation(0);
    display.drawSingleLineOfText(itemTitle.c_str(), (screenWidth / 2) - (width / 2),
                                 (screenHeight / 2) - (display.getTextHeight()) / 2);

    auto prevTitle = getItemAt((currSelection_ > 0) ? ((currSelection_ - 1) % numMenuItems_)
                                                    : (numMenuItems_ - 1))
                         .title;
    auto nextTitle = getItemAt((currSelection_ + 1) % numMenuItems_).title;

    display.getTextBounds(prevTitle.c_str(), &width, &height);

    display.drawSingleLineOfText(prevTitle.c_str(), (screenWidth / 2) - (width / 2), 80);

    display.getTextBounds(nextTitle.c_str(), &width, &height);

    display.drawSingleLineOfText(nextTitle.c_str(), (screenWidth / 2) - (width / 2),
                                 (screenHeight - 80 - display.getTextHeight()));

    display.setFont(display.style.getFontFamily().readableFont);
    display.setRotation(0);
    display.getTextBounds("Settings", &width, &height);

    display.drawSingleLineOfText("Settings", screenWidth / 2 - width / 2, 28);

    display.drawXBitmap((int16_t)((display.width() - GEAR_WIDTH) / 2), 40, GEAR_BITS, GEAR_WIDTH,
                        GEAR_HEIGHT, display.fg);

    display.getTextBounds("Read", &width, &height);

    display.drawSingleLineOfText("Read", screenWidth / 2 - width / 2, screenHeight - 30 - height);

    display.drawXBitmap((int16_t)((display.width() - BOOK_WIDTH) / 2),
                        screenHeight - 30 - height - 25, BOOK_BITS, BOOK_WIDTH, BOOK_HEIGHT,
                        display.fg);

    display.drawXBitmap((int16_t)((display.width() - ARTICLES_WIDTH) / 2) + 35,
                        screenHeight - 30 - height - 25, ARTICLES_BITS, ARTICLES_WIDTH,
                        ARTICLES_HEIGHT, display.fg);

    display.setRotation(0);
    display.updateDisplay();

    // display.drawLine(display.fg);
    needsRerender = false;
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

void PagedMenuScreen::nextSelection() {
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    if (currSelection_ == numMenuItems_ - 2) {
        currSelection_ = 0;
    }
    needsRerender = true;
}

void PagedMenuScreen::prevSelection() {
    currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
    if (currSelection_ == numMenuItems_ - 1) {
        // If there are 0 readables, this breaks the menu
        currSelection_ = numMenuItems_ - 3;
    }
    needsRerender = true;
}

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
void PagedMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> 83145fe (simplify main menu)
        // Settings are always the last item
        currSelection_ = numMenuItems_ - 1;
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
<<<<<<< HEAD
=======

        prevSelection();
        needsRerender = true;
>>>>>>> 56e5564 (add popup idea based on CS logo 4)
=======
>>>>>>> 83145fe (simplify main menu)
        break;

    case RemoteCommand::NEXT:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
        // Pocket articles is always one from the last
        /*
        currSelection_ = numMenuItems_ - 2;
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;*/

    case RemoteCommand::ENTER:

        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
<<<<<<< HEAD
<<<<<<< HEAD
        // Pocket articles is always one from the last
        /*
        currSelection_ = numMenuItems_ - 2;
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;*/

    case RemoteCommand::ENTER:

        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
=======
>>>>>>> 83145fe (simplify main menu)
    case RemoteCommand::CHANNEL_PLUS:

        nextSelection();
        needsRerender = true;

<<<<<<< HEAD
=======
    case RemoteCommand::CHANNEL_PLUS:

        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
>>>>>>> 56e5564 (add popup idea based on CS logo 4)
        break;

<<<<<<< HEAD
=======
        // Pocket articles is always one from the last
        /*
        currSelection_ = numMenuItems_ - 2;
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;*/
>>>>>>> 75ec884 (make TT work again and connect wifi if there)
=======
        break;

>>>>>>> 83145fe (simplify main menu)
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        prevSelection();
        needsRerender = true;
        break;
    default:
        break;
    }
}