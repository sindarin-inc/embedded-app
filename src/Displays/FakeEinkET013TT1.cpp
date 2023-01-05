#if EINKET013TT1 && SIMULATOR

#include "FakeEinkET013TT1.hpp"

#include <algorithm>

#include "sindarin-debug.h"

uint8_t FakeEinkET013TT1ParticlePositions[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT];
// Skip waveform processing and just set things as black and white
bool FakeEinkET013TT1SkipWaveform = false;
bool FakeEinkET013TT1SkipSignalling = false;
QueueHandle_t FakeEinkET013TT1RenderQueue = nullptr;
QueueHandle_t FakeEinkET013TT1DoneWaveformQueue = nullptr;

// How much each e-ink particle will move each frame it is charged
int FakeEinkET013TT1ParticleMovement = PARTICLE_MOVEMENT_DEFAULT;

void EinkET013TT1::init() {
    if (FakeEinkET013TT1RenderQueue == nullptr) {
        FakeEinkET013TT1RenderQueue = xQueueCreate(1, sizeof(uint8_t));
    }
    xQueueReset(FakeEinkET013TT1RenderQueue);
    if (FakeEinkET013TT1DoneWaveformQueue == nullptr) {
        FakeEinkET013TT1DoneWaveformQueue = xQueueCreate(1, sizeof(uint8_t));
    }
    xQueueReset(FakeEinkET013TT1DoneWaveformQueue);
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

void EinkET013TT1::update(const EinkET013TT1Waveform &waveform, const uint8_t *buffer) {
    if (FakeEinkET013TT1SkipWaveform) {
        // Skip waveform processing and just move the pixel all the way one way or another
        log_d("Skipping waveform processing (would have used \"%s\")", waveform.name);
        for (int x = 0; x < EinkET013TT1_PIXEL_WIDTH; x++) {
            for (int y = 0; y < EinkET013TT1_PIXEL_HEIGHT; y++) {
                auto pixel = getPixel(buffer, x, y);
                FakeEinkET013TT1ParticlePositions[y * EinkET013TT1_PIXEL_WIDTH + x] = pixel * 255;
            }
        }
        // Signal that we've drawn something
        uint8_t in = 1;
        if (!FakeEinkET013TT1SkipSignalling) {
            xQueueSend(FakeEinkET013TT1RenderQueue, &in, portMAX_DELAY);
        }
    } else {
        log_d("Updating display with waveform \"%s\"", waveform.name);
        // For each frame in the waveform
        for (int frameIndex = 0; frameIndex < waveform.frameCount; frameIndex++) {
            auto frame = waveform.frames[frameIndex];

            // For each pixel, calculate the correct way to move it for the waveform
            for (int x = 0; x < EinkET013TT1_PIXEL_WIDTH; x++) {
                for (int y = 0; y < EinkET013TT1_PIXEL_HEIGHT; y++) {
                    auto pixel = getPixel(buffer, x, y);
                    auto previousPixel = getPixel(previousBuffer_, x, y);
                    auto setting = frame.getSetting(previousPixel, pixel);
                    auto position =
                        FakeEinkET013TT1ParticlePositions[y * EinkET013TT1_PIXEL_WIDTH + x];
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
                    FakeEinkET013TT1ParticlePositions[y * EinkET013TT1_PIXEL_WIDTH + x] = position;
                }
            }
            uint8_t in = 1;
            if (!FakeEinkET013TT1SkipSignalling) {
                xQueueSend(FakeEinkET013TT1RenderQueue, &in, portMAX_DELAY);
            }

            // This doesn't account for processing time above, but probably real CPUs are fast
            // enough it doesn't matter?
            delay(1000 / framerate_);
        }

        // Move new buffer to be previous buffer
        memcpy(previousBuffer_, buffer, sizeof(previousBuffer_));
    }

    uint8_t in = 1;
    if (!FakeEinkET013TT1SkipSignalling) {
        xQueueSend(FakeEinkET013TT1DoneWaveformQueue, &in, 0);
    }
}

auto EinkET013TT1::readTemperature(ScreenSide screen) -> int16_t { return 24; }

#endif
