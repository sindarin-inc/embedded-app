#pragma once

#include <array>

#include "RendererDefs.hpp"

using namespace renderer_defs;

class Tokens {
private:
    DisplaySystem &display_;

    TokensArray tokens_{};
    Idx tokensTailIdx_{0};
    Idx tokensHeadIdx_{0};

    CStrPool cStrPool_{};
    Idx cStrPoolTailIdx_{0};
    Idx cStrPoolHeadIdx_{0};

    uint16_t maxCStrPoolCount_;
    uint16_t maxTokensCount_;

    // Increment the cStrPoolTail_ to the next location in the pool. As a null terminated string
    // must remain contiguous, it cannot be split between the end and the start of the pool. If so,
    // it is relocated to the beginning of the pool.
    inline void incrCStrPoolTailIdx(Token &token) {
        cStrPoolTailIdx_ = (cStrPoolTailIdx_ + 1) % CSTR_POOL_SIZE;

        if (cStrPoolTailIdx_ == 0) {
            if ((getCStrPoolCount() + token.length) < (CSTR_POOL_SIZE - 2)) {
                memcpy(&cStrPool_[0], token.cStr, token.length);
                token.cStr = &cStrPool_[0];
                cStrPoolTailIdx_ = token.length;
            } else {
                log_e("cStrPool_[] is too small!");
                REND_EXIT_SIM;
            }
        }
        if constexpr (REND_STATS) {
            uint16_t count = getCStrPoolCount();
            if (count > maxCStrPoolCount_) {
                maxCStrPoolCount_ = count;
            }
        }
    }

public:
    Tokens(DisplaySystem &display) : display_(display) {}
    [[nodiscard]] inline auto operator[](renderer_defs::Idx idx) const -> const Token & {
        return tokens_[idx];
    }

    [[nodiscard]] inline auto tokenAt(renderer_defs::Idx idx) -> Token & { return tokens_[idx]; }

    [[nodiscard]] inline auto getMaxTokensCount() const -> uint16_t { return maxTokensCount_; }
    [[nodiscard]] inline auto getMaxCStrPoolCount() const -> uint16_t { return maxCStrPoolCount_; }

    [[nodiscard]] inline auto getLastIdx() const -> Idx {
        if constexpr (REND_DEBUG) {
            if (isEmpty()) {
                REND_ELOG("forgetLast(): tokens is empty!");
            }
        }

        return (tokensTailIdx_ == 0) ? (TOKENS_SIZE - 1) : (tokensTailIdx_ - 1);
    }

    [[nodiscard]] inline auto getLast() -> Token & {

        if constexpr (REND_DEBUG) {
            if (isEmpty()) {
                REND_ELOG("getLast(): tokens is empty!");
            }
        }
        return tokens_[getLastIdx()];
    }

    [[nodiscard]] inline auto getTokenCount() const -> uint16_t {
        return (tokensHeadIdx_ <= tokensTailIdx_)
                   ? (tokensTailIdx_ - tokensHeadIdx_)
                   : (tokensTailIdx_ + (TOKENS_SIZE - tokensHeadIdx_));
    }

    // The caller must have checked that there is at least 2 entries in the tokens ring before using
    // this method!!
    [[nodiscard]] inline auto getBeforeLast() -> Token & {
        if constexpr (REND_DEBUG) {
            if (getTokenCount() < 2) {
                REND_ELOG("getBeforeLast(): tokens not large enough!");
            }
        }
        return tokens_[(tokensTailIdx_ > 1) ? (tokensTailIdx_ - 2)
                                            : (TOKENS_SIZE + tokensTailIdx_ - 2)];
    }

    inline void forgetLast() {
        if constexpr (REND_DEBUG) {
            if (tokensHeadIdx_ == tokensTailIdx_) {
                REND_ELOG("forgetLast(): tokens is empty!");
            }
        }

        if (tokensHeadIdx_ != tokensTailIdx_) {
            prevTokenIdx(tokensTailIdx_);
        }
    }

    [[nodiscard]] inline auto isEmpty() const -> bool { return tokensHeadIdx_ == tokensTailIdx_; }

    [[nodiscard]] inline auto getCStrPoolCount() const -> uint16_t {
        return (cStrPoolHeadIdx_ <= cStrPoolTailIdx_)
                   ? (cStrPoolTailIdx_ - cStrPoolHeadIdx_)
                   : (cStrPoolTailIdx_ + (CSTR_POOL_SIZE - cStrPoolHeadIdx_));
    }

    [[nodiscard]] inline auto cStrIsFull() const -> bool {
        return (getCStrPoolCount() >= (CSTR_POOL_SIZE - 2));
    }

    [[nodiscard]] inline auto tokensIsFull() const -> bool {
        return ((tokensTailIdx_ + 1) % TOKENS_SIZE) == tokensHeadIdx_;
    }

    [[nodiscard]] inline auto isFull() const -> bool { return (cStrIsFull() || tokensIsFull()); }

    [[nodiscard]] auto add(Token &token, const char *str = nullptr, uint8_t length = 0) -> bool;

    void addByte(Token &token, uint8_t b);
    void addBytes(Token &token, const char *str);
    void addStr(Token &token, const std::string &str);
    void insertBytesAtEnd(Token &token, const char *str);

    // Adds a null character at the end of a cStr that is in the cStr pool
    inline void endOfCStr() {
        cStrPool_[cStrPoolTailIdx_] = 0;
        cStrPoolTailIdx_ = (cStrPoolTailIdx_ + 1) % CSTR_POOL_SIZE;

        if constexpr (REND_STATS) {
            uint16_t count = getCStrPoolCount();
            if (count > maxCStrPoolCount_) {
                maxCStrPoolCount_ = count;
            }
        }
    }

    inline void clear() {
        tokensTailIdx_ = cStrPoolTailIdx_ = 0;
        tokensHeadIdx_ = cStrPoolHeadIdx_ = 0;
        if constexpr (REND_STATS) {
            maxTokensCount_ = maxCStrPoolCount_ = 0;
        }
    }

    void show() const;
    void showToken(Idx tokenIdx) const;

    [[nodiscard]] inline auto getNextAvailable() -> Token & {
        if constexpr (REND_DEBUG) {
            if (isFull()) {
                REND_ELOG("forgetLast(): tokens[] is full!");
            }
        }
        return tokens_[tokensTailIdx_];
    }

    inline void advanceTokenTail() {

        if constexpr (REND_DEBUG) {
            if (isFull()) {
                REND_ELOG("forgetLast(): tokens[] is full!");
            }
        }
        tokensTailIdx_ = (tokensTailIdx_ + 1) % TOKENS_SIZE;

        if constexpr (REND_STATS) {
            uint16_t count = getTokenCount();
            if (count > maxTokensCount_) {
                maxTokensCount_ = count;
            }
        }
    }

    void freeTokens(Idx fromTokenIdx, uint16_t count);

    [[nodiscard]] inline auto addToTokenIdx(Idx idx, uint16_t val) const -> Idx {
        return (idx + val) % TOKENS_SIZE;
    }
    [[nodiscard]] inline auto addToCStrPoolIdx(Idx idx, uint16_t val) const -> Idx {
        return (idx + val) % CSTR_POOL_SIZE;
    }

    inline void nextTokenIdx(Idx &i) const { i = addToTokenIdx(i, 1); }
    inline void prevTokenIdx(Idx &i) const { i = (i == 0) ? (TOKENS_SIZE - 1) : (i - 1); }
};
