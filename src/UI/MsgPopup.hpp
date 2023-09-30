#pragma once

#include <array>
#include <vector>

#include "Components/BoxView.hpp"

class MsgPopup : public Screen {

private:
    BoxView boxView_;
    DisplaySystem &display_;
    const std::vector<std::string> &msg_;

    static constexpr const char *ask = "Press\u00a0\u00a0\ue000\u00a0\u00a0to return";

public:
    MsgPopup(DisplaySystem &display, const std::vector<std::string> &msg)
        : boxView_(display, 8), display_(display), msg_(msg) {}
    ~MsgPopup() = default;

    void render(DisplaySystem &display) override {

        uint16_t width = display_.getTextWidth(ask);
        uint16_t height =
            display_.getTextHeight() * (msg_.size() + 1) + (display_.getTextHeight() >> 1);
        for (auto &s : msg_) {
            uint16_t w = display_.getTextWidth(s);
            if (w > width) {
                width = w;
            }
        }
        width += 20; // 5 pixels each side;
        height += 10;

        Rect rect = {.origin = {.x = (display_.width() >> 1) - (width >> 1),
                                .y = (display_.height() >> 1) - (height >> 1)},
                     .size = {.width = width, .height = height}};

        boxView_.drawInRect(rect);

        uint16_t y = (display_.height() >> 1) - (height >> 1) + 5;
        for (auto &s : msg_) {
            uint16_t x = (display_.width() >> 1) - (display_.getTextWidth(s) >> 1);
            display_.drawSingleLineOfText(s, x, y);
            y += display_.getTextHeight();
        }
        y += display_.getTextHeight() >> 1;

        display_.drawSingleLineOfText(
            ask, (display_.width() >> 1) - (display_.getTextWidth(ask) >> 1), y);

        display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    void processCommand(RemoteCommand command) override {
        switch (command) {
        case RemoteCommand::ENTER:
            back();
            break;
        default:
            break;
        }
    }
};