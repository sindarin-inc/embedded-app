#include "Arduino.h"

#include <SDL2/SDL.h>

void delay(int delay_ms) { SDL_Delay(delay_ms); }

void esp_deep_sleep_start(){};

void esp_restart(){};

void digitalWrite(int, int){};

unsigned long millis() { return SDL_GetTicks(); }
