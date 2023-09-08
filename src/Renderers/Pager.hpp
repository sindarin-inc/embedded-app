#pragma once

#include <array>

#include "Misc/BMPImage.hpp"
#include "RendererDefs.hpp"
#include "Tokens.hpp"

class Pager {
private:
    Tokens &tokens_;
    DisplaySystem &display_;

    uint16_t wordCount_{0};
    uint16_t currentPageDecilineHeight_{0};
    uint16_t currentLineWidth_{0};
    Idx firstPageLineIdx_{0};

    Lines lines_{};
    Idx linesHeadIdx_{0};
    Idx linesTailIdx_{0};

    uint16_t maxDeciLines_{0};
    uint16_t maxLineWidth_{0};
    StreamPlace nextPagePlace_;
    bool atEOF_{false};

    uint16_t spaceWidth_{0};

    std::shared_ptr<BMPImage> picture_;

    [[nodiscard]] inline auto lastLineTokenIdx(Idx lineIdx) const -> Idx {
        return lines_[lineIdx].tokenIdx + lines_[lineIdx].tokenCount - 1;
    }

    [[nodiscard]] inline auto getLineCount() const -> uint16_t {
        return (linesHeadIdx_ <= linesTailIdx_) ? (linesTailIdx_ - linesHeadIdx_)
                                                : (linesTailIdx_ + (LINES_SIZE - linesHeadIdx_));
    }

    [[nodiscard]] inline auto getLastLineIdx() const -> Idx {
        return (linesTailIdx_ == 0) ? (LINES_SIZE - 1) : (linesTailIdx_ - 1);
    }

    inline void nextLineIdx(Idx &idx) const { idx = (idx + 1) % LINES_SIZE; }
    inline void prevLineIdx(Idx &idx) const { idx = (idx == 0) ? (LINES_SIZE - 1) : (idx - 1); }

    inline void incrLinesHeadIdx() { linesHeadIdx_ = (linesHeadIdx_ + 1) % LINES_SIZE; }
    inline void incrLinesTailIdx() {
        if (linesIsFull()) {
            freeLineHead();
        }
        linesTailIdx_ = (linesTailIdx_ + 1) % LINES_SIZE;
    }

public:
    Pager(Tokens &tokens, DisplaySystem &display)
        : tokens_(tokens), display_(display), picture_(nullptr) {}

    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    [[nodiscard]] auto getMaxDeciLines() -> int16_t {
        return (display_.height() * NEWLINE_DECI_LINE_HEIGHT) / (this->display_.getTextHeight());
    }

    inline void clear() {
        atEOF_ = false;
        wordCount_ = 0;
        linesHeadIdx_ = linesTailIdx_ = 0;
        firstPageLineIdx_ = 0;
        currentLineWidth_ = 0;
        currentPageDecilineHeight_ = 0;
        maxDeciLines_ = getMaxDeciLines();
        maxLineWidth_ = display_.width();
        lines_[0].tokenCount = 0;

        uint16_t notUsed;
        display_.getTextSize(" ", &spaceWidth_, &notUsed);
    }

    // Returns false if the last token is not part of the page.
    // The token will be the first to appear on next page.
    [[nodiscard]] auto addLastTokenToPage(bool dontStop = false) -> bool;
    [[nodiscard]] auto computeWordCount() const -> uint16_t;

    [[nodiscard]] auto linesIsFull() const -> bool { return getLineCount() > (LINES_SIZE - 2); }
    [[nodiscard]] auto linesIsEmpty() const -> bool { return linesHeadIdx_ == linesTailIdx_; }

    [[nodiscard]] inline auto getPageLineCount() const -> uint16_t {
        return (firstPageLineIdx_ < linesTailIdx_)
                   ? (linesTailIdx_ - firstPageLineIdx_)
                   : (linesTailIdx_ + (LINES_SIZE - firstPageLineIdx_));
    }

    [[nodiscard]] inline auto getWordCount() const -> uint16_t {
        return firstPageLineIdx_ == 0 ? wordCount_ : computeWordCount();
    }
    [[nodiscard]] inline auto getLineLength(Idx lineIdx) const -> uint16_t {
        return lines_[firstPageLineIdx_ + lineIdx].length;
    }
    [[nodiscard]] auto getPageLength() const -> uint16_t;

    [[nodiscard]] inline auto getNextPageStreamPlace() const -> StreamPlace {
        return atEOF_ ? StreamPlace() : nextPagePlace_;
    }

    inline void setNextPagePlace(StreamPlace place) { nextPagePlace_ = place; }

    void setFirstLineIdxBackward();

    [[nodiscard]] inline auto getStreamPlaceOfFirstLine() -> StreamPlace {
        return tokens_[lines_[firstPageLineIdx_].tokenIdx].streamPlace;
    }

    [[nodiscard]] inline auto getFirstLineIdx() const -> Idx { return firstPageLineIdx_; }

    void getLine(Idx lineNbr, Line &line) const;
    void displayPage(int16_t numSkipLines);

    void setEndOfFile() {
        atEOF_ = true;
        if (lines_[linesTailIdx_].tokenCount != 0) {
            incrLinesTailIdx();
        }
    }

    [[nodiscard]] auto atEndOfFile() const -> bool { return atEOF_; }

    void setPicture(std::shared_ptr<BMPImage> picture) { picture_ = picture; }
    void completeLastLine();

    inline void freeLineHead() {
        if (linesHeadIdx_ != linesTailIdx_) {

            tokens_.freeTokens(lines_[linesHeadIdx_].tokenIdx, lines_[linesHeadIdx_].tokenCount);

            incrLinesHeadIdx();
            if constexpr (RENDERER_DEBUG) {
                if (linesHeadIdx_ == linesTailIdx_) {
                    log_e("lines_[] is empty!!");
                    if constexpr (ON_ERROR_EXIT) {
                        exit(1);
                    }
                }
            }
        }
    }

    inline void addSpaceWidth() { currentLineWidth_ += spaceWidth_; }

    void show() const;
};