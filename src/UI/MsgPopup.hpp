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
    static void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                     int duration = 2000, bool rerenderAfter = false,
                                     const std::shared_ptr<Screen> &screen = nullptr) {
        auto popup = std::make_shared<MsgPopup>(display, message);
        popup->render(display);
        if (rerenderAfter) {
            Delay(duration);
            screen->setNeedsRerender();
            screen->render(display);
        }
    }

    MsgPopup(DisplaySystem &display, const std::string &msg)
        : EinkView(display), boxView_(display, 12), textView_(display, 12) {
        textView_.setText(msg);
        textView_.alignCenter = true;
        textView_.setFont(static_cast<Font *>(&fontFace0));
    }
    ~MsgPopup() override = default;

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
            back();
            break;
        default:
            break;
        }
    }
};
