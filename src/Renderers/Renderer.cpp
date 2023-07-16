#include "Renderer.hpp"

#include <algorithm>

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
        const Token &token = tokens_[tokenIdx];

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
                std::copy_n(tokens_[i].cStr, tokens_[i].length, &line[lineIdx]);
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
        log_d("Returning from tokenIdx %" PRIu16 " %s to %" PRIu16 " %s, width: %" PRIi16
              " line: %s",
              startTokenIdx, tokens_[startTokenIdx].streamPlace.str().c_str(),
              static_cast<Idx>(tokenIdx - 1), tokens_[tokenIdx - 1].streamPlace.str().c_str(),
              runningWidth, line.data());
    }

    return tokenIdx;
}

// Show the (already loaded into buffer) current page
// and load the next page into the next buffer so we have it.
auto Renderer::renderPage(Place fromPlace, int16_t numSkipLines) -> Place {

    currentPlace_ = tokenizer_->toStreamPlace(fromPlace);

    if constexpr (RENDERER_DEBUG) {
        log_d("%s is place in file being requested", currentPlace_.str().c_str());
    }

    // This needs to happen before loading into buffer since it will affect what we load into the
    // buffer.
    display_.setFont(display_.style.getFontFamily().readableFont);

    tokenizer_->computeMaxTextTokenCharSize();

    PROFILE_START(loadCurrentPage);
    nextPlace_ = loadCurrentPage();
    PROFILE_END_LOG_MS(loadCurrentPage);

    int16_t newLineAdjust = 0;

    // A first attempt to get the the text to look better with newlines as sole
    // characters on the line with a smaller height than a full line.
    // The logic is that we split each line into 10 "decilines" and then
    // assign a height to each line based on if it's a newline or not. Newlines
    // get ENDPARAGRAPH_DECI_LINE_HEIGHT decilines (currently set to 4 out of 10).
    // The y position calucalted is based on the number of newlines we've seen so far
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
        log_w("Returning next page place to be %s", nextPlace_.str().c_str());
    }
    return tokenizer_->toPlace(nextPlace_);
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
        log_d("Paragraph startIdx: %" PRIu16 " %s, endIdx: %" PRIu16 " %s", startTokenIdx,
              tokens_[startTokenIdx].streamPlace.str().c_str(), endTokenIdx,
              tokens_[endTokenIdx].streamPlace.str().c_str());
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
                log_d("Line at firstTokenIdx %" PRIu16 " %s, next line tokenIdx: %" PRIu16
                      " %s, Content: %s",
                      firstTokenIdx, tokens_[firstTokenIdx].streamPlace.str().c_str(), tokenIdx,
                      tokens_[tokenIdx].streamPlace.str().c_str(), singleLine.data());
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

