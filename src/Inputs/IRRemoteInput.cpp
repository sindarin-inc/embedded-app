#include "IRRemoteInput.hpp"

#include <IRremote.h>

#include "sindarin-debug.h"

void IRRemoteInput::start_remote(int receive_pin_input) {
    receive_pin = receive_pin_input;
    // Start the receiver, enable feedback LED,
    // take LED feedback pin from the internal boards definition
    IrReceiver.begin(receive_pin, true);
    last_code = RemoteCommand::NONE;
    lastPressTime = 0;
    state = LOW;
}

void IRRemoteInput::start_remote() {
    DEBUG_LN("FAIL DUE TO NO PIN SENT IN IRREMOTE CONSTUCTOR");
    assert(0);
}

auto IRRemoteInput::check_press() -> int {
    int new_press = 0;
    if (IrReceiver.decode()) {
        last_code = (RemoteCommand)IrReceiver.decodedIRData.command;
        if (state == LOW) {
            state = HIGH; // Button pressed, so set state to HIGH
            print_out_code("Pressed down on", last_code);
            new_press = 1;
            // digitalWrite(LED_PIN, HIGH);
        }
        lastPressTime = millis();
        IrReceiver.resume(); // Receive the next value
    }
    return new_press;
}

auto IRRemoteInput::check_release() -> int {
    int found_release = 0;
    if ((state == HIGH) && ((millis() - lastPressTime) > MAX_TIME)) {
        state = LOW; // Haven't heard from the button for a while, so not pressed
        print_out_code("Pressed up on", last_code);
        found_release = 1;
        // digitalWrite(LED_PIN, LOW);
    }
    return found_release;
}
