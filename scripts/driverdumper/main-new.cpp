#include <cstring>

#include "Displays/EinkET013TT1.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "SPI.h"

int main() {
    EinkET013TT1 driver_(0, 1, 2, 3);
    driver_.init();
    driver_.setBorder(BORDER_SETTING_WHITE);
    driver_.clear(EINK_ORIGINAL_WAVEFORM_CLEAR);

    // set every seventh pixel to black
    uint8_t buffer[256 * 256 / 8];
    memset(buffer, 0xFF, 256 * 256 / 8);

    for (int i = 0; i < 256 * 256; i += 7) {
        // Adafruit stores bits starting with MSB and going right
        // EG pixel 0,0 is the MSB of the first byte
        buffer[i / 8] &= ~(0x80 >> (i % 8));
    }

    // Frame{bb : WHITE, bw : BLACK, wb : NOCHG, ww : BLACK},
    // NEW
    // FF 3F FC FF F3 FF CF FF,3F,FC,FF,F3,FF,CF,FF,3F,FC,FF,F3,FF,CF,FF,3F,FC,FF,F3,FF,CF,FF,3F,FC
    // 1111 1111 0011 1111 1111 1100 1111 1111 1111 0011 1111 1111 1100 1111
    //

    // Gate 256
    // F3,FF,CF,FF,3F,FC,FF,F3,FF,CF,FF
    // WRONG!

    printf("SETTING NEW FRAME WITH FULL REFRESH\n");
    // printf("BUFFER %02X,%02X,%02X,%02X,", buffer[0], buffer[1], buffer[2], buffer[3]);
    // 7E,FD,FB,F7
    // 0111 1110 1111 1101 1111 1011 1111 0111

    driver_.setBorder(BORDER_SETTING_WHITE);
    driver_.update(EINK_ORIGINAL_WAVEFORM_FULL, buffer);

    for (int i = 0; i < 256 * 256; i += 5) {
        // Adafruit stores bits starting with MSB and going right
        // EG pixel 0,0 is the MSB of the first byte
        buffer[i / 8] &= ~(0x80 >> (i % 8));
    }
    driver_.setBorder(BORDER_SETTING_WHITE);
    driver_.update(EINK_ORIGINAL_WAVEFORM_FULL, buffer);
}