auto Renderer::getFirstLineOfPageTokenIdx(renderer_defs::Idx lastTokenIdx, uint16_t deciLinesCount,
                                          uint16_t maxDeciLines) const -> renderer_defs::Idx {

    if constexpr (RENDERER_DEBUG) {
        log_d("Starting at tokenIdx: %" PRIu16 ", and deciLinesCount %" PRIu16
              ", With max of deciLines: %" PRIu16,
              lastTokenIdx, deciLinesCount, maxDeciLines);
    }

    renderer_defs::Idx currentParaLineIdx =
        paraLineLastIdx_ == 0 ? MAX_PARAGRAPH_LINES - 1 : paraLineLastIdx_ - 1;

    renderer_defs::Idx returnTokenIdx = lastTokenIdx;
    renderer_defs::Idx tokenIdx = lastTokenIdx;

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
auto Renderer::goBackwardsOnePage(StreamPlace fromPlace) -> StreamPlace {
    display_.setFont(display_.style.getFontFamily().readableFont);
    tokenizer_->computeMaxTextTokenCharSize();

    // Step 1: Figure out how much to read before the current place. If we're too
    // close to the beginning, we just read from the beginning.
    StreamPlace seekPlace = tokenizer_->subtractOffset(fromPlace, MAX_BYTES_TO_RETRIEVE);

    // Step 2: Once the data is read into the buffer, tokenize it the same as you
    // would for a forwards render. seekPlace will be returned to be at a UTF8 character
    // boundary
    seekPlace = tokenizer_->loadAndTokenizePage(seekPlace, tokens_,
                                                true, &fromPlace); // NOLINT(clang-analyzer-deadcode.DeadStores)

    // Step 3: Find the place in the token buffer where the passed in place in file
    // is represented.
    Idx endTokenIdx = tokens_.getCount() - 1;

    if constexpr (RENDERER_DEBUG) {
        log_d("BACKWARDS DISPLAY READOUT: %d, %d, %d", display_.height() - 2,
              this->display_.getTextHeight(), this->display_.getLineSpacing());
    }

    // Find which token to end with
    // Could switch to an algorithm that is log(n) instead of n if we need to
    if constexpr (RENDERER_DEBUG) {
        log_d("About to look for where to end: fromPlace = %s, starting at seekPlace %s",
              fromPlace.str().c_str(), seekPlace.str().c_str());
    }

    while ((endTokenIdx > 0) && (tokens_[endTokenIdx].streamPlace.offset > fromPlace.offset)) {
        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Going back another token at place %" PRIu32,
        //     tokens_[endTokenIdx].streamPlace); log_d("The token idx is %" PRIu16, endTokenIdx);
        // }
        endTokenIdx--;
    }

    if (endTokenIdx == 0) {
        // Can't go back any further, must be at beginning of file or the paragraph is larger than
        // MAX_CHARS_TO_RETRIEVE bytes.
        return tokenizer_->getPlaceAtBeginningOfFile(seekPlace);
    }

    if constexpr (RENDERER_DEBUG) {
        if (tokens_[endTokenIdx].streamPlace.offset != fromPlace.offset) {
            log_e("Synchronisation issue. Unable to point at fromPlace with endTokenIdx %" PRIu16
                  ": %s.",
                  endTokenIdx, tokens_[endTokenIdx].streamPlace.str().c_str());
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
        log_d("Found the place to end and it's at place %s, tokenId %" PRIu8 ", tokenIdx: %" PRIu16,
              tokens_[endTokenIdx].streamPlace.str().c_str(), (uint8_t)tokens_[endTokenIdx].tokenId,
              endTokenIdx);
    }

    Idx startTokenIdx = tokens_[endTokenIdx].prevNewParagraphTokenIdx;

    // if it's not a ENDPARAGRAPH, it means we maybe at the beginning of the file
    if (tokens_[startTokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
        startTokenIdx += 1;
    } else if (tokenizer_->atBeginningOfFile(tokens_[startTokenIdx].streamPlace)) {
        log_w("We may not have reached the beginning of previous paragraph.");
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("Last paragraph boundaries: %" PRIu16 " %s through %" PRIu16 " %s", startTokenIdx,
              tokens_[startTokenIdx].streamPlace.str().c_str(), endTokenIdx,
              tokens_[endTokenIdx].streamPlace.str().c_str());
    }

    // Step5: Iterate each paragraph in reverse order until we get at least enough deciLines to
    // cover the page size.
    int16_t deciLinesCount = 0;

    int16_t maxDeciLines = getMaxDeciLines();

    uint16_t lastDeciLinesCount = 0;

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
        } else if (tokenizer_->atBeginningOfFile(tokens_[startTokenIdx].streamPlace)) {
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
    StreamPlace returnPlace = tokens_[startTokenIdx].streamPlace;

    // methods calling this method expect this to not change what's loaded
    // into buffers, so we need to retokenize.
    // we could probably avoid this rework by introducing an extra token buffer if we wanted to.
    // log_d("loadAndTokenizePage called: need to retokenize");

    static_cast<void>(tokenizer_->loadAndTokenizePage(fromPlace, tokens_));

    if constexpr (RENDERER_DEBUG) {
        log_w("Returning the following place! %s", returnPlace.str().c_str());
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
auto Renderer::loadCurrentPage() -> StreamPlace {

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
    currentPlace_ = tokenizer_->loadAndTokenizePage(currentPlace_, tokens_);

    Idx nextTokenIdx = 0;
    Idx tokenIdx = 0;

    pageLinesCount_ = 0;

    int16_t lastDeciLinesCount = 0;

    while ((deciLinesCount <= maxDeciLines) && (tokens_[tokenIdx].tokenId != TokenId::ENDOFFILE)) {

        lastDeciLinesCount = deciLinesCount;
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

    // If we hit EOF, the next screen wraps around to the beginning (txt) or to next file (EPub).
    if (tokens_[tokenIdx].tokenId == TokenId::ENDOFFILE) {
        return tokenizer_->getPlaceAfterEndOfFileReached(tokens_[tokenIdx].streamPlace);
    }

    // tokens_.print();

    return tokens_[tokenIdx].streamPlace;
}

auto Renderer::getPreviousPlace() -> Place {
    previousPlace_ = goBackwardsOnePage(currentPlace_);
    return previousPlace_.offset;
}