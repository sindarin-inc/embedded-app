#pragma once

#include <array>

#include "RendererDefs.hpp"

using namespace renderer_defs;

class Tokens {
private:
    DisplaySystem &display_;

    TokensArray tokens_{};
    uint16_t tokensCount_{0};

    CStrPool cStrPool_{};
    uint16_t cStrPoolCount_{0};

    uint16_t spaceWidth_{0};
    uint16_t spaceHeight_{0};

    // Page related values
    uint32_t currentPageWordCount_{0};
    uint16_t currentPageDecilineHeight_{0};
    uint16_t currentLineWidth_{0};
    uint8_t lineCount_{0};
    Lines lines_{};
    uint16_t maxDeciLines_{0};
    uint16_t maxLineWidth_{0};
    StreamPlace nextPagePlace_;

public:
    Tokens(DisplaySystem &display) : display_(display) {}
    [[nodiscard]] inline auto operator[](renderer_defs::Idx idx) const -> const Token & {
        return tokens_[idx];
    }

    [[nodiscard]] inline auto getCount() const -> uint16_t { return tokensCount_; }
    [[nodiscard]] inline auto getLast() -> Token & {
        if constexpr (RENDERER_DEBUG) {
            if ((tokensCount_ == 0) || (tokensCount_ > MAX_TOKEN_COUNT)) {
                log_e("Invalid tokensCount_ value: %" PRIu16, tokensCount_);
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return tokens_[tokensCount_ - 1];
    }
    [[nodiscard]] inline auto getBeforeLast() -> Token & {
        if constexpr (RENDERER_DEBUG) {
            if ((tokensCount_ < 2) || (tokensCount_ > MAX_TOKEN_COUNT)) {
                log_e("Invalid tokensCount_ value: %" PRIu16, tokensCount_);
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return tokens_[tokensCount_ - 2];
    }

    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    [[nodiscard]] auto getMaxDeciLines() -> int16_t {
        return (display_.height() * NEWLINE_DECI_LINE_HEIGHT) / (this->display_.getTextHeight());
    }

    inline void forgetLastToken() {
        if (tokensCount_ > 0) {
            tokensCount_ -= 1;
        }
    }

    [[nodiscard]] inline auto cStrIsFull() const -> bool {
        return (cStrPoolCount_ >= (POOL_BUFFER_SIZE - 2));
    }

    [[nodiscard]] inline auto isFull() const -> bool {
        return ((cStrPoolCount_ >= (POOL_BUFFER_SIZE - 2)) ||
                (tokensCount_ >= (MAX_TOKEN_COUNT - 2)));
    }

    [[nodiscard]] auto isEmpty() const -> bool { return tokensCount_ == 0; }

    void setSpaceSize(Token &token);

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    void setGFXSpaceSize(Token &token, uint8_t nextChar);

    void add(Token &token, const char *str = nullptr, uint8_t strSize = 0);
    void addByte(Token &token, uint8_t b);

    inline void clear() {
        tokensCount_ = 0;
        cStrPoolCount_ = 0;

        currentPageWordCount_ = 0;
        lineCount_ = 0;
        currentLineWidth_ = 0;
        currentPageDecilineHeight_ = 0;
        maxDeciLines_ = getMaxDeciLines();
        maxLineWidth_ = (display_.width() - 4);
        lines_[0].tokenCount = 0;
    }

    // Returns false if the last token is not part of the
    // page. The token will be the first to appear on next
    // page.
    [[nodiscard]] auto addLastTokenToPage() -> bool;

    [[nodiscard]] auto getCurrentPageWordCount() const -> uint32_t { return currentPageWordCount_; }

    void print() const;
    void showPage() const;
};