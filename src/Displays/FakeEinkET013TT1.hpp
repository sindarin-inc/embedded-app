#pragma once
#if SIMULATOR

#include <array>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string>

#include "CustomWaveforms.hpp"
#include "EinkTypes.hpp"

#define EinkET013TT1_PIXEL_WIDTH 256
#define EinkET013TT1_PIXEL_HEIGHT 256
#define FRAMERATE_DEFAULT 24
#define PARTICLE_MOVEMENT_DEFAULT 40

extern uint8_t
    FakeEinkET013TT1ParticlePositions[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT];
extern bool FakeEinkET013TT1SkipWaveform;
extern bool FakeEinkET013TT1SkipSignalling;
extern QueueHandle_t FakeEinkET013TT1RenderQueue;
extern QueueHandle_t FakeEinkET013TT1DoneWaveformQueue;

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

    void update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer,
                int32_t overrideVcom = INT32_MAX,
                const EinkET013TT1Waveform &altWaveform = BC_WAVEFORM_NONE,
                ScreenSide screen = ScreenSide::Both, EinkBox box = EinkBox::zero());

    auto readTemperature(ScreenSide screen) -> int16_t;

    auto errored() -> bool { return false; }

    void beginPowerOn(){};

protected:
    uint8_t previousBuffer_[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8];
    int framerate_ = FRAMERATE_DEFAULT;

    auto getPixel(const uint8_t *buffer, int x, int y) -> uint8_t;
};

#endif
