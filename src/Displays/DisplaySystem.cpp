#include "DisplaySystem.hpp"

#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/PBMImage.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Fonts.hpp"
#include "sindarin-debug.h"

DisplaySystem::DisplaySystem() = default;

TimerHandle_t DisplaySystem::animationTimer = nullptr;

extern void UIInnerLoop(RemoteCommand command);

// Resonsible for handling notifiying the renderer when the renderer needs to do another line
void DisplaySystem::animationTimerCallback(TimerHandle_t timer) {
    DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::ANIMATION); });
}

auto DisplaySystem::setup() -> bool {
    gfxClass_->setTextColor(fg, bg);

#ifdef DISPLAY_ROTATION
    setRotation(DISPLAY_ROTATION);
#else
    gfxClass_->setRotation(0);
#endif

    gfxClass_->setTextWrap(false);

    uint32_t initialAnimationDuration = 2000;
    log_d("Creating timer task to wake up in %.1f seconds, but not starting timer",
          initialAnimationDuration / 1000.0);

    DisplaySystem::animationTimer =
        xTimerCreate("AnimationTimer", pdMS_TO_TICKS(initialAnimationDuration), pdFALSE, nullptr,
                     DisplaySystem::animationTimerCallback);

    if (DisplaySystem::animationTimer == nullptr) {
        log_e("Failed to create timer");
    }

    return true;
}

void DisplaySystem::invertDisplay(bool newInvert) {
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

void DisplaySystem::flipInversion() { invertDisplay(!invertedDisplay_); }

void DisplaySystem::setPartialRefresh() { partialRefresh_ = 1; }

void DisplaySystem::unsetPartialRefresh() { partialRefresh_ = 0; }

auto DisplaySystem::getPartialRefresh() const -> int { return partialRefresh_; }

void DisplaySystem::rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h) {

    switch (gfxClass_->getRotation()) {
    case 1:
        std::swap(x, y);
        std::swap(w, h);
        x = gfxClass_->width() - x - w;
        break;
    case 2:
        x = gfxClass_->width() - x - w;
        y = gfxClass_->height() - y - h;
        break;
    case 3:
        std::swap(x, y);
        std::swap(w, h);
        y = gfxClass_->height() - y - h;
        break;
    }
}

void DisplaySystem::setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    setPartialRefresh();
    partialX_ = std::min(x, static_cast<uint16_t>(width()));
    partialY_ = std::min(y, static_cast<uint16_t>(height()));
    partialW_ = std::min(w, static_cast<uint16_t>(width() - partialX_));
    partialH_ = std::min(h, static_cast<uint16_t>(height() - partialY_));
    rotate(partialX_, partialY_, partialW_, partialH_);
    // force partialX, partialW multiple of 8?
    partialW_ += partialX_ % 8;
    if (partialW_ % 8 > 0) {
        partialW_ += 8 - partialW_ % 8;
    }
    partialX_ -= partialX_ % 8;
}

void DisplaySystem::setFullWindow() {
    unsetPartialRefresh();
    partialX_ = 0;
    partialY_ = 0;
    partialW_ = gfxClass_->width();
    partialH_ = gfxClass_->height();
}

// Eventually, we could add an optional argument at the end to wrap the text
// and return whether the text was wrapped or not.
// But that's for another day.
//
// Returns the x position at the end of string, without the bounding box offset.
auto DisplaySystem::drawSingleLineOfText(const std::string &line, int x, int y, bool inverted,
                                         Font *font) -> int {
    int newXPos = x;
    if (font != nullptr) {
        gfxClass_->startWrite();
        font_defs::Bitmap canvas{};
        canvas.pixels = getBuffer();
        canvas.dim = font_defs::Dim(nativeWidth(), nativeHeight());
        newXPos = font->drawSingleLineOfText(canvas, Pos(xOffset(x), yOffset(y)), line,
                                             inverted ? !invertedDisplay_ : invertedDisplay_);
        newXPos -= displayInsets().left;
        gfxClass_->endWrite();
    }

    return newXPos;
}

void DisplaySystem::drawSingleLineOfCenteredText(const std::string &line, bool inverted,
                                                 Font *font) {
    this->drawSingleLineOfText(line, (width() - getTextWidth(line)) / 2,
                               (height() - getTextHeight(line)) / 2, inverted, font);
}

void DisplaySystem::getTextSize(const std::string &buffer, uint16_t *w, uint16_t *h, Font *font) {
    if (font != nullptr) {
        Dim dim = font->getTextSize(buffer);
        *w = dim.width;
        *h = dim.height;
    } else {
        *w = 0;
        *h = 0;
    }
}

auto DisplaySystem::getTextHeight(const std::string &buffer, Font *font) -> int {
    return (font != nullptr) ? font->getTextHeight(buffer) : 0;
}

