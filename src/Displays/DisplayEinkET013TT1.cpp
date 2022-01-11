#include "DisplayEinkET013TT1.hpp"

#include "../UI/Bitmaps/Menu_Blank.hpp"
#include "../UI/Bitmaps/Popup_Menu_Blank.hpp"
#include "../lib/bmps-for-splash/LucidaGrande10pt7b.h"
#include "../lib/bmps-for-splash/LucidaGrande14pt7b.h"
#include "../lib/bmps-for-splash/LucidaGrande5pt7b.h"
#include "../lib/bmps-for-splash/LucidaGrande6pt7b.h"
#include "../lib/bmps-for-splash/NotoSans_CondensedMedium10pt7b.h"
#include "../lib/bmps-for-splash/NotoSans_CondensedMedium14pt7b.h"
#include "../lib/bmps-for-splash/NotoSans_CondensedMedium20pt7b.h"
#include "../lib/bmps-for-splash/NotoSans_CondensedMedium5pt7b.h"
#include "../lib/bmps-for-splash/NotoSans_CondensedMedium6pt7b.h"
//#include "../lib/bmps-for-splash/Bookerly_Display6pt7b.h"
//#include "../lib/bmps-for-splash/Bookerly_Light6pt7b.h"
//#include "../lib/bmps-for-splash/Bookerly6pt7b.h"
//#include "../lib/bmps-for-splash/Bookerly5pt7b.h"

#include <EPD_TCON.h>
#include <UI/Fonts/LucidaGrande5pt7b.h>

extern const EPD_TCON_DRIVER_HAL g_TCON_HAL;
extern void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
extern const unsigned char g_WaveformData[];

