#include "Renderer.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

#define FULL_DECI_LINE_HEIGHT 10
#define NEWLINE_ONLY_DECI_LINE_HEIGHT 5

Renderer::Renderer(DisplaySystem &display) : display_(display) {}

// must be called after loadAndTokenizePage
void Renderer::tokenToString(unsigned long placeInTokenBuffer, TokenString &outputString) {
    std::copy_n(&charTokenBuffer_[currTokenBuffer_[placeInTokenBuffer].place],
                currTokenBuffer_[placeInTokenBuffer + 1].place -
                    currTokenBuffer_[placeInTokenBuffer].place,
                &outputString[0]);
    outputString[currTokenBuffer_[placeInTokenBuffer + 1].place -
                 currTokenBuffer_[placeInTokenBuffer].place] = '\0';
}

void Renderer::tokenRenderedWidthHeight(unsigned long placeInTokenBuffer, uint16_t *renderedWidth,
                                        uint16_t *renderedHeight) {
    TokenString converted;
    // Spaces are a mess. The width of them depends on the tokens before and after. Joy.
    if (currTokenBuffer_[placeInTokenBuffer].token != TOKEN_SPACE) {
        tokenToString(placeInTokenBuffer, converted);
        *renderedWidth = display_.getTextWidth(converted.data());
        *renderedHeight = display_.getTextHeight(converted.data());
    } else {
        // Check bounds. Punt otherwise.
        // Get the tokens before and after the space, then do the width
        // of that whole thing and subtract out the two tokens. That's the
        // real width of the rendered space.
        if ((placeInTokenBuffer > 0) &&
            (currTokenBuffer_[placeInTokenBuffer + 1].token != TOKEN_EOF)) {
            TokenString prev, next;
            LineBuffer temp;
            tokenToString(placeInTokenBuffer - 1, prev);
            tokenToString(placeInTokenBuffer + 1, next);
            std::copy_n(prev.data(), strlen(prev.data()), &temp[0]);
            temp[strlen(prev.data())] = ' ';
            std::copy_n(next.data(), strlen(next.data()), &temp[strlen(prev.data()) + 1]);
            temp[strlen(prev.data()) + strlen(next.data()) + 1] = '\0';
            *renderedHeight = display_.getTextHeight(temp.data());
            *renderedWidth = display_.getTextWidth(temp.data()) -
                             display_.getTextWidth(prev.data()) -
                             display_.getTextWidth(next.data());
        } else {
            tokenToString(placeInTokenBuffer, converted);
            *renderedWidth = display_.getTextWidth(converted.data());
            *renderedHeight = display_.getTextHeight(converted.data());
        }
    }
}

void Renderer::setSizeOfTextToken(unsigned long placeInTokenBuffer) {
    TokenString converted;
    tokenToString(placeInTokenBuffer, converted);
    tokenRenderedWidthHeight(placeInTokenBuffer, &currTokenBuffer_[placeInTokenBuffer].width,
                             &currTokenBuffer_[placeInTokenBuffer].height);
}

