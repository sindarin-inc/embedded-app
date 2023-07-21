#include "UI/TutorialScreen.hpp"

#include <Update.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

TutorialScreen::TutorialScreen(DisplaySystem &display, PreferencesStore &prefs, SpiramString text,
                               int bit)
    : display_(display), renderer_(display), prefs_(prefs), menu_(prefs, display) {

    auto stream = std::make_shared<StdStringStream>(text);
    renderer_.setStream(stream);

    menu_.resetMenuItems();
    menu_.addMenuItem(MenuItem("Delete this tutorial on next close", [this, bit]() {
        prefs_.tutorialsDeleted.set(prefs_.tutorialsDeleted.get().value_or(0) | bit);
        back();
    }));
    menu_.addMenuItem(MenuItem("Keep this tutorial", [this, bit]() {
        // Set it here just in case they change their mind after deleting it and perform
        // this operation a second time.
        uint32_t mask = ~bit;
        prefs_.tutorialsDeleted.set(prefs_.tutorialsDeleted.get().value_or(0) & mask);
        back();
    }));

    setCurrentScreen(*this);
}

void TutorialScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }

    newPlace_ = this->renderer_.renderPage(place_);

    display.updateDisplay(DisplaySystem::UpdateType::FULL);
}

void TutorialScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    switch (command) {
    case RemoteCommand::LEFT:
        place_ = renderer_.getPreviousPlace();
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
    case RemoteCommand::BACK:
        setCurrentScreen(menu_);
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
    case RemoteCommand::RIGHT:
    case RemoteCommand::ENTER:
        if (atEnd()) {
            setCurrentScreen(menu_);
            setNeedsRerender();
        }
        place_ = newPlace_;
        setNeedsRerender();
        break;
    default:
        break;
    }
}
