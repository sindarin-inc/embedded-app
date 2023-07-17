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

void Tokens::setSpaceSize(Token &token) {
    if (spaceWidth_ == 0) {
        display_.getTextSize(" ", &spaceWidth_, &spaceHeight_);
    }
    token.width = spaceWidth_;
    token.height = spaceHeight_;
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
void Tokens::setGFXSpaceSize(Token &spaceToken, uint8_t nextChar) {

    if (display_.getFont()->isA(FontType::GFX)) {
        // With GFX Fonts, spaces are a mess. The width of them depends on the tokens before and
        // after. Joy.

        // Check bounds. Punt otherwise.
        // Get the tokens before and nextChar that is after the space, then do the width
        // of that whole thing and subtract out the two tokens. That's the
        // real width of the rendered space.
        if ((tokensCount_ > 0) && (getLast().tokenId == TokenId::TEXT)) {
            Token &lastToken = getLast();
            char temp[4];
            const char *first = &lastToken.cStr[lastToken.length - 1];
            temp[0] = first[0];
            temp[1] = ' ';
            temp[2] = nextChar;
            temp[3] = 0;

            spaceToken.height = display_.getTextHeight(temp);
            spaceToken.width = display_.getTextWidth(temp) - display_.getTextWidth(first) -
                               display_.getTextWidth(&temp[2]);
        } else {
            setSpaceSize(spaceToken);
        }
    }
}

auto Tokens::addLastTokenToPage() -> bool {
    Token &token = getLast();

    auto newLine = [this](uint16_t height) {
        currentPageDecilineHeight_ += height;
        lineCount_ += 1;
        currentLineWidth_ = 0;
        lines_[lineCount_].tokenCount = 0;
    };

    switch (token.tokenId) {
    case TokenId::TEXT:
    case TokenId::SPACE:
    case TokenId::HYPHEN:
    case TokenId::ENDASH:
    case TokenId::EMDASH:
    case TokenId::TAB:
        if ((token.width + currentLineWidth_) >= maxLineWidth_) {
            if ((token.tokenId == TokenId::SPACE) || (token.tokenId == TokenId::TAB)) {
                // token.tokenId = TokenId::NOTOKEN;
                // or
                tokensCount_ -= 1;
            } else {
                newLine(NEWLINE_DECI_LINE_HEIGHT);
                if ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) >= maxDeciLines_) {
                    nextPagePlace_ = token.streamPlace;
                    return false;
                }
                currentLineWidth_ += token.width;
                lines_[lineCount_].tokenIdx = tokensCount_ - 1;
                lines_[lineCount_].tokenCount += 1;
            }
        } else if (lines_[lineCount_].tokenCount == 0) {
            if (((token.tokenId != TokenId::SPACE) && (token.tokenId != TokenId::TAB)) ||
                (tokensCount_ < 2) ||
                ((getBeforeLast().tokenId == TokenId::NEWLINE) ||
                 (getBeforeLast().tokenId == TokenId::ENDPARAGRAPH))) {
                lines_[lineCount_].tokenIdx = tokensCount_ - 1;
                lines_[lineCount_].tokenCount = 1;
                currentLineWidth_ = token.width;
            } else {
                tokensCount_ -= 1;
            }
        } else {
            currentLineWidth_ += token.width;
            lines_[lineCount_].tokenCount += 1;
        }

        if (token.tokenId == TokenId::TEXT) {
            currentPageWordCount_ += 1;
        }
        break;

    case TokenId::ENDOFFILE:
        nextPagePlace_ = token.streamPlace;
        return false;

    case TokenId::NEWLINE:
        // token.tokenId = TokenId::NOTOKEN;
        if (lines_[lineCount_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        break;

    case TokenId::ENDPARAGRAPH:
        if (lines_[lineCount_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        if ((currentPageDecilineHeight_ + ENDPARAGRAPH_DECI_LINE_HEIGHT) >= maxDeciLines_) {
            nextPagePlace_ = token.streamPlace;
            return false;
        }
        lines_[lineCount_].tokenCount = 1;
        lines_[lineCount_].tokenIdx = tokensCount_ - 1;
        newLine(ENDPARAGRAPH_DECI_LINE_HEIGHT);
        break;

    case TokenId::NOTOKEN:
    case TokenId::PICTURE:
        break;
    }

    return true;
}

// debugging methods
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

void Tokens::showPage() const {
    if constexpr (RENDERER_DEBUG) {
        for (int i = 0; i < lineCount_; i++) {
            std::cout << i << ": ";
            for (int j = lines_[i].tokenIdx; j < lines_[i].tokenIdx + lines_[i].tokenCount; j++) {
                if ((tokens_[j].tokenId != TokenId::NOTOKEN) &&
                    (tokens_[j].tokenId != TokenId::NEWLINE) &&
                    (tokens_[j].tokenId != TokenId::ENDPARAGRAPH)) {
                    std::cout << tokens_[j].cStr;
                }
            }
            std::cout << std::endl;
        }
        std::cout << "Next Page Place: " << nextPagePlace_.str().c_str() << std::endl;
        std::cout << "Page Word Count: " << currentPageWordCount_ << std::endl;
    }
}