//
// This loads from the stream character by character and populates
// the token buffer based on the RENDERER_TOKEN enum.
// After this is done, then we can do the heavy lifting of walking
// through the tokens, checking size, and rendering the text on
// a line by line basis.
//
void Renderer::loadAndTokenizePage(unsigned long placeInFile) {
    unsigned int plcInBuffer = 0;
    unsigned int plcInTokenBuffer = 0;
    unsigned int curPrevNewLine = 0;

    numCurrTokens_ = 0;

    charTokenBuffer_[plcInBuffer] = '\0';

    RendererToken eofToken = {TOKEN_EOF, 0, 0, 0, 0};
    currTokenBuffer_[plcInBuffer] = eofToken;

    if (!stream_->seek(placeInFile)) {
        log_e("failed to seek %lu is the place", placeInFile);
        return;
    }

    // Needs to be minus one for the corner case of all spaces or newlines
    // and need room for EOF character.
    while (stream_->available() && plcInBuffer < MAX_CHARS_PER_PAGE - 1) {
        auto curChar = stream_->read();
        RendererToken newToken = {TOKEN_EOF,      plcInBuffer, placeInFile + plcInBuffer,
                                  curPrevNewLine, 0,           0};
        RendererToken prevToken;

        // log_d("Set the stream place to %lu", placeInFile + plcInBuffer);
        charTokenBuffer_[plcInBuffer] = curChar;
        charTokenBuffer_[plcInBuffer + 1] = '\0';
        switch (curChar) {
        case '\n':
            newToken.token = TOKEN_NEWLINE;
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            // we need to set the size of the previous token
            if ((plcInTokenBuffer > 0) &&
                currTokenBuffer_[plcInTokenBuffer - 1].token == TOKEN_TEXT) {
                setSizeOfTextToken(plcInTokenBuffer - 1);
                // If we've started a new text token and the previous token was a space
                // we know that we should go back and set the size of that space token
                // based on this token and the token before the space.
                if ((plcInTokenBuffer > 2) &&
                    currTokenBuffer_[plcInTokenBuffer - 2].token == TOKEN_SPACE) {
                    setSizeOfTextToken(plcInTokenBuffer - 2);
                }
            }
            curPrevNewLine = plcInTokenBuffer;
            plcInTokenBuffer++;
            break;
        case ' ':
            newToken.token = TOKEN_SPACE;
            newToken.width = display_.getTextWidth(" ");
            newToken.height = display_.getTextHeight(" ");
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            // we need to set the size of the previous token
            if ((plcInTokenBuffer > 0) &&
                currTokenBuffer_[plcInTokenBuffer - 1].token == TOKEN_TEXT) {
                setSizeOfTextToken(plcInTokenBuffer - 1);
                // If we've started a new text token and the previous token was a space
                // we know that we should go back and set the size of that space token
                // based on this token and the token before the space.
                if ((plcInTokenBuffer > 2) &&
                    currTokenBuffer_[plcInTokenBuffer - 2].token == TOKEN_SPACE) {
                    setSizeOfTextToken(plcInTokenBuffer - 2);
                }
            }
            plcInTokenBuffer++;
            break;
        case '\t':
            newToken.token = TOKEN_TAB;
            newToken.width = display_.getTextWidth("\t");
            newToken.height = display_.getTextHeight("\t");
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            // we need to set the size of the previous token
            if ((plcInTokenBuffer > 0) &&
                currTokenBuffer_[plcInTokenBuffer - 1].token == TOKEN_TEXT) {
                setSizeOfTextToken(plcInTokenBuffer - 1);
                // If we've started a new text token and the previous token was a space
                // we know that we should go back and set the size of that space token
                // based on this token and the token before the space.
                if ((plcInTokenBuffer > 2) &&
                    currTokenBuffer_[plcInTokenBuffer - 2].token == TOKEN_SPACE) {
                    setSizeOfTextToken(plcInTokenBuffer - 2);
                }
            }
            plcInTokenBuffer++;
            break;
        case '-':
            // Check to see if this is an emdash case
            if (plcInTokenBuffer > 0) {
                prevToken = currTokenBuffer_[plcInTokenBuffer - 1];
                if (prevToken.token == TOKEN_HYPHEN) {
                    newToken.token = TOKEN_EMDASH;
                    newToken.width = display_.getTextWidth("--");
                    newToken.height = display_.getTextHeight("--");
                    newToken.place = plcInBuffer - 1;
                    newToken.streamPlace = placeInFile + plcInBuffer - 1;
                    currTokenBuffer_[plcInTokenBuffer] = newToken;
                    currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
                    // Specifically don't increment the token buffer
                    // because the previous hyphen already did that
                    break;
                }
            }
            newToken.token = TOKEN_HYPHEN;
            newToken.width = display_.getTextWidth("-");
            newToken.height = display_.getTextHeight("-");
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            // we need to set the size of the previous token
            if ((plcInTokenBuffer > 0) &&
                currTokenBuffer_[plcInTokenBuffer - 1].token == TOKEN_TEXT) {
                setSizeOfTextToken(plcInTokenBuffer - 1);
                // If we've started a new text token and the previous token was a space
                // we know that we should go back and set the size of that space token
                // based on this token and the token before the space.
                if ((plcInTokenBuffer > 2) &&
                    currTokenBuffer_[plcInTokenBuffer - 2].token == TOKEN_SPACE) {
                    setSizeOfTextToken(plcInTokenBuffer - 2);
                }
            }
            plcInTokenBuffer++;
            break;
        // All other characters are considered part of a text token,
        // so we just increment and move on.
        default:
            if (plcInTokenBuffer > 0) {
                prevToken = currTokenBuffer_[plcInTokenBuffer - 1];
            } else {
                // If this is the first character in the token buffer,
                // pretend like the previous token was EOF so we handle
                // the new token creation correctly below.
                prevToken.token = TOKEN_EOF;
            }
            if (prevToken.token != TOKEN_TEXT) {
                newToken.token = TOKEN_TEXT;
                currTokenBuffer_[plcInTokenBuffer] = newToken;
                numCurrTokens_++;
                currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
                plcInTokenBuffer++;
                break;
            }
        }
        plcInBuffer++;
    }
    // Ended on a text token, so we move forward one before setting the EOF
    // token.
    if (currTokenBuffer_[plcInTokenBuffer].token != TOKEN_EOF) {
        plcInTokenBuffer++;
    }

    // Set the EOF place correctly.
    currTokenBuffer_[plcInTokenBuffer].place = plcInBuffer;
    currTokenBuffer_[plcInTokenBuffer].streamPlace = placeInFile + plcInBuffer;
}

