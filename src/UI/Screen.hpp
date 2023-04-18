#pragma once

#include <functional>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"

/*!
Abstract base class for subclasses that represent a piece of content that takes over the whole
screen.
*/
class Screen {
public:
    virtual ~Screen() = default;
    virtual void render(DisplaySystem &display) = 0;
    virtual void processCommand(RemoteCommand command) = 0;
    virtual void setNeedsRerender() { needsRerender_ = true; }
    virtual auto needsRerender() -> bool { return needsRerender_; }

    virtual void loop(DisplaySystem &display, RemoteCommand command) {
        if (command != RemoteCommand::NONE) {
            processCommand(command);
        }
        if (needsRerender()) {
            display.beginPowerOn();
            render(display);
            needsRerender_ = false;
        }
    }

    std::function<void()> backCallback;

protected:
    bool needsRerender_ = true;
};
