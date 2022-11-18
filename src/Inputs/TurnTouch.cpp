#include "TurnTouch.hpp"

auto DecodeTurntouch(uint16_t processed) -> RemoteCommand {
    switch (processed) {
    case TT_N_PRESS:
    case TT_N_HOLD:
        return PREV;
    case TT_S_PRESS:
        return NEXT;
    case TT_S_HOLD:
        return STOP;
    case TT_W_PRESS:
        return CHANNEL_MINUS;
    case TT_W_HOLD:
        return BACK;
    case TT_E_PRESS:
        return CHANNEL_PLUS;
    case TT_E_HOLD:
        return PLAY;
    case TT_MULTI_N_S:
        return SETUP;
    case TT_MULTI_E_W:
    case TT_N_DOUBLE:
        return ENTER;
    case TT_MULTI_E_N_W:
        return FACTORY_DEFAULTS;
        // Be careful with the double press codes, since you also get two single press commands
        // immediately before
    case TT_S_DOUBLE:
    case TT_W_DOUBLE:
    case TT_E_DOUBLE:
    default:
        return NONE;
    }
}
