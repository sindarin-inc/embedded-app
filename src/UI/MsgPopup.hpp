#pragma once

#include <array>
#include <utility>
#include <vector>

#include "Components/BoxView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Screen.hpp"

class MsgPopup : public Screen, public EinkView {

private:
    BoxView boxView_;
    MultiLineTextView textView_;

public:
    MsgPopup(DisplaySystem &display, const std::string &msg)
        : EinkView(display), boxView_(display, 12), textView_(display, 12) {
        textView_.setText(msg);
        textView_.alignCenter = true;
        textView_.setFont(static_cast<Font *>(&fontFace0));
    }
    ~MsgPopup() override = default;

    void startTimer(int duration_ = 2000) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(duration_), 0);
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        auto textRect = context.setViewFrame(textView_, Rect::fromSize(size),
                                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

        context.setViewFrame(boxView_, textRect.inset(-10));

        return size;
    }

    void drawInRect(Rect rect) override {
        boxView_.draw();
        textView_.draw();
    }

    void render(DisplaySystem &display) override {
        setNeedsLayout();
        setFrame(defaultFrameRect().inset(-5));

        draw();
        display.updateDisplay();
    }

    void processCommand(RemoteCommand command) override {
        switch (command) {
        case RemoteCommand::ENTER:
        case RemoteCommand::ANIMATION:
        case RemoteCommand::BACK:
        case RemoteCommand::LEFT:
        case RemoteCommand::RIGHT:
        case RemoteCommand::UP:
        case RemoteCommand::DOWN:
            back();
            break;
        default:
            break;
        }
    }
};
