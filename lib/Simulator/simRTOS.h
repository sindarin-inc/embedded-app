#ifndef SIM_RTOS_H
#define SIM_RTOS_H

#include <cstdint>

class FreeRTOS {
public:
	static uint32_t getTimeSinceStart();
};

#endif