// debugging method
void Renderer::printOutTokensInCurrTokenBuffer() {
    auto currToken = currTokenBuffer_[0];
    for (int i = 0; (i < MAX_CHARS_PER_PAGE) && (currToken.token != TOKEN_EOF); i++) {
        currToken = currTokenBuffer_[i];
        // log_d("%u: %u", currToken.token, currToken.place);
        if (currToken.token == TOKEN_TEXT) {
            for (int j = currToken.place; j < currTokenBuffer_[i + 1].place; j++) {
                //        log_d("%c", charTokenBuffer_[j]);
            }
            //    log_d("\n");
        }
    }
}

// This method takes in a place in the token buffer and returns
// one line of text that can be rendered. It puts the string in the inputLine.
// It returns the next token in the TokenBuffer unless already at EOF, then
// it returns EOF again.
//
// XXX: Ultimately this class should have helpers (render line, render token)
// that does the heavy lifting of telling the other parts of rendering how tall and
// wide a rendered line / token / page is.
//
auto Renderer::getLineOfTextForRendering(unsigned int placeInTokenBuffer, LineBuffer &inputLine,
                                         bool readBackwards) -> unsigned int {
    unsigned int bufferPlace = placeInTokenBuffer;
    unsigned int linePlace = 0;
    bool lineFull = false;
    int maxWidth = (display_.width() - 4);
    int pixLen;
    int runningWidth = 0;

    inputLine[linePlace] = '\0';

    RendererToken nextTokenForBackwards = currTokenBuffer_[placeInTokenBuffer + 1];

    if (readBackwards) {
        runningWidth = nextTokenForBackwards.width;
    }

    while (runningWidth < maxWidth) {
        RendererToken currToken = currTokenBuffer_[bufferPlace];

        if (currToken.token == TOKEN_EOF || currToken.token == TOKEN_NEWLINE) {
            if (currToken.token == TOKEN_NEWLINE) {
                if (!readBackwards) {
                    bufferPlace++;
                } else {
                    bufferPlace--;
                }
            }
            break;
        }
        runningWidth += currToken.width;
        if (readBackwards) {
            // Catch the case where we are at the beginning of the buffer.
            if (bufferPlace == 0) {
                break;
            } else {
                bufferPlace--;
            }
        } else {
            bufferPlace++;
        }
    }

    // Go back one token as that was the token that caused the line to be full.
    // This is only in the forwards direction because the backwards direction pre-loads
    // the next token as part of the measurement to keep consistency in rendering the
    // previous page.

    if ((!readBackwards) && (runningWidth >= maxWidth)) {
        bufferPlace--;
    }

    int i;

    // Never done something like this before, but it should do the trick!
    for ((readBackwards ? (i = bufferPlace) : (i = placeInTokenBuffer));
         (readBackwards ? (i < placeInTokenBuffer) : (i < bufferPlace)); i++) {
        RendererToken curToken = currTokenBuffer_[i];
        if (currTokenBuffer_[i].token != TOKEN_EOF) {
            std::copy_n(&charTokenBuffer_[currTokenBuffer_[i].place],
                        currTokenBuffer_[i + 1].place - currTokenBuffer_[i].place,
                        &inputLine[linePlace]);
            linePlace += currTokenBuffer_[i + 1].place - currTokenBuffer_[i].place;
            inputLine[linePlace] = '\0';
        }
    }

    // log_d("Returning place %d", bufferPlace);
    return bufferPlace;
}

