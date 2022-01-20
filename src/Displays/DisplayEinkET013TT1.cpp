#include "DisplayEinkET013TT1.hpp"

#include <EPD_TCON.h>
#include <UI/Fonts/LucidaGrande5pt7b.h>

// These are defined by the lib/swtcon which we don't clang-tidy
// NOLINTNEXTLINE(readability-identifier-naming)
extern const EPD_TCON_DRIVER_HAL g_TCON_HAL;
// NOLINTNEXTLINE(readability-identifier-naming)
extern void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
// NOLINTNEXTLINE(readability-identifier-naming)
extern const unsigned char g_WaveformData[];

// Set up the U8g2lib display
DisplayEinkET013TT1::DisplayEinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy)
    : DisplaySystem(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS) {
    EPD_HAL_Init(cs, oei, rst, busy);

    autoUpdate_ = true;

    int32_t ret = EPD_TCON_Init(&g_TCON_HAL);

    if (ret < 0) {
        log_d("Bad return code from TCON Init: %d", ret);
        while (true)
            ;
    }

    Serial.println("Loading waveform...");

    ret = EPD_TCON_LoadWaveform(g_WaveformData);
    Serial.println("Waveform loaded?");

    if (!ret) {
        log_d("Bad return code from waveform load: %d", ret);
        while (true)
            ;
    }

    Serial.println("SetVcom");
    // Written on back of each individual display - value is in mV so -2.81 on back should be -2810
    // here. -cojo
    EPD_TCON_SetVcom(-2810);

    EPD_TCON_SetFrameRate(EPD_TCON_FRAME_RATE_HZ);
    Serial.println("Powering on...");
    EPD_TCON_PowerOn();
    Serial.println("Powered on! Clearing panel...");
    EPD_TCON_PanelClear();
    Serial.println("Panel cleared! Powering off...");
    EPD_TCON_PowerOff();

    setFullWindow();

#ifdef DISPLAY_ROTATION
    setRotation(DISPLAY_ROTATION);
#else
    setRotation(0);
#endif
    setFont(&LUCIDA_GRANDE5PT7B);
    setTextColor(fg, bg);
    log_d("Set up display (%ld)", (long)this);

    setX(EinkET013TT1_X_PIXELS);
    setY(EinkET013TT1_Y_PIXELS);
    setWidth(EinkET013TT1_WIDTH_IN_MM);
    setHeight(EinkET013TT1_HEIGHT_IN_MM);
}

// BC: Changed to never do partial_referesh for now.
// Change to = 1 when we want to try partial refresh again.
void DisplayEinkET013TT1::setPartialRefresh() { partialRefresh_ = 0; }

void DisplayEinkET013TT1::setAutoUpdate(bool update) { autoUpdate_ = update; }

auto DisplayEinkET013TT1::getAutoUpdate() -> bool { return autoUpdate_; }

void DisplayEinkET013TT1::getTextBounds(const char *buffer, uint16_t *w, uint16_t *h) {
    int16_t x, y;
    Adafruit_GFX::getTextBounds(buffer, 0, 0, &x, &y, w, h);
}

auto DisplayEinkET013TT1::getTextHeight() -> int { return gfxFont->yAdvance + 1; }

auto DisplayEinkET013TT1::getTextHeight(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    Adafruit_GFX::getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return h;
}

auto DisplayEinkET013TT1::getTextWidth(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    Adafruit_GFX::getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return w;
}

