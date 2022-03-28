#pragma once

#include <array>
#include <cstdint>
#include <string>

#define EinkET013TT1_PIXEL_WIDTH 256
#define EinkET013TT1_PIXEL_HEIGHT 256
#define EinkET013TT1_GATE_COUNT 512
#define MAX_FRAMES_PER_WAVEFORM 31

enum WaveformInternalCheckErrors {
    WAVEFORM_OK = 0,
    WAVEFORM_BB_FAIL = 1 << 0,
    WAVEFORM_BWWB_FAIL = 1 << 1,
    WAVEFORM_WW_FAIL = 1 << 2,
};

// The possible per-column TFT FET source settings.
enum PixelSetting {
    // NOCHG sets no voltage (possibly VCOM) (As per datasheet, same as 0b01)
    NOCHG = 0b00,
    // WHITE (0b10) sets the source voltage to VDL (-14V) and attracts positive white particles
    WHITE = 0b10,
    // BLACK (0b11) sets the source voltage to VDH (+14V) and attracts negative black particles
    BLACK = 0b11,
};

typedef union {
    uint8_t value;
    struct {
        uint8_t c4 : 1;
        uint8_t c3 : 1;
        uint8_t c2 : 1;
        uint8_t c1 : 1;
        uint8_t p4 : 1;
        uint8_t p3 : 1;
        uint8_t p2 : 1;
        uint8_t p1 : 1;

    } pixels;
} LUTPixels;

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

// Represents the possible pixel source settings for all 4 possible pixel transitions
struct Frame {
    PixelSetting bb : 2; // PixelSetting for pixels that stay black (0 -> 0)
    PixelSetting bw : 2; // PixelSetting for pixels changing from black to white (0 -> 1)
    PixelSetting wb : 2; // PixelSetting for pixels changing from white to black (1 -> 0)
    PixelSetting ww : 2; // PixelSetting for pixels that stay white (1 -> 1)

    inline auto getSetting(uint8_t oldPixel, uint8_t newPixel) -> uint8_t {
        if (oldPixel == 0x0 && newPixel == 0x0) {
            return bb;
        } else if (oldPixel == 0x0 && newPixel == 0x1) {
            return bw;
        } else if (oldPixel == 0x1 && newPixel == 0x0) {
            return wb;
        } else if (oldPixel == 0x1 && newPixel == 0x1) {
            return ww;
        }
        assert(0);
        return 0; // Should never get here
    }

    // Populates a look up table that takes a 4 pixel byte where the most significant 4
    // bits are the old pixels and the least significant 4 bits are the new pixels
    inline void makeLUT(std::array<uint8_t, 256> *lut) {
        LUTPixels pixel;
        for (int i = 0; i < 256; ++i) {
            pixel.value = i;
            uint8_t outValue = getSetting(pixel.pixels.p1, pixel.pixels.c1) << 6;
            outValue |= getSetting(pixel.pixels.p2, pixel.pixels.c2) << 4;
            outValue |= getSetting(pixel.pixels.p3, pixel.pixels.c3) << 2;
            outValue |= getSetting(pixel.pixels.p4, pixel.pixels.c4);
            (*lut)[i] = outValue;
        }
    }

    auto pixelSettingString(PixelSetting setting) -> const char * {
        switch (setting) {
        case NOCHG:
            return "NOCHG";
        case BLACK:
            return "BLACK";
        case WHITE:
            return "WHITE";
        }
        return "???";
    }

    // Create a string from this frame (for debugging purposes)
    void toString(std::string *string) {
        string->append("bb: ");
        string->append(pixelSettingString(bb));
        string->append(" bw: ");
        string->append(pixelSettingString(bw));
        string->append(" wb: ");
        string->append(pixelSettingString(wb));
        string->append(" ww: ");
        string->append(pixelSettingString(ww));
    }
};

struct EinkET013TT1Waveform {
    const char *name;
    uint8_t frameCount;
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM> frames;
};

enum BorderSetting {
    BORDER_SETTING_BLACK = 0,
    BORDER_SETTING_WHITE = 1,
    BORDER_SETTING_NONE = 2,
};

enum Register {
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
#define VCOM_DEFAULT (-4000)
#define FRAMERATE_DEFAULT 24

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

    // For the settings menu to display current value.
    auto getVcom() -> int { return (vcom_ * 5600) / 255 - 5600; }

    void setFramerate(int frameRate);

    // Configure border. This is set on every scan line.
    void setBorder(BorderSetting border);

    void clear(const EinkET013TT1Waveform &waveform);

    void update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer);

    auto checkWaveform(const EinkET013TT1Waveform &waveform) -> uint8_t;

    auto readTemperature() -> int16_t;

    auto errored() -> bool { return error_; }

protected:
    void powerOn();
    void powerOff();
    inline void writeRegister(Register index, uint8_t data);
    void writeRegister(Register index, const uint8_t *data, size_t len);
    auto readRegister(uint8_t index) -> uint8_t;
    auto readPumpStatus() -> PumpStatus;
    // According to the data sheet the device ID is 0 by default, but that's the same
    // when the screen is not connected. So I don't think `readDeviceIdReg` is useful.
    auto readDeviceIdReg() -> uint8_t;
    inline void startTimer(uint32_t ns);
    inline auto timerActivated() -> bool;
    inline auto timerRunning() -> bool;
    inline void waitForTimer();
    inline void waitForNotBusy();

    uint8_t cs_;
    uint8_t oei_;
    uint8_t rst_;
    uint8_t busy_;
    uint8_t vcom_;

    uint16_t timerOverruns_;
    uint16_t busyWaits_;
    uint32_t maxFrameCycles_;
    bool error_;

    BorderSetting previousBorder_ = BORDER_SETTING_NONE;
    BorderSetting border_ = BORDER_SETTING_NONE;

    uint8_t previousBuffer_[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8];

    float timerDelayNs_ = 0;
    uint32_t timerDurationCycles_ = 0;
    volatile uint32_t cycleCountStart_ = 0;
};
