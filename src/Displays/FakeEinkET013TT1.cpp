#if CONFIG_DISPLAY_EINKET013TT1 && SIMULATOR

#include "FakeEinkET013TT1.hpp"

#include <algorithm>
#include <cstring>
#include <stdio.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

// How much each e-ink particle will move each frame it is charged
int FakeEinkET013TT1ParticleMovement = PARTICLE_MOVEMENT_DEFAULT;

void EinkET013TT1::init() {
    if (renderQueue_ == nullptr) {
        renderQueue_ = xQueueCreate(1, sizeof(uint8_t));
    }
    xQueueReset(renderQueue_);
}

void EinkET013TT1::setVcom(int mV) {}

void EinkET013TT1::setFramerate(int frameRate) { frameRate = std::clamp(frameRate, 0, 100); }

void EinkET013TT1::setBorder(BorderSetting border) {}

void EinkET013TT1::clear(const EinkET013TT1Waveform &waveform) {
    update(waveform, previousBuffer_);
}

auto EinkET013TT1::getPixel(const uint8_t *buffer, int x, int y) -> uint8_t {
    int byteIndex = (y * EinkET013TT1_PIXEL_WIDTH + x) / 8;
    int bitIndex = (y * EinkET013TT1_PIXEL_WIDTH + x) % 8;
    return (buffer[byteIndex] >> (7 - bitIndex)) & 0x01;
}

void EinkET013TT1::update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer,
                          int32_t overrideVcom, const EinkET013TT1Waveform &altWaveform,
                          ScreenSide screen, EinkBox box) {
    if (skipWaveform_) {
        // Skip waveform processing and just move the pixel all the way one way or another
        log_d("Skipping waveform processing (would have used \"%s\")", waveform.name);
        for (int x = 0; x < EinkET013TT1_PIXEL_WIDTH; x++) {
            for (int y = 0; y < EinkET013TT1_PIXEL_HEIGHT; y++) {
                auto pixel = getPixel(buffer, x, y);
                particlePositions_[y * EinkET013TT1_PIXEL_WIDTH + x] = pixel * 255;
            }
        }
        // Signal that we've drawn something
        uint8_t in = 1;
        if (!skipSignalling_) {
            xQueueSend(renderQueue_, &in, portMAX_DELAY);
        }
    } else {
        log_d("Updating display with waveform \"%s\"", waveform.name);
        // For each frame in the waveform
        log_d("Alt waveform: \"%s\"", altWaveform.name);

        int maxFrames = std::max(waveform.frameCount, altWaveform.frameCount);

        for (int frameIndex = 0; frameIndex < maxFrames; frameIndex++) {
            auto frame = waveform.frameOrDefault(frameIndex);
            auto altFrame = altWaveform.frameOrDefault(frameIndex);

            // For each pixel, calculate the correct way to move it for the waveform
            for (int x = 0; x < EinkET013TT1_PIXEL_WIDTH; x++) {
                for (int y = 0; y < EinkET013TT1_PIXEL_HEIGHT; y++) {
                    auto pixel = getPixel(buffer, x, y);
                    auto previousPixel = getPixel(previousBuffer_, x, y);
                    uint8_t setting = box.contains(x, y) ? altFrame.getSetting(previousPixel, pixel)
                                                         : frame.getSetting(previousPixel, pixel);
                    auto position = particlePositions_[y * EinkET013TT1_PIXEL_WIDTH + x];
                    // TODO: We could probably get math-y here and simulate the movement of the
                    // particles with physics. For now we're just estimating.
                    switch (setting) {
                    case WHITE:
                        position = std::min(position + FakeEinkET013TT1ParticleMovement, 255);
                        break;
                    case BLACK:
                        position = std::max(position - FakeEinkET013TT1ParticleMovement, 0);
                        break;
                    case NOCHG:
                        break;
                    }
                    particlePositions_[y * EinkET013TT1_PIXEL_WIDTH + x] = position;
                }
            }
            uint8_t in = 1;
            if (!skipSignalling_) {
                xQueueSend(renderQueue_, &in, portMAX_DELAY);
            }

            // This doesn't account for processing time above, but probably real CPUs are fast
            // enough it doesn't matter?
            Delay(1000 / framerate_);
        }

        // Move new buffer to be previous buffer
        memcpy(previousBuffer_, buffer, sizeof(previousBuffer_));
    }
}

auto EinkET013TT1::readTemperature(ScreenSide screen) -> int16_t { return 24; }

#endif
