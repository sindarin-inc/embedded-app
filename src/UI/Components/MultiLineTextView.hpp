#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "EinkView.hpp"
#include "SingleLineTextView.hpp"
#include "UI/Fonts/Fonts.hpp"

// A view that draws multiple lines of text
class MultiLineTextView : public EinkView {
public:
    MultiLineTextView(DisplaySystem &display, int maxLines, int spacingOffset = 0,
                      bool allCaps = false)
        : EinkView(display), maxLines_(maxLines), spacingOffset_(spacingOffset),
          font_(static_cast<Font *>(&fontFace0)), allCaps_(allCaps){};

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

    // Accessor methods for font and text
    auto getFont() const -> Font * { return font_; }
    auto getText() const -> const std::string & { return text_; }

    void splitMultiLine(DisplaySystem &display, int width, const std::string &text) {
        if (cachedWidth_ == width) {
            return;
        }
        // TODO: It would be better to reuse the logic from Pager.cpp
        splitLines_.clear();
        std::string line;
        std::string word;
        for (int i = 0; i < text.size();) {
            char c = text[i];

            unsigned int charLen = 1;
            if ((c & 0xF0) == 0xF0) {
                charLen = 4;
            } else if ((c & 0xE0) == 0xE0) {
                charLen = 3;
            } else if ((c & 0xC0) == 0xC0) {
                charLen = 2;
            }

            if (i + charLen > text.size()) {
                log_e("Invalid UTF-8 string: %s", text.c_str());
                break;
            }

            std::string_view currentChar(&text[i], charLen);

            if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '-' ||
                i + charLen == text.size()) {
                if (display.getTextWidth(line + word, font_) > width) {
                    splitLines_.push_back(line);
                    line = "";
                } else if (c == '\n') {
                    splitLines_.push_back(line + word);
                    line = "";
                    word = "";
                }
                line.append(word);
                if (c != '\n') {
                    line.append(currentChar);
                }
                word = "";
            } else {
                word.append(currentChar);
            }
            // Allow going over the max lines by one, so we can add the ... to the end
            if (splitLines_.size() > maxLines_) {
                break;
            }
            i += charLen;
        }

        if (!line.empty()) {
            splitLines_.push_back(line);
        }
        cachedWidth_ = width;
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override {
        splitMultiLine(display_, size.width, text_);

        lineWidths_.clear();
        if (alignCenter || dynamicWidth) {
            for (auto &line : splitLines_) {
                int lineWidth = display_.getTextWidth(line, font_);
                lineWidths_.push_back(lineWidth);
            }
        }
        lineXOffsets_.clear();

        int width = size.width;
        if (dynamicWidth) {
            width = *std::max_element(lineWidths_.begin(), lineWidths_.end());
        }
        if (alignCenter) {
            for (auto lineWidth : lineWidths_) {
                int offset = (width - lineWidth) / 2;
                lineXOffsets_.push_back(offset);
            }
        } else {
            for (int i = 0; i < splitLines_.size(); i++) {
                lineXOffsets_.push_back(0);
            }
        }

        int lineCount = std::min(static_cast<size_t>(maxLines_), splitLines_.size());
        int height = lineCount * font_->lineHeight() + (lineCount - 1) * spacingOffset_;

        return {width, height};
    }

    void drawInRect(SolRect rect) override {
        if (splitLines_.size() > maxLines_) {
            // Determine which displayed line should receive the ellipses: normally the last
            // displayed line, but if that line is blank whitespace, move up one line.
            size_t targetIndex = maxLines_ - 1;
            if (targetIndex < splitLines_.size()) {
                const std::string &candidate = splitLines_[targetIndex];
                bool isBlank = true;
                for (char ch : candidate) {
                    if (ch != ' ' && ch != '\t') {
                        isBlank = false;
                        break;
                    }
                }
                if (isBlank && targetIndex > 0) {
                    targetIndex -= 1;
                }
            }

            if (targetIndex < splitLines_.size()) {
                std::string &lineRef = splitLines_[targetIndex];
                // Prevent repeated ellipses on multiple draws
                if (!(lineRef.size() >= 3 && lineRef.substr(lineRef.size() - 3) == "...")) {
                    // Trim trailing whitespace/hyphen/period before appending ellipses
                    while (!lineRef.empty() && (lineRef.back() == ' ' || lineRef.back() == '\t' ||
                                                lineRef.back() == '-' || lineRef.back() == '.')) {
                        lineRef.pop_back();
                    }
                    lineRef += "...";
                }
            }
            log_i("splitLines_.size() > maxLines_: %zu > %zu", splitLines_.size(), maxLines_);
            for (auto &line : splitLines_) {
                log_i("Line: %s", line.c_str());
            }
        }
        for (int i = 0; i < maxLines_ && i < splitLines_.size(); i++) {
            // The plus 1 in the y is because we know what drawSingleLineOfText will do. It's
            // not ideal, but it works.
            display_.drawSingleLineOfText(
                splitLines_[i], rect.origin.x + lineXOffsets_[i],
                rect.origin.y + i * (font_->lineHeight() + spacingOffset_) - 1, false, font_);
        }
    }

    bool dynamicWidth = false;
    bool alignCenter = false;

protected:
    std::string text_;
    size_t maxLines_;
    int spacingOffset_;
    std::vector<std::string> splitLines_;
    std::vector<int> lineXOffsets_;
    std::vector<int> lineWidths_;
    Font *font_;
    bool allCaps_;
    int cachedWidth_ = 0;
};
