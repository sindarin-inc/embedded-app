#if EINKET013TT1 && SIMULATOR

#include "FakeEinkET013TT1.hpp"

#include <algorithm>

#include "sindarin-debug.h"

uint8_t ParticlePositions[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT];
extern void SimScreenUpdateDone();
extern bool SDLNeedsRedraw;

int EinkET013TT1::particleMovement = PARTICLE_MOVEMENT_DEFAULT;
bool EinkET013TT1::skipDelay = false;

void EinkET013TT1::init() { log_d("Initialized EinkET013TT1"); }

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
                auto position = ParticlePositions[y * EinkET013TT1_PIXEL_WIDTH + x];
                // TODO: We could probably get math-y here and simulate the movement of the
                // particles with physics. For now we're just estimating.
                switch (setting) {
                case WHITE:
                    position = std::min(position + particleMovement, 255);
                    break;
                case BLACK:
                    position = std::max(position - particleMovement, 0);
                    break;
                case NOCHG:
                    break;
                }
                ParticlePositions[y * EinkET013TT1_PIXEL_WIDTH + x] = position;
            }
        }
        SDLNeedsRedraw = true;

        // This doesn't account for processing time above, but probably real CPUs are fast
        // enough it doesn't matter
        if (!skipDelay) {
            delay(1000 / framerate_);
        }
    }

    // Move new buffer to be previous buffer
    memcpy(previousBuffer_, buffer, sizeof(previousBuffer_));

    SimScreenUpdateDone();
}

auto EinkET013TT1::readTemperature(ScreenSide screen) -> int16_t { return 24; }

#endif
