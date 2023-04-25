#pragma once

#include <utility>

#include "EinkView.hpp"
#include "Misc/Style.hpp"

// A view that draws a line of text
class SingleLineTextView : public EinkView {
public:
    SingleLineTextView(DisplaySystem &display, std::string text,
                       const GFXfont *font = &TAHOMA_REGULAR_FONT5PT7B, bool allCaps = false)
        : EinkView(display), font_(font) {
        setText(std::move(text), allCaps);
    }
    ~SingleLineTextView() override = default;

    void setText(std::string text, bool allCaps = false) {
        if (allCaps) {
            std::string subCopy = text;
            std::transform(subCopy.begin(), subCopy.end(), subCopy.begin(), ::toupper);
            text = std::move(subCopy);
        }
        text_ = std::move(text);
        setNeedsLayout();
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        uint16_t w, h;
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        const GFXfont *oldFont = display_.getFont();
        display_.setFont(font_);
        display_.getTextBounds(text_.c_str(), &w, &h);

        //
        // If we don't have the option to grow horizontally as much as we want,
        // (which, honestly, we should probably never give a text view, right?)
        // we need to do this math and shrink the string until it fits.
        //
        while (w > size.width && size.width > 0) {
            if (text_.length() > 4) {
                text_ = text_.substr(0, text_.length() - 4) + "...";
            } else {
                // Practically, this shouldn't ever happen, but I'm doing
                // the bounds check here just in case we do hit it some day.
                text_ = "too big!";
                break;
            }
            display_.getTextBounds(text_.c_str(), &w, &h);
        }
        display_.setFont(oldFont);
        return {w, font_->yAdvance};
    }

    void drawInRect(Rect rect) override {
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        const GFXfont *oldFont = display_.getFont();
        display_.setFont(font_);
        display_.drawSingleLineOfText(text_, rect.origin.x, rect.origin.y);
        display_.setFont(oldFont);
    }

protected:
    std::string text_;
    const GFXfont *font_;
};