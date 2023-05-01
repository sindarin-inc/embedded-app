#include "SolMorse.hpp"

#include "Platform.hpp"

void BlinkDot() {
    DigitalWrite(RED_LED, LED_ON);
    Delay(100);
    DigitalWrite(RED_LED, LED_OFF);
    Delay(100);
}

void BlinkDash() {
    DigitalWrite(RED_LED, LED_ON);
    Delay(300);
    DigitalWrite(RED_LED, LED_OFF);
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