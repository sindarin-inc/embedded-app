#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class TestAlignmentScreen : public Screen {
public:
    TestAlignmentScreen() : Screen() {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;
};
