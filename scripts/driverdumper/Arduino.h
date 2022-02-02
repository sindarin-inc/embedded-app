#include <stdint.h>

#define log_d(format, ...) printf(format "\n", ##__VA_ARGS__)
#define log_e(format, ...) printf(format "\n", ##__VA_ARGS__)

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

void delay(uint32_t ms);

#define RSR(a, b)