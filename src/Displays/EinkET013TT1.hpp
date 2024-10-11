#pragma once

#include <array>
#include <cstdint>
#include <driver/spi_master.h>
#include <string>
#if !CONFIG_DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

#include "CustomWaveforms.hpp"
#include "EinkTypes.hpp"
#include "Misc/Dispatch.hpp"

#define EinkET013TT1_PIXEL_WIDTH 256
#define EinkET013TT1_PIXEL_HEIGHT 256
#define EinkET013TT1_GATE_COUNT 512

typedef union {
    uint8_t value;
    struct {
        uint8_t unused : 6;
        // Charge pump ready (1=ready)
        uint8_t vcpok : 1;
        // Low voltage detection status: (0=normal voltage)
        uint8_t lvds : 1;
    } bits;
} PumpStatus;

enum Register {
    PUMP_STATUS = 0x00,
    CHANNEL_SETTING = 0x01,
    DRIVER_LATCH_RESET_PULSE_GENERATE = 0x03,
    PUMP_SETTING_1 = 0x04,
    PUMP_SETTING_2 = 0x05,
    OSC_POWER_SETTING = 0x07,
    THERMAL_SENSOR_SETTING = 0x08,
    PWM_WIDTH_SELECT = 0x09,
    WRITE_GATE_SOURCE_DATA = 0x0A,
    // Not included in the datasheet
    MYSTERY_SETTING_2 = 0x0B,
    READ_SENSOR_REGISTER = 0x0F,
};

enum OSC_FREQ {
    OSC_250KHZ = (0 << 5),
    OSC_500KHZ = (1 << 5),
    OSC_1MHZ = (2 << 5),
    OSC_2MHZ = (3 << 5)
};

#define OSC_SELECT OSC_2MHZ
#define VCOM_DEFAULT (-4000)
#define FRAMERATE_DEFAULT 24

// Driver for dual Eink 1.3" 256x256 displays
class EinkET013TT1 {
public:
    // Constructor. Nothing interesting happens here until init()
    EinkET013TT1(uint8_t csLeft, uint8_t csRight, uint8_t rst)
        : csLeft_(csLeft), csRight_(csRight), rst_(rst){};
    ~EinkET013TT1() = default;

    void init();

    // Vcom value is the drain voltage for the TFT FET.
    // It is written on back of each individual display
    void setVcom(int mV);
    void setVcomNudge(int mV) { vcomNudge_ = mV; }

    // For the settings menu to display current value.
    [[nodiscard]] auto getVcom() const -> int { return vcomInMV_ + vcomNudge_; }
    [[nodiscard]] auto getVcomNudge() const -> int { return vcomNudge_; }

    void setFramerate(int frameRate);

    // Configure border. This is set on every scan line.
    void setBorder(BorderSetting border);

    void clear(const EinkET013TT1Waveform &waveform);

    void update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer,
                int32_t overrideVcom = INT32_MAX,
                const EinkET013TT1Waveform &altWaveform = BC_WAVEFORM_NONE,
                ScreenSide screen = ScreenSide::Both, EinkBox box = EinkBox::zero());

    auto readTemperature(ScreenSide screen) -> int16_t;

    [[nodiscard]] auto errored() const -> bool { return error_; }

    // When we know we will need to render UI but haven't calculated the UI yet, we can begin the
    // power on sequence in parallel to drawing the UI.
    void beginPowerOn();

    [[nodiscard]] auto getBuffer() const -> const uint8_t * { return previousBuffer_; }

    // Permanently shut down the display for this execution.
    void shutdown();

protected:
    void initSpi();
    inline void setCS(bool value, ScreenSide screen) const;
    void powerOnInternal();
    void powerOffInternal();
    inline void writeRegister(Register index, uint8_t data, ScreenSide screen,
                              bool outOfQueue = false);
    void writeRegister(Register index, const uint8_t *data, size_t len, ScreenSide screen,
                       bool outOfQueue = false);
    auto readRegister(uint8_t index, ScreenSide screen) -> uint8_t;
    auto readPumpStatus(ScreenSide screen) -> PumpStatus;
    // According to the data sheet the device ID is 0 by default, but that's the same
    // when the screen is not connected. So I don't think `readDeviceIdReg` is useful.
    auto readDeviceIdReg(ScreenSide screen) -> uint8_t;
    inline void startTimer(uint32_t ns);
    [[nodiscard]] inline auto timerActivated() const -> bool;
    [[nodiscard]] inline auto timerRunning() const -> bool;
    inline void waitForTimer();
    inline void waitForNotBusy();
    void updateInternal(const EinkET013TT1Waveform &waveform, const uint8_t *buffer,
                        int32_t overrideVcom, const EinkET013TT1Waveform &altWaveform,
                        ScreenSide screen, EinkBox box);

    uint8_t csLeft_;
    uint8_t csRight_;
    uint8_t rst_;
    // Keep track of both the vcom in the MV value (that is on the
    // sticker on the back of the display).
    // The 8-bit register value is calculated when it is set in the
    // register: ((5600 + vcomInMV_) * 255) / 5600.
    uint32_t vcomInMV_{};
    uint32_t vcomNudge_ = 0;

    uint16_t timerOverruns_{};
    uint16_t busyWaits_{};
    uint32_t maxFrameCycles_{};
    bool error_{};

    BorderSetting previousBorder_ = BORDER_SETTING_NONE;
    BorderSetting border_ = BORDER_SETTING_NONE;

    uint8_t previousBuffer_[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8]{};

    uint32_t updateStartTime_ = 0;
    float timerDelayNs_ = 0;
    uint32_t timerDurationCycles_ = 0;
    volatile uint32_t cycleCountStart_ = 0;

    spi_device_handle_t spiHigh_{};
    spi_device_handle_t spiLow_{};

    bool poweredOn_ = false;
    DispatchQueue *einkQueue_{};

#if !CONFIG_DISABLE_POWER_MANAGEMENT
    esp_pm_lock_handle_t freqLock_{};
#endif
};
