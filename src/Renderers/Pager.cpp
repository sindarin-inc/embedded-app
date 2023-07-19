#include "Pager.hpp"

auto Pager::addLastTokenToPage(bool dontStop) -> bool {
    Token &token = tokens_.getLast();

    auto newLine = [this](uint16_t height) {
        if (lineCount_ < MAX_LINES_PER_PAGE) {
            // std::cout << "NewLine with tokenCount: " << lines_[lineCount_].tokenCount <<
            // std::endl;
            currentPageDecilineHeight_ += height;
            lineCount_ += 1;
            currentLineWidth_ = 0;
            lines_[lineCount_].tokenCount = 0;
            lines_[lineCount_].length = 0;
        } else {
            log_e("Too many lines!!");
            if constexpr (ON_ERROR_EXIT) {
                exit(1);
            }
        }
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
                tokens_.forgetLast();
            } else {
                newLine(NEWLINE_DECI_LINE_HEIGHT);
                if (!dontStop &&
                    ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) >= maxDeciLines_)) {
                    nextPagePlace_ = token.streamPlace;
                    if constexpr (RENDERER_DEBUG) {
                        log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                              currentPageDecilineHeight_, maxDeciLines_);
                    }
                    return false;
                }
                currentLineWidth_ += token.width;
                lines_[lineCount_].tokenIdx = tokens_.getLastIdx();
                lines_[lineCount_].tokenCount += 1;
                lines_[lineCount_].length += token.length;
            }
        } else if (lines_[lineCount_].tokenCount == 0) {
            if (((token.tokenId != TokenId::SPACE) && (token.tokenId != TokenId::TAB)) ||
                (tokens_.getCount() < 2) ||
                ((tokens_.getBeforeLast().tokenId == TokenId::NEWLINE) ||
                 (tokens_.getBeforeLast().tokenId == TokenId::ENDPARAGRAPH))) {
                if (!dontStop &&
                    ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) >= maxDeciLines_)) {
                    nextPagePlace_ = token.streamPlace;
                    if constexpr (RENDERER_DEBUG) {
                        log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                              currentPageDecilineHeight_, maxDeciLines_);
                    }
                    return false;
                }
                lines_[lineCount_].tokenIdx = tokens_.getLastIdx();
                lines_[lineCount_].tokenCount = 1;
                lines_[lineCount_].length += token.length;
                currentLineWidth_ = token.width;
            } else {
                tokens_.forgetLast();
            }
        } else {
            currentLineWidth_ += token.width;
            lines_[lineCount_].tokenCount += 1;
            lines_[lineCount_].length += token.length;
        }

        if (token.tokenId == TokenId::TEXT) {
            wordCount_ += 1;
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
        if (!dontStop &&
            ((currentPageDecilineHeight_ + ENDPARAGRAPH_DECI_LINE_HEIGHT) >= maxDeciLines_)) {
            nextPagePlace_ = token.streamPlace;
            if constexpr (RENDERER_DEBUG) {
                log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            }
            return false;
        }
        lines_[lineCount_].tokenCount = 1;
        lines_[lineCount_].tokenIdx = tokens_.getLastIdx();
        newLine(ENDPARAGRAPH_DECI_LINE_HEIGHT);
        break;

    case TokenId::NOTOKEN:
    case TokenId::PICTURE:
        break;
    }

    return true;
}

auto Pager::computeWordCount() const -> uint16_t {
    Idx firstIdx = lines_[firstLineIdx_].tokenIdx;
    Idx lastIdx = lines_[lineCount_ - 1].tokenIdx + lines_[lineCount_ - 1].tokenCount - 1;
    uint16_t wordCount = 0;
    for (Idx i = firstIdx; i < lastIdx; i++) {
        if (tokens_[i].tokenId == TokenId::TEXT) {
            wordCount += 1;
        }
    }
    return wordCount;
}

auto Pager::getPageLength() const -> uint16_t {
    uint16_t length = 0;
    for (Idx i = firstLineIdx_; i < lineCount_; i++) {
        length += lines_[i].length;
    }
    return length;
}

