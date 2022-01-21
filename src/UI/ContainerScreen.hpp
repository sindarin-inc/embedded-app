#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

/*!
Screen that can contain other screens and switch between them.
*/
class ContainerScreen : public Screen {
public:
    void setCurrentScreen(Screen &screen);
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    Screen *currentScreen_;
};
