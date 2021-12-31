#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include <U8g2_for_Adafruit_GFX.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256

#define EinkET013TT1_WIDTH_IN_MM 23.3
#define EinkET013TT1_HEIGHT_IN_MM 23.3

#define EPD_TCON_FRAME_RATE_HZ 24

class DisplayEinkET013TT1 : public DisplaySystem, public Adafruit_GFX {
public:
    DisplayEinkET013TT1(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
    ~DisplayEinkET013TT1();

    void draw_box(int x_start, int y_start, int x_end, int y_end);

    void draw_text();
    void draw_text(String new_text);
    void draw_text(String new_text, int x, int y);

    void draw_text_in_box(String new_text, int start_x, int start_y, int end_x, int end_y);

    int get_text_height();
    int get_text_width(const char *buffer);

    void set_partial_refresh();
    void unset_partial_refresh();
    int get_partial_refresh();

    void display_splash_screen();

    void draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x, int end_y) {
        assert(0);
    }

    void set_font(const uint8_t *new_font);

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h,
                             uint16_t color);
    void refresh();
    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setFullWindow() {
        unset_partial_refresh();
        partialX = 0;
        partialY = 0;
        partialW = WIDTH;
        partialH = HEIGHT;
    }
    void fillScreen(uint16_t color) // 0x0 black, >0x0 white, to buffer
    {
        // Clamp to black or white
        uint8_t data = (color == 0) ? color : 0xFF;
        for (uint16_t x = 0; x < sizeof(buffer); x++) {
            buffer[x] = data;
        }
    }

    uint16_t bg = 0x0000; // 0xFFFF;
    uint16_t fg = 0xFFFF; // 0x0000;

protected:
    void draw_text_cmds_only(const String &new_text, int x, int y, int line_height);
    void draw_box_cmds_only(int x_start, int y_start, int x_end, int y_end);
    void setup_display_for_text();

    const uint8_t *font;
    U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

    static inline uint16_t min(uint16_t a, uint16_t b) { return (a < b ? a : b); };
    static inline uint16_t max(uint16_t a, uint16_t b) { return (a > b ? a : b); };
    template <typename T> static inline void swap(T &a, T &b) {
        T temp = a;
        a = b;
        b = temp;
    };

    uint16_t partialX, partialY, partialW, partialH;
    uint8_t buffer[(EinkET013TT1_X_PIXELS / 8) * EinkET013TT1_Y_PIXELS];
    void rotate(uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h) {
        switch (getRotation()) {
        case 1:
            swap(x, y);
            swap(w, h);
            x = WIDTH - x - w;
            break;
        case 2:
            x = WIDTH - x - w;
            y = HEIGHT - y - h;
            break;
        case 3:
            swap(x, y);
            swap(w, h);
            y = HEIGHT - y - h;
            break;
        }
    }
    void updateDisplay();
    void writeImage(uint8_t *fb, const unsigned char *img, uint16_t size);
    uint8_t writePartialImage(uint16_t x, uint16_t y, const unsigned char *img, uint16_t img_w,
                              uint16_t img_h);
};

#endif