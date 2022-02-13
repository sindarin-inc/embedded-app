#if EINKET013TT1

#include "EinkET013TT1.hpp"

#include <SPI.h>
#include <algorithm>

#include "sindarin-debug.h"

extern "C" {
#include <esp32/clk.h>
}

static const DRAM_ATTR uint8_t CLEAR_PACKET[48] = {
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// These should go in RAM since they're used per-gate and the gate timing is sensitive
static const DRAM_ATTR uint16_t GATE_SELECT_TABLE_L[] = {
    0x00C0, 0x0030, 0x000C, 0x0003, 0xC000, 0x3000, 0x0C00, 0x0300,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

inline IRAM_ATTR auto GateSelectL(uint8_t i) -> uint16_t {
    if (i >= 8) {
        return 0;
    } else {
        return ((uint16_t)0xC0 >> ((i % 4) * 2)) << ((i / 4) * 8);
    }
}

static const DRAM_ATTR uint16_t GATE_SELECT_TABLE_H[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x00C0, 0x0030, 0x000C, 0x0003, 0xC000, 0x3000, 0x0C00, 0x0300,
};

inline IRAM_ATTR auto GateSelectH(uint8_t i) -> uint16_t {
    if (i < 8) {
        return 0;
    } else {
        i -= 8;
        return ((uint16_t)0xC0 >> ((i % 4) * 2)) << ((i / 4) * 8);
    }
}

static const DRAM_ATTR uint32_t GATE_BLOCK_TABLE_L[] = {
    0x03000000, 0x0C000000, 0x30000000, 0xC0000000, 0x00030000, 0x000C0000, 0x00300000, 0x00C00000,
    0x00000300, 0x00000C00, 0x00003000, 0x0000C000, 0x00000003, 0x0000000C, 0x00000030, 0x000000C0,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

auto IRAM_ATTR GateBlockL(uint8_t i) -> uint32_t {
    if (i >= 16) {
        return 0;
    } else {
        return ((uint32_t)0x03000000 << ((i % 4) * 2)) >> ((i / 4) * 8);
    }
}

static const DRAM_ATTR uint32_t GATE_BLOCK_TABLE_H[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x000000C0, 0x00000030, 0x0000000C, 0x00000003, 0x0000C000, 0x00003000, 0x00000C00, 0x00000300,
    0x00C00000, 0x00300000, 0x000C0000, 0x00030000, 0xC0000000, 0x30000000, 0x0C000000, 0x03000000,
};

auto IRAM_ATTR GateBlockH(uint8_t i) -> uint32_t {
    if (i < 16) {
        return 0;
    } else {
        i -= 16;
        return ((uint32_t)0xC0 >> ((i % 4) * 2)) << ((i / 4) * 8);
    }
}

static const uint8_t CHANNEL_SETTINGS[] = {0x1E, 0x00, 0xE0};

// Second byte will be replaced based on the frame transition
static uint8_t const BORDER_TEMPLATE[4] = {0x00, 0x00, 0x00, 0x0C};

static uint8_t const BORDER_NONE[4] = {0x00, 0x00, 0x00, 0x03};

// According to the EinkET013TT1 datasheet, the min clock time is 50ns which equates to 20MHz
// though experimentally 30MHz seems to be fine (but doesnt seem to have much benefit)
// at 20Mhz it should take 20us to send a 50 byte scanline (50*8*(1/20000000))
static SPISettings spiSettingsHigh(20000000, SPI_MSBFIRST, SPI_MODE0);
static SPISettings spiSettingsLow(8000000, SPI_MSBFIRST, SPI_MODE0);

void EinkET013TT1::init() {
    log_d("Initializing EinkET013TT1");
    pinMode(cs_, OUTPUT);
    pinMode(oei_, OUTPUT);
    pinMode(rst_, OUTPUT);
    pinMode(busy_, INPUT);

    digitalWrite(cs_, HIGH);

    digitalWrite(rst_, LOW);
    delay(5);
    digitalWrite(oei_, LOW);
    delay(5);
    digitalWrite(rst_, HIGH);
    delay(5);

    SPI.begin();

    if (vcom_ == 0) {
        setVcom(VCOM_DEFAULT);
    }
    if (timerDelayNs_ == 0) {
        setFramerate(FRAMERATE_DEFAULT);
    }
    for (unsigned char &i : previousBuffer_) {
        i = 0xFF;
    }

    log_d("Initialized EinkET013TT1 ID:%d", readDeviceIdReg());
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
    // In the future it will probably make sense to not overwrite previousBorder_ and just
    // use whatever was already set there from the last `update`. But we're doing this for
    // now to maintain compatibility with the behavior of the original EPD driver.
    switch (border) {
    case BORDER_SETTING_BLACK:
        previousBorder_ = BORDER_SETTING_WHITE;
        break;
    case BORDER_SETTING_WHITE:
        previousBorder_ = BORDER_SETTING_BLACK;
        break;
    case BORDER_SETTING_NONE:
        break;
    }
    border_ = border;
}

void EinkET013TT1::clear(const EinkET013TT1Waveform &waveform) {
    update(waveform, previousBuffer_);
}

void EinkET013TT1::update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer) {
    timerOverruns_ = 0;
    busyWaits_ = 0;
    maxFrameCycles_ = 0;

    // Set up convenience variables to index into various parts of the scanline
    // TODO: Are there clever union types or other struct packing things we could do here?
    uint8_t scanLine[48];
    uint8_t *borderSettings = &(scanLine[0]);
    if (border_ == BORDER_SETTING_NONE) {
        memcpy(borderSettings, BORDER_NONE, 4);
    } else {
        memcpy(borderSettings, BORDER_TEMPLATE, 4);
    }
    auto *gateBlockLow = (uint32_t *)&(scanLine[4]);
    auto *gateSelectLow = (uint16_t *)&(scanLine[8]);
    uint8_t *sourceData = &(scanLine[10]);
    auto *gateSelectHigh = (uint16_t *)&(scanLine[42]);
    auto *gateBlockHigh = (uint32_t *)&(scanLine[44]);

    log_d("Updating dispay with waveform \"%s\"", waveform.name.c_str());

    powerOn();

    auto pumpStatus = readPumpStatus();
    auto temp = readTemperature();
    if (!pumpStatus.bits.vcpok) {
        log_e("Pump not ready! LVDS=%d VCPOK=%d temp=%dºC", pumpStatus.bits.lvds,
              pumpStatus.bits.vcpok, temp);
    } else {
        log_d("LVDS=%d VCPOK=%d temp=%dºC", pumpStatus.bits.lvds, pumpStatus.bits.vcpok, temp);
    }

    PROFILE_START(sendFrames);

    // For each frame in the waveform
    for (int frameIndex = 0; frameIndex < waveform.frameCount; frameIndex++) {
        auto frame = waveform.frames[frameIndex];

        // Generate a look up table to quickly look up old bits -> new bits and the resulting
        // transitions
        std::array<uint8_t, 256> lut;
        // This takes ~210us at 160MHz
        frame.makeLUT(&lut);

        if (border_ != BORDER_SETTING_NONE) {
            auto borderBits = frame.getSetting(previousBorder_ & 0x01, border_);
            // In the original EPD driver, there were `Border_Black` and `Border_White` arrays
            // that only set the rightmost two bits to either 0x03 or 0x02. But in the code,
            // when calculating scan lines, all 8 bits would get set with the bit transition.
            // I'm not sure if this is a material difference. Possible the right two bits
            // actually set the border and the other 6 are noops. But doing the same thing as
            // the original driver for now.
            borderSettings[1] = borderBits | borderBits << 2 | borderBits << 4 | borderBits << 6;
        }

        portDISABLE_INTERRUPTS();

        // For each horizontal half-line
        for (int gate = 0; gate < EinkET013TT1_GATE_COUNT; gate++) {
            // Pick the correct gate based on the index
            uint8_t gs = gate & 0x0F;
            uint8_t gb = gate >> 4;
            *gateBlockLow = GateBlockL(gb);
            *gateSelectLow = GateSelectL(gs);
            *gateSelectHigh = GateSelectH(gs);
            *gateBlockHigh = GateBlockH(gb);

            // Gate indices represent half-lines in this order (with connector down):
            // bottom left -> top left -> bottom right -> top right
            // Generate an index into our frame buffer that corresponds to this gate.
            int bufferIndex = (EinkET013TT1_PIXEL_WIDTH / 8) *
                              (EinkET013TT1_PIXEL_WIDTH - 1 - (gate % EinkET013TT1_PIXEL_WIDTH));
            if (gate >= EinkET013TT1_GATE_COUNT / 2) {
                // The latter gates are on the right half of the screen, so nudge the index
                // to point to the right half of the line.
                bufferIndex += EinkET013TT1_PIXEL_WIDTH / 2 / 8;
            }

            auto newBuffer = &(buffer[bufferIndex]);
            auto oldBuffer = &(previousBuffer_[bufferIndex]);

            // This for loop takes 4-16us at 240MHz
            for (int pixel = 0; pixel < EinkET013TT1_PIXEL_WIDTH / 2; pixel += 4) {
                uint8_t oldPixels;
                uint8_t newPixels;
                if (pixel % 8 == 0) {
                    oldPixels = (oldBuffer[pixel / 8] >> 4) & 0xF;
                    newPixels = (newBuffer[pixel / 8] >> 4) & 0xF;
                } else {
                    oldPixels = oldBuffer[pixel / 8] & 0xF;
                    newPixels = newBuffer[pixel / 8] & 0xF;
                }
                sourceData[pixel / 4] = lut[oldPixels << 4 | newPixels];
            }

            // Send the scan line!
            // `writeRegister` takes 24.7us at 160MHz (5994 cycles) which is about right since
            // we're sending 50bytes at 20MHz so 50*8*(1/20000000)=20us
            writeRegister(WRITE_GATE_SOURCE_DATA, scanLine, sizeof(scanLine));
            // Wait for frame timer and busy signal
            waitForTimer();
            startTimer(timerDelayNs_);
            waitForNotBusy();
            // Perhaps this means latch everything into place?
            writeRegister(MYSTERY_SETTING_1, 0x10);
        }

        // Send clear packet
        writeRegister(WRITE_GATE_SOURCE_DATA, CLEAR_PACKET, sizeof(CLEAR_PACKET));
        waitForTimer();
        waitForNotBusy();
        writeRegister(MYSTERY_SETTING_1, 0x10);

        portENABLE_INTERRUPTS();
    }

    PROFILE_END(sendFrames);

    powerOff();

    float timeNs = PROFILE_CALCULATE_NS(sendFrames);
    int totalScanlines = (EinkET013TT1_GATE_COUNT + 1) * waveform.frameCount;
    log_d("Sent %d frames in %.2fms (%.2fms/frame, %.2fus/gate, %d/%d overruns, %d/%d busyWaits)",
          waveform.frameCount, timeNs / 1000000, timeNs / waveform.frameCount / 1000000,
          timeNs / waveform.frameCount / (EinkET013TT1_GATE_COUNT + 1) / 1000, timerOverruns_,
          totalScanlines, busyWaits_, totalScanlines);
    if (timerOverruns_ > 0) {
        log_w("%d timer overruns happened! %.2fus maxFrame", timerOverruns_,
              CYCLES_TO_US(maxFrameCycles_));
    }

    // Move new buffer to be previous buffer
    memcpy(previousBuffer_, buffer, sizeof(previousBuffer_));
    previousBorder_ = border_;
}

void EinkET013TT1::writeRegister(Register index, const uint8_t *data, size_t len) {
    uint8_t buf[2];
    buf[0] = 0x70; // Start byte
    buf[1] = index & 0x0F;

    SPI.beginTransaction(spiSettingsHigh);
    digitalWrite(cs_, LOW);

    SPI.writeBytes(buf, 2);
    SPI.writeBytes(data, len);

    digitalWrite(cs_, HIGH);
    SPI.endTransaction();
}

inline void EinkET013TT1::writeRegister(Register index, uint8_t data) {
    writeRegister(index, &data, 1);
}

auto EinkET013TT1::readRegister(uint8_t index) -> uint8_t {
    uint8_t buf[3];
    buf[0] = 0x70;
    buf[1] = index & 0x0F;

    SPI.beginTransaction(spiSettingsLow);
    digitalWrite(cs_, LOW);
    SPI.writeBytes(buf, 2);
    digitalWrite(cs_, HIGH);
    SPI.endTransaction();

    buf[0] = 0x73;
    SPI.beginTransaction(spiSettingsLow);
    digitalWrite(cs_, LOW);
    SPI.writeBytes(buf, 1);
    SPI.write(0xFF); // necessary?
    // Prefix
    SPI.transfer(0xFF);
    SPI.write(0xFF); // necessary?
    auto result = SPI.transfer(0xFF);
    digitalWrite(cs_, HIGH);
    SPI.endTransaction();

    return result;
}

auto EinkET013TT1::readPumpStatus() -> PumpStatus { return PumpStatus{readRegister(0x00)}; }

auto EinkET013TT1::readTemperature() -> int16_t {
    uint16_t value = readRegister(0x0F);

    value &= 0x7F;

    if (value >= 0x76) {
        value |= 0x80;
        return (int16_t)((int8_t)value * 2);
    } else if (value <= 0x22) {
        return value * 2;
    }
    return -1;
}

auto EinkET013TT1::readDeviceIdReg() -> uint8_t {
    SPI.beginTransaction(spiSettingsLow);
    digitalWrite(cs_, LOW);
    SPI.write(0x71);
    SPI.write(0xFF);
    auto deviceId = SPI.transfer(0xFF);
    digitalWrite(cs_, HIGH);
    SPI.endTransaction();
    return deviceId;
}

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

inline void EinkET013TT1::startTimer(uint32_t ns) {
    RSR(CCOUNT, cycleCountStart_);
    timerDurationCycles_ = (esp_clk_cpu_freq() / 1000000000.0) * ns;
}

inline auto EinkET013TT1::timerActivated() -> bool { return timerDurationCycles_ != 0; }

inline auto EinkET013TT1::timerRunning() -> bool {
    volatile uint32_t cycleCount = 0;
    RSR(CCOUNT, cycleCount);
    if (timerDurationCycles_ != 0 && cycleCount - cycleCountStart_ < timerDurationCycles_) {
        return true;
    }
    return false;
}

inline void EinkET013TT1::waitForTimer() {
    // If we ever wait for the timer, and the timer has already completed then we can conclude
    // that that we spent too much time doing CPU things and the timer ran for longer than we
    // wanted.
    if (timerActivated() && !timerRunning()) {
        volatile uint32_t cycleCount = 0;
        RSR(CCOUNT, cycleCount);
        maxFrameCycles_ = std::max(maxFrameCycles_, cycleCount - cycleCountStart_);
        timerOverruns_ += 1;
        timerDurationCycles_ = 0;
        return;
    }
    while (timerRunning()) {
    }
    timerDurationCycles_ = 0;
}

inline void EinkET013TT1::waitForNotBusy() {
    if (digitalRead(busy_) == HIGH) {
        busyWaits_ += 1;
    }
    while (digitalRead(busy_) == HIGH) {
    }
}

#endif
