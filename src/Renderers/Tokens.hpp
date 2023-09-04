#pragma once

#include <array>

#include "RendererDefs.hpp"

using namespace renderer_defs;

class Tokens {
private:
    DisplaySystem &display_;

    TokensArray tokens_{};
    Idx tokensHeadIdx_{0};
    Idx tokensTailIdx_{0};

    CStrPool cStrPool_{};
    Idx cStrPoolHeadIdx_{0};
    Idx cStrPoolTailIdx_{0};

    uint16_t spaceWidth_{0};
    uint16_t spaceHeight_{0};

    inline void incrCStrPoolTailIdx() {
        cStrPoolTailIdx_ = (cStrPoolTailIdx_ + 1) % CSTR_POOL_SIZE;
    }

public:
    Tokens(DisplaySystem &display) : display_(display) {}
    [[nodiscard]] inline auto operator[](renderer_defs::Idx idx) const -> const Token & {
        return tokens_[idx];
    }

    [[nodiscard]] inline auto isEmpty() const -> bool { return tokensHeadIdx_ == tokensTailIdx_; }

    [[nodiscard]] inline auto getLastIdx() const -> Idx {
        if constexpr (RENDERER_DEBUG) {
            if (isEmpty()) {
                log_e("forgetLast(): tokens is empty!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return (tokensTailIdx_ == 0) ? (TOKENS_SIZE - 1) : (tokensTailIdx_ - 1);
    }

    [[nodiscard]] inline auto getLast() -> Token & {
        if constexpr (RENDERER_DEBUG) {
            if (isEmpty()) {
                log_e("getLast(): tokens is empty!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
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
        if constexpr (RENDERER_DEBUG) {
            if (getTokenCount() < 2) {
                log_e("getBeforeLast(): tokens not large enough!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return tokens_[(tokensTailIdx_ > 1) ? (tokensTailIdx_ - 2)
                                            : (TOKENS_SIZE + tokensTailIdx_ - 2)];
    }

    inline void forgetLast() {
        if constexpr (RENDERER_DEBUG) {
            if (tokensHeadIdx_ == tokensTailIdx_) {
                log_e("forgetLast(): tokens is empty!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        if (tokensHeadIdx_ != tokensTailIdx_) {
            prevTokenIdx(tokensTailIdx_);
        }
    }

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
    void addStr(Token &token, std::string str);

    void setSpaceSize(Token &token);

    inline void clear() {
        tokensHeadIdx_ = tokensTailIdx_ = 0;
        cStrPoolHeadIdx_ = cStrPoolTailIdx_ = 0;
    }

    void show() const;

    // The caller must have checked that there is at least 1 free entry in the tokens ring before
    // using this method!!
    [[nodiscard]] inline auto getNextAvailable() -> Token & {
        if constexpr (RENDERER_DEBUG) {
            if (isFull()) {
                log_e("forgetLast(): tokens[] is full!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }

        return tokens_[tokensTailIdx_];
    }

    inline void advanceTail() {
        if constexpr (RENDERER_DEBUG) {
            if (isFull()) {
                log_e("forgetLast(): tokens[] is full!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }

        tokensTailIdx_ = (tokensTailIdx_ + 1) % TOKENS_SIZE;
    }

    void freeTokens(Idx fromTokenIdx, uint16_t count);

    inline auto addToTokenIdx(Idx idx, uint16_t val) const -> Idx {
        return (idx + val) % TOKENS_SIZE;
    }
    inline auto addToCStrPoolIdx(Idx idx, uint16_t val) const -> Idx {
        return (idx + val) % CSTR_POOL_SIZE;
    }

    inline void nextTokenIdx(Idx &i) const { i = addToTokenIdx(i, 1); }
    inline void prevTokenIdx(Idx &i) const { i = (i == 0) ? (TOKENS_SIZE - 1) : (i - 1); }
};
