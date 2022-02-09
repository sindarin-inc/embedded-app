#include "DisplaySystem.hpp"

#include <UI/Fonts/LucidaGrande5pt7b.h>
#include <utility>

#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

DisplaySystem::DisplaySystem(int16_t w, int16_t h) : GFXcanvas1(w, h) {
    autoUpdate_ = true;

    setFont(&LUCIDA_GRANDE5PT7B);
    setTextColor(fg, bg);

#ifdef DISPLAY_ROTATION
    setRotation(DISPLAY_ROTATION);
#else
    setRotation(0);
#endif
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
}

void DisplaySystem::flipInversion() {
    invertedDisplay_ = !invertedDisplay_;
    invertDisplay(invertedDisplay_);
}

void DisplaySystem::setPartialRefresh() { partialRefresh_ = 1; }

void DisplaySystem::unsetPartialRefresh() { partialRefresh_ = 0; }

auto DisplaySystem::getPartialRefresh() -> int { return partialRefresh_; }

void DisplaySystem::rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h) {
    switch (getRotation()) {
    case 1:
        std::swap(x, y);
        std::swap(w, h);
        x = WIDTH - x - w;
        break;
    case 2:
        x = WIDTH - x - w;
        y = HEIGHT - y - h;
        break;
    case 3:
        std::swap(x, y);
        std::swap(w, h);
        y = HEIGHT - y - h;
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
    partialW_ = WIDTH;
    partialH_ = HEIGHT;
}

void DisplaySystem::setAutoUpdate(bool update) { autoUpdate_ = update; }

auto DisplaySystem::getAutoUpdate() -> bool { return autoUpdate_; }

auto DisplaySystem::getWidthInMm() -> float { return widthInMm_; }

auto DisplaySystem::getHeightInMm() -> float { return heightInMm_; }

void DisplaySystem::setFocusSize(float width, float height) {
    focusWidthInMm_ = width;
    focusHeightInMm_ = height;
    computeMinMax();
}

void DisplaySystem::setFocusOffset(float width, float height) {
    focusWidthOffsetInMm_ = width;
    focusHeightOffsetInMm_ = height;
    // Any time the Offset is set, we need to recompute the MinMax
    computeMinMax();
}

auto DisplaySystem::getFont() -> const GFXfont * { return gfxFont; }

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
        // if (Serial) Serial.print(string_to_draw.charAt(i));
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
    //
    // The second part of this || is to handle when there's just a single line with
    // a single character on it with no \n.
    if ((startPos != endPos) || stringToSplit.at(endPos) != '\n') {
        lines[numLines - 1] = stringToSplit.substr(startPos, endPos - startPos + 1);
    } else {
        numLines--;
    }
}