void Renderer::loaded() {
    previousPlace_ = goBackwardsOnePage(currentPlace_);

    // LOAD TEXT for prev, current.
    // The contract for users of this class is that this class will always keep the current
    // page, the previous page, and the next page in a buffer so that on page swaps, the render
    // can happen immediately without any filesystem access. Then, *after* a page is rendered,
    // the new previous and next pages can be loaded. This allows for buffering to happen while
    // a user is reading the current page.
    //
    // It's a little broken in that load_curr_into_buffer does the wrong thing and updates class
    // variables (essentially moving the current seek position in the file), hence this dance
    // here to make sure they are correct.
    // TODO(john): It doesn't look like it works this way right now. Our preloading does
    // nothing.
    // loadPrevIntoBuffer();
    nextPlace_ = loadCurrIntoBuffer();
    int oldNextPlace = nextPlace_;
    // loadNextIntoBuffer();
    nextPlace_ = oldNextPlace;
}

// Show the (already loaded into buffer) current page
// and load the next page into the next buffer so we have it.
auto Renderer::renderPage(unsigned long placeInFile) -> unsigned long {
    unsigned long nextPos = 0;
    SetCpuSpeedMhz(CPU_FREQ_MHZ_HIGH);

    // TODO: We're not taking advantage of the preloading work we do to load the next buffer
    // log_i("%lu is place in file being requested", placeInFile);
    currentPlace_ = placeInFile;
    PROFILE_START(loadCurrIntoBuffer);
    nextPlace_ = loadCurrIntoBuffer();
    PROFILE_END_LOG_MS(loadCurrIntoBuffer);

    currentPageWordCount_ = WordCount(currBuffer_.data());
    // display_.drawText(currBuffer_.data(), display_.xOffset(0), display_.yOffset(0),
    //                   display_.width(), display_.height());

    int newLineAdjust = 0;
    int currentY = 0;

    // A first attempt at a hack to get the the text to look better with newlines as sole
    // characters on the line

    for (int j = 0; j < numLinesInBuffer_; j++) {
        log_d("%s", lineBufferArray_[j].data());
        if (lineBufferArray_[j][0] == '\n') {
            newLineAdjust++;
        } else {
            display_.drawSingleLineOfText(
                lineBufferArray_[j].data(), display_.xOffset(0),
                display_.yOffset(j * (display_.getLineSpacing() + display_.getTextHeight()) -
                                 display_.getTextHeight() *
                                     (FULL_DECI_LINE_HEIGHT - NEWLINE_ONLY_DECI_LINE_HEIGHT) *
                                     newLineAdjust / FULL_DECI_LINE_HEIGHT));
        }
        currentY = j * (display_.getLineSpacing() + display_.getTextHeight()) -
                   display_.getTextHeight() * 2 * newLineAdjust / 3;
    }

    display_.updateDisplay();

    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);

    nextPos = nextPlace_;
    PROFILE_START(loadNextIntoBuffer);
    // loadNextIntoBuffer();
    PROFILE_END_LOG_MS(loadNextIntoBuffer);

    PROFILE_START(loadPrevIntoBuffer);

    //    loadPrevIntoBuffer();
    PROFILE_END_LOG_MS(loadPrevIntoBuffer);

    nextPlace_ = nextPos;
    return nextPos;
}

