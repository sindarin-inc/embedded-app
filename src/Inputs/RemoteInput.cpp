#include "RemoteInput.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

void print_out_code(const char *prec_msg, int code) {
    DEBUG_LN(prec_msg);
    switch (code) {
    case VOL_DOWN:
        DEBUG_LN("Volume Down");
        break;
    case PLAY:
        DEBUG_LN("Play");
        break;
    case VOL_UP:
        DEBUG_LN("Volume Up");
        break;
    case SETUP:
        DEBUG_LN("Setup");
        break;
    case PREV:
        DEBUG_LN("Previous");
        break;
    case STOP:
        DEBUG_LN("Stop");
        break;
    case CHANNEL_MINUS:
        DEBUG_LN("Channel -");
        break;
    case ENTER:
        DEBUG_LN("Enter");
        break;
    case CHANNEL_PLUS:
        DEBUG_LN("Channel +");
        break;
    case NOP_BUTTON:
        DEBUG_LN("NOP");
        break;
    case NEXT:
        DEBUG_LN("Next");
        break;
    case BACK:
        DEBUG_LN("Back");
        break;
    }
}

// Returns the IR code of the last button pressed or released
auto RemoteInput::get_code() -> RemoteCommand { return last_code; }