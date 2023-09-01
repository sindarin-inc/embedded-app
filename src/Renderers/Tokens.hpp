#pragma once

#include <array>

#include "RendererDefs.hpp"

using namespace renderer_defs;

class Tokens {
private:
    DisplaySystem &display_;

    TokensArray tokens_{};
    Idx tokensHead_{0};
    Idx tokensTail_{0};

    CStrPool cStrPool_{};
    uint16_t cStrPoolCount_{0};

    Idx cStrPoolHead_{0};
    Idx cStrPoolTail_{0};

    uint16_t spaceWidth_{0};
    uint16_t spaceHeight_{0};

public:
    Tokens(DisplaySystem &display) : display_(display) {}
    [[nodiscard]] inline auto operator[](renderer_defs::Idx idx) const -> const Token & {
        return tokens_[idx];
    }

    [[nodiscard]] inline auto isEmpty() const -> bool { return tokensHead_ == tokensTail_; }

    [[nodiscard]] inline auto getLastIdx() const -> Idx {
        if constexpr (RENDERER_DEBUG) {
            if (isEmpty()) {
                log_e("forgetLast(): tokens is empty!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return (tokensTail_ == 0) ? (TOKEN_COUNT - 1) : (tokensTail_ - 1);
    }

    [[nodiscard]] inline auto getLast() -> Token & {
        if constexpr (RENDERER_DEBUG) {
            if (isEmpty()) {
                log_e("forgetLast(): tokens is empty!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return tokens_[getLastIdx()];
    }

    [[nodiscard]] inline auto getCount() const -> uint16_t {
        return (tokensHead_ <= tokensTail_) ? (tokensTail_ - tokensHead_)
                                            : (tokensTail_ + (TOKEN_COUNT - tokensHead_));
    }

    // The caller must have checked that there is at least 2 entries in the tokens ring before using
    // this method!!
    [[nodiscard]] inline auto getBeforeLast() -> Token & {
        if constexpr (RENDERER_DEBUG) {
            if (getCount() < 2) {
                log_e("forgetLast(): tokens not large enough!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        return tokens_[(tokensTail_ > 1) ? (tokensTail_ - 2) : (TOKEN_COUNT + tokensTail_ - 2)];
    }

    inline void forgetLast() {
        if constexpr (RENDERER_DEBUG) {
            if (tokensHead_ == tokensTail_) {
                log_e("forgetLast(): tokens is empty!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
        if (tokensHead_ != tokensTail_) {
            tokensTail_ = (tokensTail_ == 0) ? (TOKEN_COUNT - 1) : (tokensTail_--);
        }
    }

    [[nodiscard]] inline auto cStrIsFull() const -> bool {
        return (cStrPoolCount_ >= (POOL_BUFFER_SIZE - 2));
    }

    [[nodiscard]] inline auto isFull() const -> bool {
        return ((cStrPoolCount_ >= (POOL_BUFFER_SIZE - 2)) ||
                (((tokensHead_ + 1) % TOKEN_COUNT) == tokensTail_));
    }

    [[nodiscard]] auto add(Token &token, const char *str = nullptr, uint8_t strSize = 0) -> bool;

    void addByte(Token &token, uint8_t b);
    void addStr(Token &token, std::string str);

    void setSpaceSize(Token &token);

    inline void clear() {
        tokensHead_ = tokensTail_ = 0;
        cStrPoolCount_ = 0;
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

        return tokens_[tokensTail_];
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

        tokensTail_ = (tokensTail_ + 1) % TOKEN_COUNT;
    }
};
