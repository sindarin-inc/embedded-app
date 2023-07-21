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
          allCaps_(allCaps){};

    ~MultiLineTextView() override = default;

    void setText(std::string text) {
        if (allCaps_) {
            std::transform(text.begin(), text.end(), text.begin(), ::toupper);
        }
        if (text != text_) {
            text_ = std::move(text);
            setNeedsLayout();
        }
    }

    void setFont(Font *font) { font_ = font; }

    auto splitMultiLine(DisplaySystem &display, int width, const std::string &text)
        -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string line;
        std::string word;
        for (char c : text) {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '-') {
                if (display.getTextWidth((line + word).c_str()) > width) {
                    lines.push_back(line);
                    line = "";
                }
                line += word + c;
                word = "";
            } else {
                word += c;
            }
        }

        // Account for the last word
        if (!word.empty()) {
            if (display.getTextWidth((line + word).c_str()) > width) {
                lines.push_back(line);
                line = "";
            }
            line += word;
        }

        if (!line.empty()) {
            lines.push_back(line);
        }
        return lines;
    }
    auto layout(LayoutContext &context, Size size) -> Size override {
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        Font *oldFont = display_.getFont();
        display_.setFont(font_);

        splitLines_ = splitMultiLine(display_, size.width, text_);

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
                splitLines_[maxLines_ - 1] =
                    splitLines_[maxLines_ - 1].substr(0, splitLines_[maxLines_ - 1].size() - i) +
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
};