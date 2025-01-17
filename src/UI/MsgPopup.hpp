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

    void startTimer(int duration = 2000) {
        // Must stop the timer because ChangePeriod starts it. This prevents an issue where
        // someone starts the timer multiple times and then there's an extra trigger of the Timer
        // that another screen gets.
        xTimerStop(DisplaySystem::animationTimer, 0);
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(duration), 0);
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        auto textRect = context.setViewFrame(textView_, SolRect::fromSize(size),
                                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

        context.setViewFrame(boxView_, textRect.inset(-10));

        return size;
    }

    void drawInRect(SolRect rect) override {
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
        // Stop the timer in case we get here through button press.
        // Technically this does a stop even if it's not running (in the other case), but that's ok.
        xTimerStop(DisplaySystem::animationTimer, 0);
        back();
    }
};