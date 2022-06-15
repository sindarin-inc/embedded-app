#include "DisplaySystem.hpp"

#include <utility>

#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

DisplaySystem::DisplaySystem() { autoUpdate_ = true; }

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

void DisplaySystem::flipInversion() {
    invertedDisplay_ = !invertedDisplay_;
    invertDisplay(invertedDisplay_);
}

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

void DisplaySystem::setAutoUpdate(bool update) { autoUpdate_ = update; }

auto DisplaySystem::getAutoUpdate() -> bool { return autoUpdate_; }

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

auto DisplaySystem::getFont() -> const GFXfont * { return font_; }

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
    //
    // The second part of this || is to handle when there's just a single line with
    // a single character on it with no \n.
    if (stringToSplit.length() > 0 && ((startPos != endPos) || stringToSplit.at(endPos) != '\n')) {
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

    gfxClass_->setTextColor(fg);

    for (int i = 0; i < numLines; i++) {
        // Don't know if it starts the line from the baseline or from the top -- TO LEARN: HACK
        setCursor(x, y + i * (lineHeight));
        gfxClass_->printf("%s\n", lines[i].c_str());
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
        setPartialWindow(x, y - 10, boundingBox_.width, lineHeight * numLines + 10);
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
    gfxClass_->getTextBounds(buffer, 0, 0, &x, &y, w, h);
}

auto DisplaySystem::getTextHeight() -> int { return font_->yAdvance + lineSpacing_; }

auto DisplaySystem::getTextHeight(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    gfxClass_->getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return h;
}

auto DisplaySystem::getTextWidth(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    gfxClass_->getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return w;
}

void DisplaySystem::renderTextWithoutBox(const std::string &text, bool centerHorizontally,
                                         bool centerVertically) {

    int xOffset = 0;
    int yOffset = 0;

    if (centerHorizontally) {
        xOffset = (width() - getTextWidth(text.c_str())) / 2;
    }

    if (centerVertically) {
        yOffset = (height() - getTextHeight(text.c_str())) / 2;
    }

    drawText(text, 0 + xOffset, getTextHeight() + yOffset, width(), height());
}

void DisplaySystem::renderMaxBoxWithText(const std::string &text) {
    drawTextInBox(text, 0, 0, width(), height());
}

void DisplaySystem::renderBox(int xStart, int yStart, int xEnd, int yEnd) {
    drawBox(xStart, yStart, xEnd, yEnd);
}

void DisplaySystem::drawBoxCmdsOnly(int startX, int startY, int endX, int endY) {
    gfxClass_->drawRect(xOffset(startX), yOffset(startY), endX - startX, endY - startY, fg);
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
    gfxClass_->drawXBitmap(xOffset(x), yOffset(y), bitmap, w, h, color);
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