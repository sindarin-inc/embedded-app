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
auto TxtTokenizer::loadAndTokenizePage(StreamPlace fromPlace, bool backwards, StreamPlace *toPlace)
    -> std::pair<StreamPlace, StreamPlace> {

    uint32_t streamRelativeOffset = 0;

    lastToken_ = nullptr;

    fromPlace = getFileLocation(fromPlace);

    if (fromPlace.getOffset() == LOCATION_ERROR) {
        return {getPlaceAtBeginningOfDocument(), {0, 0}};
    }

    if constexpr (REND_DEBUG) {
        log_w("Starting tokenizer at streamPlace %s", fromPlace.str().c_str());
        if (backwards) {
            log_w("Going up to place %s", toPlace->str().c_str());
        }
    }

    bool pageCompleted = false;

    // Keep it DRY

    uint8_t curChar = stream_->getNextByte();

    auto nextByte = [this, &curChar, &streamRelativeOffset]() -> void {
        curChar = stream_->getNextByte();
        streamRelativeOffset++;
    };

    // If we go backwards, get rid of the first bytes that could be the end of a line/paragraph
    if (backwards && (fromPlace.getOffset() != 0)) {
        while ((curChar != 0) && (curChar != '\n')) {
            nextByte();
        }
        if (curChar == '\n') {
            nextByte();
        }
    }

    // REND_WLOG("First Character: %c (%02" PRIX8 "H)",
    //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);

    while ((!pageCompleted) && (curChar != 0) && (!tokens_.isFull())) {

        Token &newToken = tokens_.getNextAvailable();

        newToken = {.cStr = nullptr,
                    .picture = nullptr,
                    .streamPlace = addOffset(fromPlace, streamRelativeOffset),
                    .pixWidth = 0,
                    .pixHeight = 0,
                    .length = 0,
                    .tokenId = TokenId::TEXT,
                    .spacesAfter = 0};

        if (backwards && !tokens_.isEmpty() &&
            (newToken.streamPlace.getOffset() >= toPlace->getOffset())) {
            pager_.completeLastLine();
            break;
        }

        // REND_WLOG("Set the stream place to %s",
        //       addOffset(fromPlace, streamRelativeOffset).str().c_str());
        // REND_WLOG("Current Character: %c (%02" PRIX8 "H)",
        //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);

        if (curChar == '-') {
            nextByte();
            if (curChar != '-') {
                newToken.tokenId = TokenId::HYPHEN;
                pageCompleted = addTokenToPage(newToken, backwards, "-", 1);
            } else {
                nextByte();
                if (curChar != '-') {
                    newToken.tokenId = TokenId::ENDASH;
                    pageCompleted = addTokenToPage(newToken, backwards, "--", 2);
                } else {
                    nextByte();
                    newToken.tokenId = TokenId::EMDASH;
                    pageCompleted = addTokenToPage(newToken, backwards, "---", 3);
                }
            }
            lastToken_ = &newToken;
        } else if (curChar > ' ') {
            // newToken.tokenId = TokenId::TEXT; // already set at beginning of loop
            lastToken_ = &newToken;
            uint16_t textCharCount = 0;

            do {
                if ((curChar <= 0x7FU) || (curChar >= 0xC0U)) {
                    textCharCount += 1;
                }
                if (!tokens_.cStrIsFull()) {
                    tokens_.addByte(newToken, curChar);
                } else {
                    break;
                }
                if (curChar == '-') {
                    nextByte();
                    break;
                }
                nextByte();
            } while ((curChar > ' ') &&
                     ((textCharCount < maxTextTokenCharSize_) || (curChar >= 0x80U)));

            tokens_.endOfCStr();

            // The last test of the while above is to get a complete UTF8 multi-bytes character
            // if the maximum word size is reached

            pageCompleted = addTokenToPage(newToken, backwards);
        } else if ((curChar == ' ') || (curChar == '\t')) {

            if (lastToken_ != nullptr) {
                lastToken_->spacesAfter++;
                pager_.addSpaceWidth();
            }
            // newToken.tokenId = TokenId::SPACE;
            // tokens_.setSpaceSize(newToken);
            // pageCompleted = addTokenToPage(newToken, backwards, " ", 1);
            nextByte();
        } else if (curChar == '\n') {
            if (tokens_.getTokenCount() == 0) {
                // of a newline character appears at the beginning of the parsing process, it must
                // be an end of paragraph, as the preceding newline was absorbed by the preceding
                // page
                newToken = {.cStr = nullptr,
                            .picture = nullptr,
                            .streamPlace = addOffset(fromPlace, streamRelativeOffset),
                            .pixWidth = 0,
                            .pixHeight = 0,
                            .length = 0,
                            .tokenId = TokenId::ENDPARAGRAPH,
                            .spacesAfter = 0};
                pageCompleted = addTokenToPage(newToken, backwards, "\n", 1);
                nextByte();
                // No two or more lines with only a \n are allowed.
                while (curChar == '\n') {
                    nextByte();
                }
            } else {
                newToken.tokenId = TokenId::NEWLINE;
                lastToken_ = nullptr;
                pageCompleted = addTokenToPage(newToken, backwards, "\n", 1);
                if (!pageCompleted) {
                    do {
                        nextByte();
                    } while ((curChar != '\0') && (curChar < ' ') && (curChar != '\n') &&
                             (curChar != '\t'));
                    if (curChar == '\n') {
                        // log_w("Stream relative offset:%" PRIu32, streamRelativeOffset);
                        if (backwards && !tokens_.isEmpty() &&
                            ((fromPlace.getOffset() + streamRelativeOffset) >=
                             toPlace->getOffset())) {
                            pageCompleted = true;
                            break;
                        }

                        Token &newToken = tokens_.getNextAvailable();

                        newToken = {.cStr = nullptr,
                                    .picture = nullptr,
                                    .streamPlace = addOffset(fromPlace, streamRelativeOffset),
                                    .pixWidth = 0,
                                    .pixHeight = 0,
                                    .length = 0,
                                    .tokenId = TokenId::ENDPARAGRAPH,
                                    .spacesAfter = 0};
                        pageCompleted = addTokenToPage(newToken, backwards, "\n", 1);
                        nextByte();
                        // No two or more lines with only a \n are allowed.
                        while (curChar == '\n') {
                            nextByte();
                        }
                    }
                }
            }
        } else {
            nextByte();
        }
    }

    if (!pageCompleted && tokens_.cStrIsFull()) {
        // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
        // EMDASH), so forget it
        tokens_.forgetLast();
    }

    if (curChar == 0) {
        pager_.setEndOfFile();
    }

    REND_WLOG("Computed Word Count: %" PRIu16, pager_.getWordCount());
    REND_WLOG("Tokens Count: %" PRIu16, tokens_.getTokenCount());

    if (backwards) {
        pager_.setFirstLineIdxBackward();
        pager_.setNextPagePlace(*toPlace);
    }

    return {pager_.getStreamPlaceOfFirstLine(), pager_.getNextPageStreamPlace()};
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
    if (fromPlace.getOffset() >= stream_->size()) {
        fromPlace = getPlaceAtBeginningOfDocument();
    }

    // When the offset is 0, we are at a the beginning of a file, so it’s a word boundary.
    bool atWordBoundary = (fromPlace.getOffset() == 0);
    if (!atWordBoundary) {
        // We need to start at the previous byte to check for a word boundary.
        fromPlace.decOffset();
    }

    if (!stream_->seek(fromPlace.getOffset())) {
        log_e("failed to seek at streamPlace %s in file!", fromPlace.str().c_str());
        REND_EXIT_SIM;
    } else {
        uint8_t ch = 0;
        uint16_t count = 0;

        // If not at word boundary, go forward and stop once we found one.
        // Word boundaries are:
        // - any byte with value that is lower or equal to a space (0x20). This cover any
        //   of space, tab, newline or any other special characters.
        // - any byte with value '-': this is a partial one as many hyphen can be present.
        //   (see below the while loop)
        while ((!atWordBoundary) && stream_->available()) {
            ch = stream_->read();
            atWordBoundary = ((ch <= ' ') || (ch == '-'));
            fromPlace.incOffset();
            count += 1;
        }

        if (ch == '-') {
            // Pass hyphens only if found in the first loop above, else it's going to be
            // the first shown glyph on the page (hyphen, en-dash, em-dash).
            // Single hyphen are normally shown glued to the preceeding word, but here there is
            // no simple way to manage it.
            if (count == 1) {
                // Ensure that we skip all other hyphens that are present at word boundary
                while (stream_->available()) {
                    ch = stream_->read();
                    if (ch != '-') {
                        break;
                    }
                    fromPlace.incOffset();
                }
            } else {
                fromPlace.decOffset();
            }
            stream_->seek(fromPlace.getOffset());
        }

        while (stream_->available()) {
            ch = stream_->read();
            // A valid UTF8 character is ASCII or got it's first byte
            // larger or equal to 0xC0
            if ((ch < 0x7FU) || (ch >= 0xC0U)) {
                stream_->seek(fromPlace.getOffset());
                return fromPlace;
            }
            fromPlace.incOffset();
            if constexpr (REND_DEBUG) {
                log_d("Synching to start of UTF8 character at streamPlace %s",
                      fromPlace.str().c_str());
            }
        }
    }

    fromPlace.setOffset(LOCATION_ERROR);
    return fromPlace;
}
