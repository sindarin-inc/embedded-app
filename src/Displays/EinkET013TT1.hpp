#pragma once

#include <array>
#include <cstdint>

#define EinkET013TT1_PIXEL_WIDTH 256
#define EinkET013TT1_PIXEL_HEIGHT 256
#define EinkET013TT1_GATE_COUNT 512
#define MAX_FRAMES_PER_WAVEFORM 31

// The possible per-column TFT FET source settings.
typedef enum {
    // NOCHG sets no voltage (possibly VCOM) (As per datasheet, same as 0b01)
    NOCHG = 0b00,
    // WHITE (0b10) sets the source voltage to VDL (-14V) and attracts positive white particles
    WHITE = 0b10,
    // BLACK (0b11) sets the source voltage to VDH (+14V) and attracts negative black particles
    BLACK = 0b11,
} PixelSetting;

// Represents the possible pixel source settings for all 4 possible pixel transitions
typedef struct {
    PixelSetting bb : 2; // PixelSetting for pixels that stay black (0 -> 0)
    PixelSetting bw : 2; // PixelSetting for pixels changing from black to white (0 -> 1)
    PixelSetting wb : 2; // PixelSetting for pixels changing from white to black (1 -> 0)
    PixelSetting ww : 2; // PixelSetting for pixels that stay white (1 -> 1)
} Frame;

typedef struct {
    std::string name;
    uint8_t frameCount;
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM> frames;
} EinkET013TT1Waveform;

using BorderSetting = enum {
    BLACK_BORDER,
    WHITE_BORDER,
    NONE_BORDER,
};

using Register = enum {
    PUMP_STATUS = 0x00,
    CHANNEL_SETTING = 0x01,
    // Not included in the datasheet
    MYSTERY_SETTING_1 = 0x03,
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

#define OSC_250KHZ (0 << 5)
#define OSC_500KHZ (1 << 5)
#define OSC_1MHZ (2 << 5)
#define OSC_2MHZ (3 << 5)
#define OSC_SELECT OSC_2MHZ

class EinkET013TT1 {
public:
    // Constructor. Nothing interesting happens here until init()
    EinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy)
        : cs_(cs), oei_(oei), rst_(rst), busy_(busy){};
    ~EinkET013TT1() = default;

    void init();

    // Vcom value is the drain voltage for the TFT FET.
    // It is written on back of each individual display
    void setVcom(int mV);

    void setFramerate(int frameRate);

    // Configure border. This is set on every scan line.
    void setBorder(BorderSetting border);

    void clear(const EinkET013TT1Waveform &waveform);

    void update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer);

protected:
    void powerOn();
    void powerOff();
    void writeRegister(Register index, uint8_t data);
    void writeRegister(Register index, const uint8_t *data, size_t len);
    void startTimer(uint32_t ns);
    auto timerRunning() -> bool;
    void waitForTimer();
    void waitForNotBusy();

    uint8_t cs_;
    uint8_t oei_;
    uint8_t rst_;
    uint8_t busy_;
    uint8_t vcom_;

    uint8_t previousBuffer_[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8];

    const uint8_t *borderBits_ = nullptr;

    float timerDelayNs_ = 0;
    uint32_t timerDurationCycles_ = 0;
    volatile uint32_t cycleCountStart_ = 0;
};
