#include "Tokens.hpp"

void Tokens::add(Token &token, const char *str, uint8_t strSize) {
    if (tokensCount_ < MAX_TOKEN_COUNT) {
        if (str != nullptr) {
            if ((cStrPoolCount_ + strSize + 1) < POOL_BUFFER_SIZE) {
                token.cStr = &cStrPool_[cStrPoolCount_];
                token.length = strSize;
                std::copy_n(str, strSize, &cStrPool_[cStrPoolCount_]);
                cStrPoolCount_ += strSize;
                cStrPool_[cStrPoolCount_++] = 0;
            } else {
                log_e("cStrPool_[] is too small!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
        } else {
            token.length = 0;
            token.cStr = nullptr;
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

void Tokens::addByteToLast(uint8_t b) {
    // if constexpr (RENDERER_DEBUG) {
    //     log_d("Adding byte %02" PRIX8 "H to last token.", b);
    // }
    if (cStrPoolCount_ < (POOL_BUFFER_SIZE - 2)) {
        Token &token = getLast();
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
                log_e("Hum... token length wrong: %" PRIu64 " vs %" PRIu16, strlen(token.cStr),
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

void Tokens::setSpaceSize(renderer_defs::Idx tokenIdx) {
    if (spaceWidth_ == 0) {
        display_.getTextSize(" ", &spaceWidth_, &spaceHeight_);
    }
    tokens_[tokenIdx].width = spaceWidth_;
    tokens_[tokenIdx].height = spaceHeight_;
}

// Must be called only in the context of loadAndTokenizePage() method.
// A helper method to fill the size of a token after the tokens have been loaded
// into the token buffer.
//
// The thing is, most tokens are easy to determine the width of because the
// operation is the same regardless of context. Spaces are the exception.
// A space may render differently based on the characters before and after it.
// In my limited testing on our 5pt Tahoma font, spaces can be 3 or 4 pixels wide.
// I suspect this will only grow as our font rendering becomes more sophisticated.
// So this returns the obvious thing for non-spaces and does the math for spaces
// based on the tokens before and after the space.
//
// Modified by Guy Turcotte:
//
// As this issue maybe related to the GFX way of doing things, the code has been
// modified to simplify the process when the current font is an IBMF one.
//
// Returns the area of the token (width * height)
// for use in calcuating how much space we need for a page.
//
auto Tokens::setSizes(Idx tokenIdx) -> uint16_t {
    if (display_.getFont()->isA(FontType::IBMF)) {
        if (tokens_[tokenIdx].tokenId != TokenId::SPACE) {
            display_.getTextSize(tokens_[tokenIdx].cStr, &tokens_[tokenIdx].width,
                                 &tokens_[tokenIdx].height);
        } else {
            setSpaceSize(tokenIdx);
        }
    } else {
        // With GFX Fonts, spaces are a mess. The width of them depends on the tokens before and
        // after. Joy.
        if (tokens_[tokenIdx].tokenId != TokenId::SPACE) {
            display_.getTextSize(tokens_[tokenIdx].cStr, &tokens_[tokenIdx].width,
                                 &tokens_[tokenIdx].height);
        } else {
            // Check bounds. Punt otherwise.
            // Get the tokens before and after the space, then do the width
            // of that whole thing and subtract out the two tokens. That's the
            // real width of the rendered space.
            if ((tokenIdx > 0) && ((tokenIdx + 1) < tokensCount_)) {
                char temp[4];
                temp[0] = tokens_[tokenIdx - 1].cStr[tokens_[tokenIdx - 1].length - 1];
                temp[1] = ' ';
                temp[2] = tokens_[tokenIdx + 1].cStr[0];
                temp[3] = 0;

                tokens_[tokenIdx].height = display_.getTextHeight(temp);
                tokens_[tokenIdx].width = display_.getTextWidth(temp) -
                                          display_.getTextWidth(tokens_[tokenIdx - 1].cStr) -
                                          display_.getTextWidth(tokens_[tokenIdx + 1].cStr);
            } else {
                display_.getTextSize(tokens_[tokenIdx].cStr, &tokens_[tokenIdx].width,
                                     &tokens_[tokenIdx].height);
            }
        }
    }
    return (tokens_[tokenIdx].width * tokens_[tokenIdx].height);
}

// debugging method
void Tokens::print() const {
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
