#ifndef RENDERER_H
#define RENDERER_H

#include "../Displays/DisplaySystem.hpp"
#include "../Optics/OpticsSystem.hpp"

class Renderer {
public:
    Renderer(OpticsSystem *optic_input, DisplaySystem *display_input);

    void render_max_box();
    void render_max_box_with_text(const String &text);

    void render_max_box_with_word(const String &word);

    void render_box(int x_start, int y_start, int x_end, int y_end);

    auto get_previous_place() -> unsigned long;

    auto get_pixel_width() -> int { return max_pixels_x; }
    auto get_pixel_height() -> int { return max_pixels_y; }

    int local_x_offset = 0;
    int local_y_offset = 0;

protected:
    OpticsSystem *optic;
    DisplaySystem *display;

    void compute_min_max();
    void compute_max_lines();

    int x_min = 0;
    int x_max = 0;
    int y_min = 0;
    int y_max = 0;
    int max_pixels_x = 0;
    int max_pixels_y = 0;

    int max_lines = 0;
};

#endif