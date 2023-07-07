#include "Renderer.hpp"

#include <algorithm>
#include <utility>

#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

// For deep logging when changing this file
static constexpr bool RENDERER_DEBUG = false;
static constexpr bool ON_ERROR_EXIT = false;

// Must be called only in the context of loadAndTokenizePage() method.
// Takes a place in the token buffer and places the string that the token
// represents in the outputString buffer. This is a helper since it's a
// common thing we want to do in the rendering code.
void Renderer::tokenToString(Idx tokenIdx, TokenString &outputString) const {
    std::copy_n(&page_[tokens_[tokenIdx].pageIdx], tokens_[tokenIdx].length, &outputString[0]);
    outputString[tokens_[tokenIdx].length] = '\0';
    // log_d("String: [%s] page: [%s] tokenIdx: %" PRIu16, outputString.data(), page_.data(),
    // tokenIdx);
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
auto Renderer::setTokenSizes(Idx tokenIdx) -> uint16_t {
    TokenString tokenString; // 40 bytes on stack
    if (display_.getFont()->isA(FontType::IBMF)) {
        if (tokens_[tokenIdx].tokenId != TokenId::SPACE) {
            tokenToString(tokenIdx, tokenString);
            display_.getTextSize(tokenString.data(), &tokens_[tokenIdx].width,
                                 &tokens_[tokenIdx].height);
        } else {
            if (spaceWidth_ == 0) {
                display_.getTextSize(" ", &spaceWidth_, &spaceHeight_);
            }
            tokens_[tokenIdx].width = spaceWidth_;
            tokens_[tokenIdx].height = spaceHeight_;
        }
    } else {
        // With GFX Fonts, spaces are a mess. The width of them depends on the tokens before and
        // after. Joy.
        if (tokens_[tokenIdx].tokenId != TokenId::SPACE) {
            tokenToString(tokenIdx, tokenString);
            display_.getTextSize(tokenString.data(), &tokens_[tokenIdx].width,
                                 &tokens_[tokenIdx].height);
        } else {
            // Check bounds. Punt otherwise.
            // Get the tokens before and after the space, then do the width
            // of that whole thing and subtract out the two tokens. That's the
            // real width of the rendered space.
            if ((tokenIdx > 0) && (tokens_[tokenIdx + 1].tokenId != TokenId::ENDOFFILE)) {
                // ToDo: Very expensive.... 122 bytes in the stack
                TokenString nextTokenString; // 40 bytes on stack
                TokenStringDouble temp;      // 82 bytes on stack
                tokenToString(tokenIdx - 1, tokenString);
                tokenToString(tokenIdx + 1, nextTokenString);
                std::copy_n(tokenString.data(), strlen(tokenString.data()), &temp[0]);
                temp[strlen(tokenString.data())] = ' ';
                std::copy_n(nextTokenString.data(), strlen(nextTokenString.data()),
                            &temp[strlen(tokenString.data()) + 1]);
                temp[strlen(tokenString.data()) + strlen(nextTokenString.data()) + 1] = '\0';
                tokens_[tokenIdx].height = display_.getTextHeight(temp.data());
                tokens_[tokenIdx].width = display_.getTextWidth(temp.data()) -
                                          display_.getTextWidth(tokenString.data()) -
                                          display_.getTextWidth(nextTokenString.data());
            } else {
                tokenToString(tokenIdx, tokenString);
                tokens_[tokenIdx].width = display_.getTextWidth(tokenString.data());
                tokens_[tokenIdx].height = display_.getTextHeight(tokenString.data());
            }
        }
    }

    return (tokens_[tokenIdx].width * tokens_[tokenIdx].height);
}

// Must be called only by the loadAndTokenizePage().
// From the fromPlace variable, seek into the file and synchronize
// the value at a valid UTF8 location. An UTF8 character start with
// a first byte MSBit == 0 or the two MSBits == 11
//
// returns an updated fromPlace location or the LOCATION_ERROR value.
auto Renderer::getFileLocation(Place fromPlace) -> Place {

    // if received place is larger than the stream size, reset it to
    // the beginning of file
    if (fromPlace >= stream_->size()) {
        fromPlace = 0;
    }

    if (!stream_->seek(fromPlace)) {
        log_e("failed to seek at place %" PRIu32 " in file!", fromPlace);
        if constexpr (ON_ERROR_EXIT) {
            exit(2);
        }
    } else {
        while (stream_->available()) {
            uint8_t ch = stream_->read();
            // A valid UTF8 character is ASCII or got it's first byte
            // larger or equal to 0xC0
            if ((ch < 0x7FU) || (ch >= 0xC0U)) {
                stream_->seek(fromPlace);
                return fromPlace;
            }
            fromPlace += 1;
            if constexpr (RENDERER_DEBUG) {
                log_d("Synching to start of UTF8 character at place %" PRIu32, fromPlace);
            }
        }
    }

    return LOCATION_ERROR;
}

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
// Returns the computed place where the page start really.
//
// IMPORTANT: The getFileLocation() method must be called only through this method.
auto Renderer::loadAndTokenizePage(Place fromPlace, bool backwards) -> Place {

    Idx pageIdx = 0;
    Idx tokenIdx = 0;
    Idx prevNewParagraphIdx = 0;

    uint32_t textArea = 0;
    uint32_t maxTextArea = static_cast<uint32_t>(display_.width()) * display_.height();
    uint16_t textCharCount = 0;

    tokensCount_ = 0;
    currentPageWordCount_ = 0;
    spaceWidth_ = 0;

    page_[pageIdx] = '\0';

    Token eofToken = {.streamPlace = 0,
                      .pageIdx = 0,
                      .prevNewParagraphTokenIdx = 0,
                      .width = 0,
                      .height = 0,
                      .length = 0,
                      .tokenId = TokenId::ENDOFFILE};

    Token newToken;

    tokens_[tokenIdx] = eofToken;

    // Keep it DRY
    auto computePreviousTokenSize = [this, &textArea, &tokenIdx]() -> void {
        // log_d("computePreviousTokenSize()..., tokenIdx: %" PRIu16 ", tokensCount_: %" PRIu16,
        //       tokenIdx, tokensCount_);
        //  we need to set the size of the previous token
        if ((tokenIdx > 0) && (tokens_[tokenIdx - 1].tokenId == TokenId::TEXT)) {
            textArea += setTokenSizes(tokenIdx - 1);
            // If we've started a new text token and the previous token was a space
            // we know that we should go back and set the size of that space token
            // based on this token and the token before the space.
            if ((tokenIdx > 2) && (tokens_[tokenIdx - 2].tokenId == TokenId::SPACE)) {
                textArea += setTokenSizes(tokenIdx - 2);
            }
        }
        // log_d("textArea size: %" PRIu32 ", tokenIdx: %" PRIu16, textArea, tokenIdx);
    };

    auto appendToken = [this, &newToken, &tokenIdx, &eofToken]() -> void {
        tokens_[tokenIdx] = newToken;
        tokens_[tokenIdx + 1] = eofToken;

        // Be sure that we want overflow the tokens_ array
        if (tokensCount_ < (MAX_TOKEN_COUNT - 3)) {
            tokensCount_++;
            tokenIdx++;
        } else {
            log_e("tokens_[] too small!!");
            if constexpr (ON_ERROR_EXIT) {
                exit(3);
            }
        }
    };

    fromPlace = getFileLocation(fromPlace);
    if (fromPlace == LOCATION_ERROR) {
        return 0;
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("Starting tokenizer at place %" PRIu32, fromPlace);
    }

    // Needs to be minus one for the corner case of all spaces or newlines
    // and need room for EOF character.
    while ((stream_->available()) && (pageIdx < PAGE_BUFFER_SIZE) &&
           ((backwards || textArea < maxTextArea))) {
        uint8_t curChar = stream_->read();
        newToken = {.streamPlace = fromPlace + pageIdx,
                    .pageIdx = pageIdx,
                    .prevNewParagraphTokenIdx = prevNewParagraphIdx,
                    .width = 0,
                    .height = 0,
                    .length = 1,
                    .tokenId = TokenId::ENDOFFILE};

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Set the stream place to %" PRIu32, fromPlace + pageIdx);
        // }

        page_[pageIdx] = curChar;
        page_[pageIdx + 1] = '\0';

        // log_d("Current Character: %c (%02" PRIX8 "H)",
        //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);

        switch (curChar) {
        case '\n':
            // No two or more lines with only a \n are allowed.
            if (!((tokenIdx > 0) && (tokens_[tokenIdx - 1].tokenId == TokenId::ENDPARAGRAPH))) {
                if ((tokenIdx == 0) || (tokens_[tokenIdx - 1].tokenId == TokenId::NEWLINE)) {
                    newToken.tokenId = TokenId::ENDPARAGRAPH;
                    prevNewParagraphIdx = tokenIdx;
                } else {
                    newToken.tokenId = TokenId::NEWLINE;
                }
                computePreviousTokenSize();
                appendToken();
            }

            break;
        case ' ':
            newToken.tokenId = TokenId::SPACE;
            // Space size is cached to limit redondent computation
            if (spaceWidth_ == 0) {
                display_.getTextSize(" ", &spaceWidth_, &spaceHeight_);
            }
            newToken.width = spaceWidth_;
            newToken.height = spaceHeight_;
            computePreviousTokenSize();
            appendToken();

            break;
        case '\t':
            newToken.tokenId = TokenId::TAB;
            display_.getTextSize("\t", &newToken.width, &newToken.height);
            computePreviousTokenSize();
            appendToken();

            break;
        case '-':
            // Check to see if this is an emdash case
            if ((tokenIdx > 0) && (tokens_[tokenIdx - 1].tokenId == TokenId::HYPHEN)) {
                newToken.tokenId = TokenId::EMDASH;
                display_.getTextSize("--", &newToken.width, &newToken.height);
                newToken.pageIdx = pageIdx - 1;
                newToken.streamPlace = fromPlace + pageIdx - 1;
                newToken.length = 2;
                tokens_[tokenIdx - 1] = newToken;
                tokens_[tokenIdx] = eofToken;
                // Specifically don't increment the token buffer
                // because the previous hyphen already did that
            } else {
                newToken.tokenId = TokenId::HYPHEN;
                display_.getTextSize("-", &newToken.width, &newToken.height);
                computePreviousTokenSize();
                appendToken();
            }

            break;
        // All other characters are considered part of a text token,
        // so we just increment and move on.
        default:
            // Will create a new token if
            // - This is the first token in the list, or
            // - The preceeding token was not TEXT, or
            // - There is more than (MAX_TOKEN_STRING_LENGTH - 5) bytes in the last TEXT token and
            //   curChar is a first byte of a UTF8 character.
            if ((tokenIdx == 0) || (tokens_[tokenIdx - 1].tokenId != TokenId::TEXT) ||
                (((curChar <= 0x7FU) || (curChar >= 0xC0U)) &&
                 ((tokens_[tokenIdx - 1].length > (MAX_BYTES_PER_TEXT_TOKEN - 5)) ||
                  (textCharCount >= maxTextTokenCharSize_)))) {
                newToken.tokenId = TokenId::TEXT;
                computePreviousTokenSize();
                appendToken();
                currentPageWordCount_ += 1;
                textCharCount = 1;
            } else {
                tokens_[tokenIdx - 1].length += 1;
                if ((curChar <= 0x7FU) || (curChar >= 0xC0U)) {
                    textCharCount += 1;
                }
            }
            break;
        }

        if (pageIdx < PAGE_BUFFER_SIZE) {
            pageIdx++;
        } else {
            log_e("page_[] is too small !!!");
            if constexpr (ON_ERROR_EXIT) {
                exit(4);
            }
        }
    }

    // Ended on a text token, so we move forward one before setting the EOF
    // token.
    if ((tokenIdx < (MAX_TOKEN_COUNT - 2)) && (tokens_[tokenIdx].tokenId != TokenId::ENDOFFILE)) {
        tokenIdx++;
    }

    // Set the EOF place correctly.
    tokens_[tokenIdx].pageIdx = pageIdx;
    tokens_[tokenIdx].streamPlace = fromPlace + pageIdx;

    if constexpr (RENDERER_DEBUG) {
        log_d("Finished tokenizing with running text area %" PRIu32 " and max area %" PRIu32
              ", pageIdx is %" PRIu16 ", and place "
              "in file is %" PRIu32,
              textArea, maxTextArea, pageIdx, tokens_[tokenIdx].streamPlace);
        log_d("Computed Word Count: %" PRIu32, currentPageWordCount_);
    }

    // printOutTokens(); // Debugging - use only when required

    // we need to set the size of the last token if it wasn't set
    // (e.g., when there is no newline at the end)
    if (tokenIdx < tokensCount_) {
        computePreviousTokenSize();
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("Tokens Count: %" PRIu16, tokensCount_);
    }

    return fromPlace;
}

// debugging method
void Renderer::printOutTokens() const {
    if constexpr (RENDERER_DEBUG) {
        for (Idx i = 0; i < tokensCount_; i++) {
            auto currToken = &tokens_[i];
            log_d("TokenIdx: %" PRIu16 ", TokenId: %" PRIu8 ", streamPlace: %" PRIu32
                  ", pageIdx: %" PRIu16 ", prevNewParagraphIdx: %" PRIu16 ", width: %" PRIu16
                  ", height: %" PRIu16 ", Length: %" PRIu16,
                  i, (uint8_t)currToken->tokenId, currToken->streamPlace, currToken->pageIdx,
                  currToken->prevNewParagraphTokenIdx, currToken->width, currToken->height,
                  currToken->length);
            if (currToken->tokenId == TokenId::TEXT) {
                log_d("Text Content: [%.*s]", currToken->length, &page_[currToken->pageIdx]);
            }
        }
    }
}

void Renderer::computeMaxTextTokenCharSize() {
    // Using character 'G' is a compromize to compute the number of characters that mostly fit a
    // line, between a 'n' and a 'm' in size, on the various font sizes, to try to fit better long
    // words with the available line width.

    maxTextTokenCharSize_ = (display_.width() - 4) / display_.getTextWidth("G");
}

// This method takes in a place in the token buffer.
// It puts the string in the line parameter.
// It returns the next token idx in the TokenBuffer unless already at EOF, then
// it returns EOF again.
auto Renderer::getLineOfTextForRendering(Idx startTokenIdx, Line &line) -> Idx {
    Idx tokenIdx = startTokenIdx;
    Idx lineIdx = 0;
    int16_t maxWidth = (display_.width() - 4);
    int16_t runningWidth = 0;

    while (runningWidth < maxWidth) {
        Token &token = tokens_[tokenIdx];

        if (token.tokenId == TokenId::ENDOFFILE || token.tokenId == TokenId::NEWLINE ||
            token.tokenId == TokenId::ENDPARAGRAPH) {
            if (token.tokenId == TokenId::NEWLINE) {
                tokenIdx++;
            }
            break;
        }
        runningWidth += token.width;
        tokenIdx++;
    }

    // Go back one token as that was the token that caused the line to be full.
    if (runningWidth >= maxWidth) {
        tokenIdx--;
    }

    // This handles the case where a single token is longer than the width of a line.
    // Eventually, we'll break long tokens, but for now, we'll just let it run over the length
    // of the line as the break-up text is much more complicated.
    if (tokenIdx == startTokenIdx) {
        tokenIdx++;
    }

    if constexpr (RENDERER_DEBUG) {
        if ((tokens_[tokenIdx - 1].tokenId != TokenId::NEWLINE) &&
            (tokens_[tokenIdx].tokenId == TokenId::NEWLINE)) {
            log_e("End Of Line Token not being consumed!!!");
            if constexpr (ON_ERROR_EXIT) {
                exit(5);
            }
        }
    }

    for (Idx i = startTokenIdx; i < tokenIdx; i++) {
        if (tokens_[i].tokenId != TokenId::ENDOFFILE) {
            // Skip over a space if it's the first token in the line.
            if ((i == startTokenIdx) && (tokens_[i].tokenId == TokenId::SPACE)) {
                continue;
            }
            if ((lineIdx + tokens_[i].length) < (sizeof(line) - 2)) {
                std::copy_n(&page_[tokens_[i].pageIdx], tokens_[i].length, &line[lineIdx]);
                lineIdx += tokens_[i].length;
            } else {
                log_e("line[] too small!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(6);
                }
            }
        }
    }

    line[lineIdx] = '\0';

    if constexpr (RENDERER_DEBUG) {
        log_d("Returning from tokenIdx %" PRIu16 "(%" PRIu32 ") to %" PRIu16 "(%" PRIu32
              "), width: %" PRIi16 " line: %s",
              startTokenIdx, tokens_[startTokenIdx].streamPlace, static_cast<Idx>(tokenIdx - 1),
              tokens_[tokenIdx - 1].streamPlace, runningWidth, line.data());
    }

    return tokenIdx;
}

// Show the (already loaded into buffer) current page
// and load the next page into the next buffer so we have it.
auto Renderer::renderPage(Place fromPlace, int16_t numSkipLines) -> Place {

    if constexpr (RENDERER_DEBUG) {
        log_d("%" PRIu32 " is place in file being requested", fromPlace);
    }

    currentPlace_ = fromPlace;

    // This needs to happen before loading into buffer since it will affect what we load into the
    // buffer.
    display_.setFont(display_.style.getFontFamily().readableFont);

    computeMaxTextTokenCharSize();

    PROFILE_START(loadCurrentPage);
    nextPlace_ = loadCurrentPage();
    PROFILE_END_LOG_MS(loadCurrentPage);

    int16_t newLineAdjust = 0;

    // A first attempt to get the the text to look better with newlines as sole
    // characters on the line with a smaller height than a full line.
    // The logic is that we split each line into 10 "decilines" and then
    // assign a height to each line based on if it's a newline or not. Newlines
    // get ENDPARAGRAPH_DECI_LINE_HEIGHT decilines (currently set to 4 out of 10).
    // The y position calculated is based on the number of newlines we've seen so far
    // in this page.

    for (Idx j = 0; j < pageLinesCount_; j++) {
        if constexpr (RENDERER_DEBUG) {
            log_d("%s", pageLines_[j].data());
        }
        if (pageLines_[j][0] == '\n') {
            newLineAdjust++;
            if constexpr (RENDERER_DEBUG) {
                log_w("NewLineAdjust for line %" PRIu16, j);
            }
        } else {
            // Only draw the lines if it's not skipped from the caller.
            if (j >= numSkipLines) {
                display_.drawSingleLineOfText(
                    pageLines_[j].data(), 0,
                    j * (display_.getTextHeight()) -
                        display_.getTextHeight() *
                            ((NEWLINE_DECI_LINE_HEIGHT - ENDPARAGRAPH_DECI_LINE_HEIGHT) *
                             newLineAdjust) /
                            NEWLINE_DECI_LINE_HEIGHT);
            }
        }
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("Returning next page place to be %" PRIu32, nextPlace_);
    }
    return nextPlace_;
}

auto Renderer::getParagraphDeciLinesCount(Idx startTokenIdx, Idx endTokenIdx) -> uint16_t {

    Idx tokenIdx = startTokenIdx;
    uint16_t deciLinesCount = 0;
    Line singleLine{};

    // Initialize the paragraph lines vector pointers. The vector
    // will receive the last lines pointers from the paragraph (at most
    // MAX-PARAGRAPH_LINES) and will serve to get back the first line of
    // the back page.
    paraLineFirstIdx_ = paraLineLastIdx_ = 0;

    if constexpr (RENDERER_DEBUG) {
        log_d("Paragraph startIdx: %" PRIu16 "(%" PRIu32 "), endIdx: %" PRIu16 "(%" PRIu32 ")",
              startTokenIdx, tokens_[startTokenIdx].streamPlace, endTokenIdx,
              tokens_[endTokenIdx].streamPlace);
    }

    while (tokenIdx <= endTokenIdx) {
        if (tokens_[tokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
            deciLinesCount += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            paraLineIdxs_[paraLineLastIdx_] = tokenIdx;
            tokenIdx++;
        } else {
            paraLineIdxs_[paraLineLastIdx_] = tokenIdx;
            if constexpr (RENDERER_DEBUG) {
                Idx firstTokenIdx = tokenIdx;
                tokenIdx = getLineOfTextForRendering(tokenIdx, singleLine);
                log_d("Line at firstTokenIdx %" PRIu16 "(%" PRIu32 "), next line tokenIdx: %" PRIu16
                      "(%" PRIu32 "), Content: %s",
                      firstTokenIdx, tokens_[firstTokenIdx].streamPlace, tokenIdx,
                      tokens_[tokenIdx].streamPlace, singleLine.data());
            } else {
                tokenIdx = getLineOfTextForRendering(tokenIdx, singleLine);
            }
            deciLinesCount += NEWLINE_DECI_LINE_HEIGHT;
        }

        paraLineLastIdx_ = (paraLineLastIdx_ + 1) % MAX_PARAGRAPH_LINES;
        if (paraLineLastIdx_ == paraLineFirstIdx_) {
            paraLineFirstIdx_ = (paraLineFirstIdx_ + 1) % MAX_PARAGRAPH_LINES;
        }
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("DeciLinesCount: %" PRIu16, deciLinesCount);
    }

    return deciLinesCount;
}

auto Renderer::getFirstLineOfPageTokenIdx(Idx lastTokenIdx, uint16_t deciLinesCount,
                                          uint16_t maxDeciLines) const -> Idx {

    if constexpr (RENDERER_DEBUG) {
        log_d("Starting at tokenIdx: %" PRIu16 ", and deciLinesCount %" PRIu16
              ", With max of deciLines: %" PRIu16,
              lastTokenIdx, deciLinesCount, maxDeciLines);
    }

    Idx currentParaLineIdx = paraLineLastIdx_ == 0 ? MAX_PARAGRAPH_LINES - 1 : paraLineLastIdx_ - 1;

    Idx returnTokenIdx = lastTokenIdx;
    Idx tokenIdx = lastTokenIdx;
    while (deciLinesCount <= maxDeciLines) {
        returnTokenIdx = tokenIdx;
        tokenIdx = paraLineIdxs_[currentParaLineIdx];
        if (tokens_[tokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
            deciLinesCount += ENDPARAGRAPH_DECI_LINE_HEIGHT;
        } else {
            deciLinesCount += NEWLINE_DECI_LINE_HEIGHT;
        }

        if ((deciLinesCount == maxDeciLines) || (tokenIdx == 0)) {
            returnTokenIdx = tokenIdx;
            break;
        }

        // The following should never happen as MAX_PARAGRAPH_LINES permits more lines than
        // the display height.
        if ((deciLinesCount < maxDeciLines) && (currentParaLineIdx == paraLineFirstIdx_)) {
            log_e("Last Paragraph line count not big enough: FirstIdx %" PRIu16 ", LastIdx %" PRIu16
                  ", Current %" PRIu16 "; deciLineCount %" PRIi16 ", Max %" PRIi16 ".",
                  paraLineFirstIdx_, paraLineLastIdx_, currentParaLineIdx, deciLinesCount,
                  maxDeciLines);
            if constexpr (ON_ERROR_EXIT) {
                exit(7);
            }
            break;
        }

        currentParaLineIdx =
            currentParaLineIdx == 0 ? MAX_PARAGRAPH_LINES - 1 : currentParaLineIdx - 1;
    }

    return returnTokenIdx;
}

// Seeks backwards and returns the place of one page previous.
// This function's purpose is only to find the place in the file that's one
// page previous, not to load anything into buffers.
//
// This is the most complicated alogrithm in the whole renderer currently.
//
// In order to make this work correctly while not keeping the whole readable
// in memory, we must find a previous point before the current place where
// we know a token is at the beginning of a line. That's the only way to
// ensure that our "backwards render" will match what happened in the similar
// forwards render.
//
// To do this, the method is split into a couple of parts.
//
// Step 1: Figure out how much to read before the current place. If we're too
// close to the beginning, we just read from the beginning.
//
// Step 2: Once the data is read into the buffer, tokenize it the same as you
// would for a forwards render. seekPlace will be returned to be at a UTF8 character
// boundary.
//
// Step 3: Find the place in the token buffer where the passed in place in file
// is represented.
//
// Step 4: Compute the startTokenIdx and endTokenIdx for the last paragraph.
//
// Step5: Iterate each paragraph in reverse order until we get at least enough deciLines to
// cover the page size.
//
// Step 6: Retrieve lines from the last scanned paragraph enough to have it fit in the
// available page space, starting at the end-of-paragraph marker.
//
// Got the location!
//
auto Renderer::goBackwardsOnePage(Place fromPlace) -> Place {
    Place seekPlace = 0;

    display_.setFont(display_.style.getFontFamily().readableFont);
    computeMaxTextTokenCharSize();

    // Step 1: Figure out how much to read before the current place. If we're too
    // close to the beginning, we just read from the beginning.
    if (fromPlace > MAX_BYTES_TO_RETRIEVE) {
        // Need to make sure we're at least 1 character past the place given. For tokenizer.
        seekPlace = fromPlace - MAX_BYTES_TO_RETRIEVE;
    } else {
        seekPlace = 0;
    }

    // Step 2: Once the data is read into the buffer, tokenize it the same as you
    // would for a forwards render. seekPlace will be returned to be at a UTF8 character
    // boundary
    seekPlace = loadAndTokenizePage(seekPlace, true); // NOLINT(clang-analyzer-deadcode.DeadStores)

    // Step 3: Find the place in the token buffer where the passed in place in file
    // is represented.
    Idx endTokenIdx = tokensCount_ - 1;

    if constexpr (RENDERER_DEBUG) {
        log_d("BACKWARDS DISPLAY READOUT: %d, %d, %d", display_.height() - 2,
              this->display_.getTextHeight(), this->display_.getLineSpacing());
    }

    // Find which token to end with
    // Could switch to an algorithm that is log(n) instead of n if we need to
    if constexpr (RENDERER_DEBUG) {
        log_d("About to look for where to end: place = %" PRIu32 ", starting at place %" PRIu32,
              fromPlace, seekPlace);
    }

    while ((endTokenIdx > 0) && (tokens_[endTokenIdx].streamPlace > fromPlace)) {
        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Going back another token at place %" PRIu32,
        //     tokens_[endTokenIdx].streamPlace); log_d("The token idx is %" PRIu16, endTokenIdx);
        // }
        endTokenIdx--;
    }

    if (endTokenIdx == 0) {
        // Can't go back any further, must be at beginning of file or the paragraph is larger than
        // MAX_CHARS_TO_RETRIEVE bytes.
        return 0;
    }

    if constexpr (RENDERER_DEBUG) {
        if (tokens_[endTokenIdx].streamPlace != fromPlace) {
            log_e("Synchronisation issue. Unable to point at fromPlace with endTokenIdx.");
            if constexpr (ON_ERROR_EXIT) {
                exit(8);
            }
        }
    }

    // The lastTokenIdx is a pointer on the last location of the paragaph that is
    // preceeding the last paragraph to be retrieved. It will be served to identify
    // the potential first line of the page if all of the last paragraph retrieved
    // lines would be off from the back page. For the first iteration, it is pointing
    // at the past place tokenIdx
    Idx lastTokenIdx = endTokenIdx;

    // Step 4: Compute the startTokenIdx and endTokenIdx for the last paragraph.
    // Point at the token preceeding the passed place.
    endTokenIdx -= 1;

    if constexpr (RENDERER_DEBUG) {
        log_d("Found the place to end and it's at place %" PRIu32 ", tokenId %" PRIu8
              ", tokenIdx: %" PRIu16,
              tokens_[endTokenIdx].streamPlace, (uint8_t)tokens_[endTokenIdx].tokenId, endTokenIdx);
    }

    Idx startTokenIdx = tokens_[endTokenIdx].prevNewParagraphTokenIdx;

    // if it's not a ENDPARAGRAPH, it means we maybe at the beginning of the file
    if (tokens_[startTokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
        startTokenIdx += 1;
    } else if (tokens_[startTokenIdx].streamPlace != 0) {
        log_w("We may not have reached the beginning of previous paragraph.");
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("Last paragraph boundaries: %" PRIu16 "(%" PRIu32 ") through %" PRIu16 "(%" PRIu32
              ")",
              startTokenIdx, tokens_[startTokenIdx].streamPlace, endTokenIdx,
              tokens_[endTokenIdx].streamPlace);
    }

    // Step5: Iterate each paragraph in reverse order until we get at least enough deciLines to
    // cover the page size.
    int16_t deciLinesCount = 0;

    int16_t maxDeciLines = getMaxDeciLines();

    uint16_t lastDeciLinesCount = deciLinesCount;

    while (deciLinesCount < maxDeciLines) {
        lastDeciLinesCount = deciLinesCount;
        deciLinesCount += getParagraphDeciLinesCount(startTokenIdx, endTokenIdx);

        if constexpr (RENDERER_DEBUG) {
            log_w("Max: %" PRIu16 ", We are at: %" PRIu16, maxDeciLines, deciLinesCount);
        }

        if (deciLinesCount > maxDeciLines) {
            break;
        }

        lastTokenIdx = startTokenIdx;

        if (startTokenIdx == 0) {
            if constexpr (RENDERER_DEBUG) {
                log_w("Reached beginning of file");
            }
            break;
        }

        if (deciLinesCount == maxDeciLines) {
            if constexpr (RENDERER_DEBUG) {
                log_w("Exact match!");
            }
            break;
        }

        endTokenIdx = startTokenIdx - 1;
        startTokenIdx = tokens_[endTokenIdx].prevNewParagraphTokenIdx;

        // if it's not a ENDPARAGRAPH, it means we are at the beginning of the token list
        if (tokens_[startTokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
            startTokenIdx += 1;
        } else if (tokens_[startTokenIdx].streamPlace != 0) {
            log_w("We may not have reached the beginning of previous paragraph.");
        }
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("deciLinesCount: %" PRIu16 ", max: %" PRIu16, deciLinesCount, maxDeciLines);
    }

    // Step 6: Retrieve lines from the last scanned paragraph enough to have
    // it fit in the available page space.

    if (deciLinesCount > maxDeciLines) {
        startTokenIdx = getFirstLineOfPageTokenIdx(lastTokenIdx, lastDeciLinesCount, maxDeciLines);
    } else {
        startTokenIdx = lastTokenIdx;
    }

    // Got it!
    Place returnPlace = tokens_[startTokenIdx].streamPlace;

    // methods calling this method expect this to not change what's loaded
    // into buffers, so we need to retokenize.
    // we could probably avoid this rework by introducing an extra token buffer if we wanted to.
    // log_d("loadAndTokenizePage called: need to retokenize");

    static_cast<void>(loadAndTokenizePage(fromPlace));

    if constexpr (RENDERER_DEBUG) {
        log_w("Returning the following place! %" PRIu32, returnPlace);
    }

    return returnPlace;
}

auto Renderer::getTotalLengthOfLines() const -> uint16_t {
    uint16_t totalLengthsOfLines = 0;
    for (Idx i = 0; i < pageLinesCount_; i++) {
        totalLengthsOfLines += pageLinesLengths_[i];
    }
    return totalLengthsOfLines;
}

// returns the new place of the stream
auto Renderer::loadCurrentPage() -> Place {

    int16_t deciLinesCount = 0;

    Line singleLine{};

    int16_t maxDeciLines = getMaxDeciLines();

    if constexpr (RENDERER_DEBUG) {
        log_d("Max lines calc: %d %d %d", display_.height(), display_.getTextHeight(),
              display_.getLineSpacing());
    }

    singleLine[0] = '\0';

    // Debugging stuff
    // log_d("loadAndTokenizePage called: debugging stuff");
    currentPlace_ = loadAndTokenizePage(currentPlace_);

    Idx nextTokenIdx = 0;
    Idx tokenIdx = 0;

    pageLinesCount_ = 0;

    int16_t lastDeciLinesCount = 0;

    while ((deciLinesCount <= maxDeciLines) && (tokens_[tokenIdx].tokenId != TokenId::ENDOFFILE)) {
        if constexpr (RENDERER_DEBUG) {
            lastDeciLinesCount = deciLinesCount;
        }
        // Don't start pages with new paragraphs.
        if (nextTokenIdx == 0) {
            if (tokens_[tokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
                tokenIdx++;
                deciLinesCount += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            }
        }

        if (tokens_[tokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
            if constexpr (RENDERER_DEBUG) {
                log_w("NEW PARAGRAPH on line %" PRIu16, pageLinesCount_);
            }
            nextTokenIdx = tokenIdx + 1;
            pageLines_[pageLinesCount_][0] = '\n';
            pageLines_[pageLinesCount_][1] = '\0';
            deciLinesCount += ENDPARAGRAPH_DECI_LINE_HEIGHT;
        } else {
            // If the last page is all new lines, we need to not crash.
            // Put a single space witn newline in the page so something renders, and continue.
            if (tokens_[tokenIdx].tokenId == TokenId::ENDOFFILE) {
                singleLine[0] = ' ';
                singleLine[1] = '\n';
                singleLine[2] = '\0';
                nextTokenIdx = tokenIdx;
            } else {
                nextTokenIdx = getLineOfTextForRendering(tokenIdx, singleLine);
            }

            std::copy_n(singleLine.data(), strlen(singleLine.data()),
                        pageLines_[pageLinesCount_].data());

            pageLines_[pageLinesCount_][strlen(singleLine.data())] = '\0';
            pageLinesLengths_[pageLinesCount_] = strlen(singleLine.data());

            deciLinesCount += NEWLINE_DECI_LINE_HEIGHT;
        }

        if (deciLinesCount <= maxDeciLines) {
            if (pageLinesCount_ < (MAX_LINES_PER_PAGE - 2)) {
                pageLinesCount_++;
            } else {
                log_e("pageLines_[] too small!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(9);
                }
            }
            tokenIdx = nextTokenIdx;
        }
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("lastDeciLinesCount: %" PRIi16 ", max: %" PRIi16, lastDeciLinesCount, maxDeciLines);
    }

    // if ((lastDeciLinesCount > maxDeciLines) || (lastDeciLinesCount < (maxDeciLines - 10))) {
    //     log_e("lastDeciLinesCount: %" PRIi16 " not supposed to happen!! max: %" PRIi16,
    //           lastDeciLinesCount, maxDeciLines);
    //     if constexpr (ON_ERROR_EXIT) {
    //         exit(10);
    //     }
    // }

    // If we hit EOF, the next screen wraps around.
    if (tokens_[tokenIdx].tokenId == TokenId::ENDOFFILE) {
        return 0;
    }

    return tokens_[tokenIdx].streamPlace;
}

auto Renderer::getPreviousPlace() -> Place {
    PROFILE_START(goBackwardsOnePage)
    previousPlace_ = goBackwardsOnePage(currentPlace_);
    PROFILE_END_LOG_MS(goBackwardsOnePage)
    return previousPlace_;
}
