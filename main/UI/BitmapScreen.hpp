#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class BitmapScreen : public Screen {
public:
    // Allow custom size bitmaps. By default they'll be rendered in the middle of the
    // screen. Can go from here if we want to do something different and extend the
    // functionality of this class.
    BitmapScreen(const uint8_t *bitmap, int width, int height)
        : bitmap_(bitmap), width_(width), height_(height) {}

    ~BitmapScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    const uint8_t *bitmap_;
    int width_;
    int height_;
};
