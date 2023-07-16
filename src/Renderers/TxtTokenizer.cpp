#include "TxtTokenizer.hpp"

//
// This loads from the stream character by character and populates
// the token buffer based on the TOKEN enum.
// After this is done, then we can do the heavy lifting of walking
// through the tokens, checking size, and rendering the text on
// a line by line basis.
//
// We track backwards because we need to double the amount read in a backwards
// read (2 pages instead of 1)
//
// Computes the number of word in the page.
//
// Returns the computed streamPlace where the page start really.
//
// IMPORTANT: The getFileLocation() method must be called only through this method.
auto TxtTokenizer::loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, bool backwards,
                                       StreamPlace *toPlace) -> StreamPlace {

    Idx prevNewParagraphIdx = 0;

    uint32_t textArea = 0;
    uint32_t maxTextArea = static_cast<uint32_t>(display_.width()) * display_.height();

    tokens.clear();
    currentPageWordCount_ = 0;

    Token newToken;
    Token spaceToken;

    fromPlace = getFileLocation(fromPlace);
    if (fromPlace.offset == LOCATION_ERROR) {
        return getPlaceAtBeginningOfDocument();
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("Starting tokenizer at streamPlace %s", fromPlace.str().c_str());
    }

    uint8_t curChar = stream_->getNextByte();
    Idx streamRelativeOffset = 0;

    // Keep it DRY
    auto checkSpace = [&tokens, &spaceToken]() -> void {
        if (spaceToken.tokenId == TokenId::SPACE) {
            tokens.add(spaceToken, " ", 1);
            spaceToken.tokenId = TokenId::NOTOKEN;
        }
    };

    auto nextByte = [this, &curChar, &streamRelativeOffset]() -> void {
        curChar = stream_->getNextByte();
        streamRelativeOffset++;
    };

    spaceToken.tokenId = TokenId::NOTOKEN;

    while ((curChar != 0) && !tokens.isFull() &&
           ((backwards && (toPlace->offset > tokens.getLast().streamPlace.offset)) ||
            (textArea < maxTextArea))) {
        newToken = {.cStr = nullptr,
                    .streamPlace = addOffset(fromPlace, streamRelativeOffset),
                    .prevNewParagraphTokenIdx = prevNewParagraphIdx,
                    .width = 0,
                    .height = 0,
                    .length = 0,
                    .tokenId = TokenId::ENDOFFILE};

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Set the stream place to %" PRIu32, fromPlace + streamRelativeOffset);
        //     log_d("Current Character: %c (%02" PRIX8 "H)",
        //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
        // }

        switch (curChar) {
        case '\n':
            checkSpace();
            // No two or more lines with only a \n are allowed.
            if (!((tokens.getCount() > 0) && (tokens.getLast().tokenId == TokenId::ENDPARAGRAPH))) {
                if ((tokens.getCount() == 0) || (tokens.getLast().tokenId == TokenId::NEWLINE)) {
                    newToken.tokenId = TokenId::ENDPARAGRAPH;
                    prevNewParagraphIdx = tokens.getCount();
                } else {
                    newToken.tokenId = TokenId::NEWLINE;
                }
                tokens.add(newToken, "\n", 1);
            }
            nextByte();
            break;

        case ' ':
            checkSpace();
            spaceToken = newToken;
            spaceToken.tokenId = TokenId::SPACE;
            tokens.setSpaceSize(spaceToken);
            nextByte();
            break;

        case '\t':
            checkSpace();
            newToken.tokenId = TokenId::TAB;
            display_.getTextSize("\t", &newToken.width, &newToken.height);
            tokens.add(newToken, "\t", 1);
            nextByte();
            break;

        case '-':
            checkSpace();
            // Check to see if this is an endash or emdash case
            if ((tokens.getCount() > 0) && (tokens.getLast().tokenId == TokenId::HYPHEN)) {
                Token *token = &tokens.getLast();
                token->tokenId = TokenId::ENDASH;
                display_.getTextSize("--", &token->width, &token->height);
                token->cStr = "--";
                token->length = 2;
                // Specifically don't increment the token buffer
                // because the previous hyphen already did that
            } else if ((tokens.getCount() > 0) && (tokens.getLast().tokenId == TokenId::ENDASH)) {
                Token *token = &tokens.getLast();
                token->tokenId = TokenId::EMDASH;
                display_.getTextSize("---", &token->width, &token->height);
                token->cStr = "---";
                token->length = 3;
                // Specifically don't increment the token buffer
                // because the previous hyphen already did that
            } else {
                newToken.tokenId = TokenId::HYPHEN;
                display_.getTextSize("-", &newToken.width, &newToken.height);
                tokens.add(newToken, "-", 1);
            }
            nextByte();
            break;

        // All other characters are considered part of a text token,
        default: {
            newToken.tokenId = TokenId::TEXT;
            uint16_t textCharCount = 0;

            if (display_.getFont()->isA(FontType::GFX) && (spaceToken.tokenId == TokenId::SPACE)) {
                tokens.setGFXSpaceSize(spaceToken, curChar);
            }
            checkSpace();

            do {
                if ((curChar <= 0x7FU) || (curChar >= 0xC0U)) {
                    textCharCount += 1;
                }
                if (!tokens.cStrIsFull()) {
                    tokens.addByte(newToken, curChar);
                } else {
                    break;
                }
                nextByte();
            } while ((curChar > ' ') && (curChar != '-') &&
                     ((textCharCount < maxTextTokenCharSize_) || (curChar >= 0x80U)));
            // The last test of the while above is to get a complete UTF8 multi-bytes character if
            // the maximum word size is reached

            if (!tokens.cStrIsFull()) {
                display_.getTextSize(newToken.cStr, &newToken.width, &newToken.height);
            }
            tokens.add(newToken);

            break;
        }
        }
    }

    if (tokens.cStrIsFull()) {
        // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
        // EMDASH), so forget it
        tokens.forgetLastToken();
    }

    checkSpace();

    Token eofToken = {.cStr = nullptr,
                      .streamPlace = addOffset(fromPlace, streamRelativeOffset),
                      .prevNewParagraphTokenIdx = 0,
                      .width = 0,
                      .height = 0,
                      .length = 0,
                      .tokenId = TokenId::ENDOFFILE};

    tokens.add(eofToken);

    if constexpr (RENDERER_DEBUG) {
        log_d("Finished tokenizing with running text area %" PRIu32 " and max area %" PRIu32
              ", and streamPlace in file is %s",
              textArea, maxTextArea, tokens.getLast().streamPlace.str().c_str());
        log_d("Computed Word Count: %" PRIu32, currentPageWordCount_);
    }

    // printOutTokens(); // Debugging - use only when required

    if constexpr (RENDERER_DEBUG) {
        log_d("Tokens Count: %" PRIu16, tokens.getCount());
    }

    return fromPlace;
}

