#include "Renderer.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "UI/Fonts/Tahoma4pt7b.h"
#include "config.h"
#include "sindarin-debug.h"

// For deep logging when changing this file
//#define RENDERER_DEBUG

// This is the number of SUB-lines we want to split a line into
// for the purpose of being able to render bare newlines as less
// than a full line height of text. (Deci was chosen as 1/10)/
// So what the two lines below mean is that a new line by itself
// on a line will render as 4/10s of a full line height.
// Theoretically we can do any percentage we want here, but there's
// a lot of testing to do for that!
#define FULL_DECI_LINE_HEIGHT 10
#define NEWLINE_ONLY_DECI_LINE_HEIGHT 4

Renderer::Renderer(DisplaySystem &display) : display_(display) {}

// must be called after loadAndTokenizePage
// Takes a place in the token buffer and places the string that the token
// represents in the outputString buffer. This is a helper since it's a
// common thing we want to do in the rendering code.
void Renderer::tokenToString(unsigned long placeInTokenBuffer, TokenString &outputString) {
    std::copy_n(&charTokenBuffer_[currTokenBuffer_[placeInTokenBuffer].place],
                currTokenBuffer_[placeInTokenBuffer + 1].place -
                    currTokenBuffer_[placeInTokenBuffer].place,
                &outputString[0]);
    outputString[currTokenBuffer_[placeInTokenBuffer + 1].place -
                 currTokenBuffer_[placeInTokenBuffer].place] = '\0';
}

// The thing is, most tokens are easy to determine the width of because the
// operation is the same regardless of context. Spaces are the exception.
// A space may render differently based on the characters before and after it.
// In my limited testing on our 5pt Tahoma font, spaces can be 3 or 4 pixels wide.
// I suspect this will only grow as our font rendering becomes more sophisticated.
// So this returns the obvious thing for non-spaces and does the math for spaces
// based on the tokens before and after the space.
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

// A helper method to fill the size of a token after the tokens have been loaded
// into the token buffer.
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

#ifdef RENDERER_DEBUG
        log_d("Set the stream place to %lu", placeInFile + plcInBuffer);
#endif
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
#ifdef RENDERER_DEBUG
void Renderer::printOutTokensInCurrTokenBuffer() {
    auto currToken = currTokenBuffer_[0];
    for (int i = 0; (i < MAX_CHARS_PER_PAGE) && (currToken.token != TOKEN_EOF); i++) {
        currToken = currTokenBuffer_[i];
        log_d("%u: %u", currToken.token, currToken.place);
        if (currToken.token == TOKEN_TEXT) {
            for (int j = currToken.place; j < currTokenBuffer_[i + 1].place; j++) {
                log_d("%c", charTokenBuffer_[j]);
            }
            log_d("\n");
        }
    }
}
#endif

// This method takes in a place in the token buffer.
// It puts the string in the inputLine.
// It returns the next token in the TokenBuffer unless already at EOF, then
// it returns EOF again.
//
//
auto Renderer::getLineOfTextForRendering(unsigned int placeInTokenBuffer, LineBuffer &inputLine)
    -> unsigned int {
    unsigned int bufferPlace = placeInTokenBuffer;
    unsigned int linePlace = 0;
    int maxWidth = (display_.width() - 4);
    int runningWidth = 0;

    inputLine[linePlace] = '\0';

    while (runningWidth < maxWidth) {
        RendererToken currToken = currTokenBuffer_[bufferPlace];

        if (currToken.token == TOKEN_EOF || currToken.token == TOKEN_NEWLINE) {
            if (currToken.token == TOKEN_NEWLINE) {
                bufferPlace++;
            }
            break;
        }
        runningWidth += currToken.width;
        bufferPlace++;
    }

    // Go back one token as that was the token that caused the line to be full.
    if (runningWidth >= maxWidth) {
        bufferPlace--;
    }

    // This handles the case where a single token is longer than the width of a line
    // Eventually, we'll break it up, but for now, we'll just let it run over the length
    // of the line as the break-up text is much more complicated.
    if (bufferPlace == placeInTokenBuffer) {
        bufferPlace++;
    }

    for (int i = placeInTokenBuffer; i < bufferPlace; i++) {
        if (currTokenBuffer_[i].token != TOKEN_EOF) {
            // Skip over a space if it's the first token in the line.
            if ((i == placeInTokenBuffer) && (currTokenBuffer_[i].token == TOKEN_SPACE)) {
                continue;
            }
            std::copy_n(&charTokenBuffer_[currTokenBuffer_[i].place],
                        currTokenBuffer_[i + 1].place - currTokenBuffer_[i].place,
                        &inputLine[linePlace]);
            linePlace += currTokenBuffer_[i + 1].place - currTokenBuffer_[i].place;
            inputLine[linePlace] = '\0';
        }
    }

#ifdef RENDERER_DEBUG
    log_d("Returning place %d", bufferPlace);
#endif
    return bufferPlace;
}

