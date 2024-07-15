#include "SolMorse.hpp"

#include "Platform.hpp"

void BlinkDot() {
    DigitalWrite(CONFIG_RED_LED, LED_ON);
    Delay(100);
    DigitalWrite(CONFIG_RED_LED, LED_OFF);
    Delay(100);
}

void BlinkDash() {
    DigitalWrite(CONFIG_RED_LED, LED_ON);
    Delay(300);
    DigitalWrite(CONFIG_RED_LED, LED_OFF);
    Delay(100);
}

void BlinkSpace() { Delay(500); }

void BlinkSol() {
    BlinkDot();
    BlinkDot();
    BlinkDot();

    BlinkSpace();

    BlinkDash();
    BlinkDash();
    BlinkDash();

    BlinkSpace();

    BlinkDot();
    BlinkDash();
    BlinkDot();
    BlinkDot();
}