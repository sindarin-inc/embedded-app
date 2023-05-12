#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

/*!
Screen that can contain other screens and switch between them.
*/
class ContainerScreen : public Screen {
public:
    void setCurrentScreen(Screen &screen);
    auto needsRerender() -> bool override;
    // Typically overridden by subclasses and will need to replicate the default functionality
    void render(DisplaySystem &display) override;
    void renderIfNeeded(DisplaySystem &display) override;
    // Typically overridden by subclasses and will need to replicate the default functionality
    void processCommand(RemoteCommand command) override;
    auto getCurrentScreen() -> Screen * { return currentScreen_; }

protected:
    Screen *currentScreen_ = nullptr;
};
