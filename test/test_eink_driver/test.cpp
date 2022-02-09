#include <cstring>
#include <unity.h>

#include "Displays/EinkET013TT1.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "SPI.h"

EinkET013TT1 driver(0, 1, 2, 3);

// void setUp(void) {
// // set stuff up here
// }

// void tearDown(void) {
// // clean stuff up here
// }

// void test_function_calculator_addition(void) { TEST_ASSERT_EQUAL(32, calc.add(25, 7)); }

// void test_function_calculator_subtraction(void) { TEST_ASSERT_EQUAL(20, calc.sub(23, 3)); }

// void test_function_calculator_multiplication(void) { TEST_ASSERT_EQUAL(50, calc.mul(25, 2)); }

// void test_function_calculator_division(void) { TEST_ASSERT_EQUAL(32, calc.div(100, 3)); }

int main(int argc, char **argv) {
    UNITY_BEGIN();
    driver.init();
    driver.setBorder(BORDER_SETTING_WHITE);
    driver.clear(EINK_ORIGINAL_WAVEFORM_CLEAR);

    uint8_t buffer[256 * 256 / 8];
    // Set all pixels to white
    memset(buffer, 0xFF, 256 * 256 / 8);

    // Set every seventh pixel to black
    for (int i = 0; i < 256 * 256; i += 7) {
        // Adafruit stores bits starting with MSB and going right
        // EG pixel 0,0 is the MSB of the first byte
        buffer[i / 8] &= ~(0x80 >> (i % 8));
    }

    driver.setBorder(BORDER_SETTING_WHITE);
    driver.update(EINK_ORIGINAL_WAVEFORM_FULL, buffer);

    UNITY_END();
    return 0;
}