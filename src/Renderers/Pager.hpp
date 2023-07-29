#pragma once

#include <array>

#include "RendererDefs.hpp"
#include "Tokens.hpp"

class Pager {
private:
    Tokens &tokens_;
    DisplaySystem &display_;

    uint16_t wordCount_{0};
    uint16_t currentPageDecilineHeight_{0};
    uint16_t currentLineWidth_{0};
    uint16_t lineCount_{0};
    Idx firstLineIdx_{0};
    Lines lines_{};
    uint16_t maxDeciLines_{0};
    uint16_t maxLineWidth_{0};
    StreamPlace nextPagePlace_;
    bool atEOF_{false};

    [[nodiscard]] inline auto lastLineTokenIdx(Idx lineIdx) const -> Idx {
        return lines_[lineIdx].tokenIdx + lines_[lineIdx].tokenCount - 1;
    }

public:
    Pager(Tokens &tokens, DisplaySystem &display) : tokens_(tokens), display_(display) {}

    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    [[nodiscard]] auto getMaxDeciLines() -> int16_t {
        return (display_.height() * NEWLINE_DECI_LINE_HEIGHT) / (this->display_.getTextHeight());
    }

    inline void clear() {
        atEOF_ = false;
        wordCount_ = 0;
        lineCount_ = 0;
        firstLineIdx_ = 0;
        currentLineWidth_ = 0;
        currentPageDecilineHeight_ = 0;
        maxDeciLines_ = getMaxDeciLines();
        maxLineWidth_ = display_.width();
        lines_[0].tokenCount = 0;
    }

    // Returns false if the last token is not part of the page.
    // The token will be the first to appear on next page.
    [[nodiscard]] auto addLastTokenToPage(bool dontStop = false) -> bool;
    [[nodiscard]] auto computeWordCount() const -> uint16_t;

    [[nodiscard]] inline auto getWordCount() const -> uint16_t {
        return firstLineIdx_ == 0 ? wordCount_ : computeWordCount();
    }
    [[nodiscard]] inline auto getLineCount() const -> uint16_t {
        return lineCount_ - firstLineIdx_;
    }
    [[nodiscard]] inline auto getLineLength(Idx lineIdx) const -> uint16_t {
        return lines_[firstLineIdx_ + lineIdx].length;
    }
    [[nodiscard]] auto getPageLength() const -> uint16_t;

    [[nodiscard]] inline auto getNextPageStreamPlace() const -> StreamPlace {
        return atEOF_ ? StreamPlace{.offset = 0, .fileNumber = 0} : nextPagePlace_;
    }

    inline void setNextPagePlace(StreamPlace place) { nextPagePlace_ = place; }
    void setFirstLineIdxBackward();

    [[nodiscard]] inline auto getStreamPlaceOfFirstLine() -> StreamPlace {
        return tokens_[lines_[firstLineIdx_].tokenIdx].streamPlace;
    }

    [[nodiscard]] inline auto getFirstLineIdx() const -> Idx { return firstLineIdx_; }

    void getLine(Idx lineNbr, Line &line) const;
    void displayPage(int16_t numSkipLines);

    void endOfFileReached() {
        atEOF_ = true;
        if (lines_[lineCount_].tokenCount != 0) {
            lineCount_++;
        }
    }
    void completeLastLine();

    void show() const;
};