void Renderer::loaded() {
    // previousPlace_ = goBackwardsOnePage(currentPlace_);
    nextPlace_ = loadCurrIntoBuffer();
}

// Show the (already loaded into buffer) current page
// and load the next page into the next buffer so we have it.
auto Renderer::renderPage(unsigned long placeInFile, int numSkipLines) -> unsigned long {
    SetCpuSpeedMhz(CPU_FREQ_MHZ_HIGH);

#ifdef RENDERER_DEBUG
    log_d("%lu is place in file being requested", placeInFile);
#endif
    currentPlace_ = placeInFile;
    PROFILE_START(loadCurrIntoBuffer);
    nextPlace_ = loadCurrIntoBuffer();
    PROFILE_END_LOG_MS(loadCurrIntoBuffer);

    currentPageWordCount_ = WordCount(currBuffer_.data());

    int newLineAdjust = 0;

    display_.setFont(display_.style.getFontFamily().readableFont);

    // A first attempt to get the the text to look better with newlines as sole
    // characters on the line with a smaller height than a full line.
    // The logic is that we split each line into 10 "decilines" and then
    // assign a height to each line based on if it's a newline or not. Newlines
    // get NEWLINE_ONLY_DECI_LINE_HEIGHT decilines (currently set to 4 out of 10).
    // The y position calucalted is based on the number of newlines we've seen so far
    // in this page.
    for (int j = 0; j < numLinesInBuffer_; j++) {
#ifdef RENDERER_DEBUG
        log_d("%s", lineBufferArray_[j].data());
#endif
        if (lineBufferArray_[j][0] == '\n') {
            newLineAdjust++;
        } else {
            // Only draw the lines if it's not skipped from the caller.
            if (j >= numSkipLines) {
                display_.drawSingleLineOfText(
                    lineBufferArray_[j].data(), 0,
                    j * (display_.getTextHeight()) -
                        display_.getTextHeight() *
                            (FULL_DECI_LINE_HEIGHT - NEWLINE_ONLY_DECI_LINE_HEIGHT) *
                            newLineAdjust / FULL_DECI_LINE_HEIGHT);
            }
        }
    }

    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);

    return nextPlace_;
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
// close to the beginning, we just read from the beginning. If we have enough
// room, we read MAX_CHARS_PER_PAGE backwards. XXX if we *don't* hit a newline
// in that number of characters, the results of this method won't be correct.
// it'll still render something that should end in the token before the passed
// in place, but it won't necessarily be the same rendering as what you'd see
// if you rendered that page forwards.
//
// Step 2: Once the data is read into the buffer, tokenize it the same as you
// would for a forwards render.
//
// Step 3: Find the place in the token buffer where the passed in place in file
// is represented. Go back to the start of that paragraph (the last newline + 1) and
// simulate a render from there, keeping track of how many lines we needed to render.
// This is the "first paragraph" but it really represents the last set of lines on the
// page before the place passed in.
//
// Step 4: Iterate through paragraphs (newline-terminated sequences of tokens) backwards
// until we've done enough paragraphs to fill a page. Once we've done the last paragraph
// this way, it'll likely be too many times, so we calculate the offset of the first paragraph
// that is used in the page and start counting from there.
//
// Step 5: Save off the place in the token buffer where the prevous page starts and
// return it.
auto Renderer::goBackwardsOnePage(unsigned long place) -> unsigned long {
    unsigned long seekPlace = 0;

    // Step 1 (See above)
    if (place >= MAX_CHARS_PER_PAGE) {
        // Need to make sure we're at least 1 character past the place given. For tokenizer.
        seekPlace = place - MAX_CHARS_PER_PAGE + 2;
    } else {
        seekPlace = 0;
    }

    // Step 2.
    loadAndTokenizePage(seekPlace);

    // Go backwards from the last token and do the same as the loadIntoBuffer
    // method.

    unsigned int plcInTokenBuffer = numCurrTokens_ - 1;
    int deciLinesInBuffer = 1;
    int linesInBuffer = 1;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};

    // It's minus 2 because we want to start 1 off the top and leave 1 at the bottom
    // Note that get_text_height adds 1 over the max character height
    // change to deciLines for better newline performance
    int maxDeciLines =
        (display_.height() - 2) / (this->display_.getTextHeight()) * FULL_DECI_LINE_HEIGHT;