void DisplayEinkET013TT1::drawTextInBox(String newText, int startX, int startY, int endX,
                                        int endY) {

    int lineHeight = 0;

    log_d("Printing some stuff to screen! (%ld)", (long)this);

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

void DisplayEinkET013TT1::drawBoxCmdsOnly(int startX, int startY, int endX, int endY) {

    if (!getPartialRefresh() && autoUpdate_) {
        fillScreen(bg);
    }
    drawRect(startX, startY, endX - startX, endY - startY, fg);
}

void DisplayEinkET013TT1::drawBox(int startX, int startY, int endX, int endY) {
    // clear out one pixel around all of it
    if (getPartialRefresh()) {
        log_d("Draw box: %d %d | %d %d", startX, startY, endX, endY);
        setPartialWindow(startX - 2, startY - 2, (endX - startX) + 4, (endY - startY) + 4);
    }
    log_d("Got here - drawing a box: %d %d | %d %d", startX, startY, endX, endY);

    drawBoxCmdsOnly(startX, startY, endX, endY);
    if (autoUpdate_) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplayEinkET013TT1::drawText() { drawText(stringToDraw_, xToDraw_, yToDraw_); }

void DisplayEinkET013TT1::drawText(String newText) { drawText(newText, xToDraw_, yToDraw_); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayEinkET013TT1::drawText(String newText, int x, int y) {
    DEBUG_LN("Printing to screen!");

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    int lineHeight = getTextHeight();

    if (getPartialRefresh()) {
        setPartialWindow(x, y - 10, EinkET013TT1_X_PIXELS, lineHeight * numLines_ + 10);
    }

    drawTextCmdsOnly(newText, x, y, lineHeight);
    if (autoUpdate_) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplayEinkET013TT1::drawTextCmdsOnly(const String &newText, int x, int y, int lineHeight) {

    stringToDraw_ = newText;

    // We want to draw the characters down from the x/y coordinates given
    // so the renderer doesn't need to concern itself with how tall the
    // font is, etc.
    // u8g2Fonts->setFontPosTop();

    splitStringToLines();

    // We want to have only 1 pixel length difference between the rows of
    // text to squeeze as much in there as we can
    // ascent = u8g2Fonts->getAscent();
    // descent = u8g2Fonts->getDescent();

    if (!getPartialRefresh() && autoUpdate_) {
        fillScreen(bg);
    }

    for (int i = 0; i < numLines_; i++) {
        // Don't know if it starts the line from the baseline or from the top -- TO LEARN: HACK
        setCursor(x, y + i * (lineHeight));
        println(lines_[i] + "\n");
        // DEBUG_LN(lines[i]);
        // DEBUG_VAR(x);
        // DEBUG_LN(" is the x direction");
        // DEBUG_VAR(y+i*(line_height+1));
        // DEBUG_LN(" is the y direction");
    }
}

/* This is BC's code to try one additional calibration screen
   attempt that uses Adafruit GFX rendering instead of a bmp to draw the
   screen. It takes in the bounding box (start x & y, end x & y) and
   calculates where everything needs to go. It currently isn't called
   from anywhere. */
void DisplayEinkET013TT1::drawCalibrationScreen(int startX, int startY, int endX, int endY) {
    const int edgeOffset = 5;
    int midpointX = ((endX - startX) / 2) + startX;
    int midpointY = ((endY - startY) / 2) + startY;
    int boxWidth = endX - startX;
    int boxHeight = endY - startY;

    fillRect(startX, startY, boxWidth, boxHeight, fg);
    fillRect(startX + 2, startY + 2, boxWidth - 4, boxHeight - 4, bg);
    setFont(&LUCIDA_GRANDE5PT7B);

    int letsWidth = getTextWidth("let's");
    int focusWidth = getTextWidth("focus");
    int letsHeight = getTextHeight();
    int focusHeight = getTextHeight();

    for (int i = 30; i > 23; i--) {
        drawCircle(midpointX, midpointY, i, fg);
    }

    setCursor(midpointX - letsWidth / 2, midpointY - 3);
    println("let's");
    setCursor(midpointX - focusWidth / 2, midpointY + focusHeight - 6);
    println("focus");

    setCursor(endX - getTextWidth("lower right") - edgeOffset, endY - edgeOffset - 2);
    println("lower right");
    setCursor(startX + edgeOffset, endY - edgeOffset - 2); // lower left
    println("lower left");
    setCursor(startX + edgeOffset, startY + getTextHeight() - 2); // upper left
    println("upper left");
    setCursor(endX - getTextWidth("upper right") - edgeOffset, startY + getTextHeight() - 2);
    println("upper right");
}

void DisplayEinkET013TT1::setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    setPartialRefresh();
    partialX_ = min(x, width());
    partialY_ = min(y, height());
    partialW_ = min(w, width() - partialX_);
    partialH_ = min(h, height() - partialY_);
    rotate(partialX_, partialY_, partialW_, partialH_);
    // force partialX, partialW multiple of 8?
    partialW_ += partialX_ % 8;
    if (partialW_ % 8 > 0) partialW_ += 8 - partialW_ % 8;
    partialX_ -= partialX_ % 8;
}

void DisplayEinkET013TT1::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) {
        return;
    }
    // Move according to rotation as needed
    switch (getRotation()) {
    case 1:
        swap(x, y);
        x = WIDTH - x - 1;
        break;
    case 2:
        x = WIDTH - x - 1;
        y = HEIGHT - y - 1;
        break;
    case 3:
        swap(x, y);
        y = HEIGHT - y - 1;
        break;
    }
    if (getPartialRefresh()) {
        // Transpose partial window to 0,0
        x -= partialX_;
        y -= partialY_;
        // Enforce partial window size
        if ((x < 0) || (x >= partialW_) || (y < 0) || (y >= partialH_)) {
            log_d("drawPixel out of partial window bounds; short-circuiting call: %d %d | %d %d", x,
                  y, partialW_, partialH_);
            return;
        }
    }
    uint16_t i = x / 8 + y * (partialW_ / 8);
    buffer_[i] =
        (color == 0x0000) ? (buffer_[i] & ~(1 << (7 - x % 8))) : (buffer_[i] | (1 << (7 - x % 8)));
}

void DisplayEinkET013TT1::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w,
                                      int16_t h, uint16_t color) {
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

void DisplayEinkET013TT1::updateDisplay() {
    if (getPartialRefresh()) {
        writePartialImage(partialX_, partialY_, buffer_, partialW_, partialH_);
        EPD_TCON_PowerOn();
        EPD_TCON_Update(WF_MODE_GU4);
        EPD_TCON_PowerOff();
    } else {
        writeImage(EPD_TCON_GetUpdateBuffer(), buffer_,
                   (EinkET013TT1_X_PIXELS * EinkET013TT1_Y_PIXELS / 8));
        EPD_TCON_Set_Border(bg == 0x00 ? EPD_BORDER_BLACK : EPD_BORDER_WHITE);
        EPD_TCON_PowerOn();
        EPD_TCON_Update(WF_MODE_GC4);
        EPD_TCON_PowerOff();
    }
    // Clear buffer afterwards
    fillScreen(bg);
}

void DisplayEinkET013TT1::writeImage(uint8_t *fb, const unsigned char *img, uint16_t size) {
    int i;
    for (i = 0; i < size; i++) {
        fb[i * 2] &= 0xf0;
        fb[i * 2] |= img[i] >> 4;
        fb[(i * 2) + 1] &= 0xf0;
        fb[(i * 2) + 1] |= img[i] & 0x0f;
    }
}

auto DisplayEinkET013TT1::writePartialImage(uint16_t x, uint16_t y, const unsigned char *img,
                                            uint16_t imgW, uint16_t imgH) -> uint8_t {
    uint8_t *fb;
    uint32_t i;

    if (((x + imgW) > EinkET013TT1_X_PIXELS) || ((y + imgH) > EinkET013TT1_Y_PIXELS))
        return 0; // over range

    for (i = 0; i < imgH; ++i) {
        fb = EPD_TCON_GetUpdateBuffer();

        fb += (y + i) * (EinkET013TT1_X_PIXELS / 4);
        fb += (x >> 2);

        writeImage(fb, img + (i * (imgW / 8)), (imgW / 8));
    }
    return 1;
}