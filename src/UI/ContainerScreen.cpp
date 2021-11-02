#include "UI/ContainerScreen.hpp"

void ContainerScreen::set_current_screen(Screen &screen) {
    screen.needs_rerender = true;
    current_screen = &screen;
}

void ContainerScreen::render() {
    if (current_screen != nullptr) {
        current_screen->render();
    }
};

void ContainerScreen::process_command(RemoteCommand command) {
    if (current_screen != nullptr) {
        current_screen->process_command(command);
    }
}
