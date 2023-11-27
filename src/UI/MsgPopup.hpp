#pragma once

#include <array>
#include <vector>

#include "Components/BoxView.hpp"
#include "UI/Screen.hpp"

class MsgPopup : public Screen {

private:
    BoxView boxView_;
    BoxView backgroundView_;
    DisplaySystem &display_;
    const std::vector<std::string> &msg_;

    static constexpr const char *ASK = "Press\u00a0\u00a0\ue000\u00a0\u00a0to return";

public:
    MsgPopup(DisplaySystem &display, const std::vector<std::string> &msg)
        : boxView_(display, 8), backgroundView_(display, 8), display_(display), msg_(msg) {}
    ~MsgPopup() override = default;

    void render(DisplaySystem &display) override {

        display_.setFont(display_.style.getFontFamily().popupFont);

        uint16_t width = display_.getTextWidth(ASK);
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
        Rect backgroundRect = {
            .origin = {.x = rect.origin.x - 5, .y = rect.origin.y - 5},
            .size = {.width = rect.size.width + 10, .height = rect.size.height + 10}};

        backgroundView_.fillColor = display_.bg;
        backgroundView_.strokeColor = display_.bg;

        backgroundView_.drawInRect(backgroundRect);

        boxView_.drawInRect(rect);

        uint16_t y = (display_.height() >> 1) - (height >> 1) + 5;
        for (auto &s : msg_) {
            uint16_t x = (display_.width() >> 1) - (display_.getTextWidth(s) >> 1);
            display_.drawSingleLineOfText(s, x, y);
            y += display_.getTextHeight();
        }
        y += display_.getTextHeight() >> 1;

        display_.drawSingleLineOfText(
            ASK, (display_.width() >> 1) - (display_.getTextWidth(ASK) >> 1), y);

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
