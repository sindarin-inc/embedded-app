#include "RemoteInput.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

auto CodeToString(RemoteCommand code) -> const char * {
    switch (code) {
    case VOL_DOWN:
        return "Volume Down";
    case PLAY:
        return "Play";
    case VOL_UP:
        return "Volume Up";
    case SETUP:
        return "Setup";
    case PREV:
        return "Previous";
    case STOP:
        return "Stop";
    case CHANNEL_MINUS:
        return "Channel -";
    case ENTER:
        return "Enter";
    case CHANNEL_PLUS:
        return "Channel +";
    case NOP_BUTTON:
        return "NOP";
    case NEXT:
        return "Next";
    case BACK:
        return "Back";
    case NONE:
    case FACTORY_DEFAULTS:
    default: {
    }
    }
    return "Unknown";
}

// Returns the IR code of the last button pressed or released
auto RemoteInput::getCode() -> RemoteCommand { return lastCode_; }