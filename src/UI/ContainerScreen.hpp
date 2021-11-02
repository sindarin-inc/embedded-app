#pragma once

#include "UI/Screen.hpp"

/*!
Screen that can contain other screens and switch between them.
*/
class ContainerScreen : public Screen {
public:
    void set_current_screen(Screen &screen);
    void render();
    void process_command(RemoteCommand command);

protected:
    Screen *current_screen;
};
