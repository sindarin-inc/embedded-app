#pragma once

#include <cstdint>

#include "RemoteInput.hpp"

// See also: https://shop.turntouch.com/pages/open-source
#define TURNTOUCH_BUTTON_SVC_ID "1523"
#define TURNTOUCH_BUTTON_SVC_UUID "99c31523-dc4f-41b1-bb04-4e4deb81fadd"
#define TURNTOUCH_BUTTON_CHR_UUID "99c31525-dc4f-41b1-bb04-4e4deb81fadd"
#define TURNTOUCH_NICKNAME_CHR_UUID "99c31526-dc4f-41b1-bb04-4e4deb81fadd"
#define TURNTOUCH_BATTERY_SVC_ID "180F"
#define TURNTOUCH_DIS_SVC_ID "180A"
#define TURNTOUCH_BATTERY_CHR_ID "2A19"
#define TURNTOUCH_BUILD_CHR_ID "2A28"

enum {
    TT_NONE = 0xFF00,
    TT_N_PRESS = 0xFE00,
    TT_N_DOUBLE = 0xEF00,
    TT_N_HOLD = 0xFEFF,
    TT_S_PRESS = 0xF700,
    TT_S_DOUBLE = 0x7F00,
    TT_S_HOLD = 0xF7FF,
    TT_E_PRESS = 0xFD00,
    TT_E_DOUBLE = 0xDF00,
    TT_E_HOLD = 0xFDFF,
    TT_W_PRESS = 0xFB00,
    TT_W_DOUBLE = 0xBF00,
    TT_W_HOLD = 0xFBFF,
    TT_MULTI_N_S = 0xF600,
    TT_MULTI_E_W = 0xF900,
    TT_MULTI_E_N_W = 0xF800,
    TT_MULTI_N_W_S = 0xF400,
    TT_MULTI_S_E_N = 0xF200,
    TT_MULTI_W_S_E = 0xF100,
    TT_MULTI_E_N_W_S = 0xF000
};

auto DecodeTurntouch(uint16_t processed) -> RemoteCommand;
