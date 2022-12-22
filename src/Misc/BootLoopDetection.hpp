#pragma once

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

void BootLoopCheck();
void SetGoodBoot(TimerHandle_t timer);