#include "SolMorse.hpp"

#include "Platform.hpp"

void blinkDot() {
    DigitalWrite(RED_LED, LED_ON);
    Delay(100);
    DigitalWrite(RED_LED, LED_OFF);
    Delay(100);
}

void blinkDash() {
    DigitalWrite(RED_LED, LED_ON);
    Delay(300);
    DigitalWrite(RED_LED, LED_OFF);
    Delay(100);
}

void blinkSpace() { Delay(500); }

void blinkSOL() {
    blinkDot();
    blinkDot();
    blinkDot();

    blinkSpace();

    blinkDash();
    blinkDash();
    blinkDash();

    blinkSpace();

    blinkDot();
    blinkDash();
    blinkDot();
    blinkDot();
}