#include "LayoutTestScreen.hpp"

void LayoutTestScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    view.setFrame({{0, 0}, {display.width(), display.height()}});
    view.layoutIfNeeded();
    view.draw();

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;
    needsRerender = false;
}

void LayoutTestScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_PLUS:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
