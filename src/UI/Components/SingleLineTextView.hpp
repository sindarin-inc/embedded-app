#pragma once

#include "EinkView.hpp"

// A view that draws a line of text
class SingleLineTextView : public EinkView {
public:
    SingleLineTextView(DisplaySystem &display, std::string text)
        : EinkView(display), text_(std::move(text)) {}
    ~SingleLineTextView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        uint16_t w, h;
        display_.getTextBounds(text_.c_str(), &w, &h);
        return {w, display_.getFont()->yAdvance};
    }

    void drawInRect(Rect rect) override {
        display_.drawSingleLineOfText(text_, rect.origin.x, rect.origin.y);
    }

protected:
    std::string text_;
};