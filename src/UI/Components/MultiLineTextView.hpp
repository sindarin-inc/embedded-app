#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "EinkView.hpp"
#include "Misc/Style.hpp"
#include "SingleLineTextView.hpp"
#include "UI/Fonts/Fonts.hpp"

// A view that draws multiple lines of text
class MultiLineTextView : public EinkView {
public:
    MultiLineTextView(DisplaySystem &display, int maxLines, int spacingOffset = 0,
                      bool allCaps = false)
        : EinkView(display), maxLines_(maxLines), spacingOffset_(spacingOffset),
          font_(static_cast<Font *>(&ibmf75Face0)), allCaps_(allCaps){};

    ~MultiLineTextView() override = default;

    void setText(std::string text) {
        if (allCaps_) {
            std::transform(text.begin(), text.end(), text.begin(), ::toupper);
        }
        if (text == text_) {
            return;
        }
        text_ = std::move(text);
        setNeedsLayout();
        cachedWidth_ = 0;
    }

    void setFont(Font *font) { font_ = font; }

    void splitMultiLine(DisplaySystem &display, int width, const std::string &text) {
        if (cachedWidth_ == width) {
            return;
        }
        splitLines_.clear();
        std::string line;
        std::string word;
        for (int i = 0; i < text.size(); i++) {
            char c = text[i];
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '-' ||
                i == text.size() - 1) {
                if (display.getTextWidth(line + word) > width) {
                    splitLines_.push_back(line);
                    line = "";
                }
                line += word + c;
                word = "";
            } else {
                word += c;
            }
            // Allow going over the max lines by one, so we can add the ... to the end
            if (splitLines_.size() > maxLines_) {
                break;
            }
        }

        if (!line.empty()) {
            splitLines_.push_back(line);
        }
        cachedWidth_ = width;
    }
    auto layout(LayoutContext &context, Size size) -> Size override {
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        Font *oldFont = display_.getFont();
        display_.setFont(font_);

        splitMultiLine(display_, size.width, text_);

        int lineCount = std::min((size_t)maxLines_, splitLines_.size());
        int height = lineCount * font_->lineHeight() + (lineCount - 1) * spacingOffset_;

        display_.setFont(oldFont);
        return {size.width, height};
    }

    void drawInRect(Rect rect) override {
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        Font *oldFont = display_.getFont();
        display_.setFont(font_);
        if (splitLines_.size() > maxLines_) {
            int i = 1;
            while ((splitLines_[maxLines_ - 1][splitLines_[maxLines_ - 1].size() - i] != ' ') &&
                   (splitLines_[maxLines_ - 1][splitLines_[maxLines_ - 1].size() - i] != '\t') &&
                   (splitLines_[maxLines_ - 1][splitLines_[maxLines_ - 1].size() - i] != '-')) {
                i++;
            }
            if (i < splitLines_.size() - 1) {
                splitLines_[maxLines_ - 1] = Utf8Substr(splitLines_[maxLines_ - 1], 0,
                                                        splitLines_[maxLines_ - 1].size() - i) +
                                             "...";
            } else {
                log_i("Didn't find a good delimeter for the ... addition");
            }
        }
        for (int i = 0; i < maxLines_ && i < splitLines_.size(); i++) {
            // The plus 1 in the y is because we know what drawSingleLineOfText will do. It's
            // not ideal, but it works.
            display_.drawSingleLineOfText(splitLines_[i], rect.origin.x,
                                          rect.origin.y +
                                              i * (font_->lineHeight() + spacingOffset_) - 1);
        }
        display_.setFont(oldFont);
    }

protected:
    std::string text_;
    int maxLines_;
    int spacingOffset_;
    std::vector<std::string> splitLines_;
    Font *font_;
    bool allCaps_;
    int cachedWidth_ = 0;
};