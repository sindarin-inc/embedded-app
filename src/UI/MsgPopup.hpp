#pragma once

#include <array>
#include <utility>
#include <vector>

#include "Components/BoxView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Screen.hpp"

class MsgPopup : public Screen, public EinkView {

private:
    DisplaySystem &display_;
    BoxView boxView_;
    MultiLineTextView textView_;

public:
    MsgPopup(DisplaySystem &display, const std::string &msg)
        : EinkView(display), display_(display), boxView_(display, 8), textView_(display, 8) {
        textView_.setText(msg);
        textView_.alignCenter = true;
        textView_.setFont(display_.style.getFontFamily().popupFont);
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
        int adjustedXPadding = display.defaultXPadding - 5;
        int adjustedYPadding = display.defaultYPadding - 5;
        setFrame({{display.xAbsolute(adjustedXPadding), display.yAbsolute(adjustedYPadding)},
                  {display.defaultScreenWidth - 2 * adjustedXPadding,
                   display.defaultScreenHeight - 2 * adjustedYPadding}});

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
