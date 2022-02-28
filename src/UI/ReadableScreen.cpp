#include "UI/ReadableScreen.hpp"

#include <utility>

#include "sindarin-debug.h"

void ReadableScreen::setLeftPopupButton(const std::string &title,
                                        std::function<void(void)> callback) {
    popup_.setLeftButtonTitle(title);
    leftButtonCallback_ = std::move(callback);
}

void ReadableScreen::render(DisplaySystem &display) {

    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }

    if (!needsRerender) {
        return;
    }
    if (!inPopup_) {
        newPlace_ = renderer_.renderPage(readable_.place);
        log_d("%lu is the new place in the text", newPlace_);
        needsRerender = false;
    } else {
        log_d("Rendering popup menu");
        popup_.render(display);
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
            readable_.place = renderer_.getPreviousPlace();
            prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
            needsRerender = true;
        } else {
            if (leftButtonCallback_) {
                leftButtonCallback_();
            } else {
                // Go back to the main menu from the popup menu if they press left
                back();
            }
        }
        break;
    case RemoteCommand::CHANNEL_PLUS:
        if (!inPopup_) {
            readable_.placePrev = readable_.place;
            readable_.place = newPlace_;
            prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
            needsRerender = true;
        } else {
            // Reserved for future use? Potentially lighting up and down?
            this->renderer_.getDisplay()->flipInversion();
            // When we invert, we should do a full clear of the display to make sure
            // the blacks are really gone.
            this->renderer_.getDisplay()->clearDisplay();
            prefs_.invertSetting.set(this->renderer_.getDisplay()->getInversion());
            needsRerender = true;
            popup_.needsRerender = true;
        }
        break;
    case RemoteCommand::PREV:
        if (!inPopup_) {
            back();
        } else {
            inPopup_ = false;
            needsRerender = true;
            popup_.needsRerender = true;
            this->renderer_.getDisplay()->clearDisplay();
        }
        break;
    case RemoteCommand::NEXT:
        if (!inPopup_) {
            inPopup_ = true;
            needsRerender = true;
        } else {
            DisplaySystem *display = this->renderer_.getDisplay();

            auto settingsMenu = new PagedMenuScreen(this->power_);

            // We want arrows on both sides so it's clear you can go back to the
            // popup menu from this menu.
            settingsMenu->setBidirectional();

            settingsMenu->addMenuItem(MenuItem("Adjust Offset", [this, settingsMenu, display]() {
                log_d("Loading adjust offset");
                auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_, this->power_);
                display->clearDisplay();
                this->setCurrentScreen(*offsetAdjust);
                log_d("setting up callback");
                offsetAdjust->backCallback = [this, offsetAdjust, settingsMenu, display]() {
                    display->clearDisplay();
                    setCurrentScreen(*settingsMenu);
                    delete offsetAdjust;
                };
                log_d("set up callback");
            }));
            settingsMenu->addMenuItem(
                MenuItem("Calibration Screen", [this, settingsMenu, display]() {
                    auto calibrationScreen = new CalibrationScreen();
                    display->clearDisplay();
                    this->setCurrentScreen(*calibrationScreen);
                    calibrationScreen->backCallback = [this, calibrationScreen, settingsMenu,
                                                       display]() {
                        display->clearDisplay();
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
#if USE_TURN_TOUCH_REMOTE
            settingsMenu->addMenuItem(MenuItem("Unpair TurnTouch", [this, display]() {
                prefs_.pairedTurnTouchAddress.remove();
                log_w("Unpaired TurnTouch. Rebooting...");
                display->renderMaxBoxWithText("Unpaired TurnTouch! Resetting...");
                delay(2000);
                esp_restart();
            }));
#endif
            settingsMenu->backCallback = [this, settingsMenu, display]() {
                inPopup_ = false;
                display->clearDisplay();
                setCurrentScreen(*this);
                needsRerender = true;
                popup_.needsRerender = true;
                delete settingsMenu;
            };
            log_d("Setting current screen to the newly created settingsMenu");
            this->renderer_.getDisplay()->clearDisplay();
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