#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_154_D67 // GDEH0154D67 200x200

// SS is usually used for CS. define here for easy change
#ifndef EPD_CS
#define EPD_CS SS
#endif

#if defined(GxEPD2_DISPLAY_CLASS) && defined(GxEPD2_DRIVER_CLASS)

// somehow there should be an easier way to do this
#define GxEPD2_BW_IS_GxEPD2_BW true
#define GxEPD2_3C_IS_GxEPD2_3C true
#define GxEPD2_7C_IS_GxEPD2_7C true
#define GxEPD2_1248_IS_GxEPD2_1248 true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)
#define IS_GxEPD2_3C(x) IS_GxEPD(GxEPD2_3C_IS_, x)
#define IS_GxEPD2_7C(x) IS_GxEPD(GxEPD2_7C_IS_, x)
#define IS_GxEPD2_1248(x) IS_GxEPD(GxEPD2_1248_IS_, x)

#endif

#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
#define MAX_HEIGHT(EPD)                                                                            \
    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8)                                     \
         ? EPD::HEIGHT                                                                             \
         : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

#define Eink154in_X_PIXELS 200
#define Eink154in_Y_PIXELS 200

#define Eink154in_WIDTH_IN_MM 28.5
#define Eink154in_HEIGHT_IN_MM 28.5

#if !defined(DISABLE_SPIRAM)
#define Eink154in_USE_SPRAM 1
#endif

class DisplayEink154in : public DisplaySystem {
public:
    DisplayEink154in();
    ~DisplayEink154in();

#ifdef Eink154in_USE_SPRAM
    auto operator new(size_t size) -> void * {
        void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        return p;
    }

    void operator delete(void *p) { heap_caps_free(p); }
#endif

    void draw_box(int x_start, int y_start, int x_end, int y_end) override;

    void draw_text() override;
    void draw_text(String new_text) override;
    void draw_text(String new_text, int x, int y) override;

    void draw_text_in_box(String new_text, int start_x, int start_y, int end_x, int end_y) override;

    auto get_text_height() -> int override;
    auto get_text_width(const char *buffer) -> int override;

    void set_partial_refresh();
    void unset_partial_refresh();
    auto get_partial_refresh() -> int;

    void display_splash_screen() override;
    void put_up_hello_world();

    void draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x,
                              int end_y) override {
        assert(0);
    }

    void set_font(const uint8_t *new_font);

protected:
    void draw_text_cmds_only(const String &new_text, int x, int y, int line_height);
    void draw_box_cmds_only(int x_start, int y_start, int x_end, int y_end);
    void setup_display_for_text();

    const uint8_t *font;
    // U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2;
    U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
#ifdef HUZZAH32_SINDARIN
    GxEPD2_BW<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display =
        GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(
            GxEPD2_DRIVER_CLASS(/*CS=5*/ 14, /*DC=*/15, /*RST=*/27, /*BUSY=*/13));
#else
    GxEPD2_BW<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display =
        GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(
            GxEPD2_DRIVER_CLASS(/*CS=5*/ 5, /*DC=*/4, /*RST=*/32, /*BUSY=*/33));
#endif

    uint16_t bg = GxEPD_WHITE;
    uint16_t fg = GxEPD_BLACK;
};

#endif