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
auto TxtTokenizer::loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, bool backwards)
    -> StreamPlace {

    Idx pageIdx = 0;
    Idx prevNewParagraphIdx = 0;

    uint32_t textArea = 0;
    uint32_t maxTextArea = static_cast<uint32_t>(display_.width()) * display_.height();
    uint16_t textCharCount = 0;

    tokens.clear();
    currentPageWordCount_ = 0;

    Token newToken;

    // Keep it DRY
    auto computePreviousTokenSize = [this, &textArea, &tokens]() -> void {
        // log_d("computePreviousTokenSize()..., tokenIdx: %" PRIu16 ", tokensCount_: %" PRIu16,
        //       tokenIdx, tokensCount_);
        //  we need to set the size of the previous token
        if ((tokens.getCount() > 0) && (tokens.getLast().tokenId == TokenId::TEXT)) {
            textArea += tokens.setSizes(tokens.getCount() - 1);
            // If we've started a new text token and the previous token was a space
            // we know that we should go back and set the size of that space token
            // based on this token and the token before the space.
            if ((tokens.getCount() > 2) && (tokens.getBeforeLast().tokenId == TokenId::SPACE)) {
                textArea += tokens.setSizes(tokens.getCount() - 2);
            }
        }
        // log_d("textArea size: %" PRIu32 ", tokenIdx: %" PRIu16, textArea, tokenIdx);
    };

    fromPlace = getFileLocation(fromPlace);
    if (fromPlace.offset == LOCATION_ERROR) {
        return getPlaceAtBeginningOfDocument();
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("Starting tokenizer at streamPlace %s", fromPlace.str().c_str());
    }

    // Needs to be minus one for the corner case of all spaces or newlines
    // and need room for EOF character.

    uint8_t curChar;
    while ((curChar = stream_->getNextByte()) && (!tokens.cStrPoolIsFull()) &&
           ((backwards || (textArea < maxTextArea)))) {
        newToken = {.cStr = nullptr,
                    .streamPlace = addOffset(fromPlace, pageIdx),
                    .prevNewParagraphTokenIdx = prevNewParagraphIdx,
                    .width = 0,
                    .height = 0,
                    .length = 0,
                    .tokenId = TokenId::ENDOFFILE};

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Set the stream place to %" PRIu32, fromPlace + pageIdx);
        //     log_d("Current Character: %c (%02" PRIX8 "H)",
        //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
        // }

        switch (curChar) {
        case '\n':
            // No two or more lines with only a \n are allowed.
            if (!((tokens.getCount() > 0) && (tokens.getLast().tokenId == TokenId::ENDPARAGRAPH))) {
                if ((tokens.getCount() == 0) || (tokens.getLast().tokenId == TokenId::NEWLINE)) {
                    newToken.tokenId = TokenId::ENDPARAGRAPH;
                    prevNewParagraphIdx = tokens.getCount();
                } else {
                    newToken.tokenId = TokenId::NEWLINE;
                }
                computePreviousTokenSize();
                tokens.add(newToken, "\n", 1);
            }
            break;

        case ' ':
            newToken.tokenId = TokenId::SPACE;
            computePreviousTokenSize();
            tokens.add(newToken, " ", 1);
            tokens.setSpaceSize(tokens.getCount() - 1);
            break;

        case '\t':
            newToken.tokenId = TokenId::TAB;
            display_.getTextSize("\t", &newToken.width, &newToken.height);
            computePreviousTokenSize();
            tokens.add(newToken, "\t", 1);
            break;

        case '-':
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
                computePreviousTokenSize();
                tokens.add(newToken, "-", 1);
            }
            break;

        // All other characters are considered part of a text token,
        // so we just increment and move on.
        default:
            // if constexpr (RENDERER_DEBUG) {
            //     log_d("Default...");
            // }
            // Will create a new token if
            // - This is the first token in the list, or
            // - The preceeding token was not TEXT, or
            // - There is more than (MAX_TOKEN_STRING_LENGTH - 5) bytes in the last TEXT token and
            //   curChar is a first byte of a UTF8 character.
            if ((tokens.getCount() == 0) || (tokens.getLast().tokenId != TokenId::TEXT) ||
                (((curChar <= 0x7FU) || (curChar >= 0xC0U)) &&
                 ((tokens.getLast().length > (MAX_BYTES_PER_TEXT_TOKEN - 5)) ||
                  (textCharCount >= maxTextTokenCharSize_)))) {
                newToken.tokenId = TokenId::TEXT;
                computePreviousTokenSize();
                tokens.add(newToken);
                tokens.addByteToLast(curChar);
                currentPageWordCount_ += 1;
                textCharCount = 1;
            } else {
                tokens.addByteToLast(curChar);
                if ((curChar <= 0x7FU) || (curChar >= 0xC0U)) {
                    textCharCount += 1;
                }
            }
            break;
        }

        pageIdx++;
    }

    if (tokens.cStrPoolIsFull()) {
        // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
        // EMDASH), so forget it
        tokens.forgetLastToken();
    }

    computePreviousTokenSize();

    Token eofToken = {.cStr = nullptr,
                      .streamPlace = addOffset(fromPlace, pageIdx),
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
