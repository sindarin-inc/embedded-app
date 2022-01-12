#ifndef RAW_RENDERER_H
#define RAW_RENDERER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"

#define MAX_CHARS_PER_PAGE 1500
#define PAGE_READ 1
#define SCREED_READ 2

class RawRenderer : public Renderer {
public:
    RawRenderer(OpticsSystem *optic_input, DisplaySystem *display_input, std::string &string);

    void set_string(std::string &string);

    void set_read_mode(int new_mode);
    auto get_read_mode() -> int;
    void flip_read_mode();

    void increase_speed();
    void decrease_speed();
    void set_speed(int new_speed);

    // Always call this on each loop.
    // If not screed, it'll be a nop
    auto render_continue_screed() -> unsigned long;

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    auto render_page_with_box(unsigned long place_in_file) -> unsigned long;

    auto get_previous_place() -> unsigned long;

protected:
    std::istringstream data_stream;

    unsigned long screed_last_time;

    int screed_speed;

    int read_mode;

    unsigned long current_place = 0;
    unsigned long previous_place = 0;
    unsigned long next_place = 0;
    int first_go_around = 1;

    char prev_buffer[MAX_CHARS_PER_PAGE];
    char curr_buffer[MAX_CHARS_PER_PAGE];
    char next_buffer[MAX_CHARS_PER_PAGE];

    auto load_prev_into_buffer() -> unsigned long;
    auto load_curr_into_buffer() -> unsigned long;
    auto load_next_into_buffer() -> unsigned long;

    // This does the work of lineify from the old system. Loads one page of
    // text into that buffer given the current offset in the file
    auto load_into_buffer(char *buffer, unsigned long place) -> unsigned long;
};

#endif