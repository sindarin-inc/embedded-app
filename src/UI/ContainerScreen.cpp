#include "UI/ContainerScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/MsgPopup.hpp"

void ContainerScreen::setCurrentScreen(Screen &screen) {
    screen.setNeedsRerender();
    currentScreen_ = &screen;
}

auto ContainerScreen::needsRerender() -> bool {
    if (currentScreen_ != nullptr && currentScreen_ != this) {
        return currentScreen_->needsRerender();
    } else {
        return Screen::needsRerender();
    }
};

void ContainerScreen::render(DisplaySystem &display) {
    if (currentScreen_ != nullptr) {
        currentScreen_->render(display);
    }
};

void ContainerScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != nullptr) {
        currentScreen_->processCommand(command);
    }
}

void ContainerScreen::clearNeedsRerender() {
    if (currentScreen_ != nullptr && currentScreen_ != this) {
        currentScreen_->clearNeedsRerender();
    } else {
        Screen::clearNeedsRerender();
    }
}

void ContainerScreen::showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                           int duration, bool rerenderAfter) {
    auto *popup = new MsgPopup(display, {message}, false);
    setCurrentScreen(*popup);
    render(display);
    delete popup;
    Delay(duration);
    if (rerenderAfter) {
        setNeedsRerender();
        this->render(display);
    }
}