// The place that is being passed in points to the first character of a page.
// Going back one place from there points to a space. Going back two places
// from there points to the last character of the previous word. Hence, we
// need to rewind two spaces from the place passed in for the work we do in
// this method.
//
// Could also be a const int, but that's up to the code reviewer overlords ;)
//
#define PLACE_REWIND 2

// Seeks backwards and returns the place of one page previous. Uses a similar
// logic to the load_into_buffer function but working backwards instead.
// This function's purpose is only to find the place in the file that's one
// page previous, not to load anything into buffers. It's a little less
// efficient, but given the load_into_buffer function works, we will opt for
// the inefficiency for now rather than rewrite the whole thing.
//
// XXX this is broken because it doesn't take into account the fact that the
// token that causes the overflow is in a line in the current implemetation
// is the PREVIOUS token to the line, not the NEXT token after the line.
// So to make the "goBackwards" algorithm work, we need to keep the NEXT
// token and use that to check to see if a line is full, not the PREVIOUS
// token. This will require a bit more copying around, but should give us
// a backwards seek that will yield exactly the same result as a forward
// seek!
auto Renderer::goBackwardsOnePage(unsigned long place) -> unsigned long {

    char curChar = '\0';
    std::array<char, MAX_CHARS_PER_PAGE + 1> mainBackwardsBuffer;
    unsigned long seekPlace = 0;
    int numCharsToRead;

    if (place >= MAX_CHARS_PER_PAGE) {
        // Need to make sure we're at least 1 character past the place given. For tokenizer.
        seekPlace = place - MAX_CHARS_PER_PAGE + 2;
        numCharsToRead = MAX_CHARS_PER_PAGE;
    } else {
        seekPlace = 0;
        numCharsToRead = place;
    }

    // We'll just tokenize everything from the last and work our way from the
    // end of the tokens.
    loadAndTokenizePage(seekPlace);

    // Go backwards from the last token and do the same as the loadIntoBuffer
    // method.

    unsigned int plcInTokenBuffer = numCurrTokens_ - 1;
    unsigned int plcInBuffer = 0;
    int deciLinesInBuffer = 1;
    int linesInBuffer = 1;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};

    // It's minus 2 because we want to start 1 off the top and leave 1 at the bottom
    // Note that get_text_height adds 1 over the max character height
    // change to deciLines for better newline performance
    int maxDeciLines =
        (display_.height() - 2) / (this->display_.getTextHeight()) * FULL_DECI_LINE_HEIGHT;

    // log_d("BACKWARDS DISPLAY READOUT: %d, %d, %d", display_.height() - 2,
    //      this->display_.getTextHeight(), this->display_.getLineSpacing());
    PageBuffer buffer;

    buffer[0] = '\0';
    singleLine[0] = '\0';

    // printOutTokensInCurrTokenBuffer();
    unsigned int nextToken = plcInTokenBuffer;
    unsigned int currToken = plcInTokenBuffer;

    // Find which token to start with
    // Could switch to an algorithm that is log(n) instead of n if we need to
    // log_d("About to look for where to start: place = %d", place);
    while ((currTokenBuffer_[currToken].streamPlace > place) && (currToken > 0)) {
        // log_d("Going back another token %d", currTokenBuffer_[currToken].streamPlace);
        // log_d("The token is %d", currToken);
        currToken--;
    }

    if (currToken == 0) {
        // Can't go back any further, must be at beginning of file.
        return 0;
    }
    // log_d("Found the place to start and it's %d token type %d",
    //       currTokenBuffer_[currToken].streamPlace, currTokenBuffer_[currToken + 1].token);

    unsigned int paragraphToken = currTokenBuffer_[currToken].prevNewline + 1;

    // First, check the immediate paragraph for how many lines fit before
    // the passed in token;
    while ((deciLinesInBuffer <= maxDeciLines) && (paragraphToken < currToken)) {
        paragraphToken = getLineOfTextForRendering(paragraphToken, singleLine);
        //   log_d("paragraphToke: %d, currToke: %d, First PARAGRAPH ANALYSIS %s", paragraphToken,
        //         currToken, singleLine.data());
        if (singleLine[0] == '\n') {
            // log_d("Found a blank line");
            deciLinesInBuffer += NEWLINE_ONLY_DECI_LINE_HEIGHT;
        } else {
            deciLinesInBuffer += FULL_DECI_LINE_HEIGHT;
        }
        linesInBuffer++;
    }

    unsigned int numLinesInLastParagraph = linesInBuffer;
    unsigned int numDeciLinesInLastParagraph = deciLinesInBuffer;

    deciLinesInBuffer = 1;
    linesInBuffer = 1;

    unsigned int prevParagraphToken =
        currTokenBuffer_[currTokenBuffer_[currToken].prevNewline].prevNewline + 1;
    unsigned int nextyToken = currTokenBuffer_[currToken].prevNewline;

    while ((deciLinesInBuffer + numDeciLinesInLastParagraph) <= maxDeciLines) {
        paragraphToken = prevParagraphToken;
        while (paragraphToken <= nextyToken) {
            paragraphToken = getLineOfTextForRendering(paragraphToken, singleLine);
            //  log_d("paragraphToke: %d, PrevToke: %d, Second PARAGRAPH ANALYSIS %s",
            //  paragraphToken,
            //        prevParagraphToken, singleLine.data());
            if (singleLine[0] == '\n') {
                // log_d("Found a blank line");
                deciLinesInBuffer += NEWLINE_ONLY_DECI_LINE_HEIGHT;
            } else {
                deciLinesInBuffer += FULL_DECI_LINE_HEIGHT;
            }
            linesInBuffer++;
        }
        // log_d("Done with that paragraph! onto the next");
        prevParagraphToken =
            currTokenBuffer_[currTokenBuffer_[nextyToken].prevNewline].prevNewline + 1;
        // there's no newline at beginning of file, so need to check for that.
        if (prevParagraphToken == 1) {
            prevParagraphToken = 0;
        }
        nextyToken = currTokenBuffer_[nextyToken].prevNewline;
    }

    if (prevParagraphToken != 0) {
        paragraphToken = prevParagraphToken - 1;
    } else {
        paragraphToken = 0;
        // ????
        linesInBuffer -= 3;
    }

    int firstParagraphLinesOffset =
        linesInBuffer + numLinesInLastParagraph - maxDeciLines / FULL_DECI_LINE_HEIGHT;

    // log_d("First Paragraph Lines Offset %d", firstParagraphLinesOffset);

    for (int i = 0; i < firstParagraphLinesOffset; i++) {
        paragraphToken = getLineOfTextForRendering(paragraphToken, singleLine);
    }

    // At this point, we found the place to start the new page!
    currToken = paragraphToken;
    /*
    linesInBuffer = 1;

    while ((linesInBuffer <= maxLines) && (currToken > 0)) {
        nextToken = getLineOfTextForRendering(currToken, singleLine, false);
        std::copy_n(singleLine.data(), strlen(singleLine.data()), &buffer[plcInBuffer]);
        log_d("GO BACKWARDS line is %s", singleLine.data());
        plcInBuffer += strlen(singleLine.data());
        // if it doesn't have a \n at the end of the line, we need to add it.
        if (singleLine[strlen(singleLine.data()) - 1] != '\n') {
            buffer[plcInBuffer] = '\n';
            plcInBuffer++;
        }

        buffer[plcInBuffer] = '\0';
        linesInBuffer++;
        currToken = nextToken;
        log_d("lines in buffer %d, maxLines %d, currToken %d", linesInBuffer, maxLines, currToken);
    }*/

    unsigned long returnValue = currTokenBuffer_[currToken].streamPlace;

    // methods calling this method expect this to not change what's loaded
    // into buffers, so we need to retokenize.
    // we could probably avoid this rework by introducing an extra token buffer if we want
    loadAndTokenizePage(place);
    // log_d("RETURNING THE FOLLOWING PLACE! %d", returnValue);
    return returnValue;
}

