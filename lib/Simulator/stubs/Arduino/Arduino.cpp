#include "Arduino.h"

#include <SDL2/SDL.h>

void delay(int delay_ms) { SDL_Delay(delay_ms); }

void esp_deep_sleep_start(){};

void esp_restart(){};

void digitalWrite(uint8_t pin, uint8_t val){};

void pinMode(uint8_t pin, uint8_t mode) {}

int digitalRead(uint8_t pin) { return 0; }

unsigned long millis() { return SDL_GetTicks(); }