auto DisplaySystem::getTextWidth(const std::string &buffer, Font *font) -> int {
    return (font != nullptr) ? font->getTextWidth(buffer) : 0;
}

void DisplaySystem::drawBox(int startX, int startY, int endX, int endY) {
    gfxClass_->drawRect(xOffset(startX), yOffset(startY), endX - startX, endY - startY, fg);
}

void DisplaySystem::drawDottedBox(int startX, int startY, int endX, int endY, int dotSize,
                                  int gapSize) {
    if (((endX - startX) < (dotSize + gapSize)) || ((endY - startY) < (dotSize + gapSize)) ||
        (dotSize < 1) || (gapSize < 1) || (endX < startX) || (endY < startY)) {
        log_e("Error in inputs to drawDottedBox: startX %d startY %d endX %d endY %d dotSize %d "
              "gapSize %d",
              startX, startY, endX, endY, dotSize, gapSize);
        return;
    }
    // Draw a rect with dotted lines that are dotSize pixels long and gapSize pixels in between the
    // dots Do not fill in the rect

    // Draw the top line
    for (int i = startX; i < endX; i += dotSize + gapSize) {
        gfxClass_->drawLine(xOffset(i), yOffset(startY), xOffset(i + dotSize - 1), yOffset(startY),
                            fg);
    }

    // Draw the bottom line
    for (int i = startX; i < endX; i += dotSize + gapSize) {
        gfxClass_->drawLine(xOffset(i), yOffset(endY), xOffset(i + dotSize - 1), yOffset(endY), fg);
    }

    // Draw the left line
    for (int i = startY + 2; i < endY - 1; i += dotSize + gapSize) {
        gfxClass_->drawLine(xOffset(startX), yOffset(i), xOffset(startX), yOffset(i + dotSize - 1),
                            fg);
    }

    // Draw the right line
    for (int i = startY + 2; i < endY - 1; i += dotSize + gapSize) {
        gfxClass_->drawLine(xOffset(endX), yOffset(i), xOffset(endX), yOffset(i + dotSize - 1), fg);
    }
}

void DisplaySystem::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                                uint16_t color) {
    if (getPartialRefresh()) {
        setPartialWindow(x, y, w, h);
    }
    gfxClass_->drawXBitmap(xOffset(x), yOffset(y), bitmap, w, h, color);
}

void DisplaySystem::drawScaledXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w,
                                      int16_t h, uint16_t color, int scale) {
    if (getPartialRefresh()) {
        setPartialWindow(x, y, w, h);
    }
    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t bit = 0;

    gfxClass_->startWrite();
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (i & 7) {
                bit >>= 1;
            } else {
                bit = bitmap[j * byteWidth + i / 8];
            }
            if (bit & 0x01) {
                // Instead of drawing a pixel, draw a scaled rectangle
                gfxClass_->writeFillRect(xOffset(x + i * scale), yOffset(y + j * scale), scale,
                                         scale, color);
            }
        }
    }
    gfxClass_->endWrite();
}

void DisplaySystem::drawPBM(int16_t x, int16_t y, const uint8_t *pbmbytes, int size) {
    BufferStream stream(pbmbytes, size);
    drawPBM(x, y, stream);
}

void DisplaySystem::drawPBM(int16_t x, int16_t y, const std::string &fname, FileSystem &fs) {
    if (!fs.exists(fname)) {
        log_e("cannot display pbm, file not present: %s", fname.c_str());
        return;
    }
    SFile pbmFile = fs.open(fname);
    ContainerStream<SFile> stream(pbmFile);
    drawPBM(x, y, stream);
}

void DisplaySystem::drawPBM(int16_t x, int16_t y, RendererStream &stream) {
    int width;
    int height;
    if (!ParsePbmHeader(stream, width, height)) {
        log_e("failed to parse PBM header");
        return;
    }

    log_i("parsed PBM header, w %d height %d", width, height);
    int byteSize = width * height / 8;

    // if this is a full-screen image, load directly into memory
    // otherwise load separately and draw with drawBitmap

    // check for fullscreen
    if (x == 0 && y == 0 && width == gfxClass_->width() && height == gfxClass_->height()) {
        drawFullscreenBitmapFromFile(stream);
    } else {
        // not fullscreen
        // load bitmap into memory
        auto bitmap = SpiRamArrayPtr<uint8_t>(byteSize);
        if (stream.readBytes(bitmap.get(), byteSize) != byteSize) {
            log_e("failed to read bitmap");
            return;
        }

        gfxClass_->drawBitmap(x, y, bitmap.get(), width, height, 0, 1);
    }
}

void DisplaySystem::clearScreen() {
    fillScreen(bg);
    updateDisplay();
}
