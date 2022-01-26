#include "UI/ReadableScreen.hpp"

#include "sindarin-debug.h"

void ReadableScreen::render(DisplaySystem &display) {

    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }

    if (!needsRerender) {
        return;
    }
    if (!inPopup_) {
        log_d("Rendering some text in READ MODE");
        newPlace_ = fileRenderer_->renderPageWithBox(readable_.place);
        log_d("%d is the new place in the text", newPlace_);
        needsRerender = false;
    } else {
        log_d("Rendering popup menu");
        popup_->render(display);
        needsRerender = false;
    }
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
        needsRerender = true;
    };

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        if (!inPopup_) {
            readable_.place = fileRenderer_->getPreviousPlace();
            prefs_.readablePreviousPlace.set(readable_.id, readable_.place);
            needsRerender = true;
        } else {
            // Go back to the main menu from the popup menu of they press left
            back();
        }
        break;
    case RemoteCommand::CHANNEL_PLUS:
        if (!inPopup_) {
            readable_.placePrev = readable_.place;
            readable_.place = newPlace_;
            prefs_.readablePlace.set(readable_.id, readable_.place);
            prefs_.readablePreviousPlace.set(readable_.id, readable_.placePrev);
            needsRerender = true;
        } else {
            // Reserved for future use? Potentially lighting up and down?
        }
        break;
    case RemoteCommand::PREV:
        if (!inPopup_) {
            back();
        } else {
            inPopup_ = false;
            needsRerender = true;
            popup_->needsRerender = true;
        }
        break;
    case RemoteCommand::NEXT:
        if (!inPopup_) {
            inPopup_ = true;
            needsRerender = true;
        } else {
            DisplaySystem *display = this->fileRenderer_->getDisplay();

            auto settingsMenu = new PagedMenuScreen(this->power_);

            settingsMenu->addMenuItem(MenuItem("Adjust Offset", [this, settingsMenu, display]() {
                log_d("Loading adjust offset");
                auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_, this->power_);
                this->setCurrentScreen(*offsetAdjust);
                log_d("setting up callback");
                offsetAdjust->backCallback = [this, offsetAdjust, settingsMenu]() {
                    setCurrentScreen(*settingsMenu);
                    delete offsetAdjust;
                };
                log_d("set up callback");
            }));
            settingsMenu->addMenuItem(
                MenuItem("Calibration Screen", [this, settingsMenu, display]() {
                    auto calibrationScreen = new CalibrationScreen();
                    this->setCurrentScreen(*calibrationScreen);
                    calibrationScreen->backCallback = [this, calibrationScreen, settingsMenu]() {
                        setCurrentScreen(*settingsMenu);
                        delete calibrationScreen;
                    };
                }));
            settingsMenu->addMenuItem(MenuItem("Reset Prefs", [this, display]() {
                prefs_.clear();
                log_w("Preferences cleared. Rebooting...");

                display->renderMaxBoxWithText("Preferences cleared! Resetting...");
                delay(2000);
                esp_restart();
            }));
            settingsMenu->addMenuItem(MenuItem("Unpair TurnTouch", [this, display]() {
                prefs_.pairedTurnTouchAddress.remove();
                log_w("Unpaired TurnTouch. Rebooting...");
                display->renderMaxBoxWithText("Unpaired TurnTouch! Resetting...");
                delay(2000);
                esp_restart();
            }));
            settingsMenu->backCallback = [this, settingsMenu]() {
                inPopup_ = false;
                setCurrentScreen(*this);
                needsRerender = true;
                popup_->needsRerender = true;
                delete settingsMenu;
            };
            log_d("Setting current screen to the newly created settingsMenu");
            setCurrentScreen(*settingsMenu);
            inPopup_ = false;
        }
        break;
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::NOP_BUTTON:
        needsRerender = true;
        break;
    default:
        break;
    }
}