// Superclass for various display systems
//
// Specific display should be derived from this class and should
// define their characteristics and use the underlying u8g2 libraries
// for drawing text and other things. This will allow the Renderer
// class to automatically use arbirary displays and optics systems
// without having to rewrite the Rendering code.
//
// When deriving from this class, add the new class header to
// the file "AllDisplays.hpp" so clients of these classes only
// need to include a single header to get all the different display
// options.
//

#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H

#include <Arduino.h>

// This isn't the best -- we may want to make this dynamically
// allocated instead in the object?
#define MAX_LINES_IN_SPLIT 100

class DisplaySystem {
public:
    auto get_x() -> int;
    auto get_y() -> int;
    auto get_width() -> float;
    auto get_height() -> float;

    void set_x(int new_x);
    void set_y(int new_y);

    void set_width(float new_width);
    void set_height(float new_height);

    void set_text(const String &new_text);

    void set_coordinates(int x, int y);

    virtual void set_partial_refresh();
    virtual void unset_partial_refresh();
    virtual auto get_partial_refresh() -> int;

    virtual auto get_text_height() -> int = 0;
    virtual auto get_text_width(const char *buffer) -> int = 0;

    virtual void draw_box(int x_start, int y_start, int x_end, int y_end) = 0;

    virtual void draw_text() = 0;
    virtual void draw_text(String new_text) = 0;
    virtual void draw_text(String new_text, int x, int y) = 0;

    virtual void draw_text_in_box(String new_text, int start_x, int start_y, int end_x,
                                  int end_y) = 0;

    virtual void draw_big_word_in_box(String new_text, int start_x, int start_y, int end_x,
                                      int end_y) = 0;

    virtual void display_splash_screen() = 0;

    DisplaySystem();
    ~DisplaySystem();

protected:
    // Number of pixels in the X direction
    int x;
    // Number of pixels in the Y direction
    int y;

    float width_in_mm;
    float height_in_mm;

    int x_to_draw = 0;
    int y_to_draw = 0;

    String string_to_draw;

    void split_string_to_lines();

    int partial_refresh = 0;

    int num_lines = 0;
    String lines[MAX_LINES_IN_SPLIT];
};

#endif