void Pager::getLine(Idx lineNbr, Line &line) const {
    line[0] = 0;
    for (Idx j = lines_[lineNbr].tokenIdx; j <= lastLineTokenIdx(lineNbr); j++) {
        if ((tokens_[j].tokenId != TokenId::NOTOKEN) && (tokens_[j].tokenId != TokenId::NEWLINE) &&
            (tokens_[j].tokenId != TokenId::ENDPARAGRAPH)) {
            strcat(line.data(), tokens_[j].cStr);
        }
    }
}

void Pager::displayPage(int16_t numSkipLines) const {
    uint16_t paraLineAdjust = 0;

    // std::cout << "First Line Idx: " << firstLineIdx_ << ", lineCount: " << lineCount_ <<
    // std::endl;

    uint16_t lineNbr = 0;
    for (Idx lineIdx = firstLineIdx_; lineIdx < lineCount_; lineIdx++, lineNbr++) {

        if (tokens_[lines_[lineIdx].tokenIdx].tokenId == TokenId::ENDPARAGRAPH) {
            paraLineAdjust++;
        } else {
            // Only draw the lines if it's not skipped from the caller.
            if (lineNbr >= numSkipLines) {
                Line line;
                getLine(lineIdx, line);
                if constexpr (RENDERER_DEBUG) {
                    log_d("%s", line.data());
                }
                display_.drawSingleLineOfText(
                    line.data(), 0,
                    lineNbr * (display_.getTextHeight()) -
                        display_.getTextHeight() *
                            ((NEWLINE_DECI_LINE_HEIGHT - ENDPARAGRAPH_DECI_LINE_HEIGHT) *
                             paraLineAdjust) /
                            NEWLINE_DECI_LINE_HEIGHT);
            }
        }
    }
}

auto Pager::getFirstLineIdxBackward() -> Idx {
    Idx lineIdx = lineCount_;
    if (lineIdx > 0) {
        uint16_t deciLines = 0;
        do {
            lineIdx -= 1;
            deciLines += (tokens_[lines_[lineIdx].tokenIdx].tokenId == TokenId::ENDPARAGRAPH)
                             ? ENDPARAGRAPH_DECI_LINE_HEIGHT
                             : NEWLINE_DECI_LINE_HEIGHT;
        } while ((lineIdx > 0) && (deciLines <= maxDeciLines_));

        if (deciLines > maxDeciLines_) {
            lineIdx += 1;
        }
    }
    firstLineIdx_ = lineIdx;
    return lineIdx;
}

// void Pager::compareWith(PageLines &pageLines, uint16_t lineCount) const {

//     if (lineCount != lineCount_) {
//         log_e("Line Count differ: %" PRIu16 " vs %" PRIu16, lineCount_, lineCount);
//         exit(1);
//     }

//     for (Idx i = 0; i < lineCount_; i++) {
//         Line line;
//         Line line2;
//         line2 = pageLines[i];
//         while (true) {
//             if ((line2[strlen(line2.data()) - 1] == '\n') ||

//                 (line2[strlen(line2.data()) - 1] == '\t')) {
//                 line2[strlen(line2.data()) - 1] = 0;
//             } else {
//                 break;
//             }
//         }
//         getLine(i, line);
//         if (strcmp(line.data(), line2.data()) != 0) {
//             log_e("Line %" PRIu16 " differ:\nnew: [%s]\nold: [%s]", i, line.data(),
//             line2.data()); exit(1);
//         }
//     }
// }

void Pager::show() const {
    if constexpr (RENDERER_DEBUG) {
        for (Idx i = 0; i < lineCount_; i++) {
            std::cout << i << ": ";
            for (Idx j = lines_[i].tokenIdx; j <= lastLineTokenIdx(i); j++) {
                if ((tokens_[j].tokenId != TokenId::NOTOKEN) &&
                    (tokens_[j].tokenId != TokenId::NEWLINE) &&
                    (tokens_[j].tokenId != TokenId::ENDPARAGRAPH)) {
                    std::cout << tokens_[j].cStr;
                }
            }
            std::cout << std::endl;
        }
        std::cout << "Next Page Place: " << nextPagePlace_.str().c_str() << std::endl;
        std::cout << "Page Word Count: " << wordCount_ << std::endl;
    }
}