#include "Pager.hpp"

auto Pager::addLastTokenToPage(bool dontStop) -> bool {

    Token &token = tokens_.getLast();

    auto newLine = [this](uint16_t height) {
        if constexpr (RENDERER_DEBUG) {
            log_w("NewLine with tokenCount: %" PRIu16, lines_[linesTailIdx_].tokenCount);
        }

        currentPageDecilineHeight_ += height;
        incrLinesTailIdx();

        currentLineWidth_ = 0;
        lines_[linesTailIdx_].tokenCount = 0;
        lines_[linesTailIdx_].length = 0;
    };

    switch (token.tokenId) {
    case TokenId::TEXT:
    case TokenId::HYPHEN:
    case TokenId::ENDASH:
    case TokenId::EMDASH:
    case TokenId::TAB:
        if ((token.width + currentLineWidth_) > maxLineWidth_) {
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
                currentLineWidth_ += token.width;
                lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
                lines_[linesTailIdx_].tokenCount = 1;
                lines_[linesTailIdx_].length += token.length;
            }
        } else if (lines_[linesTailIdx_].tokenCount == 0) {

            if ((token.tokenId != TokenId::TAB) || (linesTailIdx_ == linesHeadIdx_) ||
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
                lines_[linesTailIdx_].tokenCount = 1;
                lines_[linesTailIdx_].length += token.length;
                currentLineWidth_ = token.width;
            } else {
                tokens_.forgetLast();
            }
        } else {
            currentLineWidth_ += token.width;
            lines_[linesTailIdx_].tokenCount += 1;
            lines_[linesTailIdx_].length += token.length;
        }

        if (token.tokenId == TokenId::TEXT) {
            wordCount_ += 1;
        }
        break;

    case TokenId::NEWLINE:
        // token.tokenId = TokenId::NOTOKEN;
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        break;

    case TokenId::ENDPARAGRAPH:
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        if (!dontStop &&
            ((currentPageDecilineHeight_ + ENDPARAGRAPH_DECI_LINE_HEIGHT) > maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            nextPagePlace_ = token.streamPlace;
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

    case TokenId::NOTOKEN:
        break;
    }

    return true;
}

// Called when going back and the end place is found. Their may
// remains a line that was not completed.
void Pager::completeLastLine() {
    if (lines_[linesTailIdx_].tokenCount > 0) {
        if ((lines_[linesTailIdx_].tokenCount == 1) &&
            (tokens_[lines_[linesTailIdx_].tokenIdx].tokenId == TokenId::ENDPARAGRAPH)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
        } else {
            currentPageDecilineHeight_ += NEWLINE_DECI_LINE_HEIGHT;
        }
        incrLinesTailIdx();
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

void Pager::getLine(Idx lineNbr, Line &line) const {
    line[0] = 0;
    Idx endTokenIdx = tokens_.addToTokenIdx(lines_[lineNbr].tokenIdx, lines_[lineNbr].tokenCount);
    Idx lineIdx = 0;
    for (Idx tokenIdx = lines_[lineNbr].tokenIdx; tokenIdx != endTokenIdx;
         tokens_.nextTokenIdx(tokenIdx)) {

        if ((tokens_[tokenIdx].tokenId != TokenId::NOTOKEN) &&
            (tokens_[tokenIdx].tokenId != TokenId::NEWLINE) &&
            (tokens_[tokenIdx].tokenId != TokenId::ENDPARAGRAPH)) {
            if (tokens_[tokenIdx].cStr != nullptr) {
                // strcat(line.data(), tokens_[tokenIdx].cStr);
                memcpy(line.data() + lineIdx, tokens_[tokenIdx].cStr, tokens_[tokenIdx].length);
                lineIdx += tokens_[tokenIdx].length;
            }
            if (tokens_[tokenIdx].spacesAfter != 0) {
                line[lineIdx++] = ' ';
            }
        }
    }
    line[lineIdx] = 0;
}

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
                    log_d("%s: <PARA NOT SHOWN>", token.streamPlace.str().c_str());
                }
                noCareLines += 1;
                continue;
            }

            startOfPage = false;
        }

        if (token.tokenId == TokenId::ENDPARAGRAPH) {
            if constexpr (RENDERER_DEBUG) {
                log_d("%s: <PARA>", token.streamPlace.str().c_str());
            }
            paraLineAdjust++;
        } else if (token.tokenId == TokenId::PICTURE) {
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
                        display_.getTextHeight() *
                            ((NEWLINE_DECI_LINE_HEIGHT - ENDPARAGRAPH_DECI_LINE_HEIGHT) *
                             paraLineAdjust) /
                            NEWLINE_DECI_LINE_HEIGHT);
            }
        }
    }
}

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