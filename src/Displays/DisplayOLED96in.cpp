#include "DisplayOLED96in.hpp"

#include <U8g2lib.h>
#include <utility>

#include "friend.h"
#include "sindarin-debug.h"

// Set up the U8g2lib display
DisplayOLED96in::DisplayOLED96in() {
    font = u8g2_font_tom_thumb_4x6_tf;
    big_font = u8g2_font_6x13_tf;
    u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/U8X8_PIN_NONE, I2C_SCL,
                                                   I2C_SDA);
    if (u8g2) {
        u8g2->begin();
        u8g2->clearBuffer(); // clear the internal memory
        u8g2->setFont(font);
    }
    set_x(OLED96in_X_PIXELS);
    set_y(OLED96in_Y_PIXELS);
    set_width(OLED96in_WIDTH_IN_MM);
    set_height(OLED96in_HEIGHT_IN_MM);
}

DisplayOLED96in::~DisplayOLED96in() {
    if (u8g2) delete u8g2;
}

// Show off the "Friend" graphic.
void DisplayOLED96in::display_splash_screen() {
    u8g2->firstPage();
    do {
        u8g2->drawXBM(0, 6, FRIEND_WIDTH, FRIEND_HEIGHT, friend_bits);
    } while (u8g2->nextPage());
}

void DisplayOLED96in::set_partial_refresh() { partial_refresh = 1; }

void DisplayOLED96in::unset_partial_refresh() { partial_refresh = 0; }

auto DisplayOLED96in::get_partial_refresh() -> int { return partial_refresh; }

void DisplayOLED96in::set_font(const uint8_t *new_font) { font = new_font; }

void DisplayOLED96in::draw_box(int start_x, int start_y, int end_x, int end_y) {
    u8g2->firstPage();
    do {
        draw_box_cmds_only(start_x, start_y, end_x, end_y);
    } while (u8g2->nextPage());
}

void DisplayOLED96in::draw_text_in_box(String new_text, int start_x, int start_y, int end_x,
                                       int end_y) {
    u8g2->setFontPosTop();
    u8g2->setFont(font);
    u8g2->firstPage();
    do {
        draw_box_cmds_only(start_x, start_y, end_x, end_y);
        draw_text_cmds_only(new_text, start_x + 3, start_y + 2);
    } while (u8g2->nextPage());
}

void DisplayOLED96in::draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x,
                                           int end_y) {
    int width = 0;
    u8g2->setFontPosCenter();
    u8g2->setFont(big_font);
    u8g2->firstPage();
    width = get_text_width(new_text.c_str());

    do {
        draw_box_cmds_only(start_x, start_y, end_x, end_y);
        draw_text_cmds_only(new_text, start_x + (end_x - start_x) / 2 - width / 2,
                            (end_y - start_y) / 2);
    } while (u8g2->nextPage());
}

void DisplayOLED96in::draw_box_cmds_only(int start_x, int start_y, int end_x, int end_y) {
    u8g2->drawLine(start_x, start_y, end_x, start_y);
    u8g2->drawLine(start_x, start_y, start_x, end_y);
    u8g2->drawLine(start_x, end_y, end_x, end_y);
    u8g2->drawLine(end_x, start_y, end_x, end_y);
    // Uncomment if you want to draw the lines from corner to corner
    // u8g2->drawLine(start_x, start_y, end_x, end_y);
    // u8g2->drawLine(end_x, start_y, start_x, end_y);
}

void DisplayOLED96in::draw_text() { draw_text(string_to_draw, x_to_draw, y_to_draw); }

void DisplayOLED96in::draw_text(String new_text) { draw_text(new_text, x_to_draw, y_to_draw); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayOLED96in::draw_text(String new_text, int x, int y) {
    u8g2->setFontPosTop();
    u8g2->setFont(font);
    u8g2->firstPage();
    do {
        draw_text_cmds_only(new_text, x, y);
    } while (u8g2->nextPage());
}

auto DisplayOLED96in::get_text_height() -> int {
    int ascent = 0;
    int descent = 0;
    int line_height = 0;

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    // text to squeeze as much in there as we can
    ascent = u8g2->getAscent();
    descent = u8g2->getDescent();

    // Note that descent is returned as a negative number. Therefore to get
    // the correct height, you have to subtract off the descent. We add
    // 1 to add the additional pixel between lines.
    line_height = ascent - descent + 1;

    return line_height;
}

auto DisplayOLED96in::get_text_width(const char *buffer) -> int {
    return u8g2->getStrWidth(buffer);
}

void DisplayOLED96in::draw_text_cmds_only(const String &new_text, int x, int y) {

    int line_height = 0;

    string_to_draw = std::move(new_text);

    split_string_to_lines();

    line_height = get_text_height();

    for (int i = 0; i < num_lines; i++) {
        u8g2->setCursor(x, y + i * line_height);
        u8g2->print(lines[i]);
    }
}