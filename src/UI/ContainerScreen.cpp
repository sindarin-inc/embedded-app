#include "UI/ContainerScreen.hpp"

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