// Set up the U8g2lib display
DisplayEinkET013TT1::DisplayEinkET013TT1(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY)
    : Adafruit_GFX(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS) {
    EPD_HAL_Init(CS, OEI, RST, BUSY);

    auto_update = true;

    int32_t ret = EPD_TCON_Init(&g_TCON_HAL);

    if (ret < 0) {
        log_d("Bad return code from TCON Init: %d", ret);
        while (1)
            ;
    }

    Serial.println("Loading waveform...");

    ret = EPD_TCON_LoadWaveform(g_WaveformData);
    Serial.println("Waveform loaded?");

    if (!ret) {
        log_d("Bad return code from waveform load: %d", ret);
        while (1)
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
    setFont(&LucidaGrande5pt7b);
    log_d("Set up display (%ld)", (long)this);

    set_x(EinkET013TT1_X_PIXELS);
    set_y(EinkET013TT1_Y_PIXELS);
    set_width(EinkET013TT1_WIDTH_IN_MM);
    set_height(EinkET013TT1_HEIGHT_IN_MM);
}

// Fill in with the right splash screen for Eink
void DisplayEinkET013TT1::display_splash_screen() {}

DisplayEinkET013TT1::~DisplayEinkET013TT1() = default;

// BC: Changed to never do partial_referesh for now.
// Change to = 1 when we want to try partial refresh again.
void DisplayEinkET013TT1::set_partial_refresh() { partial_refresh = 0; }

void DisplayEinkET013TT1::unset_partial_refresh() { partial_refresh = 0; }

auto DisplayEinkET013TT1::get_partial_refresh() -> int { return partial_refresh; }

void DisplayEinkET013TT1::set_auto_update(bool update) { auto_update = update; }

bool DisplayEinkET013TT1::get_auto_update() { return auto_update; }

const GFXfont *DisplayEinkET013TT1::get_font() { return gfxFont; }

void DisplayEinkET013TT1::set_font(const GFXfont *new_font) { setFont(new_font); }

void DisplayEinkET013TT1::get_text_bounds(const char *buffer, uint16_t *w, uint16_t *h) {
    int16_t x, y;
    getTextBounds(buffer, 0, 0, &x, &y, w, h);
}

auto DisplayEinkET013TT1::get_text_height() -> int { return gfxFont->yAdvance + 1; }

auto DisplayEinkET013TT1::get_text_height(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return h;
}

auto DisplayEinkET013TT1::get_text_width(const char *buffer) -> int {
    int16_t x, y;
    uint16_t w, h;
    getTextBounds(buffer, 0, 0, &x, &y, &w, &h);
    return w;
}

void DisplayEinkET013TT1::draw_text_in_box(String new_text, int start_x, int start_y, int end_x,
                                           int end_y) {

    int line_height = 0;

    log_d("Printing some stuff to screen! (%ld)", (long)this);

    line_height = get_text_height();

    if (get_partial_refresh()) {
        setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4, (end_y - start_y) + 4);
    }

    draw_box_cmds_only(start_x, start_y, end_x, end_y);
    draw_text_cmds_only(new_text, start_x + 3, start_y + line_height + 1, line_height);
    if (auto_update) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplayEinkET013TT1::draw_menu_lines_with_item(String new_text, int start_x, int start_y,
                                                    int end_x, int end_y, String battery_status) {
    int line_height = 0;

    log_d("Printing some stuff to screen from menu items! (%ld)", (long)this);

    if (get_partial_refresh()) {
        setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4, (end_y - start_y) + 4);
    }

    log_d("set partial window complete");

    draw_menu_lines_cmds_only(start_x, start_y, end_x, end_y);

    log_d("drew commands only");
    // ugly
    setup_display_for_text();
    // u8g2Fonts.setFont(u8g2_font_t0_22b_tf);
    /*
       u8g2Fonts.setFont(u8g2_font_6x10_tf);
       line_height = get_text_height();
       partial_refresh = 1;
       u8g2Fonts.setCursor((end_x-start_x)/2+start_x-12, (end_y - start_y)/6 + start_y);
       u8g2Fonts.println("Exit");

       u8g2Fonts.setCursor(start_x+3, (end_y - start_y)/2 + start_y-8);
       u8g2Fonts.println("Home");

       u8g2Fonts.setCursor(end_x-30, (end_y - start_y)/2 + start_y-8);
       u8g2Fonts.println("Items");

       u8g2Fonts.setCursor((end_x-start_x)/2+start_x-21, end_y-(end_y - start_y)/4);
       u8g2Fonts.println("Settings");


       u8g2Fonts.setCursor((end_x-start_x)/2+start_x-30, end_y-3);
       u8g2Fonts.println(new_text);

      */
    /* Popup menu text!
        setFont(&NotoSans_CondensedMedium5pt7b);
        setCursor((end_x-start_x)/2+start_x-12, (end_y - start_y)/6 + start_y);
        println("Exit");

        setCursor(start_x+3, (end_y - start_y)/2 + start_y-8);
        println("Home");

        setCursor(end_x-30, (end_y - start_y)/2 + start_y-8);
        println("Items");

        setCursor((end_x-start_x)/2+start_x-21, end_y-(end_y - start_y)/4);
        println("Settings");

        setFont(&NotoSans_CondensedMedium10pt7b);
        setCursor((end_x-start_x)/2+start_x-30, end_y-3);
        println(new_text);
        */
    // setCursor((end_x-start_x)/2+start_x - 20,50);
    // println("Let's Focus");
    int16_t returnX, returnY;
    uint16_t returnW, returnH;

    // getTextBounds("Let's Focus", (end_x-start_x)/2+start_x-20, 50, &returnX, &returnY, &returnW,
    // &returnH);

    // log_d("all the return vals from getTextBounds: %d %d %d %d", returnX, returnY, returnW,
    // returnH);
    setFont(&LucidaGrande10pt7b);
    // setFont(&NotoSans_CondensedMedium10pt7b);
    setCursor((end_x - start_x) / 2 + start_x - get_text_width(new_text.c_str()) / 2,
              (end_y - start_y) / 2 + start_y + get_text_height() / 2 - 2);
    // this is at the bottom
    // setCursor((end_x-start_x)/2+start_x-get_text_width(new_text.c_str())/2,
    // (end_y-3)-get_text_height());
    println(new_text);

    setFont(&LucidaGrande5pt7b);

    setCursor((end_x - start_x) / 2 + start_x - get_text_width(battery_status.c_str()) / 2, end_y);

    println(battery_status);

    // setFont(&LucidaGrande5pt7b);

    // setFont(&NotoSans_CondensedMedium6pt7b);

    // draw_text_cmds_only(new_text, start_x + 10, (end_y - start_y)/2 + start_y, line_height);
    // partial_refresh = 0;
    log_d("drew text commands only");

    updateDisplay();
    log_d("completed update display");
}