void DisplaySystem::drawTextCmdsOnly(const std::string &newText, int x, int y, int lineHeight) {
    // We want to draw the characters down from the x/y coordinates given
    // so the renderer doesn't need to concern itself with how tall the
    // font is, etc.
    // u8g2Fonts->setFontPosTop();

    std::array<std::string, MAX_LINES_IN_SPLIT> lines;
    int numLines;
    splitStringToLines(newText, lines, numLines);

    // We want to have only 1 pixel length difference between the rows of
    // text to squeeze as much in there as we can
    // ascent = u8g2Fonts->getAscent();
    // descent = u8g2Fonts->getDescent();

    setTextColor(fg);

    for (int i = 0; i < numLines; i++) {
        // Don't know if it starts the line from the baseline or from the top -- TO LEARN: HACK
        setCursor(x, y + i * (lineHeight));
        printf("%s\n", lines[i].c_str());
    }
}

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.
void DisplaySystem::drawText(const std::string &newText, int x, int y, int width, int height) {
    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    int lineHeight = getTextHeight();
    int numLines = height / lineHeight;

    if (getPartialRefresh()) {
        setPartialWindow(x, y - 10, this->getWidthInMm(), lineHeight * numLines + 10);
    }

    drawTextCmdsOnly(newText, x, y, lineHeight);
    if (autoUpdate_) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplaySystem::drawTextInBox(const std::string &newText, int startX, int startY, int endX,
                                  int endY) {

    int lineHeight = 0;

    log_d("Printing some stuff to screen!");

    if (!getPartialRefresh() && autoUpdate_) {
        fillScreen(bg);
    }

    lineHeight = getTextHeight();

    if (getPartialRefresh()) {
        setPartialWindow(startX - 2, startY - 2, (endX - startX) + 4, (endY - startY) + 4);
    }

    drawBoxCmdsOnly(startX, startY, endX, endY);
    drawTextCmdsOnly(newText, startX + 3, startY + lineHeight + 1, lineHeight);
    if (autoUpdate_) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplaySystem::getTextBounds(const char *buffer, uint16_t *w, uint16_t *h) {
    int16_t x, y;
    Adafruit_GFX::getTextBounds(buffer, 0, 0, &x, &y, w, h);
}

auto DisplaySystem::getTextHeight() -> int { return gfxFont->yAdvance + 1; }

auto DisplaySystem::getTextHeight(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    Adafruit_GFX::getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return h;
}

auto DisplaySystem::getTextWidth(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    Adafruit_GFX::getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return w;
}

void DisplaySystem::renderTextWithoutBox(const std::string &text) {
    drawText(text, xMin + localXOffset, yMin + localYOffset + getTextHeight(), xMax + localXOffset,
             yMax + localYOffset);
}

void DisplaySystem::renderMaxBoxWithText(const std::string &text) {
    drawTextInBox(text, xMin + localXOffset, yMin + localYOffset, xMax + localXOffset,
                  yMax + localYOffset);
}

void DisplaySystem::renderBox(int xStart, int yStart, int xEnd, int yEnd) {
    int startX = xMin + localXOffset;
    int startY = yMin + localYOffset;
    drawBox(startX + xStart, startY + yStart, startX + xEnd, startY + yEnd);
}

void DisplaySystem::drawBoxCmdsOnly(int startX, int startY, int endX, int endY) {
    drawRect(startX, startY, endX - startX, endY - startY, fg);
}

void DisplaySystem::drawBox(int startX, int startY, int endX, int endY) {
    // clear out one pixel around all of it
    if (getPartialRefresh()) {
        log_d("Draw partial box: %d %d | %d %d", startX, startY, endX, endY);
        setPartialWindow(startX - 2, startY - 2, (endX - startX) + 4, (endY - startY) + 4);
    }

    drawBoxCmdsOnly(startX, startY, endX, endY);
    if (autoUpdate_) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplaySystem::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                                uint16_t color) {
    if (getPartialRefresh()) {
        setPartialWindow(x, y, w, h);
    }
    Adafruit_GFX::drawXBitmap(x, y, bitmap, w, h, color);
    log_d("Bitmap drawn, updating display!");
    if (autoUpdate_) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplaySystem::clearScreen() {
    fillScreen(bg);
    updateDisplay();
}

void DisplaySystem::computeMinMax() {
    // TODO(john): How valuable is it to pre-compute all this

    int midpointX;
    int midpointY;
    // pixels per mm
    float pixelPitchX;
    float pixelPitchY;

    log_d("Computing Min and Max pixels for Renderer");

    midpointX = this->width() / 2;
    midpointY = this->height() / 2;

    log_d("midpoint = (%d, %d)", midpointX, midpointY);

    pixelPitchX = ((float)this->width()) / this->getWidthInMm();
    pixelPitchY = ((float)this->height()) / this->getHeightInMm();

    log_d("pixel pitch = (%.0f, %.0f)", pixelPitchX, pixelPitchY);

    // rounding down is ok here and what we want.
    maxPixelsX = (int)((this->getFocusWidth() + this->getFocusOffsetWidth()) * pixelPitchX);
    maxPixelsY = (int)((this->getFocusHeight() + this->getFocusOffsetHeight()) * pixelPitchY);

    log_d("max pixels = (%d, %d)", maxPixelsX, maxPixelsY);

    xMin = midpointX - maxPixelsX / 2;
    xMax = midpointX + maxPixelsX / 2;
    yMin = midpointY - maxPixelsY / 2;
    yMax = midpointY + maxPixelsY / 2;

    log_d("min = (%d, %d) max = (%d, %d)", xMin, yMin, xMax, yMax);
}
