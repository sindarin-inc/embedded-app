#include "EinkET013TT1.hpp"

#include <Arduino.h>
#include <SPI.h>
#include <algorithm>

extern "C" {
#include <esp32/clk.h>
}

static const uint8_t CLEAR_PACKET[48] = {
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint16_t GATE_SELECT_TABLE_L[] = {
    0x00C0, 0x0030, 0x000C, 0x0003, 0xC000, 0x3000, 0x0C00, 0x0300,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t GATE_SELECT_TABLE_H[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x00C0, 0x0030, 0x000C, 0x0003, 0xC000, 0x3000, 0x0C00, 0x0300,
};

static const uint32_t GATE_BLOCK_TABLE_L[] = {
    0x03000000, 0x0C000000, 0x30000000, 0xC0000000, 0x00030000, 0x000C0000, 0x00300000, 0x00C00000,
    0x00000300, 0x00000C00, 0x00003000, 0x0000C000, 0x00000003, 0x0000000C, 0x00000030, 0x000000C0,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static const uint32_t GATE_BLOCK_TABLE_H[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x000000C0, 0x00000030, 0x0000000C, 0x00000003, 0x0000C000, 0x00003000, 0x00000C00, 0x00000300,
    0x00C00000, 0x00300000, 0x000C0000, 0x00030000, 0xC0000000, 0x30000000, 0x0C000000, 0x03000000,
};

static const uint8_t CHANNEL_SETTINGS[] = {0x1E, 0x00, 0xE0};

static uint8_t const BORDER_BLACK[4] = {0x00, 0x03, 0x00, 0x0C};

static uint8_t const BORDER_WHITE[4] = {0x00, 0x02, 0x00, 0x0C};

static uint8_t const BORDER_NONE[4] = {0x00, 0x00, 0x00, 0x03};

static SPISettings spiSettings(8000000, SPI_MSBFIRST, SPI_MODE0);

void EinkET013TT1::init() {
    log_d("Initializing EinkET013TT1");
    pinMode(cs_, OUTPUT);
    pinMode(oei_, OUTPUT);
    pinMode(rst_, OUTPUT);
    pinMode(busy_, INPUT);

    digitalWrite(cs_, HIGH);

    digitalWrite(rst_, LOW);
    // Not sure if this is necessary but it's in the orginal EPD driver
    for (int i = 0; i < 100000; ++i) {
        ;
    }
    digitalWrite(oei_, LOW);
    for (int i = 0; i < 100000; ++i) {
        ;
    }
    digitalWrite(rst_, HIGH);

    SPI.begin();

    if (vcom_ == 0) {
        setVcom(-2810);
    }
    if (timerDelayNs_ == 0) {
        setFramerate(24);
    }
    if (borderBits_ == nullptr) {
        setBorder(NONE_BORDER);
    }

    for (unsigned char &i : previousBuffer_) {
        i = 0xFF;
    }
}

void EinkET013TT1::setVcom(int mV) {
    mV = std::clamp(mV, -5600, 0);
    // 0 for -5600mV, 255 for 0V
    vcom_ = ((5600 + mV) * 255) / 5600;
}

void EinkET013TT1::setFramerate(int frameRate) {
    frameRate = std::clamp(frameRate, 0, 100);
    timerDelayNs_ = 1000000000 / frameRate / EinkET013TT1_GATE_COUNT;
}

void EinkET013TT1::setBorder(BorderSetting border) {
    switch (border) {
    case BLACK_BORDER:
        borderBits_ = BORDER_BLACK;
        break;
    case WHITE_BORDER:
        borderBits_ = BORDER_WHITE;
        break;
    case NONE_BORDER:
        borderBits_ = BORDER_NONE;
        break;
    }
}

void EinkET013TT1::clear(const EinkET013TT1Waveform &waveform) {
    update(waveform, previousBuffer_);
}

void EinkET013TT1::update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer) {
    // Set up convenience variables to index into various parts of the scanline
    // TODO: Are there clever union types or other struct packing things we could do here?
    uint8_t scanLine[48];
    uint8_t *borderSettings = &(scanLine[0]);
    memcpy(borderSettings, borderBits_, 4);
    auto *gateBlockLow = (uint32_t *)&(scanLine[4]);
    auto *gateSelectLow = (uint16_t *)&(scanLine[8]);
    uint8_t *sourceData = &(scanLine[10]);
    auto *gateSelectHigh = (uint16_t *)&(scanLine[42]);
    auto *gateBlockHigh = (uint32_t *)&(scanLine[44]);

    powerOn();
    log_d("Updating dispay with waveform \"%s\"", waveform.name.c_str());

    // For each frame in the waveform
    for (int frameIndex = 0; frameIndex < waveform.frameCount; frameIndex++) {
        auto frame = waveform.frames[frameIndex];

        // For each horizontal half-line
        for (int gate = 0; gate < EinkET013TT1_GATE_COUNT; gate++) {
            // Pick the correct gate based on the index
            uint16_t gs = gate & 0x0F;
            uint16_t gb = gate >> 4;
            *gateBlockLow = GATE_BLOCK_TABLE_L[gb];
            *gateSelectLow = GATE_SELECT_TABLE_L[gs];
            *gateSelectHigh = GATE_SELECT_TABLE_H[gs];
            *gateBlockHigh = GATE_BLOCK_TABLE_H[gb];

            // Gate indices represent half-lines in this order:
            // top right -> bottom right -> top left -> bottom left
            int bufferIndex;
            if (gate < EinkET013TT1_GATE_COUNT / 2) {
                // The first half of the gates are on the right side of the screen, offset by
                // width / 2
                bufferIndex = (EinkET013TT1_PIXEL_WIDTH / 2 + EinkET013TT1_PIXEL_WIDTH * gate) / 8;
            } else {
                bufferIndex = (EinkET013TT1_PIXEL_WIDTH * (gate % EinkET013TT1_PIXEL_WIDTH)) / 8;
            }

            auto newBuffer = &(buffer[bufferIndex]);
            auto oldBuffer = &(previousBuffer_[bufferIndex]);

            // If necessary, we can go back to the full-byte LUT for this and do 4 of these
            // operations at a time. But for now, optimizing for readability / understandability.
            for (int pixel = 0; pixel < EinkET013TT1_PIXEL_WIDTH / 2; pixel++) {
                // pixel ordering is the other way around from how it's stored in adafruit
                // Flip our index
                uint8_t rPixel = EinkET013TT1_PIXEL_WIDTH / 2 - pixel;
                auto newPixel = (newBuffer[rPixel / 8] & (0x80 >> (rPixel & 7))) != 0;
                auto oldPixel = (previousBuffer_[rPixel / 8] & (0x80 >> (rPixel & 7))) != 0;

                uint8_t sourceSetting = 0;
                if (oldPixel == 0x0 && newPixel == 0x0) {
                    sourceSetting = frame.bb;
                } else if (oldPixel == 0x0 && newPixel == 0x1) {
                    sourceSetting = frame.bw;
                } else if (oldPixel == 0x1 && newPixel == 0x0) {
                    sourceSetting = frame.wb;
                } else if (oldPixel == 0x1 && newPixel == 0x1) {
                    sourceSetting = frame.ww;
                }

                // Clear the bits
                sourceData[pixel / 4] &= ~(0x03 << (2 * (3 - pixel % 4)));
                // Set the bits
                sourceData[pixel / 4] |= (sourceSetting << (2 * (3 - pixel % 4)));
            }

            // Send the scan line!
            writeRegister(WRITE_GATE_SOURCE_DATA, scanLine, sizeof(scanLine));
            // Wait for frame timer and busy signal
            waitForTimer();
            startTimer(timerDelayNs_);
            waitForNotBusy();
            // Perhaps this means latch everything into place?
            writeRegister(MYSTERY_SETTING_1, 0x10);
        }

        // Extra wait time because that's what the original EPD driver did
        // We should have already waited at the last gate, so this may not be necessary.
        waitForTimer();

        // Send clear packet
        writeRegister(WRITE_GATE_SOURCE_DATA, CLEAR_PACKET, sizeof(CLEAR_PACKET));
        waitForNotBusy();
        writeRegister(MYSTERY_SETTING_1, 0x10);
    }

    powerOff();

    // Move new buffer to be previous buffer
    memcpy(previousBuffer_, buffer, sizeof(previousBuffer_));
}

void EinkET013TT1::writeRegister(Register index, const uint8_t *data, size_t len) {
    uint8_t buf[2];
    buf[0] = 0x70; // Start byte
    buf[1] = index & 0x0F;

    SPI.beginTransaction(spiSettings);
    digitalWrite(cs_, LOW);

    SPI.writeBytes(buf, 2);
    SPI.writeBytes(data, len);

    digitalWrite(cs_, HIGH);
    SPI.endTransaction();
}

void EinkET013TT1::writeRegister(Register index, uint8_t data) { writeRegister(index, &data, 1); }

void EinkET013TT1::powerOn() {
    log_d("Powering on EinkET013TT1");
    digitalWrite(rst_, HIGH);

    // Set the system clock speed
    // It's unclear what the 0x10 does. It's not in the datasheet.
    writeRegister(OSC_POWER_SETTING, OSC_SELECT | 0x10);

    // LVDS OFF, Band-gap circuit ON, Temperature sensor ON
    writeRegister(THERMAL_SENSOR_SETTING, 0x03);

    // Configure which channels use gate voltages and which channels use source voltages.
    // I think this is a function of how the shift registers are physically wired to the
    // screen and isn't worth changing, but I'm not 100% sure on that.
    writeRegister(CHANNEL_SETTING, CHANNEL_SETTINGS, sizeof(CHANNEL_SETTINGS));

    // CPS=0b11 Charge pump strength to max
    // CPM=0b11 Charge pump soft start mode to max
    writeRegister(PUMP_SETTING_1, 0x0F);
    // Select PWM width for vcom. Specified value is divided into 256 levels between the
    // voltages of GND and VCL (-5.6V)
    writeRegister(PWM_WIDTH_SELECT, vcom_);

    // Fire up the charge pumps
    // Disable everything (probably a no-op)
    writeRegister(PUMP_SETTING_2, 0x00);
    delay(30);
    // Enable positive charge pump VH (VDH +14V & VGH +19V)
    writeRegister(PUMP_SETTING_2, 0x01);
    delay(30);
    // Also enable negative charge pump VN (VDL -14V & VGL -19V)
    writeRegister(PUMP_SETTING_2, 0x03);
    delay(30);
    // Also enable negative charge pump VCL (-5.6V)
    writeRegister(PUMP_SETTING_2, 0x07);
    delay(60);
    // Also enable (connect) VCOM
    writeRegister(PUMP_SETTING_2, 0x0F);
    delay(30);

    // This is not in the datasheet and its function is a mystery
    writeRegister(MYSTERY_SETTING_1, 0x10);
}

void EinkET013TT1::powerOff() {
    // Disable (disconnect) VCOM
    writeRegister(PUMP_SETTING_2, 0x07);
    delay(30);
    // Also disable negative charge pump VCL (-5.6V)
    writeRegister(PUMP_SETTING_2, 0x03);
    delay(30);
    // Also disable negative charge pump VN (VDL -14V & VGL -19V)
    writeRegister(PUMP_SETTING_2, 0x01);
    delay(30);
    // This is not in the datasheet and its function is a mystery
    writeRegister(MYSTERY_SETTING_2, 0x00);
    // Disable internal charge pump for oscillator power
    writeRegister(OSC_POWER_SETTING, 0x40);
    // CPS=0b10 Charge pump strength to half
    // CPM=0b00 Charge pump soft start mode to min
    writeRegister(PUMP_SETTING_1, 0x80);
    // Also disable positive charge pump VH (VDH +14V & VGH +19V)
    // All charge pumps are off at this point
    writeRegister(PUMP_SETTING_2, 0x00);
    delay(30);
    // LVDS OFF, Band-gap circuit OFF, Temperature sensor OFF
    writeRegister(THERMAL_SENSOR_SETTING, 0x00);
    delay(30);
    // This is not in the datasheet and its function is a mystery
    writeRegister(MYSTERY_SETTING_2, 0x02);

    log_d("EinkET013TT1 powered off");
}

void EinkET013TT1::startTimer(uint32_t ns) {
    RSR(CCOUNT, cycleCountStart_);
    timerDurationCycles_ = (esp_clk_cpu_freq() / 1000000000.0) * ns;
}

auto EinkET013TT1::timerRunning() -> bool {
    volatile uint32_t cycleCount = 0;
    RSR(CCOUNT, cycleCount);
    if (timerDurationCycles_ != 0 && cycleCount - cycleCountStart_ < timerDurationCycles_) {
        return true;
    }
    return false;
}

void EinkET013TT1::waitForTimer() {
    while (timerRunning()) {
    }
}

void EinkET013TT1::waitForNotBusy() {
    while (digitalRead(busy_) == HIGH) {
    }
}
