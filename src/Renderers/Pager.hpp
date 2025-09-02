#pragma once

#include <array>
#include <memory>
#include <utility>

#include "EPub/EPubFile.hpp"
#include "Misc/BMPImage.hpp"
#include "RendererDefs.hpp"
#include "Tokens.hpp"
#include "config.h"

// From the token stream, generate pages content, separating tokens into lines and pages that fit
// the current display region. The last page retrieved is found in the lines_[] circular array.
class Pager {
private:
    Tokens &tokens_;
    DisplaySystem &display_;
    DisplayRect rect_;
    std::shared_ptr<EPubFile> stream_;

    uint16_t wordCount_{0};
    uint16_t currentPageDecilineHeight_{0};
    uint16_t currentLineWidth_{0};
    Idx firstPageLineIdx_{0};

    Lines lines_{};
    Idx linesHeadIdx_{0};
    Idx linesTailIdx_{0};

    uint16_t leftIndent_{0};
    uint16_t rightIndent_{0};

    uint16_t maxDeciLines_{0};
    uint16_t maxLineWidth_{0};
    StreamPlace nextPagePlace_;
    bool atEOF_{false};

    uint16_t spaceWidth_{0};

    MenuParameters menuParameters_;

    int lineSpacing_{0};

    Font *font_{nullptr};

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

#if CONFIG_FONT_TTF
    auto getLineSegment(LineInfo &lineInfo, Idx tokenIdx, Idx endTokenIdx, Line &line) const -> Idx;
    void drawSingleLine(LineInfo &lineInfo, int yPos) const;
#else
    void getLine(LineInfo &lineStruct, Line &line) const;
#endif

    auto computeDarkPercent(const uint8_t *bitmap, uint16_t width, uint16_t height) const
        -> uint8_t;
    void newLine(uint16_t height);

public:
    Pager(Tokens &tokens, DisplaySystem &display, DisplayRect &rect)
        : tokens_(tokens), display_(display), rect_(rect) {}
    Pager(const Pager &) = delete;

    void setStream(const std::shared_ptr<RendererStream> &stream) {
        stream_ = std::static_pointer_cast<EPubFile>(stream);
    }

    [[nodiscard]] auto getLineSpacing() const -> int { return lineSpacing_; }
    void setLineSpacing(int newSpacing) { lineSpacing_ = newSpacing; }

    void setFont(Font *font) { font_ = font; }

    [[nodiscard]] auto getLineHeightPlusSpacing() const -> int {
        return font_->lineHeight() + lineSpacing_;
    }
    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    [[nodiscard]] auto getMaxDeciLines() const -> int16_t {
        return (rect_.height * NEWLINE_DECI_LINE_HEIGHT) / (getLineHeightPlusSpacing());
    }

    inline void clear() {

        leftIndent_ = 0;
        rightIndent_ = 0;
        atEOF_ = false;
        wordCount_ = 0;
        linesHeadIdx_ = linesTailIdx_ = 0;
        firstPageLineIdx_ = 0;
        currentLineWidth_ = 0;
        currentPageDecilineHeight_ = 0;
        maxDeciLines_ = getMaxDeciLines();
        maxLineWidth_ = rect_.width;
        lines_[0] = {0, 0, 0, 0, 0};
        uint16_t notUsed;
        display_.getTextSize(" ", &spaceWidth_, &notUsed);
        menuParameters_.clear();
    }

    inline void setLineIndent(uint16_t leftIndent = 0, uint16_t rightIndent = 0) {
        leftIndent_ = leftIndent;
        lines_[linesTailIdx_].leftIndent = leftIndent_;
        currentLineWidth_ = leftIndent_;

        rightIndent_ = rightIndent;
        maxLineWidth_ = rect_.width - rightIndent;

        // log_w("Set Line Indent: %" PRIu16 " - %" PRIu16, leftIndent_, rightIndent_);
    }

    inline auto getLineIndent() -> std::pair<uint16_t, uint16_t> {
        return {leftIndent_, rightIndent_};
    }

    // Returns false if the last token is not part of the page.
    // The token will be the first to appear on next page.
    [[nodiscard]] auto addLastTokenToPage(bool dontStop = false) -> bool;
    [[nodiscard]] auto computeWordCount() const -> uint16_t;

    [[nodiscard]] inline auto linesIsFull() const -> bool {
        return getLineCount() > (LINES_SIZE - 2);
    }
    [[nodiscard]] inline auto linesIsEmpty() const -> bool {
        return linesHeadIdx_ == linesTailIdx_;
    }

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
        log_d("getNextPageStreamPlace: (%s) next: %s", atEOF_ ? "at EOF" : "not at eof",
              nextPagePlace_.str().c_str());
        return nextPagePlace_;
    }

    inline void setNextPagePlace(StreamPlace place) { nextPagePlace_ = place; }

    void setFirstLineIdxBackward();

    [[nodiscard]] inline auto getStreamPlaceOfFirstLine() -> StreamPlace {
        return tokens_[lines_[firstPageLineIdx_].tokenIdx].streamPlace;
    }

    [[nodiscard]] inline auto getFirstLineIdx() const -> Idx { return firstPageLineIdx_; }

    auto displayPage(int16_t numSkipLines) -> uint8_t;

    void setEndOfFile() {
        atEOF_ = true;
        if (lines_[linesTailIdx_].tokenCount != 0) {
            incrLinesTailIdx();
        }
    }

    [[nodiscard]] auto atEndOfFile() const -> bool { return atEOF_; }

    void completeLastLine();

    inline void freeLineHead() {
        if (linesHeadIdx_ != linesTailIdx_) {

            // REND_WLOG("Freeing line at idx %" PRIu16, linesHeadIdx_);

            tokens_.freeTokens(lines_[linesHeadIdx_].tokenIdx, lines_[linesHeadIdx_].tokenCount);

            incrLinesHeadIdx();
            if constexpr (REND_DEBUG) {
                if (linesHeadIdx_ == linesTailIdx_) {
                    REND_ELOG("lines_[] is empty!!");
                }
            }
        }
    }

    inline void addSpaceWidth() { currentLineWidth_ += spaceWidth_; }

    [[nodiscard]] inline auto deciToPix(uint16_t decilineValue) const -> uint16_t {
        return (getLineHeightPlusSpacing() * decilineValue) / NEWLINE_DECI_LINE_HEIGHT;
    }

    [[nodiscard]] inline auto pixToDeci(uint16_t pixValue) const -> uint16_t {
        return ((NEWLINE_DECI_LINE_HEIGHT * pixValue)) / getLineHeightPlusSpacing();
    }

    void addMenuParameter(const std::string &key, const std::string &value);
    [[nodiscard]] auto getMenuParameters() const -> const MenuParameters &;

    void show() const;

    void setRect(DisplayRect rect) { rect_ = rect; }

#if REND_DEBUGGING
    auto getTokenIdStr(TokenId id) const -> const char * {
        for (int idx = 0; idx < 16; idx++) {
            if ((uint16_t)id & (1 << idx)) {
                return TokenIdStr[idx];
            }
        }
        return "UNKNOWN";
    }
#endif
};
