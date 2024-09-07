#include "DisplayEink047TC1.hpp"

#include <algorithm>
#include <freertos/task.h>

#include "Communications/USBSerial.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

auto DisplayEink047TC1::setup() -> bool {
    DisplaySystem::setup();

    log_d("Initializing EPD driver");
    epd_init();

    setFullWindow();

    setPartialRefresh();

    return true;
}

void DisplayEink047TC1::invertDisplay(bool newInvert) {
    invertedDisplay_ = newInvert;
    if (invertedDisplay_) {
        bg = 0x0000;
        fg = 0xFFFF;
    } else {
        bg = 0xFFFF;
        fg = 0x0000;
    }
    fillScreen(bg);
}

uint16_t inline reverseBits(uint16_t value) {
    uint16_t reversed = 0;
    for (int i = 0; i < 16; ++i) {
        if (value & (1 << i)) {
            reversed |= (1 << (15 - i));
        }
    }
    return reversed;
}

void DisplayEink047TC1::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                      int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                      ScreenSide screen, Rect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (mirrorToSerial) {
        sendBufferToSerial(gfxCanvas_.getBuffer());
    }
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }

    // Convert the buffer by reveresing the bits of each word (two byte chunks)
    uint8_t *new4bitBuffer =
        (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 8, MALLOC_CAP_SPIRAM);
    for (int i = 0; i < EPD_WIDTH * EPD_HEIGHT / 16; i++) {
        // write the 1-bit buffer to a 4-bit part of the right index into new4bitBuffer
        ((uint16_t *)new4bitBuffer)[i] =
            reverseBits(gfxCanvas_.getBuffer()[2 * i] | (gfxCanvas_.getBuffer()[2 * i + 1] << 8));
    }

    switch (updateType) {
    case UNSPECIFIED:
    case FULL:
        epd_clear_area(epd_full_screen());
        // epd_draw_grayscale_image(epd_full_screen(), new4bitBuffer);
        epd_draw_frame_1bit(epd_full_screen(), new4bitBuffer, BLACK_ON_WHITE, 4000);
        break;
    case PARTIAL:
    case FAST:
    default:
        // epd_draw_grayscale_image(epd_full_screen(), new4bitBuffer);
        epd_draw_frame_1bit(epd_full_screen(), new4bitBuffer, BLACK_ON_WHITE, 4000);
        //  log_e("Unsupported update type");
        break;
    }

    heap_caps_free(new4bitBuffer);
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

void DisplayEink047TC1::beginPowerOn() { epd_poweron(); }

void DisplayEink047TC1::clearDisplay() { epd_clear(); }

void DisplayEink047TC1::drawFullscreenBitmapFromFile(RendererStream &stream) {
    uint8_t *buffer = gfxCanvas_.getBuffer();
    int size = EPD_WIDTH * EPD_HEIGHT / 8;

    if (stream.readBytes(buffer, size) != size) {
        log_e("failed to read pbm bytes");
        return;
    }

    for (int i = 0; i < size; i++) {
        buffer[i] = buffer[i] ^ 0xFF; // XOR to invert all bits
    }
}

void DisplayEink047TC1::sendBufferToSerial(const uint8_t *buffer, bool screenshot) const {
    constexpr size_t byteSize = EPD_WIDTH * EPD_HEIGHT / 8;

    std::string prefix = screenshot ? "SCREENSHOT " : "FRAME ";
    prefix += std::to_string(byteSize);

    USBSerial::getInstance().txSendLine(prefix);
    USBSerial::getInstance().txSendData(buffer, byteSize);
    USBSerial::getInstance().txSendLine("");
}

void DisplayEink047TC1::sendCurrentScreenToSerial(bool screenshot) const {
    sendBufferToSerial(gfxCanvas_.getBuffer(), screenshot);
}
