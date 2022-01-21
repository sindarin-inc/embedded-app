#include "DisplayEinkET013TT1.hpp"

#include <EPD_TCON.h>
#include <UI/Fonts/LucidaGrande5pt7b.h>
#include <algorithm>

#define EinkET013TT1_WIDTH_IN_MM 23.3
#define EinkET013TT1_HEIGHT_IN_MM 23.3
#define EPD_TCON_FRAME_RATE_HZ 24

// These are defined by the lib/swtcon which we don't clang-tidy
// NOLINTNEXTLINE(readability-identifier-naming)
extern const EPD_TCON_DRIVER_HAL g_TCON_HAL;
// NOLINTNEXTLINE(readability-identifier-naming)
extern void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
// NOLINTNEXTLINE(readability-identifier-naming)
extern const unsigned char g_WaveformData[];

DisplayEinkET013TT1::DisplayEinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy)
    : DisplaySystem(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS) {
    EPD_HAL_Init(cs, oei, rst, busy);

    auto ret = EPD_TCON_Init(&g_TCON_HAL);
    if (ret < 0) {
        log_e("Bad return code from TCON Init: %d", ret);
        esp_deep_sleep_start();
    }

    log_d("Loading waveform...");
    ret = EPD_TCON_LoadWaveform(g_WaveformData);
    log_d("Waveform loaded?");
    if (!ret) {
        log_e("Bad return code from waveform load: %d", ret);
        esp_deep_sleep_start();
    }

    // Written on back of each individual display - value is in mV so -2.81 on back should be -2810
    // here. -cojo
    log_d("SetVcom");
    EPD_TCON_SetVcom(-2810);

    EPD_TCON_SetFrameRate(EPD_TCON_FRAME_RATE_HZ);
    log_d("Powering on...");
    EPD_TCON_PowerOn();
    log_d("Powered on! Clearing panel...");
    EPD_TCON_PanelClear();
    log_d("Panel cleared! Powering off...");
    EPD_TCON_PowerOff();

    setFullWindow();

    // TODO: Move these out of the e-ink class? They're not particularly driver specific
    setFont(&LUCIDA_GRANDE5PT7B);
    setTextColor(fg, bg);

    widthInMm_ = EinkET013TT1_WIDTH_IN_MM;
    heightInMm_ = EinkET013TT1_HEIGHT_IN_MM;

    log_i("Display ready! (%ld)", (long)this);
}

void DisplayEinkET013TT1::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) {
        return;
    }
    // Move according to rotation as needed
    switch (getRotation()) {
    case 1:
        std::swap(x, y);
        x = WIDTH - x - 1;
        break;
    case 2:
        x = WIDTH - x - 1;
        y = HEIGHT - y - 1;
        break;
    case 3:
        std::swap(x, y);
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

    if (((x + imgW) > EinkET013TT1_X_PIXELS) || ((y + imgH) > EinkET013TT1_Y_PIXELS)) {
        return 0; // over range
    }

    for (i = 0; i < imgH; ++i) {
        fb = EPD_TCON_GetUpdateBuffer();

        fb += (y + i) * (EinkET013TT1_X_PIXELS / 4);
        fb += (x >> 2);

        writeImage(fb, img + (i * (imgW / 8)), (imgW / 8));
    }
    return 1;
}
