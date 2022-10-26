#pragma once
#if SIMULATOR

#include <array>
#include <cstdint>
#include <string>

#include "EinkWaveform.hpp"

#define EinkET013TT1_PIXEL_WIDTH 256
#define EinkET013TT1_PIXEL_HEIGHT 256
#define FRAMERATE_DEFAULT 24
#define PARTICLE_MOVEMENT_DEFAULT 40

enum class ScreenSide {
    Left = 1 << 0,
    Right = 1 << 1,
    Both = Left | Right,
};

enum BorderSetting {
    BORDER_SETTING_BLACK = 0,
    BORDER_SETTING_WHITE = 1,
    BORDER_SETTING_NONE = 2,
};

extern uint8_t ParticlePositions[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT];

// Driver for dual Eink 1.3" 256x256 displays
class EinkET013TT1 {
public:
    // Constructor. Nothing interesting happens here until init()
    EinkET013TT1(uint8_t csLeft, uint8_t csRight, uint8_t rst){};
    ~EinkET013TT1() = default;

    void init();

    // Vcom value is the drain voltage for the TFT FET.
    // It is written on back of each individual display
    void setVcom(int mV);

    // For the settings menu to display current value.
    auto getVcom() -> int { return -4000; }

    void setFramerate(int frameRate);

    // Configure border. This is set on every scan line.
    void setBorder(BorderSetting border);

    void clear(const EinkET013TT1Waveform &waveform);

    void update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer);

    auto readTemperature(ScreenSide screen) -> int16_t;

    auto errored() -> bool { return false; }

    // How much the particle will move each frame it is charged
    static int particleMovement;
    // Whether to skip the framerate delay
    static bool skipDelay;

protected:
    uint8_t previousBuffer_[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8];
    int framerate_ = FRAMERATE_DEFAULT;

    auto getPixel(const uint8_t *buffer, int x, int y) -> uint8_t;
};

#endif
