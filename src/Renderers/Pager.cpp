#include "Pager.hpp"

// Takes the last token entered in the tokens_[] array and include it in the page.
//
// The dontStop parameter is used in the context of the TXT file format when going backward, such
// that lines will be continuously added to the lines_[] circular array. The caller method is then
// responsible to stop the process of adding tokens when the target location is reached.
//
// returns true if there is still some room in the page or dontStop is true, false if the page is
// complete.
auto Pager::addLastTokenToPage(bool dontStop) -> bool {

    Token &token = tokens_.getLast();

    // if constexpr (RENDERER_DEBUG) {
    //     log_w("[%" PRIu16 "](%" PRIu16 ") tokenIdx:%" PRIu16 ", Id:%" PRIu8 ", cStrPool:%"
    //     PRIi32,
    //           linesTailIdx_, lines_[linesTailIdx_].tokenCount, tokens_.getLastIdx(),
    //           (uint8_t)token.tokenId, tokens_.getCStrPoolCount());
    // }

    switch (token.tokenId) {
    case TokenId::TEXT:
    case TokenId::GTEXT: // Not in use yet
    case TokenId::HYPHEN:
    case TokenId::ENDASH:
    case TokenId::EMDASH:
    case TokenId::TAB:
        if ((token.pixWidth + currentLineWidth_) > maxLineWidth_) {
            // We are at the end of a line
            if (token.tokenId == TokenId::TAB) {
                // get rid of tab character present at the end of the line
                tokens_.forgetLast();
            } else {
                // Complete the line, passing to the next one
                newLine(NEWLINE_DECI_LINE_HEIGHT);
                if (!dontStop &&
                    ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) > maxDeciLines_)) {
                    // The page is complete. The current token will be the first to next line
                    setNextPagePlace(token.streamPlace);
                    if constexpr (RENDERER_DEBUG) {
                        log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                              currentPageDecilineHeight_, maxDeciLines_);
                    }
                    return false;
                }
                // The page is not complete, the current token is the first to appear on next line
                currentLineWidth_ += token.pixWidth;
                lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
                lines_[linesTailIdx_].tokenCount++;
                lines_[linesTailIdx_].length += token.length;
            }
        } else if (lines_[linesTailIdx_].tokenCount == 0) {
            // We are at the beginning of a line
            if ((token.tokenId != TokenId::TAB) || (tokens_.getTokenCount() < 2) ||
                ((tokens_.getBeforeLast().tokenId == TokenId::NEWLINE) ||
                 (tokens_.getBeforeLast().tokenId == TokenId::ENDPARAGRAPH))) {
                if (!dontStop &&
                    ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) > maxDeciLines_)) {
                    setNextPagePlace(token.streamPlace);
                    if constexpr (RENDERER_DEBUG) {
                        log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                              currentPageDecilineHeight_, maxDeciLines_);
                    }
                    return false;
                }
                lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
                lines_[linesTailIdx_].tokenCount++;
                lines_[linesTailIdx_].length += token.length;
                currentLineWidth_ = token.pixWidth;
            } else {
                tokens_.forgetLast();
            }
        } else {
            currentLineWidth_ += token.pixWidth;
            lines_[linesTailIdx_].tokenCount++;
            lines_[linesTailIdx_].length += token.length;
        }

        if (token.tokenId == TokenId::TEXT) {
            wordCount_ += 1;
        }
        break;

    case TokenId::NEWLINE:
        // token.tokenId = TokenId::NOTOKEN;
        tokens_.forgetLast();
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        break;

    case TokenId::HRULE: {
        uint16_t height = 0;
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
            height = ENDPARAGRAPH_DECI_LINE_HEIGHT;
        }
        if (!dontStop && ((currentPageDecilineHeight_ + height + ENDPARAGRAPH_DECI_LINE_HEIGHT) >
                          maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            setNextPagePlace(token.streamPlace);
            if constexpr (RENDERER_DEBUG) {
                log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            }
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(height + ENDPARAGRAPH_DECI_LINE_HEIGHT);
    } break;

    case TokenId::ENDPARAGRAPH:
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        if (!dontStop &&
            ((currentPageDecilineHeight_ + ENDPARAGRAPH_DECI_LINE_HEIGHT) > maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            setNextPagePlace(token.streamPlace);
            if constexpr (RENDERER_DEBUG) {
                log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            }
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(ENDPARAGRAPH_DECI_LINE_HEIGHT);
        break;

    case TokenId::PICTURE:
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        if (!dontStop && (currentPageDecilineHeight_ > 0)) {
            setNextPagePlace(token.streamPlace);
            if constexpr (RENDERER_DEBUG) {
                log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            }
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(maxDeciLines_);
        break;

    case TokenId::EPICTURE: {
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        uint16_t deciHeight = pixToDeci(token.pixHeight);
        if (!dontStop && ((currentPageDecilineHeight_ + deciHeight) > maxDeciLines_)) {
            setNextPagePlace(token.streamPlace);
            if constexpr (RENDERER_DEBUG) {
                log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            }
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(deciHeight + 4);
    } break;

    case TokenId::NOTOKEN:
        break;
    }

    return true;
}

void Pager::newLine(uint16_t height) {

    if constexpr (RENDERER_DEBUG) {
        log_w("[%" PRIu16 "](%" PRIu16 ") NewLine, first tokenidx %" PRIu16 " %s", linesTailIdx_,
              lines_[linesTailIdx_].tokenCount, lines_[linesTailIdx_].tokenIdx,
              tokens_[lines_[linesTailIdx_].tokenIdx].streamPlace.str().c_str());
    }

    currentPageDecilineHeight_ += height;
    lines_[linesTailIdx_].deciHeight = height;

    incrLinesTailIdx();

    currentLineWidth_ = 0;
    lines_[linesTailIdx_] = {};
};

// Called when going back and the end place is found. Their may
// remains a line that was not completed.
void Pager::completeLastLine() {
    if (lines_[linesTailIdx_].tokenCount > 0) {
        if ((lines_[linesTailIdx_].tokenCount == 1) &&
            (tokens_[lines_[linesTailIdx_].tokenIdx].tokenId == TokenId::ENDPARAGRAPH)) {
            newLine(ENDPARAGRAPH_DECI_LINE_HEIGHT);
        } else {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
    }
}

auto Pager::computeWordCount() const -> uint16_t {
    uint16_t wordCount = 0;
    if (!linesIsEmpty()) {
        Idx lastLineIdx = getLastLineIdx();
        Idx firstTokenIdx = lines_[firstPageLineIdx_].tokenIdx;
        Idx lastTokenIdx =
            tokens_.addToTokenIdx(lines_[lastLineIdx].tokenIdx, lines_[lastLineIdx].tokenCount);
        for (Idx tokenIdx = firstTokenIdx; tokenIdx != lastTokenIdx;
             tokens_.nextTokenIdx(tokenIdx)) {
            if (tokens_[tokenIdx].tokenId == TokenId::TEXT) {
                wordCount += 1;
            }
        }
    }
    return wordCount;
}

auto Pager::getPageLength() const -> uint16_t {
    uint16_t length = 0;
    for (Idx lineIdx = firstPageLineIdx_; lineIdx != linesTailIdx_; nextLineIdx(lineIdx)) {
        length += lines_[lineIdx].length;
    }
    return length;
}

// Prepare a line to be displayed on screen
void Pager::getLine(LineInfo &lineStruct, Line &line) const {
    line[0] = 0;
    Idx endTokenIdx = tokens_.addToTokenIdx(lineStruct.tokenIdx, lineStruct.tokenCount);
    Idx lineIdx = 0;
    for (Idx tokenIdx = lineStruct.tokenIdx; tokenIdx != endTokenIdx;
         tokens_.nextTokenIdx(tokenIdx)) {

        if ((tokens_[tokenIdx].tokenId != TokenId::NOTOKEN) &&
            (tokens_[tokenIdx].tokenId != TokenId::NEWLINE) &&
            (tokens_[tokenIdx].tokenId != TokenId::ENDPARAGRAPH)) {
            if (tokens_[tokenIdx].cStr != nullptr) {
                // strcat(line.data(), tokens_[tokenIdx].cStr);
                memcpy(line.data() + lineIdx, tokens_[tokenIdx].cStr, tokens_[tokenIdx].length);
                lineIdx += tokens_[tokenIdx].length;
            }
            for (int i = 0; i < tokens_[tokenIdx].spacesAfter; i++) {
                line[lineIdx++] = ' ';
            }
        }
    }
    line[lineIdx] = 0;
}

#if 1
void Pager::displayPage(int16_t numSkipLines) {

    // log_w("First Line Idx: %" PRIu16 ", Last Line Idx: %" PRIu16 , firstPageLineIdx_,
    // linesTailIdx_);

    uint16_t lineNbr = 0;
    int deciY = 0;

    for (Idx lineIdx = firstPageLineIdx_; lineIdx != linesTailIdx_;
         nextLineIdx(lineIdx), lineNbr++) {

        const Token &token = tokens_[lines_[lineIdx].tokenIdx];

        // log_w("TokenId: %" PRIu8, (uint8_t) token.tokenId);

        switch (token.tokenId) {
        case TokenId::PICTURE:
            if constexpr (RENDERER_DEBUG) {
                log_d("[%" PRIu16 "] %s: <PICTURE> %s", lineIdx, token.streamPlace.str().c_str(),
                      token.cStr);
            }
            if ((token.picture != nullptr) && (token.picture.get() != nullptr)) {
                int width = token.pixWidth;
                int height = token.pixHeight;
                display_.fillScreen(display_.bg);
                display_.drawXBitmap((display_.width() - width) / 2,
                                     (display_.height() - height) / 2, token.picture->getBuffer(),
                                     width, height, display_.fg);
                break;
            } else {
                log_e("Picture not there!!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
            break;

        case TokenId::EPICTURE:
            if constexpr (RENDERER_DEBUG) {
                log_d("[%" PRIu16 "] %s: <EPICTURE> %s", lineIdx, token.streamPlace.str().c_str(),
                      token.cStr);
            }
            if ((token.picture != nullptr) && (token.picture.get() != nullptr)) {
                int width = token.pixWidth;
                int height = token.pixHeight;
                // int16_t y = deciToPix(deci_y + ENDPARAGRAPH_DECI_LINE_HEIGHT) + height / 2;
                display_.drawXBitmap((display_.width() - width) / 2, deciToPix(deciY),
                                     token.picture->getBuffer(), width, height, display_.fg);
                deciY += pixToDeci(height + 4);
            } else {
                log_e("Picture not there!!");
                if constexpr (ON_ERROR_EXIT) {
                    exit(1);
                }
            }
            break;

        case TokenId::ENDPARAGRAPH:
            if (deciY == 0) {
                if constexpr (RENDERER_DEBUG) {
                    log_d("[%" PRIu16 "] %s: <PARA NOT SHOWN>", lineIdx,
                          token.streamPlace.str().c_str());
                }
                continue;
            }
            if constexpr (RENDERER_DEBUG) {
                log_d("[%" PRIu16 "] %s: <PARA>", lineIdx, token.streamPlace.str().c_str());
            }
            deciY += lines_[lineIdx].deciHeight;
            break;

        case TokenId::HRULE: {
            if constexpr (RENDERER_DEBUG) {
                log_d("[%" PRIu16 "] %s: <HRULE>", lineIdx, token.streamPlace.str().c_str());
            }
            uint16_t y = deciToPix(deciY + (lines_[lineIdx].deciHeight / 2));
            display_.drawLine(0, y, display_.width(), y, display_.fg);
            deciY += lines_[lineIdx].deciHeight;
        } break;

        default:
            // Only draw the lines if it's not skipped from the caller.
            if (lineNbr >= numSkipLines) {
                Line line;
                getLine(lines_[lineIdx], line);
                if constexpr (RENDERER_DEBUG) {
                    log_d("[%" PRIu16 "] %s: <%s>", lineIdx, token.streamPlace.str().c_str(),
                          line.data());
                }
                display_.drawSingleLineOfText(line.data(), 0, deciToPix(deciY));
            } else {
                if constexpr (RENDERER_DEBUG) {
                    log_d("[%" PRIu16 "] %s: <SKIPPED>", lineIdx, token.streamPlace.str().c_str());
                }
            }
            deciY += NEWLINE_DECI_LINE_HEIGHT;
            break;
        }
    }
}
#else

// old version
void Pager::displayPage(int16_t numSkipLines) {

    // log_w("First Line Idx: %" PRIu16 ", Last Line Idx: %" PRIu16 , firstPageLineIdx_,
    // linesTailIdx_);

    uint16_t lineNbr = 0;
    uint16_t noCareLines = 0;
    uint16_t paraLineAdjust = 0;
    bool startOfPage = true;

    for (Idx lineIdx = firstPageLineIdx_; lineIdx != linesTailIdx_;
         nextLineIdx(lineIdx), lineNbr++) {

        const Token &token = tokens_[lines_[lineIdx].tokenIdx];

        // log_w("TokenId: %" PRIu8, (uint8_t) token.tokenId);

        if (startOfPage) {
            if (token.tokenId == TokenId::ENDPARAGRAPH) {
                if constexpr (RENDERER_DEBUG) {
                    log_d("[%" PRIu16 "] %s: <PARA NOT SHOWN>", lineIdx,
                          token.streamPlace.str().c_str());
                }
                noCareLines += 1;
                continue;
            }

            startOfPage = false;
        }

        if (token.tokenId == TokenId::ENDPARAGRAPH) {
            if constexpr (RENDERER_DEBUG) {
                log_d("[%" PRIu16 "] %s: <PARA>", lineIdx, token.streamPlace.str().c_str());
            }
            paraLineAdjust++;
        } else if (token.tokenId == TokenId::PICTURE) {
            if constexpr (RENDERER_DEBUG) {
                log_d("[%" PRIu16 "] %s: <PICTURE> %s", lineIdx, token.streamPlace.str().c_str(),
                      token.cStr);
            }
            if (picture_ != nullptr) {
                int width = picture_.get()->getWidth();
                int height = picture_.get()->getHeight();
                display_.fillScreen(display_.bg);
                display_.drawXBitmap((display_.width() - width) / 2,
                                     (display_.height() - height) / 2, picture_.get()->getBuffer(),
                                     width, height, display_.fg);
            }
        } else {
            // Only draw the lines if it's not skipped from the caller.
            if (lineNbr >= numSkipLines) {
                Line line;
                getLine(lineIdx, line);
                if constexpr (RENDERER_DEBUG) {
                    log_d("[%" PRIu16 "] %s: <%s>", lineIdx, token.streamPlace.str().c_str(),
                          line.data());
                }
                display_.drawSingleLineOfText(
                    line.data(), 0,
                    (lineNbr - noCareLines) * (display_.getTextHeight()) -
                        (display_.getTextHeight() *
                         ((NEWLINE_DECI_LINE_HEIGHT - ENDPARAGRAPH_DECI_LINE_HEIGHT) *
                          paraLineAdjust)) /
                            NEWLINE_DECI_LINE_HEIGHT);
            }
        }
    }
}

#endif

// Used in the TXT book/article context to find the beginning of the page to show when going
// backward.
void Pager::setFirstLineIdxBackward() {
    if constexpr (RENDERER_DEBUG) {
        log_w("Start of First Line Idx search...");
    }
    Idx lineIdx = linesTailIdx_;
    if (lineIdx != linesHeadIdx_) {
        uint16_t deciLines = 0;
        do {
            prevLineIdx(lineIdx);
            TokenId tokenId = tokens_[lines_[lineIdx].tokenIdx].tokenId;
            uint16_t height = (tokenId == TokenId::ENDPARAGRAPH) ? ENDPARAGRAPH_DECI_LINE_HEIGHT
                              : (tokenId == TokenId::PICTURE)    ? maxDeciLines_
                                                                 : NEWLINE_DECI_LINE_HEIGHT;
            if ((deciLines + height) > maxDeciLines_) {
                nextLineIdx(lineIdx);
                break;
            }
            deciLines += height;
        } while (lineIdx != linesHeadIdx_);
    } else {
        if constexpr (RENDERER_DEBUG) {
            log_e("Lines[] is empty!!");
            if constexpr (ON_ERROR_EXIT) {
                exit(1);
            }
        }
    }
    firstPageLineIdx_ = lineIdx;
}

// Debugging stuff
void Pager::show() const {
    if constexpr (RENDERER_DEBUG) {
        std::cout << "head:" << linesHeadIdx_ << ", Tail:" << linesTailIdx_ << std::endl;
        for (Idx lineIdx = linesHeadIdx_; lineIdx != linesTailIdx_; nextLineIdx(lineIdx)) {
            std::cout << lineIdx << " " << tokens_[lines_[lineIdx].tokenIdx].streamPlace.str()
                      << ": <";
            Idx lastTokenIdx =
                tokens_.addToTokenIdx(lines_[lineIdx].tokenIdx, lines_[lineIdx].tokenCount);
            for (Idx tokenIdx = lines_[lineIdx].tokenIdx; tokenIdx != lastTokenIdx;
                 tokens_.nextTokenIdx(tokenIdx)) {
                if ((tokens_[tokenIdx].tokenId != TokenId::NOTOKEN) &&
                    (tokens_[tokenIdx].tokenId != TokenId::NEWLINE) &&
                    (tokens_[tokenIdx].tokenId != TokenId::ENDPARAGRAPH)) {
                    std::cout << tokens_[tokenIdx].cStr;
                    if (tokens_[tokenIdx].spacesAfter) {
                        std::cout << " ";
                    }
                }
            }
            std::cout << ">" << std::endl;
        }
        std::cout << "Next Page Place: " << nextPagePlace_.str().c_str() << std::endl;
        std::cout << "Page Word Count: " << wordCount_ << std::endl;
    }
}