#include "DisplayEinkET013TT1.hpp"

#include <EPD_TCON.h>

extern const EPD_TCON_DRIVER_HAL g_TCON_HAL;
extern void EPD_HAL_Init(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY);
extern const unsigned char g_WaveformData[];

// Set up the U8g2lib display
DisplayEinkET013TT1::DisplayEinkET013TT1(uint8_t CS, uint8_t OEI, uint8_t RST, uint8_t BUSY)
    : Adafruit_GFX(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS) {
    EPD_HAL_Init(CS, OEI, RST, BUSY);

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

    font = u8g2_font_tom_thumb_4x6_tf;
#ifdef DISPLAY_ROTATION
    setRotation(DISPLAY_ROTATION);
#else
    setRotation(0);
#endif
    u8g2Fonts.begin(*this);
    u8g2Fonts.setFont(font);
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

void DisplayEinkET013TT1::set_font(const uint8_t *new_font) { font = new_font; }

auto DisplayEinkET013TT1::get_text_height() -> int {
    int ascent = 0;
    int descent = 0;
    int line_height = 0;

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    // text to squeeze as much in there as we can
    ascent = u8g2Fonts.getFontAscent();
    descent = u8g2Fonts.getFontDescent();

    // Note that descent is returned as a negative number. Therefore to get
    // the correct height, you have to subtract off the descent. We add
    // 1 to add the additional pixel between lines.
    line_height = ascent - descent + 1;

    return line_height;
}

auto DisplayEinkET013TT1::get_text_width(const char *buffer) -> int {
    return u8g2Fonts.getUTF8Width(buffer);
}

void DisplayEinkET013TT1::draw_text_in_box(String new_text, int start_x, int start_y, int end_x,
                                           int end_y) {

    int line_height = 0;

    setup_display_for_text();

    log_d("Printing some stuff to screen! (%ld)", (long)this);

    line_height = get_text_height();

    if (get_partial_refresh()) {
        setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4, (end_y - start_y) + 4);
    }

    draw_box_cmds_only(start_x, start_y, end_x, end_y);
    draw_text_cmds_only(new_text, start_x + 3, start_y + line_height + 1, line_height);
    // DO WRITE AND REFRESH
    updateDisplay();
}

void DisplayEinkET013TT1::draw_box_cmds_only(int start_x, int start_y, int end_x, int end_y) {

    if (!get_partial_refresh()) {
        fillScreen(bg);
    }
    drawRect(start_x, start_y, end_x - start_x, end_y - start_y, fg);
}

void DisplayEinkET013TT1::draw_box(int start_x, int start_y, int end_x, int end_y) {
    // clear out one pixel around all of it
    if (get_partial_refresh()) {
        log_d("Draw box: %d %d | %d %d", start_x, start_y, end_x, end_y);
        setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4, (end_y - start_y) + 4);
    }
    log_d("Got here - drawing a box: %d %d | %d %d", start_x, start_y, end_x, end_y);

    draw_box_cmds_only(start_x, start_y, end_x, end_y);
    // DO WRITE AND REFRESH
    updateDisplay();
}

void DisplayEinkET013TT1::draw_text() { draw_text(string_to_draw, x_to_draw, y_to_draw); }

void DisplayEinkET013TT1::draw_text(String new_text) { draw_text(new_text, x_to_draw, y_to_draw); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayEinkET013TT1::draw_text(String new_text, int x, int y) {

    int ascent = 0;
    int descent = 0;
    int line_height = 0;

    setup_display_for_text();

    DEBUG_LN("Printing to screen!");

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    // text to squeeze as much in there as we can
    ascent = u8g2Fonts.getFontAscent();
    descent = u8g2Fonts.getFontDescent();

    // Note that descent is returned as a negative number. Therefore to get
    // the correct height, you have to subtract off the descent. We add
    // 1 to add the additional pixel between lines.
    line_height = ascent - descent + 1;

    if (get_partial_refresh()) {
        setPartialWindow(x, y - 10, EinkET013TT1_X_PIXELS, (line_height + 1) * num_lines + 10);
    }

    draw_text_cmds_only(new_text, x, y, line_height);
    // DO WRITE AND REFRESH
    updateDisplay();
}

void DisplayEinkET013TT1::setup_display_for_text() {

    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(fg);
    u8g2Fonts.setBackgroundColor(bg);
    u8g2Fonts.setFont(font);
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

    if (!get_partial_refresh()) {
        fillScreen(bg);
    }

    for (int i = 0; i < num_lines; i++) {
        // Don't know if it starts the line from the baseline or from the top -- TO LEARN: HACK
        u8g2Fonts.setCursor(x, y + i * (line_height));
        u8g2Fonts.println(lines[i] + "\n");
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