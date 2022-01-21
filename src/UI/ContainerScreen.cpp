#include "UI/ContainerScreen.hpp"

void ContainerScreen::setCurrentScreen(Screen &screen) {
    screen.needsRerender = true;
    currentScreen_ = &screen;
}

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