// returns the new place of the stream
auto Renderer::loadIntoBuffer(PageBuffer &buffer, unsigned long place) -> unsigned long {
    unsigned int plcInTokenBuffer = 0;
    unsigned int plcInBuffer = 0;
    int deciLinesInBuffer = 1;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};

    // It's minus 2 because we want to start 1 off the top and leave 1 at the bottom
    // Note that get_text_height adds 1 over the max character height
    //
    // XXX: This has to be rethought to realize that the rendering will impact
    // the max number of lines that can be rendered since \n will be handled
    // differently as well chapter headers, etc.
    //
    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    int maxDeciLines =
        ((display_.height() - 2) / (this->display_.getTextHeight())) * FULL_DECI_LINE_HEIGHT -
        FULL_DECI_LINE_HEIGHT;

    // log_d("Max lines calc: %d %d %d", display_.height(), display_.getTextHeight(),
    //       display_.getLineSpacing());

    buffer[0] = '\0';
    singleLine[0] = '\0';

    // Debugging stuff
    loadAndTokenizePage(place);
    // printOutTokensInCurrTokenBuffer();
    unsigned int nextToken = 0;
    unsigned int currToken = 0;

    numLinesInBuffer_ = 0;

    while (deciLinesInBuffer <= maxDeciLines) {

        nextToken = getLineOfTextForRendering(currToken, singleLine);

        std::copy_n(singleLine.data(), strlen(singleLine.data()), &buffer[plcInBuffer]);

        std::copy_n(singleLine.data(), strlen(singleLine.data()),
                    lineBufferArray_[numLinesInBuffer_].data());

        lineBufferArray_[numLinesInBuffer_][strlen(singleLine.data())] = '\0';

        numLinesInBuffer_++;
        plcInBuffer += strlen(singleLine.data());

        // if it doesn't have a \n at the end of the line, we need to add it.
        if (singleLine[strlen(singleLine.data()) - 1] != '\n') {
            buffer[plcInBuffer] = '\n';
            plcInBuffer++;
        }

        buffer[plcInBuffer] = '\0';

        // A regular new line is 10 deciLines, a newline all by itself is 3/10 (30% of a line)
        if (lineBufferArray_[numLinesInBuffer_][0] == '\n') {
            deciLinesInBuffer += NEWLINE_ONLY_DECI_LINE_HEIGHT;
        } else {
            deciLinesInBuffer += FULL_DECI_LINE_HEIGHT;
        }

        currToken = nextToken;
    }

    // log_d("loaded this into buffer %s", buffer.data());

    // log_d("Number of lines in buffer: %d out of a max of %d", linesInBuffer, maxLines);

    // If we hit EOF, the next screen wraps around.
    if (currTokenBuffer_[currToken].token == TOKEN_EOF) {
        return 0;
    } else {
        return currTokenBuffer_[currToken].streamPlace;
    }
}

// Deprecating this
auto Renderer::loadPrevIntoBuffer() -> unsigned long {
    // return loadIntoBuffer(prevBuffer_, previousPlace_);
    return 0;
}

auto Renderer::loadCurrIntoBuffer() -> unsigned long {
    return loadIntoBuffer(currBuffer_, currentPlace_);
}

// Deprecating this
auto Renderer::loadNextIntoBuffer() -> unsigned long {
    return 0;
    // return loadIntoBuffer(nextBuffer_, nextPlace_);
}

auto Renderer::getPreviousPlace() -> unsigned long {
    previousPlace_ = goBackwardsOnePage(currentPlace_);
    return previousPlace_;
}