#include "Pager.hpp"

auto Pager::addLastTokenToPage() -> bool {
    Token &token = tokens_.getLast();

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
                tokens_.forgetLast();
            } else {
                newLine(NEWLINE_DECI_LINE_HEIGHT);
                if ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) >= maxDeciLines_) {
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
            }
        } else if (lines_[lineCount_].tokenCount == 0) {
            if (((token.tokenId != TokenId::SPACE) && (token.tokenId != TokenId::TAB)) ||
                (tokens_.getCount() < 2) ||
                ((tokens_.getBeforeLast().tokenId == TokenId::NEWLINE) ||
                 (tokens_.getBeforeLast().tokenId == TokenId::ENDPARAGRAPH))) {
                if ((currentPageDecilineHeight_ + NEWLINE_DECI_LINE_HEIGHT) >= maxDeciLines_) {
                    nextPagePlace_ = token.streamPlace;
                    if constexpr (RENDERER_DEBUG) {
                        log_w("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                              currentPageDecilineHeight_, maxDeciLines_);
                    }
                    return false;
                }
                lines_[lineCount_].tokenIdx = tokens_.getLastIdx();
                lines_[lineCount_].tokenCount = 1;
                currentLineWidth_ = token.width;
            } else {
                tokens_.forgetLast();
            }
        } else {
            currentLineWidth_ += token.width;
            lines_[lineCount_].tokenCount += 1;
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
        if ((currentPageDecilineHeight_ + ENDPARAGRAPH_DECI_LINE_HEIGHT) >= maxDeciLines_) {
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

void Pager::getLine(Idx lineNbr, Line &line) const {
    line[0] = 0;
    for (Idx j = lines_[lineNbr].tokenIdx; j <= lastLineTokenIdx(lineNbr); j++) {
        if ((tokens_[j].tokenId != TokenId::NOTOKEN) && (tokens_[j].tokenId != TokenId::NEWLINE) &&
            (tokens_[j].tokenId != TokenId::ENDPARAGRAPH)) {
            strcat(line.data(), tokens_[j].cStr);
        }
    }
}

void Pager::compareWith(PageLines &pageLines, uint16_t lineCount) const {

    if (lineCount != lineCount_) {
        log_e("Line Count differ: %" PRIu16 " vs %" PRIu16, lineCount_, lineCount);
        exit(1);
    }

    for (Idx i = 0; i < lineCount_; i++) {
        Line line;
        Line line2;
        line2 = pageLines[i];
        while (true) {
            if ((line2[strlen(line2.data()) - 1] == '\n') ||

                (line2[strlen(line2.data()) - 1] == '\t')) {
                line2[strlen(line2.data()) - 1] = 0;
            } else {
                break;
            }
        }
        getLine(i, line);
        if (strcmp(line.data(), line2.data()) != 0) {
            log_e("Line %" PRIu16 " differ:\nnew: [%s]\nold: [%s]", i, line.data(), line2.data());
            exit(1);
        }
    }
}

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