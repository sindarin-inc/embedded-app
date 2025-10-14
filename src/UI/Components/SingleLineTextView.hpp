#pragma once

#include <algorithm>
#include <utility>

#include "EinkView.hpp"
#include "UI/Fonts.hpp"
#include "UI/TextTruncation.hpp"

// A view that draws a line of text
class SingleLineTextView : public EinkView {
public:
    SingleLineTextView(DisplaySystem &display)
        : EinkView(display), font_(static_cast<Font *>(&fontFace0)) {}

    //! Convenience method to initialize and then set text and font
    SingleLineTextView(DisplaySystem &display, std::string text,
                       Font *font = static_cast<Font *>(&fontFace0), bool allCaps = false)
        : SingleLineTextView(display) {
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

    auto getTextHeight() -> uint16_t {
        uint16_t w, h;
        display_.getTextSize(text_, &w, &h, font_);
        return h;
    }

    void setTopPadding(uint8_t topPaddingValue) { topPadding = topPaddingValue; }

    auto getText() -> const std::string & { return text_; }

    void setFont(Font *font) { font_ = font; }

    auto getFont() -> const Font * { return font_; }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        uint16_t w, h;
        // Hard cap the size of the text to the no more than 1/3 the number of pixels (ie assume the
        // average glyph + space width is at least 3px wide)
        // TODO: It would be nice if the font told us its minimum glyph size
        if (text_.size() > size.width / 3) {
            text_ = Utf8Substr(text_, 0, std::min(size.width / 3, static_cast<int>(text_.size())));
        }

        display_.getTextSize(text_, &w, &h, font_);
        // TODO: Ask Guy to fix this in the renderer
        // Any number that starts with a 1 has 2 extra pixels on the right side as
        // rendered by the font.
        if (text_[0] == '1') {
            w -= 2;
        }

        // If we don't have the option to grow horizontally as much as we want,
        // (which, honestly, we should probably never give a text view, right?)
        // we need to do this math and shrink the string until it fits.
        if (w > size.width && size.width > 0) {
            text_ = TruncateStringToFit(display_, font_, text_, size.width, w);
        }

        return {w, font_->lineHeight()};
    }

    void drawInRect(SolRect rect) override {
        display_.drawSingleLineOfText(text_, rect.origin.x, rect.origin.y, false, font_);
    }

    uint8_t topPadding = 0;

protected:
    std::string text_;
    Font *font_;
};
