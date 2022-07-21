#if EINKET013TT1

#include "EinkET013TT1.hpp"

#include <algorithm>
#include <esp_err.h>

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

inline auto GateSelectL(uint8_t i) -> uint16_t {
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

inline auto GateSelectH(uint8_t i) -> uint16_t {
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

inline auto GateBlockL(uint8_t i) -> uint32_t {
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

inline auto GateBlockH(uint8_t i) -> uint32_t {
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

void EinkET013TT1::init() {
    log_d("Initializing EinkET013TT1");
    pinMode(csLeft_, OUTPUT);
    pinMode(csRight_, OUTPUT);
    pinMode(oei_, OUTPUT);
    pinMode(rst_, OUTPUT);
#if !DISABLE_BUSY
    pinMode(busy_, INPUT);
#endif

    setCS(HIGH, ScreenSide::Both);

    digitalWrite(rst_, LOW);
    delay(5);
    // TODO: OEI is always low, can we reclaim it? What is it useful for?
    digitalWrite(oei_, LOW);
    delay(5);
    digitalWrite(rst_, HIGH);
    delay(5);

    // Initialize the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = VSPI_IOMUX_PIN_NUM_MOSI,
        .miso_io_num = VSPI_IOMUX_PIN_NUM_MISO,
        .sclk_io_num = VSPI_IOMUX_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    // TODO: Why don't we have any available DMA channels? Maybe they are used by Flash and RAM?
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_DISABLED));

    spi_device_interface_config_t devcfg = {
        .command_bits = 16,
        .mode = 0,
        // According to the EinkET013TT1 datasheet, the min clock time is 50ns which equates to
        // 20MHz though experimentally 30MHz seems to be fine (but doesnt seem to have much benefit)
        // at 20Mhz it should take 20us to send a 50 byte scanline (50*8*(1/20000000))
        .clock_speed_hz = 20 * 1000 * 1000, // Clock out at 20 MHz
        // TODO: The SPI driver _should_ handle the CS pin automatically, but this isn't working for
        // some reason
        // .spics_io_num = cs_,
        .queue_size = 5,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spiHigh_));

    // For reading, the EinkET013TT1 datasheet specifies 8MHz
    devcfg.clock_speed_hz = 8 * 1000 * 1000;
    devcfg.command_bits = 8;

    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spiLow_));

    if (vcom_ == 0) {
        setVcom(VCOM_DEFAULT);
    }
    if (timerDelayNs_ == 0) {
        setFramerate(FRAMERATE_DEFAULT);
    }
    for (unsigned char &i : previousBuffer_) {
        i = 0xFF;
    }

#if !DISABLE_POWER_MANAGEMENT
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "eink", &freqLock_));
#endif

    log_d("Initialized EinkET013TT1");
}

void EinkET013TT1::setVcom(int mV) {
    mV = std::clamp(mV, -5600, 0);
    // 0 for -5600mV, 255 for 0V
    vcom_ = ((5600 + mV) * 255) / 5600;
    log_d("Setting VCOM to %d mV (register 0x9 = %d)", mV, vcom_);
}

void EinkET013TT1::setFramerate(int frameRate) {
    frameRate = std::clamp(frameRate, 0, 100);
    timerDelayNs_ = 1000000000.0f / frameRate / EinkET013TT1_GATE_COUNT;
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

auto EinkET013TT1::checkWaveform(const EinkET013TT1Waveform &waveform) -> uint8_t {
    // Track the number of whites and blacks in each transition type
    // BW and WB are combined as they should be counted together for
    // the symmetry calculation
    uint32_t numBlacks[3] = {0, 0, 0};
    uint32_t numWhites[3] = {0, 0, 0};
    uint8_t returnVal = WAVEFORM_OK;

    for (int frameIndex = 0; frameIndex < waveform.frameCount; frameIndex++) {
        auto frame = waveform.frames[frameIndex];
        switch (frame.bb) {
        case BLACK:
            numBlacks[0]++;
            break;
        case WHITE:
            numWhites[0]++;
            break;
        default:
            break;
        }

        switch (frame.bw) {
        case BLACK:
            numBlacks[1]++;
            break;
        case WHITE:
            numWhites[1]++;
            break;
        default:
            break;
        }

        switch (frame.wb) {
        case BLACK:
            numBlacks[1]++;
            break;
        case WHITE:
            numWhites[1]++;
            break;
        default:
            break;
        }

        switch (frame.ww) {
        case BLACK:
            numBlacks[2]++;
            break;
        case WHITE:
            numWhites[2]++;
            break;
        default:
            break;
        }
    }

    if (numBlacks[0] != numWhites[0]) {
        returnVal |= WAVEFORM_BB_FAIL;
    }

    if (numBlacks[1] != numWhites[1]) {
        returnVal |= WAVEFORM_BWWB_FAIL;
    }

    if (numBlacks[2] != numWhites[2]) {
        returnVal |= WAVEFORM_WW_FAIL;
    }

    if (returnVal != 0) {
        log_d("Waveform Check Failed on %s, failure code %d!", waveform.name, returnVal);
    }

    return returnVal;
}

inline void EinkET013TT1::setCS(bool value, ScreenSide screen) {
    if (screen == ScreenSide::Left) {
        digitalWrite(csLeft_, value);
    } else if (screen == ScreenSide::Right) {
        digitalWrite(csRight_, value);
    } else if (screen == ScreenSide::Both) {
        digitalWrite(csLeft_, value);
        digitalWrite(csRight_, value);
    }
}

void EinkET013TT1::update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer) {
    timerOverruns_ = 0;
    busyWaits_ = 0;
    maxFrameCycles_ = 0;
    error_ = false;

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

    log_d("Updating dispay with waveform \"%s\"", waveform.name);

    powerOn();

#if !DISABLE_POWER_MANAGEMENT
    ESP_ERROR_CHECK(esp_pm_lock_acquire(freqLock_));
#endif

    // TODO: This is a very useful debug thing but doesn't actually affect how well the displays
    // refresh. Might be worth removing in release builds to slightly shorten refresh time.
    auto leftPumpStatus = readPumpStatus(ScreenSide::Left);
    auto leftTemp = readTemperature(ScreenSide::Left);
    auto rightPumpStatus = readPumpStatus(ScreenSide::Right);
    auto rightTemp = readTemperature(ScreenSide::Right);
    if (!leftPumpStatus.bits.vcpok || !rightPumpStatus.bits.vcpok) {
        log_e("Pump not ready! Left: LVDS=%d VCPOK=%d temp=%dºC Right: LVDS=%d VCPOK=%d temp=%dºC",
              leftPumpStatus.bits.lvds, leftPumpStatus.bits.vcpok, leftTemp,
              rightPumpStatus.bits.lvds, rightPumpStatus.bits.vcpok, rightTemp);
        error_ = true;
    } else {
        log_d("Left: LVDS=%d VCPOK=%d temp=%dºC Right: LVDS=%d VCPOK=%d temp=%dºC",
              leftPumpStatus.bits.lvds, leftPumpStatus.bits.vcpok, leftTemp,
              rightPumpStatus.bits.lvds, rightPumpStatus.bits.vcpok, rightTemp);
    }

    ESP_ERROR_CHECK(spi_device_acquire_bus(spiHigh_, portMAX_DELAY));

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
    }

    PROFILE_END(sendFrames);
    spi_device_release_bus(spiHigh_);

