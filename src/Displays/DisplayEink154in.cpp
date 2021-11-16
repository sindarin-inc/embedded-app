#include "DisplayEink154in.hpp"

#include <Fonts/FreeMonoBold9pt7b.h>
#include <utility>

const char HelloWorld[] = "Hello Fy World!";

void DisplayEink154in::put_up_hello_world() {

    display.setRotation(2);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center the bounding box by transposition of the origin:
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    uint16_t y = ((display.height() - tbh) / 2) - tby;
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(x, y);
        display.print(HelloWorld);
    } while (display.nextPage());
}

// Set up the U8g2lib display
DisplayEink154in::DisplayEink154in() {
    font = u8g2_font_tom_thumb_4x6_tf;
#ifdef HUZZAH32_SINDARIN
    display.init();
    display.setRotation(2);
#else
    display.init();
    display.setRotation(0);
#endif
    u8g2Fonts.begin(display);
    u8g2Fonts.setFont(font);
    DEBUG_F("Set up display (%ld)\n", (long)&display);

    // put_up_hello_world();

    set_x(Eink154in_X_PIXELS);
    set_y(Eink154in_Y_PIXELS);
    set_width(Eink154in_WIDTH_IN_MM);
    set_height(Eink154in_HEIGHT_IN_MM);
}

// Fill in with the right splash screen for Eink
void DisplayEink154in::display_splash_screen() {}

DisplayEink154in::~DisplayEink154in() = default;

void DisplayEink154in::set_partial_refresh() { partial_refresh = 1; }

void DisplayEink154in::unset_partial_refresh() { partial_refresh = 0; }

auto DisplayEink154in::get_partial_refresh() -> int { return partial_refresh; }

void DisplayEink154in::set_font(const uint8_t *new_font) { font = new_font; }

auto DisplayEink154in::get_text_height() -> int {
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

auto DisplayEink154in::get_text_width(const char *buffer) -> int {
    return u8g2Fonts.getUTF8Width(buffer);
}

void DisplayEink154in::draw_text_in_box(String new_text, int start_x, int start_y, int end_x,
                                        int end_y) {

    int line_height = 0;

    setup_display_for_text();

    DEBUG_F("Printing some stuff to screen! (%ld)\n", (long)&display);

    line_height = get_text_height();

    if (get_partial_refresh()) {
        display.setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4,
                                 (end_y - start_y) + 4);
    }

    display.firstPage();
    do {
        draw_box_cmds_only(start_x, start_y, end_x, end_y);
        draw_text_cmds_only(new_text, start_x + 3, start_y + line_height + 1, line_height);
    } while (display.nextPage());

    /*
     display.setRotation(2);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  }
  while (display.nextPage());*/
}

void DisplayEink154in::draw_box_cmds_only(int start_x, int start_y, int end_x, int end_y) {

    if (!get_partial_refresh()) {
        display.fillScreen(bg);
    }
    display.drawRect(start_x, start_y, end_x - start_x, end_y - start_y, fg);
}

void DisplayEink154in::draw_box(int start_x, int start_y, int end_x, int end_y) {
    // clear out one pixel around all of it
    if (get_partial_refresh()) {
        display.setPartialWindow(start_x - 2, start_y - 2, (end_x - start_x) + 4,
                                 (end_y - start_y) + 4);
    }
    DEBUG_LN("Got here: drawing a box");

    display.firstPage();
    do {
        draw_box_cmds_only(start_x, start_y, end_x, end_y);
    } while (display.nextPage());
}

void DisplayEink154in::draw_text() { draw_text(string_to_draw, x_to_draw, y_to_draw); }

void DisplayEink154in::draw_text(String new_text) { draw_text(new_text, x_to_draw, y_to_draw); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayEink154in::draw_text(String new_text, int x, int y) {

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
        display.setPartialWindow(x, y - 10, Eink154in_X_PIXELS, (line_height + 1) * num_lines + 10);
    }

    display.firstPage();
    do {
        draw_text_cmds_only(new_text, x, y, line_height);
    } while (display.nextPage());
}

void DisplayEink154in::setup_display_for_text() {

    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(fg);
    u8g2Fonts.setBackgroundColor(bg);
    u8g2Fonts.setFont(font);
}

void DisplayEink154in::draw_text_cmds_only(const String &new_text, int x, int y, int line_height) {

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
        display.fillScreen(bg);
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
