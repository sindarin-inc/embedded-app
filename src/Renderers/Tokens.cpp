#include "Tokens.hpp"

// Add a token to the tokens list. The null terminated string is *not* copied to the pool and is
// expected to be a literal. The token may not be added if there is no space remaining in the tokens
// array.
auto Tokens::add(Token &token, const char *str, uint8_t strSize) -> bool {
    if (tokensCount_ < MAX_TOKEN_COUNT) {
        if (str != nullptr) {
            token.cStr = str;
            token.length = strSize;
        }

        // ToDo: Remove the last test when the old Page computation algo is out
        if ((token.width == 0) && (token.cStr != nullptr) && (token.cStr[0] != '\n')) {
            display_.getTextSize(token.cStr, &token.width, &token.height);
        }

        tokens_[tokensCount_++] = token;
        // if constexpr (RENDERER_DEBUG) {
        //     log_d("After: tokensCount_: %" PRIu16 ", cStrPoolCount_: %" PRIu16, tokensCount_,
        //           cStrPoolCount_);
        // }
    } else {
        log_e("tokens_[] too small!");
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
    if (cStrPoolCount_ < (POOL_BUFFER_SIZE - 2)) {
        if (token.cStr == nullptr) {
            token.cStr = &cStrPool_[cStrPoolCount_];
            token.length = 1;
            cStrPool_[cStrPoolCount_++] = b;
            cStrPool_[cStrPoolCount_++] = 0;
        } else {
            cStrPool_[cStrPoolCount_ - 1] = b;
            cStrPool_[cStrPoolCount_++] = 0;
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
    if ((cStrPoolCount_ + len) < (POOL_BUFFER_SIZE - 2)) {
        token.length += len;
        const char *s = str.c_str();
        while (len--) {
            if (token.cStr == nullptr) {
                token.cStr = &cStrPool_[cStrPoolCount_];
            }
            cStrPool_[cStrPoolCount_++] = *s++;
        }
        cStrPool_[cStrPoolCount_++] = 0;
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

// Set the size of a space character for the token. If not already retrived for caching, get it
// through the displau_/font.
void Tokens::setSpaceSize(Token &token) {
    if (spaceWidth_ == 0) {
        display_.getTextSize(" ", &spaceWidth_, &spaceHeight_);
    }
    token.width = spaceWidth_;
    token.height = spaceHeight_;
}

// debugging methods
void Tokens::show() const {
    if constexpr (RENDERER_DEBUG) {
        for (Idx i = 0; i < tokensCount_; i++) {
            auto currToken = &tokens_[i];
            log_d("TokenIdx: %" PRIu16 ", TokenId: %" PRIu8
                  ", streamPlace: %s, prevNewParagraphIdx: %" PRIu16 ", width: %" PRIu16
                  ", height: %" PRIu16 ", Length: %" PRIu16,
                  i, (uint8_t)currToken->tokenId, currToken->streamPlace.str().c_str(),
                  currToken->prevNewParagraphTokenIdx, currToken->width, currToken->height,
                  currToken->length);
            if (currToken->tokenId == TokenId::TEXT) {
                log_d("Text Content: [%s]", currToken->cStr);
            }
        }
    }
}