#if !DISABLE_POWER_MANAGEMENT
    esp_pm_lock_release(freqLock_);
#endif

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
    spi_transaction_t t = {
        .cmd = (uint16_t)((0x70 << 8) | (index & 0x0F)), // 0x70 is the command prefix
        .length = len * 8,
        .tx_buffer = data};

    setCS(LOW, ScreenSide::Both);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spiHigh_, &t));
    setCS(HIGH, ScreenSide::Both);
}

inline void EinkET013TT1::writeRegister(Register index, uint8_t data) {
    writeRegister(index, &data, 1);
}

auto EinkET013TT1::readRegister(uint8_t index, ScreenSide screen) -> uint8_t {
    ESP_ERROR_CHECK(spi_device_acquire_bus(spiHigh_, portMAX_DELAY));
    spi_transaction_t t = {.cmd = (uint16_t)((0x70 << 8) | (index & 0x0F))};
    // TODO: Hardcoding reading to the left one for now
    setCS(LOW, screen);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spiHigh_, &t));
    setCS(HIGH, screen);
    spi_device_release_bus(spiHigh_);

    ESP_ERROR_CHECK(spi_device_acquire_bus(spiLow_, portMAX_DELAY));
    spi_transaction_t t2 = {.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
                            .cmd = 0x73,
                            .length = 8,
                            .tx_data = {0xFF}};
    setCS(LOW, screen);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spiLow_, &t2));
    setCS(HIGH, screen);
    uint8_t result = t2.rx_data[0];
    spi_device_release_bus(spiLow_);

    return result;
}

auto EinkET013TT1::readPumpStatus(ScreenSide screen) -> PumpStatus {
    return PumpStatus{readRegister(0x00, screen)};
}

auto EinkET013TT1::readTemperature(ScreenSide screen) -> int16_t {
    uint16_t value = readRegister(0x0F, screen);

    value &= 0x7F;

    if (value >= 0x76) {
        value |= 0x80;
        return (int16_t)((int8_t)value * 2);
    } else if (value <= 0x22) {
        return value * 2;
    }
    return -1;
}

auto EinkET013TT1::readDeviceIdReg(ScreenSide screen) -> uint8_t {
    ESP_ERROR_CHECK(spi_device_acquire_bus(spiLow_, portMAX_DELAY));
    spi_transaction_t t2 = {.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
                            .cmd = 0x71,
                            .length = 8,
                            .tx_data = {0xFF}};
    setCS(LOW, screen);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spiLow_, &t2));
    setCS(HIGH, screen);

    uint8_t result = t2.rx_data[0];
    spi_device_release_bus(spiLow_);

    return result;
}

void EinkET013TT1::powerOn() {
    log_d("Powering on EinkET013TT1");
    digitalWrite(rst_, HIGH);

    ESP_ERROR_CHECK(spi_device_acquire_bus(spiHigh_, portMAX_DELAY));

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

    spi_device_release_bus(spiHigh_);
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
#if !DISABLE_BUSY
    if (digitalRead(busy_) == HIGH) {
        busyWaits_ += 1;
    }
    while (digitalRead(busy_) == HIGH) {
    }
#endif
}

#endif