void DisplayEinkET013TT1::draw_box_cmds_only(int start_x, int start_y, int end_x, int end_y) {

    if (!get_partial_refresh() && auto_update) {
        fillScreen(bg);
    }
    drawRect(start_x, start_y, end_x - start_x, end_y - start_y, fg);
}

void DisplayEinkET013TT1::draw_menu_lines_cmds_only(int start_x, int start_y, int end_x,
                                                    int end_y) {

    if (!get_partial_refresh()) {
        fillScreen(bg);
    }
    /* INSERT ALL THE LINES AND TRIANGLES HERE */
    log_d("start cmds");
    Adafruit_GFX::drawXBitmap(0, 0, Menu_Blank_bits, 256, 256, fg);
    // Adafruit_GFX::drawXBitmap(0, 0, Popup_Menu_Blank_bits, 256, 256, fg);
    /*
     fillRect(start_x, start_y + (end_y-start_y)/3 - 1, end_x-start_x, 3, fg);
       log_d("rect 1 complete %d %d %d %d", start_x, start_y, end_x, end_y);
       log_d("rec coordinates %d %d %d %d", start_x, start_y + (end_y-start_y)/3 - 1, end_x-start_x,
     5); fillRect(start_x, end_y - (end_y-start_y)/3 - 1, end_x-start_x, 3, fg); log_d("rect 2
     complete"); fillTriangle(end_x, (end_y-start_y) / 2, end_x-6, (end_y-start_y) / 2-6, end_x+6,
     (end_y-start_y) / 2+6, fg); log_d("triangle 1 complete"); fillTriangle((end_x-start_x)/2,
     start_y, (end_x-start_x)/2-12, start_y+6, (end_x-start_x)/2+12, start_y+6, fg); log_d("triangle
     2 complete"); fillTriangle((end_x-start_x)/2, end_y, (end_x-start_x)/2-12, end_y - 6,
     (end_x-start_x)/2+12, end_y-6, fg); log_d("triangle 3 complete"); log_d("end cmds");
     */
    /* BC: CALIBRATION SCREEN
        int midpoint_x = ((end_x-start_x)/2) + start_x;
        int midpoint_y = ((end_y-start_y)/2) + start_y;
        int box_width = end_x - start_x;
        int box_height = end_y - start_y;

        fillRect(start_x, start_y, box_width, box_height, fg);
        fillRect(start_x+2, start_y+2, box_width-4, box_height-4, bg);
        setFont(&NotoSans_CondensedMedium6pt7b);
        int lets_width = get_text_width("let's");
        int focus_width= get_text_width("focus");
        int lets_height = get_text_height();
        int focus_height = get_text_height();

        for (int i = 30; i > 23; i--) {
            drawCircle(midpoint_x, midpoint_y, i, fg);
        }

        setCursor(midpoint_x - lets_width / 2, midpoint_y - 3);
        println("let's");
        setCursor(midpoint_x - focus_width / 2, midpoint_y + focus_height - 6);
        println("focus");

        int EDGE_OFFSET = 5;

        setCursor(end_x - get_text_width("lower right") - EDGE_OFFSET, end_y - EDGE_OFFSET - 2);
        println("lower right");
        setCursor(start_x + EDGE_OFFSET, end_y - EDGE_OFFSET - 2); // lower left
        println("lower left");
        setCursor(start_x + EDGE_OFFSET, start_y + get_text_height() - 2); // upper left
        println("upper left");
        setCursor(end_x - get_text_width("upper right") - EDGE_OFFSET, start_y + get_text_height() -
       2); println("upper right");
    */
}

void DisplayEinkET013TT1::draw_box(int start_x, int start_y, int end_x, int end_y) {
    // clear out one pixel around all of it
    if (get_partial_refresh()) {
        log_d("Draw box: %d %d | %d %d", start_x, start_y, end_x, end_y);
        setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4, (end_y - start_y) + 4);
    }
    log_d("Got here - drawing a box: %d %d | %d %d", start_x, start_y, end_x, end_y);

    draw_box_cmds_only(start_x, start_y, end_x, end_y);
    if (auto_update) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplayEinkET013TT1::draw_text() { draw_text(string_to_draw, x_to_draw, y_to_draw); }

