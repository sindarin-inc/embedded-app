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
auto TxtTokenizer::loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, Pager &pager,
                                       bool backwards, StreamPlace *toPlace) -> StreamPlace {

    Idx prevNewParagraphIdx = 0;
    Idx streamRelativeOffset = 0;

    tokens.clear();
    pager.clear();

    Token newToken;

    fromPlace = getFileLocation(fromPlace);

    if (fromPlace.offset == LOCATION_ERROR) {
        return getPlaceAtBeginningOfDocument();
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("Starting tokenizer at streamPlace %s", fromPlace.str().c_str());
        if (backwards) {
            log_w("Going up to place %s", toPlace->str().c_str());
        }
    }

    bool pageCompleted = false;

    // Keep it DRY
    auto addToPage = [&pageCompleted, &backwards, &pager]() {
        pageCompleted = !pager.addLastTokenToPage(backwards) && !backwards;
    };

    uint8_t curChar = stream_->getNextByte();

    auto nextByte = [this, &curChar, &streamRelativeOffset]() -> void {
        curChar = stream_->getNextByte();
        streamRelativeOffset++;
    };

    // If we goo backward, get rid of the first bytes that could be the end of a line/paragraph
    if (backwards && (fromPlace.offset != 0)) {
        while ((curChar != 0) && (curChar != '\n')) {
            nextByte();
        }
        if (curChar == '\n') {
            nextByte();
        }
    }

    // if constexpr (RENDERER_DEBUG) {
    //     log_d("First Character: %c (%02" PRIX8 "H)",
    //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
    // }

    while ((!pageCompleted) && (curChar != 0) && (!tokens.isFull())) {

        newToken = {.cStr = nullptr,
                    .streamPlace = addOffset(fromPlace, streamRelativeOffset),
                    .prevNewParagraphTokenIdx = prevNewParagraphIdx,
                    .width = 0,
                    .height = 0,
                    .length = 0,
                    .tokenId = TokenId::NOTOKEN};

        if (backwards && !tokens.isEmpty() && (newToken.streamPlace.offset >= toPlace->offset)) {
            pager.completeLastLine();
            break;
        }

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Set the stream place to %s",
        //           addOffset(fromPlace, streamRelativeOffset).str().c_str());
        //     log_d("Current Character: %c (%02" PRIX8 "H)",
        //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
        // }

        switch (curChar) {
        case '\n':
            if (!pageCompleted) {
                // No two or more lines with only a \n are allowed.
                newToken.tokenId = TokenId::NEWLINE;
                tokens.add(newToken, "\n", 1);
                addToPage();
                if (!pageCompleted) {
                    nextByte();
                    if (curChar == '\n') {
                        prevNewParagraphIdx = tokens.getCount();
                        newToken.tokenId = TokenId::ENDPARAGRAPH;
                        tokens.add(newToken, "\n", 1);
                        addToPage();
                        nextByte();
                        while (curChar == '\n') {
                            nextByte();
                        }
                    }
                }
            }
            break;

        case ' ':
            if (!pageCompleted) {
                newToken.tokenId = TokenId::SPACE;
                tokens.setSpaceSize(newToken);
                tokens.add(newToken, " ", 1);
                addToPage();
                nextByte();
            }
            break;

        case '\t':
            if (!pageCompleted) {
                newToken.tokenId = TokenId::TAB;
                tokens.add(newToken, "\t", 1);
                addToPage();
                nextByte();
            }
            break;

        case '-':
            if (!pageCompleted) {
                nextByte();
                if (curChar != '-') {
                    newToken.tokenId = TokenId::HYPHEN;
                    tokens.add(newToken, "-", 1);
                } else {
                    nextByte();
                    if (curChar != '-') {
                        newToken.tokenId = TokenId::ENDASH;
                        tokens.add(newToken, "--", 2);
                    } else {
                        nextByte();
                        newToken.tokenId = TokenId::EMDASH;
                        tokens.add(newToken, "---", 3);
                    }
                }
                addToPage();
            }
            break;

        // All other characters are considered part of a text token,
        default:
            newToken.tokenId = TokenId::TEXT;
            uint16_t textCharCount = 0;

            if (!pageCompleted) {
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
                // The last test of the while above is to get a complete UTF8 multi-bytes character
                // if the maximum word size is reached

                tokens.add(newToken);
                addToPage();
            }
            break;
        }
    }

    if (!pageCompleted) {
        if (tokens.cStrIsFull()) {
            // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
            // EMDASH), so forget it
            tokens.forgetLast();
        }
    }

    if (curChar == 0) {
        pager.endOfFileReached();
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("Computed Word Count: %" PRIu16, pager.getWordCount());
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
