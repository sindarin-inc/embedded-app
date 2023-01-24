#pragma once

#include "EinkView.hpp"
#include "Misc/Style.hpp"

// A view that draws a line of text
class SingleLineTextView : public EinkView {
public:
    SingleLineTextView(DisplaySystem &display, std::string text,
                       const GFXfont *font = &TAHOMA_REGULAR_FONT5PT7B, bool allCaps = false)
        : EinkView(display), text_(std::move(text)), font_(font) {
        if (allCaps) {
            std::string subCopy = text_;
            std::transform(subCopy.begin(), subCopy.end(), subCopy.begin(), ::toupper);
            text_ = std::move(subCopy);
        }
    }
    ~SingleLineTextView() override = default;

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
        return {w, display_.getFont()->yAdvance};
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