void DisplayEinkET013TT1::draw_text(String new_text) { draw_text(new_text, x_to_draw, y_to_draw); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayEinkET013TT1::draw_text(String new_text, int x, int y) {
    DEBUG_LN("Printing to screen!");

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    int lineHeight = get_text_height();

    if (get_partial_refresh()) {
        setPartialWindow(x, y - 10, EinkET013TT1_X_PIXELS, lineHeight * num_lines + 10);
    }

    draw_text_cmds_only(new_text, x, y, lineHeight);
    if (auto_update) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplayEinkET013TT1::draw_text_cmds_only(const String &new_text, int x, int y,
                                              int line_height) {

    string_to_draw = std::move(new_text);

    // We want to draw the characters down from the x/y coordinates given
    // so the renderer doesn't need to concern itself with how tall the
    // font is, etc.
    // u8g2Fonts->setFontPosTop();

    split_string_to_lines();

    // We want to have only 1 pixel length difference between the rows of
    // text to squeeze as much in there as we can
    // ascent = u8g2Fonts->getAscent();
    // descent = u8g2Fonts->getDescent();

    if (!get_partial_refresh() && auto_update) {
        fillScreen(bg);
    }

    for (int i = 0; i < num_lines; i++) {
        // Don't know if it starts the line from the baseline or from the top -- TO LEARN: HACK
        setCursor(x, y + i * (line_height));
        println(lines[i] + "\n");
        // DEBUG_LN(lines[i]);
        // DEBUG_VAR(x);
        // DEBUG_LN(" is the x direction");
        // DEBUG_VAR(y+i*(line_height+1));
        // DEBUG_LN(" is the y direction");
    }
}

void DisplayEinkET013TT1::setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    set_partial_refresh();
    partialX = min(x, width());
    partialY = min(y, height());
    partialW = min(w, width() - partialX);
    partialH = min(h, height() - partialY);
    rotate(partialX, partialY, partialW, partialH);
    // force partialX, partialW multiple of 8?
    partialW += partialX % 8;
    if (partialW % 8 > 0) partialW += 8 - partialW % 8;
    partialX -= partialX % 8;
}

void DisplayEinkET013TT1::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) {
        DEBUG_LN("drawPixel received invalid parameters; short-circuiting call");
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
    if (get_partial_refresh()) {
        // Transpose partial window to 0,0
        x -= partialX;
        y -= partialY;
        // Enforce partial window size
        if ((x < 0) || (x >= partialW) || (y < 0) || (y >= partialH)) {
            log_d("drawPixel out of partial window bounds; short-circuiting call: %d %d | %d %d", x,
                  y, partialW, partialH);
            return;
        }
    }
    uint16_t i = x / 8 + y * (partialW / 8);
    buffer[i] =
        (color == 0x0000) ? (buffer[i] & ~(1 << (7 - x % 8))) : (buffer[i] | (1 << (7 - x % 8)));
}

void DisplayEinkET013TT1::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w,
                                      int16_t h, uint16_t color) {
    if (get_partial_refresh()) {
        setPartialWindow(x, y, w, h);
    }
    Adafruit_GFX::drawXBitmap(x, y, bitmap, w, h, color);
    log_d("Bitmap drawn, updating display!");
    if (auto_update) {
        // DO WRITE AND REFRESH
        updateDisplay();
    }
}

void DisplayEinkET013TT1::updateDisplay() {
    if (get_partial_refresh()) {
        writePartialImage(partialX, partialY, buffer, partialW, partialH);
        EPD_TCON_PowerOn();
        EPD_TCON_Update(WF_MODE_GU4);
        EPD_TCON_PowerOff();
    } else {
        writeImage(EPD_TCON_GetUpdateBuffer(), buffer,
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

uint8_t DisplayEinkET013TT1::writePartialImage(uint16_t x, uint16_t y, const unsigned char *img,
                                               uint16_t img_w, uint16_t img_h) {
    uint8_t *fb;
    uint32_t i;

    if (((x + img_w) > EinkET013TT1_X_PIXELS) || ((y + img_h) > EinkET013TT1_Y_PIXELS))
        return 0; // over range

    for (i = 0; i < img_h; ++i) {
        fb = EPD_TCON_GetUpdateBuffer();

        fb += (y + i) * (EinkET013TT1_X_PIXELS / 4);
        fb += (x >> 2);

        writeImage(fb, img + (i * (img_w / 8)), (img_w / 8));
    }
    return 1;
}