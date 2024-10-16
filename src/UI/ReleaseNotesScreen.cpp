#include "UI/ReleaseNotesScreen.hpp"

#include <Update.h>

#include "Misc/Platform.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "UI/Menu/NewMenuItemSingleLine.hpp"
#include "sindarin-debug.h"

ReleaseNotesScreen::ReleaseNotesScreen(DisplaySystem &display, PreferencesStore &prefs,
                                       PowerManager &power, LocalStore &localStore,
                                       const Firmware &firmware)
    : display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs), static_cast<Font *>(&fontFace0)),
      prefs_(prefs), power_(power), localStore_(localStore), firmware_(firmware),
      menu_(prefs, display) {

    SpiramString message = SpiRamStringFormat(
        "New firmware %s is now available! Here's what's new:\n\n", firmware.version.c_str());
    message += firmware.releaseNotes;
    message += "\n\nPress \ue000 (center) to update!";
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);

    menu_.resetMenuItems();
    menu_.addMenuItem(std::make_shared<NewMenuItemSingleLine>("Update Now", [this]() {
        inMenu_ = false;
        runUpdate();
    }));
    menu_.addMenuItem(std::make_shared<NewMenuItemSingleLine>("Maybe later", [this]() {
        inMenu_ = false;
        back();
    }));
}

void ReleaseNotesScreen::render(DisplaySystem &display) {
    if (inMenu_) {
        menu_.render(display);
        return;
    }

    uint8_t darkPicturePercent = 0;
    std::tie(newPlace_, darkPicturePercent) = this->renderer_.renderPage(place_);

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                              display.darkPictureVComAdj(darkPicturePercent));
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                              display.darkPictureVComAdj(darkPicturePercent));
    }
}

void ReleaseNotesScreen::processCommand(RemoteCommand command) {
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

void ReleaseNotesScreen::updateError(const std::string &logMessage) {
    auto error = Update.getError();
    log_e("%s error (%d)", logMessage.c_str(), error);
    display_.drawSingleLineOfCenteredText(StringFormat("Install failed (%d)", error));
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    prefs_.firmwareLoaded.remove();
    localStore_.resetFirmware();
    back();
}

void ReleaseNotesScreen::runUpdate() {
    display_.drawSingleLineOfCenteredText("Updating...");
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

    // TODO: Would be better if we had this saved in the firmware model
    if (!Update.begin(firmware_.fileSize, U_FLASH)) {
        updateError("Update.begin");
        return;
    }

    if (!Update.setMD5(firmware_.md5.c_str())) {
        updateError("Update.setMD5");
        return;
    }

    // Rerun all of the bytes through the MD5 for the checker
    Update.recalculateMD5();

    // TODO: Not sure if this is going to work since we've already written the firmware
    // Will have to test.
    if (!Update.end(true)) {
        updateError("Update.end");
        return;
    }

    prefs_.firmwareLoaded.remove();
    localStore_.resetFirmware();

    display_.drawSingleLineOfCenteredText(
        StringFormat("%s installed! Restarting...", firmware_.version.c_str()));
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

    Delay(2000);

    // Reboot
    power_.reboot();
}
