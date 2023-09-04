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

        // ToDo: Remove the last test when the old Page computation algo is out
        if ((token.width == 0) && (token.cStr != nullptr) && (token.cStr[0] != '\n')) {
            display_.getTextSize(token.cStr, &token.width, &token.height);
        }

        advanceTail();

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("After: tokens count: %" PRIu16 ", cStrPoolCount_: %" PRIu16, getCount(),
        //           cStrPoolCount_);
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

// Add a byte to the token string. The byte is copied to the pool. The token's length and null
// terminated string are adjusted.
void Tokens::addByte(Token &token, uint8_t b) {
    // if constexpr (RENDERER_DEBUG) {
    //   log_d("Adding byte %02" PRIX8 "H to last token at %" PRIu16 ".", b, cStrPoolCount_);
    // }
    if (!cStrIsFull()) {
        if (token.cStr == nullptr) {
            token.cStr = &cStrPool_[cStrPoolTailIdx_];
            token.length = 1;
            cStrPool_[cStrPoolTailIdx_] = b;
            incrCStrPoolTailIdx();
            cStrPool_[cStrPoolTailIdx_] = 0;
            incrCStrPoolTailIdx();
        } else {
            cStrPool_[(cStrPoolTailIdx_ == 0) ? (CSTR_POOL_SIZE - 1) : (cStrPoolTailIdx_ - 1)] = b;
            cStrPool_[cStrPoolTailIdx_] = 0;
            incrCStrPoolTailIdx();
            token.length++;
        }
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

// Associate the string to the token. The string will be copied to the pool.
void Tokens::addStr(Token &token, std::string str) {
    // if constexpr (RENDERER_DEBUG) {
    //   log_d("Adding byte %02" PRIX8 "H to last token at %" PRIu16 ".", b, cStrPoolCount_);
    // }
    uint16_t len = str.length();
    if ((getCStrPoolCount() + len) < (CSTR_POOL_SIZE - 2)) {
        token.length += len;
        const char *s = str.c_str();
        if (token.cStr == nullptr) {
            token.cStr = &cStrPool_[cStrPoolTailIdx_];
        }
        while (len--) {
            cStrPool_[cStrPoolTailIdx_] = *s++;
            incrCStrPoolTailIdx();
        }
        cStrPool_[cStrPoolTailIdx_] = 0;
        incrCStrPoolTailIdx();

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

// Set the size of a space character for the token. If not already retrieved for caching, get it
// through the display_/font.
void Tokens::setSpaceSize(Token &token) {
    if (spaceWidth_ == 0) {
        display_.getTextSize(" ", &spaceWidth_, &spaceHeight_);
    }
    token.width = spaceWidth_;
    token.height = spaceHeight_;
}

// This will free space taken by the tokens that are part of the Head Line. It is expected the
// fromTokenIdx is equal to the tokensHeadIdx_.
void Tokens::freeTokens(Idx fromTokenIdx, uint16_t count) {

    if constexpr (RENDERER_DEBUG) {
        log_w("fromTokenIdx: %" PRIu16 " , count: %" PRIu16, fromTokenIdx, count);
        if (fromTokenIdx != tokensHeadIdx_) {
            log_e("freeTokens received a bad fromTokenIdx (%" PRIu16 "), expected %" PRIu16,
                  fromTokenIdx, tokensHeadIdx_);
            if constexpr (ON_ERROR_EXIT) {
                exit(1);
            }
        }
    }
    if (count > 0) {
        if constexpr (RENDERER_DEBUG) {
            Idx lastTokenIdx = addToTokenIdx(fromTokenIdx, count);
            Idx tokenIdx = fromTokenIdx;
            while (tokenIdx != lastTokenIdx) {
                if ((tokens_[tokenIdx].tokenId == TokenId::TEXT) ||
                    (tokens_[tokenIdx].tokenId == TokenId::PICTURE)) {
                    if ((tokens_[tokenIdx].cStr - cStrPool_.data()) != cStrPoolHeadIdx_) {
                        log_e("cStrPool pointers discrepancies: %" PRIu16 " vs %" PRIu16,
                              (Idx)(tokens_[tokenIdx].cStr - cStrPool_.data()), cStrPoolHeadIdx_);
                        if constexpr (ON_ERROR_EXIT) {
                            exit(1);
                        }
                    }
                    break;
                }
                nextTokenIdx(tokenIdx);
            }
        }
        Idx tokenIdx = addToTokenIdx(fromTokenIdx, count - 1);
        // Find last entry that point at string in pool
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

// debugging method
void Tokens::show() const {
    if constexpr (RENDERER_DEBUG) {
        for (Idx tokenIdx = tokensHeadIdx_; tokenIdx != tokensTailIdx_; nextTokenIdx(tokenIdx)) {
            auto &currToken = tokens_[tokenIdx];
            log_d("TokenIdx: %" PRIu16 ", TokenId: %" PRIu8 ", streamPlace: %s, width: %" PRIu16
                  ", height: %" PRIu16 ", Length: %" PRIu16,
                  tokenIdx, (uint8_t)currToken.tokenId, currToken.streamPlace.str().c_str(),
                  currToken.width, currToken.height, currToken.length);
            if (currToken.tokenId == TokenId::TEXT) {
                log_d("Text Content: [%s]", currToken.cStr);
            }
        }
    }
}