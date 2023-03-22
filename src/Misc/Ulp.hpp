#pragma once

#include "Misc/SolClock.hpp"

void UlpWakeupCheck(SolClock &clock);
void UlpDeepSleepMagnetometerWakeup();
void UlpDeepSleepMagnetometerWakeupWithWakeTime(time_t wakeTime);