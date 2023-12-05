#include "Tokens.hpp"

// Add a token to the tokens list. The null terminated string is *not* copied to the pool and is
// expected to be a literal. The token may not be added if there is no space remaining in the tokens
// array.
auto Tokens::add(Token &token, const char *str, uint8_t length) -> bool {
    if (!isFull()) {
        if (str != nullptr) {
            token.cStr = str;
            token.length = length;
        }

        if ((token.pixWidth == 0) && (token.cStr != nullptr) && (token.cStr[0] != '\n')) {
            token.pixWidth = (token.tokenId == TokenId::ITEXT)
                                 ? display_.getTextWidth(token.cStr)
                                 : display_.getTextWidthQuick(token.cStr);
            // log_w("Token width of %s is %" PRIu16, token.cStr, token.pixWidth);
        }

        if constexpr (REND_DEBUG) {
            showToken(tokensTailIdx_);
        }

        advanceTokenTail();

        // REND_WLOG("After: tokens count: %" PRIu16 ", cStr pool count: %" PRIu16,
        //     getTokensCount(),
        //     getCStrPoolCount());
    } else {
        log_e("tokens_[] too small!: (%s)", tokensIsFull() ? "TOKENS" : "CSTR_POOL");
        REND_EXIT_SIM;
        return false;
    }
    return true;
}

// Add a byte to the token string. The byte is copied to the pool. The token's length is adjusted. A
// call to endCStr() will be required to complete the null terminated string.
void Tokens::addByte(Token &token, uint8_t b) {
    // REND_WLOG("Adding byte %02" PRIX8 "H to last token at %" PRIu16 ".", b, cStrPoolCount_);
    if (!cStrIsFull()) {
        if (token.cStr == nullptr) {
            token.cStr = &cStrPool_[cStrPoolTailIdx_];
            token.length = 0;
        }
        cStrPool_[cStrPoolTailIdx_] = b;
        token.length++; // must be done BEFORE the call to incrCStrPoolTailIdx()
        incrCStrPoolTailIdx(token);
    } else {
        log_e("cStrPool_[] is too small!");
        REND_EXIT_SIM;
    }
}

void Tokens::addBytes(Token &token, const char *str) {
    while (*str) {
        addByte(token, *str++);
    }
}

void Tokens::insertBytesAtEnd(Token &token, const char *str) {
    if (token.cStr != nullptr) {
        cStrPoolTailIdx_ = (cStrPoolTailIdx_ == 0) ? (CSTR_POOL_SIZE - 1) : cStrPoolTailIdx_ - 1;

        while (*str) {
            addByte(token, *str++);
        }
        endOfCStr();
    }
}

// Associate the string to the token. The string will be copied to the pool. No other characters can
// be added to the token after it.
void Tokens::addStr(Token &token, const std::string &str) {
    // REND_WLOG("Adding byte %02" PRIX8 "H to last token at %" PRIu16 ".", b, cStrPoolCount_);

    uint16_t len = str.length();
    if ((getCStrPoolCount() + len) < (CSTR_POOL_SIZE - 2)) {
        const char *s = str.c_str();
        if (token.cStr == nullptr) {
            token.cStr = &cStrPool_[cStrPoolTailIdx_];
        }
        while (len--) {
            cStrPool_[cStrPoolTailIdx_] = *s++;
            token.length++; // must be done BEFORE the call to incrCStrPoolTailIdx(). DO NOT
                            // OPTIMIZE!
            incrCStrPoolTailIdx(token);
        }
        endOfCStr();

        if constexpr (REND_DEBUG) {
            if (strlen(token.cStr) != token.length) {
                REND_ELOG("Hum... token length wrong: %zu vs %" PRIu16, strlen(token.cStr),
                          token.length);
            }
            // log_d("After: tokensCount_: %" PRIu16 ", token length: %" PRIu16
            //       ", cStrPoolCount_: %" PRIu16,
            //       tokensCount_, token.length, cStrPoolCount_);
        }
    } else {
        log_e("cStrPool_[] is too small!");
        REND_EXIT_SIM;
    }
}

