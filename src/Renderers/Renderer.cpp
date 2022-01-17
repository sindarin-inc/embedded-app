#include "Renderer.hpp"

#include <utility>

#include "sindarin-debug.h"

Renderer::Renderer(OpticsSystem *optic_input, DisplaySystem *display_input) {
    optic = optic_input;
    display = display_input;
    compute_min_max();
}

void Renderer::compute_min_max() {
    int midpoint_x;
    int midpoint_y;
    // pixels per mm
    float pixel_pitch_x;
    float pixel_pitch_y;

    log_d("Computing Min and Max pixels for Renderer");

    midpoint_x = display->get_x() / 2;
    midpoint_y = display->get_y() / 2;

    log_d("midpoint = (%d, %d)", midpoint_x, midpoint_y);

    pixel_pitch_x = ((float)display->get_x()) / display->get_width();
    pixel_pitch_y = ((float)display->get_y()) / display->get_height();

    log_d("pixel pitch = (%d, %d)", pixel_pitch_x, pixel_pitch_y);

    // rounding down is ok here and what we want.
    max_pixels_x = (int)(optic->get_x_mm() * pixel_pitch_x);
    max_pixels_y = (int)(optic->get_y_mm() * pixel_pitch_y);

    log_d("max pixels = (%d, %d)", max_pixels_x, max_pixels_y);

    x_min = midpoint_x - max_pixels_x / 2;
    x_max = midpoint_x + max_pixels_x / 2;
    y_min = midpoint_y - max_pixels_y / 2;
    y_max = midpoint_y + max_pixels_y / 2;

    log_d("min = (%d, %d) max = (%d, %d)", x_min, y_min, x_max, y_max);
}

// Ask the display for the line height so we can set the max lines based
// on our calcualtion.

void Renderer::compute_max_lines() {}

void Renderer::render_max_box() {
    display->draw_box(x_min + local_x_offset, y_min + local_y_offset, x_max + local_x_offset,
                      y_max + local_y_offset);
}

void Renderer::render_max_box_with_text(const String &text) {
    display->draw_text_in_box(std::move(text), x_min + local_x_offset, y_min + local_y_offset,
                              x_max + local_x_offset, y_max + local_y_offset);
}

void Renderer::render_max_box_with_word(const String &word) {
    display->draw_big_word_in_box(std::move(word), x_min + local_x_offset, y_min + local_y_offset,
                                  x_max + local_x_offset, y_max + local_y_offset);
}

void Renderer::render_box(int x_start, int y_start, int x_end, int y_end) {
    int startX = x_min + local_x_offset;
    int startY = y_min + local_y_offset;
    display->draw_box(startX + x_start, startY + y_start, startX + x_end, startY + y_end);
}
