#pragma once

#include <algorithm>
#include <utility>

#include "EinkView.hpp"
#include "Misc/Style.hpp"
#include "UI/Fonts/TextTruncation.hpp"

// A view that draws a line of text
class SingleLineTextView : public EinkView {
public:
    SingleLineTextView(DisplaySystem &display) : EinkView(display) {}

    //! Convenience method to initialize and then set text and font
    SingleLineTextView(DisplaySystem &display, std::string text,
                       Font *font = static_cast<Font *>(&fontFace0), bool allCaps = false)
        : SingleLineTextView(display) {
        setFont(font);
        setText(std::move(text), allCaps);
    }

    ~SingleLineTextView() override = default;

    void setText(std::string text, bool allCaps = false) {
        if (allCaps) {
            std::transform(text.begin(), text.end(), text.begin(), ::toupper);
        }
        if (text != text_) {
            text_ = std::move(text);
            setNeedsLayout();
        }
    }

    void setTopPadding(uint8_t topPaddingValue) { topPadding = topPaddingValue; }

    auto getText() -> const std::string & { return text_; }

    void setFont(Font *font) { font_ = font; }

    auto layout(LayoutContext &context, Size size) -> Size override {
        uint16_t w, h;
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        Font *oldFont = display_.getFont();
        display_.setFont(font_);

        // Hard cap the size of the text to the no more than 1/3 the number of pixels (ie assume the
        // average glyph + space width is at least 3px wide)
        // TODO: It would be nice if the font told us its minimum glyph size
        if (text_.size() > size.width / 3) {
            text_ = Utf8Substr(text_, 0, std::min(size.width / 3, static_cast<int>(text_.size())));
        }

        display_.getTextSize(text_, &w, &h);

        // If we don't have the option to grow horizontally as much as we want,
        // (which, honestly, we should probably never give a text view, right?)
        // we need to do this math and shrink the string until it fits.
        if (w > size.width && size.width > 0) {
            text_ = TruncateStringToFit(display_, font_, text_, size.width, w);
        }
        display_.setFont(oldFont);

        return {w, font_->lineHeight()};
    }

    void drawInRect(Rect rect) override {
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        Font *oldFont = display_.getFont();
        display_.setFont(font_);
        display_.drawSingleLineOfText(text_, rect.origin.x, rect.origin.y);
        display_.setFont(oldFont);
    }

    uint8_t topPadding = 0;

protected:
    std::string text_;
    Font *font_{};
};
