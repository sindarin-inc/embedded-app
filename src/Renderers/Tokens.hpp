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

    [[nodiscard]] inline auto cStrPoolIsFull() const -> bool {
        return !(cStrPoolCount_ < (POOL_BUFFER_SIZE - 2));
    }
    void setSpaceSize(renderer_defs::Idx tokenIdx);

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    auto setSizes(Idx tokenIdx) -> uint16_t;

    void add(Token &token, const char *str = nullptr, uint8_t strSize = 0);
    void addByteToLast(uint8_t b);

    inline void clear() {
        tokensCount_ = 0;
        cStrPoolCount_ = 0;
    }

    void print() const;
};