// Must be called only by the loadAndTokenizePage().
// From the fromPlace variable, seek into the file and synchronize
// the value at a valid UTF8 location. An UTF8 character start with
// a first byte MSBit == 0 or the two MSBits == 11
//
// returns an updated fromPlace location or the LOCATION_ERROR value.
auto TxtTokenizer::getFileLocation(StreamPlace fromPlace) -> StreamPlace {

    // if received streamPlace is larger than the stream size, reset it to
    // the beginning of file
    if (fromPlace.offset >= stream_->size()) {
        fromPlace = getPlaceAtBeginningOfDocument();
    }

    if (!stream_->seek(fromPlace.offset)) {
        log_e("failed to seek at streamPlace %s in file!", fromPlace.str().c_str());
        if constexpr (ON_ERROR_EXIT) {
            exit(2);
        }
    } else {
        while (stream_->available()) {
            uint8_t ch = stream_->read();
            // A valid UTF8 character is ASCII or got it's first byte
            // larger or equal to 0xC0
            if ((ch < 0x7FU) || (ch >= 0xC0U)) {
                stream_->seek(fromPlace.offset);
                return fromPlace;
            }
            fromPlace.offset += 1;
            if constexpr (RENDERER_DEBUG) {
                log_d("Synching to start of UTF8 character at streamPlace %s",
                      fromPlace.str().c_str());
            }
        }
    }

    fromPlace.offset = LOCATION_ERROR;
    return fromPlace;
}