#ifdef RENDERER_DEBUG
    log_d("BACKWARDS DISPLAY READOUT: %d, %d, %d", display_.height() - 2,
          this->display_.getTextHeight(), this->display_.getLineSpacing());
#endif
    PageBuffer buffer;

    buffer[0] = '\0';
    singleLine[0] = '\0';

    unsigned int currToken = plcInTokenBuffer;

// Find which token to start with
// Could switch to an algorithm that is log(n) instead of n if we need to
#ifdef RENDERER_DEBUG
    log_d("About to look for where to start: place = %d", place);
#endif
    while ((currTokenBuffer_[currToken].streamPlace > place) && (currToken > 0)) {
#ifdef RENDERER_DEBUG
        log_d("Going back another token %d", currTokenBuffer_[currToken].streamPlace);
        log_d("The token is %d", currToken);
#endif
        currToken--;
    }

    if (currToken == 0) {
        // Can't go back any further, must be at beginning of file.
        return 0;
    }
#ifdef RENDERER_DEBUG
    log_d("Found the place to start and it's %d token type %d",
          currTokenBuffer_[currToken].streamPlace, currTokenBuffer_[currToken + 1].token);
#endif

    unsigned int paragraphToken = currTokenBuffer_[currToken].prevNewline + 1;

    // Step 3.
    // First, check the immediate paragraph for how many lines fit before
    // the passed in token;
    while (paragraphToken < currToken) {
        paragraphToken = getLineOfTextForRendering(paragraphToken, singleLine);
#ifdef RENDERER_DEBUG
        log_d("paragraphToke: %d, currToke: %d, First PARAGRAPH ANALYSIS %s", paragraphToken,
              currToken, singleLine.data());
#endif
        if (singleLine[0] == '\n') {
#ifdef RENDERER_DEBUG
            log_d("Found a blank line");
#endif
            deciLinesInBuffer += NEWLINE_ONLY_DECI_LINE_HEIGHT;
        } else {
            deciLinesInBuffer += FULL_DECI_LINE_HEIGHT;
        }
        linesInBuffer++;
    }

    unsigned int numDeciLinesInFirstParagraph = deciLinesInBuffer;

    deciLinesInBuffer = 1;
    linesInBuffer = 1;
    unsigned int newlinesInLastSection = 0;

    unsigned int prevParagraphToken =
        currTokenBuffer_[currTokenBuffer_[currToken].prevNewline].prevNewline + 1;
    unsigned int nextyToken = currTokenBuffer_[currToken].prevNewline;

    // Step 4.
    while ((deciLinesInBuffer + numDeciLinesInFirstParagraph) <= maxDeciLines) {
        paragraphToken = prevParagraphToken;
        newlinesInLastSection = 0;
        while (paragraphToken <= nextyToken) {
            paragraphToken = getLineOfTextForRendering(paragraphToken, singleLine);
#ifdef RENDERER_DEBUG
            log_d("paragraphToke: %d, PrevToke: %d, Second PARAGRAPH ANALYSIS %s", paragraphToken,
                  prevParagraphToken, singleLine.data());
#endif
            if (singleLine[0] == '\n') {
#ifdef RENDERER_DEBUG
                log_d("Found a blank line");
#endif
                deciLinesInBuffer += NEWLINE_ONLY_DECI_LINE_HEIGHT;
                newlinesInLastSection++;
            } else {
                deciLinesInBuffer += FULL_DECI_LINE_HEIGHT;
            }
            linesInBuffer++;
        }
#ifdef RENDERER_DEBUG
        log_d("Done with that paragraph! onto the next");
#endif
        prevParagraphToken =
            currTokenBuffer_[currTokenBuffer_[nextyToken].prevNewline].prevNewline + 1;
#ifdef RENDERER_DEBUG
        log_d("After done prevParagraphToken = %d", prevParagraphToken);
#endif
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
    }

    int overage = (deciLinesInBuffer + numDeciLinesInFirstParagraph) - maxDeciLines;

    int firstParagraphLinesOffset = 0;

    // Only index in if we need to.
    if (overage > FULL_DECI_LINE_HEIGHT) {
#ifdef RENDERER_DEBUG
        log_d("The overage was %d overlines", overage);
#endif
        firstParagraphLinesOffset = overage / FULL_DECI_LINE_HEIGHT;

        if (overage % FULL_DECI_LINE_HEIGHT != 0) {
            firstParagraphLinesOffset++;
        }
        firstParagraphLinesOffset++;
    }

