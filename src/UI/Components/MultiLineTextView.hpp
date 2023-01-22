#pragma once

#include <string>
#include <vector>

#include "EinkView.hpp"
#include "Misc/Style.hpp"
#include "SingleLineTextView.hpp"

class TextSplitter {
public:
    TextSplitter(DisplaySystem &display, int width) : display_(display), width_(width) {}

    auto split(const std::string &text) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string line;
        std::string word;
        for (char c : text) {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '-') {
                if (display_.getTextWidth(line.c_str()) + display_.getTextWidth(word.c_str()) >
                    width_) {
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
            if (display_.getTextWidth(line.c_str()) + display_.getTextWidth(word.c_str()) >
                width_) {
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

private:
    DisplaySystem &display_;
    int width_;
};

// A view that draws multiple lines of text
class MultiLineTextView : public EinkView {
public:
    MultiLineTextView(DisplaySystem &display, std::string text, int maxLines, int spacingOffset = 0,
                      const GFXfont *font = &TAHOMA_REGULAR_FONT5PT7B, bool allCaps = false)
        : EinkView(display), text_(std::move(text)), maxLines_(maxLines),
          spacingOffset_(spacingOffset), font_(font) {
        if (allCaps) {
            std::string subCopy = text_;
            std::transform(subCopy.begin(), subCopy.end(), subCopy.begin(), ::toupper);
            text_ = std::move(subCopy);
        }
    }
    ~MultiLineTextView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        uint16_t w, h;
        TextSplitter splitter(display_, size.width);

        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        const GFXfont *oldFont = display_.getFont();
        display_.setFont(font_);
        display_.getTextBounds(text_.c_str(), &w, &h);

        splitLines_ = splitter.split(text_);
        int i;

        for (i = 0; i < maxLines_ && i < splitLines_.size(); i++) {
            context.setViewFrame(*new SingleLineTextView(display_, splitLines_[i], font_),
                                 {{0, i * (display_.getFont()->yAdvance + spacingOffset_)}, size},
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
        }

        display_.setFont(oldFont);
        return {size.width, i * (display_.getFont()->yAdvance + spacingOffset_) - spacingOffset_};
    }

    void drawInRect(Rect rect) override {
        // Leave the display font the way we found it.
        // Probably not ideal, but for now it'll have to do.
        const GFXfont *oldFont = display_.getFont();
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

            display_.drawSingleLineOfText(splitLines_[i], rect.origin.x,
                                          rect.origin.y +
                                              i * (display_.getFont()->yAdvance + spacingOffset_));
        }
        display_.setFont(oldFont);
    }

protected:
    std::string text_;
    int maxLines_;
    int spacingOffset_;
    std::vector<std::string> splitLines_;
    const GFXfont *font_;
};