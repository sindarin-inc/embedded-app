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
                // Do the -5% thing. This is unlikley to be right long-term
                // but it's way better if you overshoot.
                readable_.placePrev = readable_.place;

                size_t place = renderer_.getCurrentPlace();
                size_t lengthOfFile = renderer_.size();

                // Subtract 5% to the file
                if (readable_.place >= lengthOfFile / 20) {
                    readable_.place = readable_.place - lengthOfFile / 20;
                } else {
                    readable_.place = 0;
                }

                log_d("Setting place from %d to %d", readable_.placePrev, readable_.place);

                renderer_.setCurrentPlace(readable_.place);
                prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
                needsRerender = true;
                popup_.needsRerender = true;
                this->renderer_.getDisplay()->clearDisplay();
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
            readable_.placePrev = readable_.place;

            size_t place = renderer_.getCurrentPlace();
            size_t lengthOfFile = renderer_.size();

            // Add 5% to the file
            readable_.place = readable_.place + lengthOfFile / 20;

            log_d("Setting place from %d to %d", readable_.placePrev, readable_.place);

            // Don't set place past EOF
            if (readable_.place >= lengthOfFile) {
                readable_.place = lengthOfFile - 1;
            }

            renderer_.setCurrentPlace(readable_.place);
            prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
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
        }
        break;
    case RemoteCommand::NEXT:
        if (!inPopup_) {
            inPopup_ = true;
            needsRerender = true;
        } else {
            this->renderer_.getDisplay()->flipInversion();
            // When we invert, we should do a full clear of the display to make sure
            // the blacks are really gone.
            this->renderer_.getDisplay()->clearDisplay();
            prefs_.invertSetting.set(this->renderer_.getDisplay()->getInversion());
            needsRerender = true;
            popup_.needsRerender = true;
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