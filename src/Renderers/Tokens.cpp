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

        if ((token.width == 0) && (token.cStr != nullptr) && (token.cStr[0] != '\n')) {
            token.width = display_.getTextWidthQuick(token.cStr);
            // log_w("Token width of %s is %" PRIu16, token.cStr, token.width);
        }

        // if constexpr (RENDERER_DEBUG) {
        //     showToken(tokensTailIdx_);
        // }

        advanceTokenTail();

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("After: tokens count: %" PRIu16 ", cStr pool count: %" PRIu16,
        //     getTokensCount(),
        //           getCStrPoolCount());
        // }
    } else {
        log_e("tokens_[] too small!: (%s)", tokensIsFull() ? "TOKENS" : "CSTR_POOL");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
        return false;
    }
    return true;
}

// Add a byte to the token string. The byte is copied to the pool. The token's length is adjusted. A
// call to endCStr() will be required to complete the null terminated string.
void Tokens::addByte(Token &token, uint8_t b) {
    // if constexpr (RENDERER_DEBUG) {
    //   log_d("Adding byte %02" PRIX8 "H to last token at %" PRIu16 ".", b, cStrPoolCount_);
    // }
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
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }
}

// Associate the string to the token. The string will be copied to the pool. No other characters can
// be added to the token after it.
void Tokens::addStr(Token &token, const std::string &str) {
    // if constexpr (RENDERER_DEBUG) {
    //   log_d("Adding byte %02" PRIX8 "H to last token at %" PRIu16 ".", b, cStrPoolCount_);
    // }
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

        if constexpr (RENDERER_DEBUG) {
            if (strlen(token.cStr) != token.length) {
                log_e("Hum... token length wrong: %zu vs %" PRIu16, strlen(token.cStr),
                      token.length);
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
            // log_d("After: tokensCount_: %" PRIu16 ", token length: %" PRIu16
            //       ", cStrPoolCount_: %" PRIu16,
            //       tokensCount_, token.length, cStrPoolCount_);
        }
    } else {
        log_e("cStrPool_[] is too small!");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }
}

// This will free space taken by the tokens that are part of the Head Line. It is expected the
// fromTokenIdx is equal to the tokensHeadIdx_.
void Tokens::freeTokens(Idx fromTokenIdx, uint16_t count) {

    if constexpr (RENDERER_DEBUG) {
        // log_w("fromTokenIdx: %" PRIu16 " , count: %" PRIu16, fromTokenIdx, count);
        if (fromTokenIdx != tokensHeadIdx_) {
            log_e("freeTokens received a bad fromTokenIdx (%" PRIu16 "), expected %" PRIu16,
                  fromTokenIdx, tokensHeadIdx_);
            if constexpr (ON_ERROR_EXIT) {
                exit(1);
            }
        }
    }
    if (count > 0) {
    //     if constexpr (RENDERER_DEBUG) {
    //         Idx lastTokenIdx = addToTokenIdx(fromTokenIdx, count);
    //         Idx tokenIdx = fromTokenIdx;
    //         while (tokenIdx != lastTokenIdx) {
    //             if ((tokens_[tokenIdx].tokenId == TokenId::TEXT) ||
    //                 (tokens_[tokenIdx].tokenId == TokenId::PICTURE)) {
    //                 Idx poolIdx = (Idx)(tokens_[tokenIdx].cStr - cStrPool_.data());
    //                 Idx poolHead = cStrPoolHeadIdx_;
    //                 if ((cStrPoolHeadIdx_ + tokens_[tokenIdx].length + 1) >= CSTR_POOL_SIZE) {
    //                     cStrPoolHeadIdx_ = 0;
    //                 }
    //                 if (poolIdx != cStrPoolHeadIdx_) {
    //                     log_e("cStrPool pointers discrepancies: %" PRIu16 " vs %" PRIu16, poolIdx,
    //                           cStrPoolHeadIdx_);
    //                     if constexpr (ON_ERROR_EXIT) {
    //                         exit(1);
    //                     }
    //                 }
    //                 break;
    //             }
    //             nextTokenIdx(tokenIdx);
    //         }
    //     }

        // Search the last token that contains a pointer inside the pool
        int32_t poolCount = getCStrPoolCount();
        Idx tokenIdx = addToTokenIdx(fromTokenIdx, count - 1);
        while ((tokenIdx != fromTokenIdx) && (tokens_[tokenIdx].tokenId != TokenId::TEXT) &&
               (tokens_[tokenIdx].tokenId != TokenId::PICTURE)) {
            prevTokenIdx(tokenIdx);
        }
        if ((tokens_[tokenIdx].tokenId == TokenId::TEXT) ||
            (tokens_[tokenIdx].tokenId == TokenId::PICTURE)) {
            // Free space in string pool
            Idx poolIdx = tokens_[tokenIdx].cStr - cStrPool_.data();
            cStrPoolHeadIdx_ = addToCStrPoolIdx(poolIdx, tokens_[tokenIdx].length + 1);
        }

        if (poolCount < getCStrPoolCount()) {
            log_e("cStrPool is larger than before freeing space! old size: %" PRIi32
                  ", new: %" PRIi32,
                  poolCount, getCStrPoolCount());
            if constexpr (ON_ERROR_EXIT) {
                exit(1);
            }
        }

        // if constexpr (RENDERER_DEBUG) {
        //     log_w("cStrPool reduced from %" PRIi32 " to %" PRIi32, poolCount, getCStrPoolCount());
        // }

        tokensHeadIdx_ = addToTokenIdx(fromTokenIdx, count);

        if constexpr (RENDERER_DEBUG) {
            if (getTokenCount() == 0) {
                log_e("Hum... Tokens is empty!!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        }
    }
}

// debugging methods

void Tokens::showToken(Idx tokenIdx) const {
    if constexpr (RENDERER_DEBUG) {
        const Token &token = tokens_[tokenIdx];
        log_d("TokenIdx: %" PRIu16 ", TokenId: %" PRIu8 ", streamPlace: %s, width: %" PRIu16
              //", height: %" PRIu16
              ", Length: %" PRIu16,
              tokenIdx, (uint8_t)token.tokenId, token.streamPlace.str().c_str(), token.width,
              // token.height,
              token.length);
        if (token.tokenId == TokenId::TEXT) {
            if (token.cStr != nullptr) {
                log_d("Text Content: [%s]", token.cStr);
            } else {
                log_e("Text content is null!!");
            }
        }
    }
}

void Tokens::show() const {
    if constexpr (RENDERER_DEBUG) {
        for (Idx tokenIdx = tokensHeadIdx_; tokenIdx != tokensTailIdx_; nextTokenIdx(tokenIdx)) {
            showToken(tokenIdx);
        }
    }
}