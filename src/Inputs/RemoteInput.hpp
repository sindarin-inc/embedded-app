#ifndef RemoteInput_H
#define RemoteInput_H

#include <cstdint>
#include <functional>
#include <utility>

#define PT_NONE 0xFF00
#define PT_N_PRESS 0xFE00
#define PT_N_HOLD 0xFEFF
#define PT_E_PRESS 0xFD00
#define PT_E_HOLD 0xFDFF
#define PT_W_PRESS 0xFB00
#define PT_W_HOLD 0xFBFF
#define PT_S_PRESS 0xF700
#define PT_S_HOLD 0xF7FF
#define PT_C_PRESS 0xEF00
#define PT_C_HOLD 0xEFFF
#define PT_MULTI_N_S 0xFAFF
#define PT_MULTI_E_W 0xF5FF
#define PT_MULTI_N_E_W 0xF8FF

enum RemoteCommand : int {
    VOL_DOWN = 0x0,
    PLAY = 0x1,
    VOL_UP = 0x2,
    SETUP = 0x4,
    // Up
    PREV = 0x5,
    STOP = 0x6,
    // Left
    CHANNEL_MINUS = 0x8,
    ENTER = 0x9,
    // Right
    CHANNEL_PLUS = 0xA,
    NOP_BUTTON = 0xC,
    // Down
    NEXT = 0xD,
    BACK = 0xE,
    FACTORY_DEFAULTS = 0xFF,

    NONE = -1,
};

auto DecodePageTurner(uint16_t processed) -> RemoteCommand;

auto CodeToString(RemoteCommand code) -> const char *;

class RemoteInput {

public:
    auto setCallback(std::function<void(RemoteCommand)> callback) -> void {
        callback_ = std::move(callback);
    }

protected:
    std::function<void(RemoteCommand)> callback_;
};

#endif
