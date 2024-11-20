#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "sindarin-debug.h"

#define MAX_FRAMES_PER_WAVEFORM 31

enum WaveformInternalCheckErrors {
    WAVEFORM_OK = 0,
    WAVEFORM_BB_FAIL = 1 << 0,
    WAVEFORM_BWWB_FAIL = 1 << 1,
    WAVEFORM_WW_FAIL = 1 << 2,
};

#if CONFIG_DISPLAY_EINKET013TT1
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

// Represents the possible pixel source settings for all 4 possible pixel transitions
struct Frame {
    PixelSetting bb : 2; // PixelSetting for pixels that stay black (0 -> 0)
    PixelSetting bw : 2; // PixelSetting for pixels changing from black to white (0 -> 1)
    PixelSetting wb : 2; // PixelSetting for pixels changing from white to black (1 -> 0)
    PixelSetting ww : 2; // PixelSetting for pixels that stay white (1 -> 1)

    [[nodiscard]] inline auto getSetting(uint8_t oldPixel, uint8_t newPixel) const -> uint8_t {
        if (oldPixel == 0x0 && newPixel == 0x0) {
            return static_cast<uint8_t>(bb);
        } else if (oldPixel == 0x0 && newPixel == 0x1) {
            return static_cast<uint8_t>(bw);
        } else if (oldPixel == 0x1 && newPixel == 0x0) {
            return static_cast<uint8_t>(wb);
        } else if (oldPixel == 0x1 && newPixel == 0x1) {
            return static_cast<uint8_t>(ww);
        }
        assert(0);
        return 0; // Should never get here
    }

    // Populates a look up table that takes a 4 pixel byte where the most significant 4
    // bits are the old pixels and the least significant 4 bits are the new pixels
    inline void makeLUT(std::array<uint8_t, 256> *lut) const {
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

    constexpr static auto nochg() -> Frame { return Frame{NOCHG, NOCHG, NOCHG, NOCHG}; }
};

struct EinkET013TT1Waveform {
    const char *name;
    uint8_t frameCount;
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM> frames;

    [[nodiscard]] auto check() const -> uint8_t {
        // Track the number of whites and blacks in each transition type
        // BW and WB are combined as they should be counted together for
        // the symmetry calculation
        uint32_t numBlacks[3] = {0, 0, 0};
        uint32_t numWhites[3] = {0, 0, 0};
        uint8_t returnVal = WAVEFORM_OK;

        for (int frameIndex = 0; frameIndex < this->frameCount; frameIndex++) {
            auto frame = this->frames[frameIndex];
            switch (static_cast<PixelSetting>(frame.bb)) {
            case BLACK:
                numBlacks[0]++;
                break;
            case WHITE:
                numWhites[0]++;
                break;
            default:
                break;
            }

            switch (static_cast<PixelSetting>(frame.bw)) {
            case BLACK:
                numBlacks[1]++;
                break;
            case WHITE:
                numWhites[1]++;
                break;
            default:
                break;
            }

            switch (static_cast<PixelSetting>(frame.wb)) {
            case BLACK:
                numBlacks[1]++;
                break;
            case WHITE:
                numWhites[1]++;
                break;
            default:
                break;
            }

            switch (static_cast<PixelSetting>(frame.ww)) {
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
            log_d("Waveform Check Failed on %s, failure code %d!", this->name, returnVal);
        }

        return returnVal;
    }

    [[nodiscard]] inline auto frameOrDefault(int index) const -> Frame {
        return index < frames.size() ? frames[index] : Frame::nochg();
    }
};

struct EinkBox {
    int leftX;
    int rightX;
    int topY;
    int bottomY;

    [[nodiscard]] inline auto contains(int x, int y) const -> bool {
        return ((x >= leftX) && (x < rightX) && (y >= topY) && (y < bottomY));
    }

    constexpr static auto zero() -> EinkBox { return {0, 0, 0, 0}; };
};

enum BorderSetting {
    BORDER_SETTING_BLACK = 0,
    BORDER_SETTING_WHITE = 1,
    BORDER_SETTING_NONE = 2,
};

#endif

enum ScreenSide {
    Left = 1 << 0,
    Right = 1 << 1,
    Both = Left | Right,
};
