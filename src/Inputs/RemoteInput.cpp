#include "RemoteInput.hpp"

#include "sindarin-debug.h"

auto DecodePageTurner(uint16_t processed) -> RemoteCommand {
    switch (processed) {
    case PT_N_PRESS:
    case PT_N_HOLD:
        return RemoteCommand::UP;
    case PT_E_PRESS:
        return RemoteCommand::RIGHT;
    case PT_W_PRESS:
        return RemoteCommand::LEFT;
    case PT_W_HOLD:
        return RemoteCommand::BACK;
    case PT_S_PRESS:
        return RemoteCommand::DOWN;
    case PT_MULTI_E_W:
    case PT_C_PRESS:
    case PT_C_HOLD:
        return ENTER;
    case PT_MULTI_N_E_W:
    case PT_S_HOLD:
    case PT_MULTI_N_S:
    case PT_E_HOLD:
    default:
        return NONE;
    }
}

auto CodeToString(RemoteCommand code) -> const char * {
    switch (code) {
    case UP:
        return "Up";
    case LEFT:
        return "Left";
    case ENTER:
        return "Enter";
    case RIGHT:
        return "Right";
    case DOWN:
        return "Down";
    case BACK:
        return "Back";
    case NONE:
    default: {
    }
    }
    return "Unknown";
}
