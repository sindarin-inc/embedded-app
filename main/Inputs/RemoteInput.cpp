#include "RemoteInput.hpp"

#include "sindarin-debug.h"

auto DecodePageTurner(uint16_t processed) -> RemoteCommand {
    switch (processed) {
    case PT_N_PRESS:
    case PT_N_HOLD:
        return PREV;
    case PT_E_PRESS:
        return CHANNEL_PLUS;
    case PT_W_PRESS:
        return CHANNEL_MINUS;
    case PT_W_HOLD:
        return BACK;
    case PT_S_PRESS:
        return NEXT;
    case PT_S_HOLD:
        return STOP;
    case PT_MULTI_N_S:
        return SETUP;
    case PT_MULTI_E_W:
    case PT_C_PRESS:
    case PT_C_HOLD:
        return ENTER;
    case PT_MULTI_N_E_W:
        return FACTORY_DEFAULTS;
    case PT_E_HOLD:
    default:
        return NONE;
    }
}

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
