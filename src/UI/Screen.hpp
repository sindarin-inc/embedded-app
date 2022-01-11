#pragma once

#include <Arduino.h>
#include <functional>

#include "Inputs/RemoteInput.hpp"

/*!
Abstract base class for subclasses that represent a piece of content that takes over the whole
screen.
*/
class Screen {
public:
    virtual ~Screen() = default;
    virtual void render() = 0;
    virtual void process_command(RemoteCommand command) = 0;

    virtual void loop(RemoteCommand command) {
        if (command != RemoteCommand::NONE) {
            process_command(command);
        }
        render();
    }

    std::function<void()> back_callback;
    bool needs_rerender = true;
};
