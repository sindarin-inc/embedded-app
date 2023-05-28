#include "TurnTouch.hpp"

auto DecodeTurntouch(uint16_t processed) -> RemoteCommand {
    switch (processed) {
    case TT_N_PRESS:
    case TT_N_HOLD:
        return RemoteCommand::UP;
    case TT_S_PRESS:
        return RemoteCommand::DOWN;
    case TT_W_PRESS:
        return RemoteCommand::LEFT;
    case TT_W_HOLD:
        return RemoteCommand::BACK;
    case TT_E_PRESS:
        return RemoteCommand::RIGHT;
    case TT_MULTI_E_W:
    case TT_N_DOUBLE:
        return RemoteCommand::ENTER;
    case TT_MULTI_E_N_W:
        // Be careful with the double press codes, since you also get two single press commands
        // immediately before
    case TT_MULTI_N_S:
    case TT_S_HOLD:
    case TT_E_HOLD:
    case TT_S_DOUBLE:
    case TT_W_DOUBLE:
    case TT_E_DOUBLE:
    default:
        return NONE;
    }
}
