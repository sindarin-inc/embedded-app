#include "DisplaySystem.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/PBMImage.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Fonts/Font.hpp"
#include "UI/Fonts/IBMFDriver/IBMFFont.hpp"
#include "sindarin-debug.h"

DisplaySystem::DisplaySystem() = default;

TimerHandle_t DisplaySystem::animationTimer = nullptr;

extern void UIInnerLoop(RemoteCommand command);

// Resonsible for handling notifiying the renderer when the renderer needs to do another line
void DisplaySystem::animationTimerCallback(TimerHandle_t timer) {
    DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::ANIMATION); });
}

auto DisplaySystem::setup() -> bool {
    // Default to original font
    setFont(style.getFontFamily().mainMenuFont);
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

auto DisplaySystem::getPartialRefresh() -> int { return partialRefresh_; }

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
    partialX_ = std::min(x, (uint16_t)width());
    partialY_ = std::min(y, (uint16_t)height());
    partialW_ = std::min(w, (uint16_t)(width() - partialX_));
    partialH_ = std::min(h, (uint16_t)(height() - partialY_));
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

void DisplaySystem::ensureBoundaries() {
    int gfxW = gfxClass_->width();
    int gfxH = gfxClass_->height();

    log_d("Bounding Box before is %s", boundingBox_.toString().c_str());

    if (boundingBox_.x < 0) {
        boundingBox_.x = 0;
    }

    if (boundingBox_.y < 0) {
        boundingBox_.y = 0;
    }

    // Always keep the x & y the same. Use the width
    // and height of the rectangle to stay in bounds.
    //
    // We may choose to do something more sophisticated later
    // but for now this is fine.
    if ((boundingBox_.x + boundingBox_.width) > gfxW) {
        boundingBox_.width = gfxW - boundingBox_.x;
    }

    if ((boundingBox_.y + boundingBox_.height) > gfxH) {
        boundingBox_.height = gfxH - boundingBox_.y;
    }

    log_d("Bounding Box after is %s", boundingBox_.toString().c_str());
}

auto DisplaySystem::setBoundingBox(DisplayRect newBox) -> DisplayRect {
    // Do the bounds checking
    boundingBox_ = newBox;
    ensureBoundaries();
    return boundingBox_;
}

auto DisplaySystem::getFont() -> Font * { return font_; }

// Takes the member variable string_to_draw and splits it
// based on the \n characater and populates the String* lines;
void DisplaySystem::splitStringToLines(const std::string &stringToSplit,
                                       std::array<std::string, MAX_LINES_IN_SPLIT> &lines,
                                       int &numLines) {
    int len, i, startPos, endPos;

    len = stringToSplit.length();

    // Track where we are in the string
    startPos = 0;
    endPos = 0;
    numLines = 1;

    for (i = 0; i < len; i++) {
        // We found a new line. Split off the previous line and
        // start processing the new line

        if (stringToSplit.at(i) == '\n') {
            lines[numLines - 1] = stringToSplit.substr(startPos, endPos - startPos + 1);
            numLines++;
            assert(numLines < MAX_LINES_IN_SPLIT);
            startPos = i + 1;
            endPos = i + 1;
        } else {
            endPos = i;
        }
    }

    // If \n is *not* the last character, we need to add the last line to the
    // array, but if \n is the last character, we don't need to do anything and
    // we can decrement num_lines since the above loop assumes that there will
    // be more characters after the \n.
    if (stringToSplit.length() > 0 && (startPos != endPos)) {
        lines[numLines - 1] = stringToSplit.substr(startPos, endPos - startPos + 1);
    } else {
        numLines--;
    }
}

// Eventually, we could add an optional argument at the end to wrap the text
// and return whether the text was wrapped or not.
// But that's for another day.
void DisplaySystem::drawSingleLineOfText(const std::string &line, int x, int y) {
    if (font_ != nullptr) {
        if (font_->isA(FontType::GFX)) {

            gfxClass_->setTextColor(fg);
            // We are allowing the this method to take an the upper left corner of the text
            // as an argument. But the setCursor method takes the lower left corner of text
            // to be drawn (or rather, that's how printf does it). So we need to adjust the
            // y value by the height of the font. getTextHeight includes lineSpacing_ so
            // we subtract that out.
            // The magic number (3) is a hack for now to handle the bottom of the display.
            // Need to investigate why the text is getting cut off when the y dimension is
            // all the way at the bottom. This hasn't impacted the top.
            gfxClass_->setCursor(xOffset(x), yOffset(y + getTextHeight() - lineSpacing_ - 3));
            gfxClass_->printf("%s", line.c_str());
        } else {
            gfxClass_->startWrite();
            ibmf_defs::Bitmap canvas;
            canvas.pixels = canvasBuffer_;
            canvas.dim = ibmf_defs::Dim(canvasWidth_, canvasHeight_);
            static_cast<IBMFFont *>(font_)->drawSingleLineOfText(
                canvas, Pos(xOffset(x), yOffset(y + getTextHeight() - lineSpacing_ - 3)), line,
                invertedDisplay_);
            gfxClass_->endWrite();
        }
    }
}

void DisplaySystem::drawSingleLineOfCenteredText(const std::string &line) {
    this->drawSingleLineOfText(
        line, this->xAbsolute(this->xAbsoluteCenter() - this->getTextWidth(line.c_str()) / 2),
        this->yAbsolute(this->yAbsoluteCenter() - this->getTextHeight() / 2));
}

void DisplaySystem::getTextSize(const char *buffer, uint16_t *w, uint16_t *h) {
    if (font_ != nullptr) {
        if (font_->isA(FontType::GFX)) {
            int16_t x, y;
            gfxClass_->getTextBounds(buffer, 0, 0, &x, &y, w, h);
        } else {
            Dim dim = static_cast<IBMFFont *>(font_)->getTextSize(buffer);
            *w = dim.width;
            *h = dim.height;
        }
    }
}

auto DisplaySystem::getTextHeight() -> int { return font_->lineHeight() + lineSpacing_; }

auto DisplaySystem::getTextHeight(const char *buffer) -> int {
    uint16_t h = 0;
    if (font_ != nullptr) {
        if (font_->isA(FontType::GFX)) {
            int16_t x, y;
            uint16_t w;
            gfxClass_->getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
        } else {
            h = static_cast<IBMFFont *>(font_)->getTextHeight(buffer);
        }
    }
    return h;
}

auto DisplaySystem::getTextWidth(const char *buffer) -> int {
    uint16_t w = 0;
    if (font_ != nullptr) {
        if (font_->isA(FontType::GFX)) {
            int16_t x, y;
            uint16_t h;
            gfxClass_->getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
        } else {
            w = static_cast<IBMFFont *>(font_)->getTextWidth(buffer);
        }
    }
    return w;
}

void DisplaySystem::drawBox(int startX, int startY, int endX, int endY) {
    gfxClass_->drawRect(xOffset(startX), yOffset(startY), endX - startX, endY - startY, fg);
}

void DisplaySystem::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                                uint16_t color) {
    if (getPartialRefresh()) {
        setPartialWindow(x, y, w, h);
    }
    gfxClass_->drawXBitmap(xOffset(x), yOffset(y), bitmap, w, h, color);
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