// This will free space taken by the tokens that are part of the Head Line. It is expected the
// fromTokenIdx is equal to the tokensHeadIdx_.
void Tokens::freeTokens(Idx fromTokenIdx, uint16_t count) {

    if constexpr (REND_DEBUG) {
        // log_w("fromTokenIdx: %" PRIu16 " , count: %" PRIu16, fromTokenIdx, count);
        if (fromTokenIdx != tokensHeadIdx_) {
            REND_ELOG("freeTokens received a bad fromTokenIdx (%" PRIu16 "), expected %" PRIu16,
                      fromTokenIdx, tokensHeadIdx_);
        }
    }
    if (count > 0) {
        //     if constexpr (REND_DEBUG) {
        //         Idx lastTokenIdx = addToTokenIdx(fromTokenIdx, count);
        //         Idx tokenIdx = fromTokenIdx;
        //         while (tokenIdx != lastTokenIdx) {
        //             if ((tokens_[tokenIdx].tokenId == TokenId::TEXT) ||
        //                 (tokens_[tokenIdx].tokenId == TokenId::PICTURE)) {
        //                 Idx poolIdx = (Idx)(tokens_[tokenIdx].cStr - cStrPool_.data());
        //                 Idx poolHead = cStrPoolHeadIdx_;
        //                 if ((cStrPoolHeadIdx_ + tokens_[tokenIdx].length + 1) >= CSTR_POOL_SIZE)
        //                 {
        //                     cStrPoolHeadIdx_ = 0;
        //                 }
        //                 if (poolIdx != cStrPoolHeadIdx_) {
        //                     log_e("cStrPool pointers discrepancies: %" PRIu16 " vs %" PRIu16,
        //                     poolIdx,
        //                           cStrPoolHeadIdx_);
        //                     REND_EXIT_SIM;
        //                 }
        //                 break;
        //             }
        //             nextTokenIdx(tokenIdx);
        //         }
        //     }

        // Release pointers on pictures if exists, save the last entry where there is a pool string
        // pointer
        Idx idx = fromTokenIdx;
        Idx lastTokenPoolIdx = fromTokenIdx;
        TokenId id = tokens_[idx].tokenId;

        for (int i = 0; i < count; i++, nextTokenIdx(idx)) {
            if (tokens_[idx].picture != nullptr) {
                tokens_[idx].picture.reset();
            }
            // Not all tokens that are using the pool
            if ((static_cast<uint16_t>(id) & POOL_TOKENS) != 0) {
                lastTokenPoolIdx = idx;
                id = tokens_[idx].tokenId;
            }
        }

        // Search the last token that contains a pointer inside the pool
        int32_t poolCount = getCStrPoolCount();
        // Idx tokenIdx = addToTokenIdx(fromTokenIdx, count - 1);
        // TokenId id = tokens_[tokenIdx].tokenId;
        // while ((tokenIdx != fromTokenIdx) && (id != TokenId::TEXT) && (id != TokenId::PICTURE) &&
        //        (id != TokenId::EPICTURE)) {
        //     prevTokenIdx(tokenIdx);
        //     id = tokens_[tokenIdx].tokenId;
        // }
        // Not all tokens are using the pool
        if ((static_cast<uint16_t>(id) & POOL_TOKENS) != 0) {
            // Free space in string pool
            Idx poolIdx = tokens_[lastTokenPoolIdx].cStr - cStrPool_.data();

            if constexpr (REND_DEBUG) {
                if (poolIdx >= CSTR_POOL_SIZE) {
                    REND_ELOG("poolIdx too large: %" PRIu16 ", must be smaller than %" PRIu16,
                              poolIdx, CSTR_POOL_SIZE);
                }
            }

            cStrPoolHeadIdx_ = addToCStrPoolIdx(poolIdx, tokens_[lastTokenPoolIdx].length + 1);
        }

        if (poolCount < getCStrPoolCount()) {
            log_e("cStrPool is larger than before freeing space! old size: %" PRIi32
                  ", new: %" PRIu16,
                  poolCount, getCStrPoolCount());
            REND_EXIT_SIM;
        }

        // REND_WLOG("cStrPool reduced from %" PRIi32 " to %" PRIi32, poolCount,
        //     getCStrPoolCount());

        tokensHeadIdx_ = addToTokenIdx(fromTokenIdx, count);

        if constexpr (REND_DEBUG) {
            if (getTokenCount() == 0) {
                REND_ELOG("Hum... Tokens is empty!!");
            }
        }
    }
}

// debugging methods

void Tokens::showToken(Idx tokenIdx) const {
    if constexpr (REND_DEBUG) {
        const Token &token = tokens_[tokenIdx];
        log_d("TokenIdx: %d, TokenId: %" PRIu8 ", streamPlace: %s, width: %" PRIu16
              //", height: %" PRIu16
              ", Length: %" PRIu16,
              tokenIdx, (uint8_t)token.tokenId, token.streamPlace.str().c_str(), token.pixWidth,
              // token.height,
              token.length);
        if (static_cast<uint16_t>(token.tokenId) &
            (static_cast<uint16_t>(TokenId::TEXT) | static_cast<uint16_t>(TokenId::ITEXT) |
             static_cast<uint16_t>(TokenId::HTEXT) | static_cast<uint16_t>(TokenId::NTEXT))) {
            if (token.cStr != nullptr) {
                log_d("Text Content: \"%s\"", token.cStr);
            } else {
                log_e("Text content is null!!");
            }
        }
    }
}

void Tokens::show() const {
    if constexpr (REND_DEBUG) {
        for (Idx tokenIdx = tokensHeadIdx_; tokenIdx != tokensTailIdx_; nextTokenIdx(tokenIdx)) {
            showToken(tokenIdx);
        }
    }
}