#ifdef RENDERER_DEBUG

    log_d("First Paragraph Lines Offset %d", firstParagraphLinesOffset);

#endif

    for (int i = 0; i < firstParagraphLinesOffset; i++) {
        paragraphToken = getLineOfTextForRendering(paragraphToken, singleLine);
    }

    // Step 5.
    // At this point, we found the place to start the new page!
    currToken = paragraphToken;

    unsigned long returnValue = currTokenBuffer_[currToken].streamPlace;

    // methods calling this method expect this to not change what's loaded
    // into buffers, so we need to retokenize.
    // we could probably avoid this rework by introducing an extra token buffer if we wanted to.
    loadAndTokenizePage(place);

#ifdef RENDERER_DEBUG
    log_d("Returning the following place! %d", returnValue);
#endif
    return returnValue;
}

// Get the length of the line at place in the lineBufferArray
//
auto Renderer::getLengthOfLine(unsigned int index) -> unsigned int {
    return lineBufferLengthsArray_[index];
}

auto Renderer::getTotalLengthOfLines() -> unsigned int {
    int totalLengthsOfLines = 0;
    for (int i = 0; i < numLinesInBuffer_; i++) {
        totalLengthsOfLines += lineBufferLengthsArray_[i];
    }
    return totalLengthsOfLines;
}

// returns the new place of the stream
auto Renderer::loadIntoBuffer(PageBuffer &buffer, unsigned long place) -> unsigned long {
    unsigned int plcInBuffer = 0;
    int deciLinesInBuffer = 1;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};

    // It's minus 2 because we want to start 1 off the top and leave 1 at the bottom
    // Note that get_text_height adds 1 over the max character height
    //
    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    int maxDeciLines =
        ((display_.height() - 2) / (this->display_.getTextHeight())) * FULL_DECI_LINE_HEIGHT;

#ifdef RENDERER_DEBUG
    log_d("Max lines calc: %d %d %d", display_.height(), display_.getTextHeight(),
          display_.getLineSpacing());
#endif

    buffer[0] = '\0';
    singleLine[0] = '\0';

    // Debugging stuff
    loadAndTokenizePage(place);

    unsigned int nextToken = 0;
    unsigned int currToken = 0;

    numLinesInBuffer_ = 0;

    while (deciLinesInBuffer <= maxDeciLines) {
        // Don't start pages with newlines.
        if (nextToken == 0) {
            while (currTokenBuffer_[currToken].token == TOKEN_NEWLINE) {
                currToken++;
            }
        }
        nextToken = getLineOfTextForRendering(currToken, singleLine);

        std::copy_n(singleLine.data(), strlen(singleLine.data()), &buffer[plcInBuffer]);

        std::copy_n(singleLine.data(), strlen(singleLine.data()),
                    lineBufferArray_[numLinesInBuffer_].data());

        lineBufferArray_[numLinesInBuffer_][strlen(singleLine.data())] = '\0';
        lineBufferLengthsArray_[numLinesInBuffer_] = strlen(singleLine.data());

        plcInBuffer += strlen(singleLine.data());

        // if it doesn't have a \n at the end of the line, we need to add it.
        if (singleLine[strlen(singleLine.data()) - 1] != '\n') {
            buffer[plcInBuffer] = '\n';
            plcInBuffer++;
        }

        buffer[plcInBuffer] = '\0';

        // A regular new line is 10 deciLines, a newline all by itself is 4/10 (40% of a line)
        if (lineBufferArray_[numLinesInBuffer_][0] == '\n') {
            deciLinesInBuffer += NEWLINE_ONLY_DECI_LINE_HEIGHT;
        } else {
            deciLinesInBuffer += FULL_DECI_LINE_HEIGHT;
        }

        numLinesInBuffer_++;
        currToken = nextToken;
    }

#ifdef RENDERER_DEBUG
    log_d("loaded this into buffer %s", buffer.data());
#endif

    // If we hit EOF, the next screen wraps around.
    if (currTokenBuffer_[currToken].token == TOKEN_EOF) {
        return 0;
    } else {
        return currTokenBuffer_[currToken].streamPlace;
    }
}

auto Renderer::loadCurrIntoBuffer() -> unsigned long {
    return loadIntoBuffer(currBuffer_, currentPlace_);
}

auto Renderer::getPreviousPlace() -> unsigned long {
    previousPlace_ = goBackwardsOnePage(currentPlace_);
    return previousPlace_;
}