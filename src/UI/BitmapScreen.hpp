#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Misc/PBMImage.hpp"
#include "UI/Screen.hpp"

class BitmapScreen : public Screen {
public:
    // Allow custom size bitmaps. By default they'll be rendered in the middle of the
    // screen. Can go from here if we want to do something different and extend the
    // functionality of this class.
    BitmapScreen(const uint8_t *bitmap, int size) : bitmap_(bitmap), size_(size) {

        log_i("creating bitmap screen");
        BufferStream stream(bitmap, size);
        if (!ParsePbmHeader(stream, width_, height_)) {
            log_e("BitmapScreen: could not parse pbm header");
        }
        log_i("done creating");
    }

    ~BitmapScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    const uint8_t *bitmap_;
    int size_;
    int width_;
    int height_;
};
