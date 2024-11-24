#include "UI/TutorialScreen.hpp"

#include <Update.h>

#include "Misc/Platform.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "sindarin-debug.h"

TutorialScreen::TutorialScreen(DisplaySystem &display, PreferencesStore &prefs, SpiramString text,
                               int bit)
    : display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs), static_cast<Font *>(&fontFace0)),
      prefs_(prefs), menu_(prefs, display) {

    auto stream = std::make_shared<StdStringStream>(text);
    renderer_.setStream(stream);

    menu_.resetMenuItems();
    menu_.addMenuItem(std::make_shared<MenuItemSingleLine>("Delete this tutorial", [this, bit]() {
        prefs_.tutorialsDeleted.set(prefs_.tutorialsDeleted.get().value_or(0) | bit);
        inMenu_ = false;
        back();
    }));
    menu_.addMenuItem(std::make_shared<MenuItemSingleLine>("Keep this tutorial", [this, bit]() {
        // Set it here just in case they change their mind after deleting it and perform
        // this operation a second time.
        uint32_t mask = ~bit;
        prefs_.tutorialsDeleted.set(prefs_.tutorialsDeleted.get().value_or(0) & mask);
        inMenu_ = false;
        back();
    }));
}

void TutorialScreen::render(DisplaySystem &display) {
    if (inMenu_) {
        menu_.render(display);
        return;
    }

    uint8_t darkPicturePercent = 0;
    std::tie(newPlace_, darkPicturePercent) = this->renderer_.renderPage(place_);

    display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                          display.darkPictureVComAdj(darkPicturePercent));
}

void TutorialScreen::processCommand(RemoteCommand command) {
    if (inMenu_) {
        menu_.processCommand(command);
        if (menu_.needsRerender()) {
            setNeedsRerender();
        }
        return;
    }

    switch (command) {
    case RemoteCommand::LEFT:
        place_ = renderer_.getPreviousPlace();
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
    case RemoteCommand::BACK:
        inMenu_ = true;
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
    case RemoteCommand::RIGHT:
    case RemoteCommand::ENTER:
        if (atEnd()) {
            inMenu_ = true;
            setNeedsRerender();
        }
        place_ = newPlace_;
        setNeedsRerender();
        break;
    default:
        break;
    }
}
