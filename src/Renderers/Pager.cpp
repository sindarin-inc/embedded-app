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

    REND_WLOG("%s...", getTokenIdStr(token.tokenId));

    // REND_WLOG("[%" PRIu16 "](%" PRIu16 ") tokenIdx:%" PRIu16 ", Id:%" PRIu8 ", cStrPool:%"
    //       PRIi32,
    //       linesTailIdx_, lines_[linesTailIdx_].tokenCount, tokens_.getLastIdx(),
    //       (uint8_t)token.tokenId, tokens_.getCStrPoolCount());

    switch (token.tokenId) {
    case TokenId::TEXT:
    case TokenId::ITEXT:
    case TokenId::HTEXT:
    case TokenId::NTEXT:

#if CONFIG_FONT_TTF
    case TokenId::SUPTEXT:
    case TokenId::SUBTEXT:
#endif

    case TokenId::HYPHEN:
    case TokenId::ENDASH: // Not used for EPub, used for TXT
    case TokenId::EMDASH: // Not used for EPub, used for TXT
    {
        if (token.tokenId == TokenId::ITEXT) {
            leftIndent_ += token.pixWidth; // For next lines
            // log_w("IText (%s) new left indent %" PRIu16 ", Current Line Width: %" PRIu16,
            //     token.cStr, leftIndent_, currentLineWidth_);
        }
        uint16_t heightRequired =
            (token.tokenId == TokenId::HTEXT)
                ? (NEWLINE_DECI_LINE_HEIGHT << 1) + ENDPARAGRAPH_DECI_LINE_HEIGHT
                : NEWLINE_DECI_LINE_HEIGHT;
        if ((token.pixWidth + currentLineWidth_) > (maxLineWidth_ - rightIndent_)) {
            // We are at the end of a line

            // log_w("--> End of line, Line width: %" PRIu16, currentLineWidth_);
            // Complete the line, passing to the next one
            newLine(NEWLINE_DECI_LINE_HEIGHT);
            if (!dontStop && ((currentPageDecilineHeight_ + heightRequired) > maxDeciLines_)) {
                // The page is complete. The current token will be the first to next page
                setNextPagePlace(token.streamPlace);
                REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                          currentPageDecilineHeight_, maxDeciLines_);
                return false;
            }
            // The page is not complete, the current token is the first to appear on next line
            currentLineWidth_ += token.pixWidth;
            lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
            lines_[linesTailIdx_].tokenCount++;
            lines_[linesTailIdx_].length += token.length;
        } else if (lines_[linesTailIdx_].tokenCount == 0) {
            if (!dontStop && ((currentPageDecilineHeight_ + heightRequired) > maxDeciLines_)) {
                setNextPagePlace(token.streamPlace);
                REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                          currentPageDecilineHeight_, maxDeciLines_);
                return false;
            }
            lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
            lines_[linesTailIdx_].tokenCount = 1;
            lines_[linesTailIdx_].length += token.length;
            currentLineWidth_ += token.pixWidth;
        } else {
            currentLineWidth_ += token.pixWidth;
            lines_[linesTailIdx_].tokenCount++;
            lines_[linesTailIdx_].length += token.length;
        }

        if (static_cast<uint16_t>(token.tokenId) &
            (static_cast<uint16_t>(TokenId::TEXT) | static_cast<uint16_t>(TokenId::ITEXT) |
             static_cast<uint16_t>(TokenId::HTEXT) | static_cast<uint16_t>(TokenId::NTEXT)
#if CONFIG_FONT_TTF
             | static_cast<uint16_t>(TokenId::SUBTEXT) | static_cast<uint16_t>(TokenId::SUPTEXT)
#endif
                 )) {
            wordCount_ += 1;
        }
    } break;

    case TokenId::NEWLINE:
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
        if (!dontStop &&
            ((currentPageDecilineHeight_ + height + HRULE_DECI_LINE_HEIGHT) > maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            setNextPagePlace(token.streamPlace);
            REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(height + HRULE_DECI_LINE_HEIGHT);
    } break;

    case TokenId::ENDPARAGRAPH:
        if (tokens_.getTokenCount() > 1) {
            Token &token = tokens_.getBeforeLast();
            if (token.tokenId == TokenId::ENDPARAGRAPH) {
                tokens_.forgetLast();
                return true;
            }
        }

        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        if (!dontStop &&
            ((currentPageDecilineHeight_ + ENDPARAGRAPH_DECI_LINE_HEIGHT) > maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            setNextPagePlace(token.streamPlace);
            REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16 " token at %s",
                      currentPageDecilineHeight_, maxDeciLines_, token.streamPlace.str().c_str());
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
            REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
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
        // Try to fit the picture and some place for at least one line of caption into the current
        // page. The picture will go on next page if this doesn't fit.

        uint16_t deciHeight = pixToDeci(token.pixHeight);
        if (!dontStop && ((currentPageDecilineHeight_ + deciHeight + ENDPARAGRAPH_DECI_LINE_HEIGHT +
                           NEWLINE_DECI_LINE_HEIGHT) > maxDeciLines_)) {
            setNextPagePlace(token.streamPlace);
            REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(deciHeight + 4);
    } break;

    case TokenId::BTABLE: {
        // Will show the cStr (normally {TABLE}) followed by a newline. Add an ENDPARAGRAPH before
        // if not at beginning of a page and the previous line is to be completed.
        uint16_t height = NEWLINE_DECI_LINE_HEIGHT;
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
            height += ENDPARAGRAPH_DECI_LINE_HEIGHT;
        }
        if (!dontStop && ((currentPageDecilineHeight_ + height) > maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            setNextPagePlace(token.streamPlace);
            REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(height);
    } break;

    case TokenId::ETABLE: {
        // Will show a HRULE followed with the cStr text (normally {END TABLE}) and an ENDPARAGRAPH
        // if enough space on page.
        uint16_t height = NEWLINE_DECI_LINE_HEIGHT + HRULE_DECI_LINE_HEIGHT;
        if (lines_[linesTailIdx_].tokenCount > 0) {
            newLine(NEWLINE_DECI_LINE_HEIGHT);
        }
        if (!dontStop && ((currentPageDecilineHeight_ + height) > maxDeciLines_)) {
            currentPageDecilineHeight_ += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            setNextPagePlace(token.streamPlace);
            REND_WLOG("Page completed: %" PRIu16 " decilines. Max: %" PRIu16,
                      currentPageDecilineHeight_, maxDeciLines_);
            return false;
        }
        lines_[linesTailIdx_].tokenCount = 1;
        lines_[linesTailIdx_].tokenIdx = tokens_.getLastIdx();
        newLine(height + ENDPARAGRAPH_DECI_LINE_HEIGHT);
    } break;
    }

    // log_w("Current line width: %" PRIu16, currentLineWidth_);
    return true;
}

void Pager::newLine(uint16_t height) {

    REND_WLOG("[%" PRIu16 "](%" PRIu16 ") NewLine, first tokenidx %" PRIu16 " %s", linesTailIdx_,
              lines_[linesTailIdx_].tokenCount, lines_[linesTailIdx_].tokenIdx,
              tokens_[lines_[linesTailIdx_].tokenIdx].streamPlace.str().c_str());

    currentPageDecilineHeight_ += height;
    lines_[linesTailIdx_].deciHeight = height;

    incrLinesTailIdx();

    currentLineWidth_ = leftIndent_;
    lines_[linesTailIdx_] = {0, 0, 0, 0, leftIndent_};

    // log_w("NewLine left Indent: %" PRIu16, leftIndent_);
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
            if (static_cast<uint16_t>(tokens_[tokenIdx].tokenId) &
                (static_cast<uint16_t>(TokenId::TEXT) | static_cast<uint16_t>(TokenId::ITEXT) |
                 static_cast<uint16_t>(TokenId::HTEXT) | static_cast<uint16_t>(TokenId::NTEXT)
#if CONFIG_FONT_TTF
                 | static_cast<uint16_t>(TokenId::SUBTEXT) | static_cast<uint16_t>(TokenId::SUPTEXT)
#endif
                     )) {
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

#if CONFIG_FONT_TTF

// Retrieve a single segment from the linsinfo. superscript and subscripts are cutting lines in
// multiple segments.
auto Pager::getLineSegment(LineInfo &lineInfo, Idx nextTokenIdx, Idx endTokenIdx,
                           Line &lineSegment) const -> Idx {
    lineSegment[0] = 0;
    Idx lineIdx = 0;

    Idx tokenIdx = nextTokenIdx;
    bool first = true;
    while (tokenIdx != endTokenIdx) {

        auto tokenId = tokens_[tokenIdx].tokenId;

        if (!first &&
            (static_cast<uint16_t>(tokenId) & (static_cast<uint16_t>(TokenId::SUBTEXT) |
                                               static_cast<uint16_t>(TokenId::SUPTEXT))) != 0) {
            break;
        }

        if ((static_cast<uint16_t>(tokenId) & (static_cast<uint16_t>(TokenId::NEWLINE) |
                                               static_cast<uint16_t>(TokenId::ENDPARAGRAPH))) ==
            0) {
            if (tokens_[tokenIdx].cStr != nullptr) {
                // strcat(line.data(), tokens_[tokenIdx].cStr);
                memcpy(lineSegment.data() + lineIdx, tokens_[tokenIdx].cStr,
                       tokens_[tokenIdx].length);
                lineIdx += tokens_[tokenIdx].length;
            }

            if (tokens_[tokenIdx].spacesAfter) {
                lineSegment[lineIdx++] = ' ';
            }
        }

        tokens_.nextTokenIdx(tokenIdx);

        if (first &&
            (static_cast<uint16_t>(tokenId) & (static_cast<uint16_t>(TokenId::SUBTEXT) |
                                               static_cast<uint16_t>(TokenId::SUPTEXT))) != 0) {
            break;
        }

        first = false;
    }

    lineSegment[lineIdx] = 0;

    return tokenIdx;
}

// Draw a line on screen. A line is composed of potentially multiple segments, some are subscript or
// superscript for which a y offset must be applied.
void Pager::drawSingleLine(LineInfo &lineInfo, int yPos) const {

    Line lineSegment;
    int xPos = lineInfo.leftIndent + rect_.x;
    yPos += rect_.y;
    Idx nextTokenIdx = lineInfo.tokenIdx;
    Idx endTokenIdx = tokens_.addToTokenIdx(lineInfo.tokenIdx, lineInfo.tokenCount);

    while (nextTokenIdx != endTokenIdx) {

        auto tokenId = tokens_[nextTokenIdx].tokenId;

        nextTokenIdx = getLineSegment(lineInfo, nextTokenIdx, endTokenIdx, lineSegment);

        int theYPos = yPos;

        if ((static_cast<uint16_t>(tokenId) & (static_cast<uint16_t>(TokenId::SUBTEXT) |
                                               static_cast<uint16_t>(TokenId::SUPTEXT))) != 0) {

            int32_t yOffset = display_.getTextHeight() / 5;
            auto normalHeight = display_.getTextHeight();

            display_.getFont()->setSupSubFontSize();

            if (tokenId == TokenId::SUBTEXT) {
                theYPos += normalHeight - display_.getTextHeight() + yOffset;
            } else { // TokenId::SUPTEXT
                theYPos += normalHeight - display_.getTextHeight() - yOffset;
            }
            xPos = display_.drawSingleLineOfText(lineSegment.data(), xPos, theYPos);
            display_.getFont()->setNormalFontSize();
        } else {
            xPos = display_.drawSingleLineOfText(lineSegment.data(), xPos, yPos);
        }
    }
}

#else

// Prepare a line to be displayed on screen
void Pager::getLine(LineInfo &lineStruct, Line &line) const {
    line[0] = 0;
    Idx endTokenIdx = tokens_.addToTokenIdx(lineStruct.tokenIdx, lineStruct.tokenCount);
    Idx lineIdx = 0;
    for (Idx tokenIdx = lineStruct.tokenIdx; tokenIdx != endTokenIdx;
         tokens_.nextTokenIdx(tokenIdx)) {

        auto tokenId = tokens_[tokenIdx].tokenId;
        if ((static_cast<uint16_t>(tokenId) & (static_cast<uint16_t>(TokenId::NEWLINE) |
                                               static_cast<uint16_t>(TokenId::ENDPARAGRAPH))) ==
            0) {
            if (tokens_[tokenIdx].cStr != nullptr) {
                // strcat(line.data(), tokens_[tokenIdx].cStr);
                memcpy(line.data() + lineIdx, tokens_[tokenIdx].cStr, tokens_[tokenIdx].length);
                lineIdx += tokens_[tokenIdx].length;
            }
            if (tokens_[tokenIdx].spacesAfter) {
                line[lineIdx++] = ' ';
            }
            // for (int i = 0; i < tokens_[tokenIdx].spacesAfter; i++) {
            //     line[lineIdx++] = ' ';
            // }
        }
    }
    line[lineIdx] = 0;
}
#endif

auto Pager::computeDarkPercent(const uint8_t *bitmap, uint16_t width, uint16_t height) const
    -> uint8_t {
    static uint8_t pixelsDensity[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3,
        4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4,
        4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4,
        5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5,
        4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2,
        3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5,
        5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4,
        5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6,
        4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

    uint16_t size = ((width + 7) >> 3) * height;
    int32_t sum = 0;
    for (int i = 0; i < size; i++) {
        sum += pixelsDensity[bitmap[i]];
    }
    return (sum * 100) / 65536;
}

// Returns true if page contains picture(s) requiring constrast adjustment (dark pictures)
auto Pager::displayPage(int16_t numSkipLines) -> uint8_t {

    REND_WLOG("First Line Idx: %" PRIu16 ", Last Line Idx: %" PRIu16, firstPageLineIdx_,
              linesTailIdx_);

    uint16_t lineNbr = 0;
    int deciY = 0;
    uint8_t darkPicturePercent = 0;

    for (Idx lineIdx = firstPageLineIdx_; lineIdx != linesTailIdx_;
         nextLineIdx(lineIdx), lineNbr++) {

        Token &token = tokens_.tokenAt(lines_[lineIdx].tokenIdx);

        // log_w("TokenId: %" PRIu8, (uint8_t) token.tokenId);

        switch (token.tokenId) {
        case TokenId::PICTURE:
            REND_WLOG("[%" PRIu16 "] %s: <PICTURE> %s", lineIdx, token.streamPlace.str().c_str(),
                      token.cStr);
            if ((token.picture == nullptr) || (token.picture == nullptr)) {
                REND_PROFILE_START(ReadPictureFromEPub);
                auto [buffer, len] = stream_->getFile(token.cStr);
                REND_PROFILE_END(ReadPictureFromEPub);

                if (buffer != nullptr) {
                    token.picture = std::make_shared<BMPImage>();
                    if (!token.picture->readFromBytes(buffer.get(), len)) {
                        log_e("Unable ro retrieve the BMP picture from EPub");
                        REND_EXIT_SIM;
                    }
                } else {
                    REND_ELOG("buffer is empty!");
                }
            }
            if ((token.picture != nullptr) && (token.picture != nullptr)) {
                auto bitmap = token.picture->getBuffer();
                uint16_t width = token.pixWidth;
                uint16_t height = token.pixHeight;

                darkPicturePercent = computeDarkPercent(bitmap, width, height);
                REND_WLOG("Dark Picture Percentage: %" PRIu8, darkPicturePercent);

                display_.fillScreen(display_.bg);
                display_.drawXBitmap((display_.width() - width) / 2,
                                     (display_.height() - height) / 2, bitmap, width, height,
                                     display_.fg);

                break;
            } else {
                log_e("Picture not there!!");
                REND_EXIT_SIM;
            }
            break;

        case TokenId::EPICTURE:
            REND_WLOG("[%" PRIu16 "] %s: <EPICTURE> %s", lineIdx, token.streamPlace.str().c_str(),
                      token.cStr);
            if ((token.picture == nullptr) || (token.picture == nullptr)) {
                REND_PROFILE_START(ReadPictureFromEPub);
                auto [buffer, len] = stream_->getFile(token.cStr);
                REND_PROFILE_END(ReadPictureFromEPub);

                if (buffer != nullptr) {
                    token.picture = std::make_shared<BMPImage>();
                    if (!token.picture->readFromBytes(buffer.get(), len)) {
                        log_e("Unable ro retrieve the BMP picture from EPub");
                        REND_EXIT_SIM;
                    }
                } else {
                    REND_ELOG("buffer is empty!");
                }
            }
            if ((token.picture != nullptr) && (token.picture != nullptr)) {
                auto bitmap = token.picture->getBuffer();
                uint16_t width = token.pixWidth;
                uint16_t height = token.pixHeight;
                // int16_t y = deciToPix(deci_y + ENDPARAGRAPH_DECI_LINE_HEIGHT) + height / 2;
                // uint8_t darkPercent = computeDarkPercent(bitmap, width, height);
                // if (darkPercent > darkPicturePercent) {
                //     darkPicturePercent = darkPercent;
                // }
                display_.drawXBitmap(rect_.x + (rect_.width - width) / 2,
                                     rect_.y + deciToPix(deciY), bitmap, width, height,
                                     display_.fg);
                deciY += pixToDeci(height + 4);
            } else {
                log_e("Picture not there!!");
                REND_EXIT_SIM;
            }
            break;

        case TokenId::ENDPARAGRAPH:
            if (deciY == 0) {
                REND_WLOG("[%" PRIu16 "] %s: <PARA NOT SHOWN>", lineIdx,
                          token.streamPlace.str().c_str());
                continue;
            }
            REND_WLOG("[%" PRIu16 "] %s: <PARA>", lineIdx, token.streamPlace.str().c_str());
            deciY += lines_[lineIdx].deciHeight;
            break;

        case TokenId::HRULE: {
            REND_WLOG("[%" PRIu16 "] %s: <HRULE>", lineIdx, token.streamPlace.str().c_str());
            uint16_t y = deciToPix(deciY + (lines_[lineIdx].deciHeight / 2));
            display_.drawLine(rect_.x, rect_.y + y, rect_.x + rect_.width, rect_.y + y,
                              display_.fg);
            deciY += lines_[lineIdx].deciHeight;
        } break;

        case TokenId::BTABLE:
            REND_WLOG("[%" PRIu16 "] %s: <BTABLE>", lineIdx, token.streamPlace.str().c_str());
            if (lines_[lineIdx].deciHeight > NEWLINE_DECI_LINE_HEIGHT) {
                deciY += ENDPARAGRAPH_DECI_LINE_HEIGHT;
            }

#if CONFIG_FONT_TTF
            drawSingleLine(lines_[lineIdx], deciToPix(deciY));
#else
            Line line;
            getLine(lines_[lineIdx], line);
            display_.drawSingleLineOfText(line.data(), rect_.x + lines_[lineIdx].leftIndent,
                                          rect_.y + deciToPix(deciY));
#endif
            deciY += NEWLINE_DECI_LINE_HEIGHT;
            break;

        case TokenId::ETABLE: {
            REND_WLOG("[%" PRIu16 "] %s: <ETABLE>", lineIdx, token.streamPlace.str().c_str());
            uint16_t y = deciToPix(deciY + (HRULE_DECI_LINE_HEIGHT / 2));
            display_.drawLine(rect_.x, rect_.y + y, rect_.x + rect_.width, rect_.y + y,
                              display_.fg);
            deciY += HRULE_DECI_LINE_HEIGHT;

#if CONFIG_FONT_TTF
            drawSingleLine(lines_[lineIdx], deciToPix(deciY));
#else
            Line line;
            getLine(lines_[lineIdx], line);
            display_.drawSingleLineOfText(line.data(), rect_.x + lines_[lineIdx].leftIndent,
                                          rect_.y + deciToPix(deciY));
#endif
            deciY += NEWLINE_DECI_LINE_HEIGHT + ENDPARAGRAPH_DECI_LINE_HEIGHT;
        } break;

        default:
            // Only draw the lines if it's not skipped from the caller.
            if (lineNbr >= numSkipLines) {
#if CONFIG_FONT_TTF
                drawSingleLine(lines_[lineIdx], deciToPix(deciY));
#else
                Line line;
                getLine(lines_[lineIdx], line);
                // log_w("Real line width: %" PRIu16 " indent: %" PRIu16,
                //       display_.getTextWidth(line.data()) + lines_[lineIdx].leftIndent,
                //       lines_[lineIdx].leftIndent);
                REND_WLOG("[%" PRIu16 "] %s: <%s>", lineIdx, token.streamPlace.str().c_str(),
                          line.data());
                display_.drawSingleLineOfText(line.data(), rect_.x + lines_[lineIdx].leftIndent,
                                              rect_.y + deciToPix(deciY));
#endif
            } else {
                REND_WLOG("[%" PRIu16 "] %s: <SKIPPED>", lineIdx, token.streamPlace.str().c_str());
            }
            deciY += NEWLINE_DECI_LINE_HEIGHT;
            break;
        }
    }

    return darkPicturePercent;
}

// Used in the TXT book/article context to find the beginning of the page to show when going
// backward.
void Pager::setFirstLineIdxBackward() {
    REND_WLOG("Start of First Line Idx search...");
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
        REND_ELOG("Lines[] is empty!!");
    }
    firstPageLineIdx_ = lineIdx;
}

void Pager::addMenuParameter(const std::string &key, const std::string &value) {
    menuParameters_[key] = value;
}

auto Pager::getMenuParameters() const -> const MenuParameters & { return menuParameters_; }

// Debugging stuff
void Pager::show() const {
    if constexpr (REND_DEBUG) {
        std::cout << "head:" << linesHeadIdx_ << ", Tail:" << linesTailIdx_ << std::endl;
        for (Idx lineIdx = linesHeadIdx_; lineIdx != linesTailIdx_; nextLineIdx(lineIdx)) {
            std::cout << lineIdx << " " << tokens_[lines_[lineIdx].tokenIdx].streamPlace.str()
                      << ": <";
            Idx lastTokenIdx =
                tokens_.addToTokenIdx(lines_[lineIdx].tokenIdx, lines_[lineIdx].tokenCount);
            for (Idx tokenIdx = lines_[lineIdx].tokenIdx; tokenIdx != lastTokenIdx;
                 tokens_.nextTokenIdx(tokenIdx)) {
                if ((static_cast<uint16_t>(tokens_[tokenIdx].tokenId) &
                     (static_cast<uint16_t>(TokenId::NEWLINE) |
                      static_cast<uint16_t>(TokenId::ENDPARAGRAPH))) == 0) {
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