#include "UI/ReleaseNotesScreen.hpp"

#include <Update.h>

#include "sindarin-debug.h"

ReleaseNotesScreen::ReleaseNotesScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       PowerManager &power, const Firmware &firmware)
    : display_(display), renderer_(display), prefs_(prefs), power_(power), firmware_(firmware),
      menu_(prefs, display) {

    SpiramString message = SpiRamStringFormat(
        "New firmware %s is now available! Here's what's new:\n\n", firmware.version.c_str());
    message += firmware.releaseNotes;
    message +=
        "\n\nPress the center button to update, or the up button to return to the main menu.";
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);

    menu_.resetMenuItems();
    menu_.addMenuItem(MenuItem("Update Now", [this]() { runUpdate(); }));
    menu_.addMenuItem(MenuItem("Maybe later", [this]() { backCallback(); }));

    setCurrentScreen(*this);
}

void ReleaseNotesScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }

    newPlace_ = this->renderer_.renderPage(place_);

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    needsRerender = false;
}

void ReleaseNotesScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        place_ = renderer_.getPreviousPlace();
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
    case RemoteCommand::BACK:
        setCurrentScreen(menu_);
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::ENTER:
        if (atEnd()) {
            setCurrentScreen(menu_);
            needsRerender = true;
        }
        place_ = newPlace_;
        needsRerender = true;
        break;
    default:
        break;
    }
}

void ReleaseNotesScreen::updateError(const std::string &logMessage) {
    auto error = Update.getError();
    log_e("%s error (%d)", logMessage.c_str(), error);
    display_.drawSingleLineOfCenteredText(StringFormat("Install failed (%d)", error));
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    prefs_.firmwareLoaded.remove();
    prefs_.firmwareLoadedSize.remove();
    backCallback();
    return;
}

void ReleaseNotesScreen::runUpdate() {
    display_.drawSingleLineOfCenteredText("Updating...");
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

    // TODO: Would be better if we had this saved in the firmware model
    if (!Update.begin(prefs_.firmwareLoadedSize.get().value_or(0), U_FLASH)) {
        updateError("Update.begin");
        return;
    }

    if (!Update.setMD5(firmware_.md5.c_str())) {
        updateError("Update.setMD5");
        return;
    }

    // TODO: Not sure if this is going to work since we've already written the firmware
    // Will have to test.
    if (!Update.end(true)) {
        updateError("Update.end");
        return;
    }

    prefs_.firmwareLoaded.remove();
    prefs_.firmwareLoadedSize.remove();

    display_.drawSingleLineOfCenteredText("Rebooting into new version!");
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

    // Reboot
    power_.reboot();

    // Only for the sim
    backCallback();
}
