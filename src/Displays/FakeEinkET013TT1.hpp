#pragma once
#if SIMULATOR

#include <array>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <memory>
#include <string>

#include "CustomWaveforms.hpp"
#include "EinkTypes.hpp"

#ifndef SIM_WIDTH
#define EinkET013TT1_PIXEL_WIDTH 256
#else
#define EinkET013TT1_PIXEL_WIDTH SIM_WIDTH
#endif
#ifndef SIM_HEIGHT
#define EinkET013TT1_PIXEL_HEIGHT 256
#else
#define EinkET013TT1_PIXEL_HEIGHT SIM_HEIGHT
#endif
#define FRAMERATE_DEFAULT 24
#define PARTICLE_MOVEMENT_DEFAULT 40

// Driver for dual Eink 1.3" 256x256 displays
class EinkET013TT1 {
public:
    // Constructor. Nothing interesting happens here until init()
    EinkET013TT1() {
        particlePositions_ =
            std::make_unique<uint8_t[]>(EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT);
    };
    ~EinkET013TT1() {
        if (renderQueue_ != nullptr) {
            vQueueDelete(renderQueue_);
        }
    };

    void init();

    // Vcom value is the drain voltage for the TFT FET.
    // It is written on back of each individual display
    void setVcom(int mV);
    void setVcomNudge(int mV) {}

    // For the settings menu to display current value.
    auto getVcom() -> int { return -4000; }
    auto getVcomNudge() -> int { return 0; }

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

    // Simulator-specific methods below
    auto renderQueue() -> QueueHandle_t { return renderQueue_; }

    auto buffer() -> std::unique_ptr<uint8_t[]> & { return particlePositions_; }

    auto getPixel(const uint8_t *buffer, int x, int y) -> uint8_t;

    auto getSkipWaveform() -> bool { return skipWaveform_; }

    void setSkipWaveform(bool skip) { skipWaveform_ = skip; }

    auto getSkipSignalling() -> bool { return skipSignalling_; }

    void setSkipSignalling(bool skip) { skipSignalling_ = skip; }

    auto getBuffer() const -> const uint8_t * { return previousBuffer_; }

    void shutdown() {}

protected:
    // Simulated positions of the eink particles rendered as 8-bit grayscale
    std::unique_ptr<uint8_t[]> particlePositions_;
    QueueHandle_t renderQueue_ = nullptr;
    bool skipWaveform_ = false;
    bool skipSignalling_ = false;

    uint8_t previousBuffer_[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8];
    int framerate_ = FRAMERATE_DEFAULT;
};

#endif
