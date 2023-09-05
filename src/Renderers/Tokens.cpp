#include "Tokens.hpp"

void Tokens::add(Token &token, const char *str, uint8_t strSize) {
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
    }
}

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
