#include "Renderer.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

Renderer::Renderer(DisplaySystem &display) : display_(display) {}

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

    charTokenBuffer_[plcInBuffer] = '\0';

    RendererToken eofToken = {TOKEN_EOF, 0};
    currTokenBuffer_[plcInBuffer] = eofToken;

    if (!stream_->seek(placeInFile)) {
        log_e("failed to seek %lu is the place", placeInFile);
        return;
    }

    // Needs to be minus one for the corner case of all spaces or newlines
    // and need room for EOF character.
    while (stream_->available() && plcInBuffer < MAX_CHARS_PER_PAGE - 1) {
        auto curChar = stream_->read();
        RendererToken newToken = {TOKEN_EOF, plcInBuffer, placeInFile + plcInBuffer};
        RendererToken prevToken;

        charTokenBuffer_[plcInBuffer] = curChar;
        charTokenBuffer_[plcInBuffer + 1] = '\0';
        switch (curChar) {
        case '\n':
            newToken.token = TOKEN_NEWLINE;
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            plcInTokenBuffer++;
            break;
        case ' ':
            newToken.token = TOKEN_SPACE;
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            plcInTokenBuffer++;
            break;
        case '\t':
            newToken.token = TOKEN_TAB;
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
            plcInTokenBuffer++;
            break;
        case '-':
            // Check to see if this is an emdash case
            if (plcInTokenBuffer > 0) {
                prevToken = currTokenBuffer_[plcInTokenBuffer - 1];
                if (prevToken.token == TOKEN_HYPHEN) {
                    newToken.token = TOKEN_EMDASH;
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
            currTokenBuffer_[plcInTokenBuffer] = newToken;
            numCurrTokens_++;
            currTokenBuffer_[plcInTokenBuffer + 1] = eofToken;
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
        log_d("%u: %u", currToken.token, currToken.place);
        if (currToken.token == TOKEN_TEXT) {
            for (int j = currToken.place; j < currTokenBuffer_[i + 1].place; j++) {
                log_d("%c", charTokenBuffer_[j]);
            }
            log_d("\n");
        }
    }
}

// This method takes in a place in the token buffer and returns
// one line of text that can be rendered. It puts the string in the inputLine.
// It returns the next token in the TokenBuffer unless already at EOF, then
// it returns EOF again.
unsigned int Renderer::getLineOfTextForRendering(unsigned int placeInTokenBuffer,
                                                 LineBuffer &inputLine, bool readBackwards) {
    unsigned int bufferPlace = placeInTokenBuffer;
    unsigned int linePlace = 0;
    bool lineFull = false;
    int maxWidth = (display_.width() - 4);
    int pixLen;

    inputLine[linePlace] = '\0';
    while (!lineFull) {
        RendererToken curToken = currTokenBuffer_[bufferPlace];
        switch (curToken.token) {
        case TOKEN_TEXT:
        case TOKEN_SPACE:
        case TOKEN_TAB:
        case TOKEN_HYPHEN:
        case TOKEN_EMDASH:
            std::copy_n(&charTokenBuffer_[currTokenBuffer_[bufferPlace].place],
                        currTokenBuffer_[bufferPlace + 1].place -
                            currTokenBuffer_[bufferPlace].place,
                        &inputLine[linePlace]);
            linePlace +=
                currTokenBuffer_[bufferPlace + 1].place - currTokenBuffer_[bufferPlace].place;
            inputLine[linePlace] = '\0';
            pixLen = display_.getTextWidth(inputLine.data());
            // This token caused us to go too far, so we need to back up.
            if (pixLen > maxWidth) {
                // It's sufficient to simply null terminate the string where it should have
                // been!
                linePlace -=
                    currTokenBuffer_[bufferPlace + 1].place - currTokenBuffer_[bufferPlace].place;
                inputLine[linePlace] = '\n';
                inputLine[linePlace + 1] = '\0';
                lineFull = true;
                if (readBackwards) {
                    bufferPlace++;
                } else {
                    bufferPlace--;
                }
            }
            break;
        case TOKEN_NEWLINE:
            lineFull = true;
            inputLine[linePlace] = '\n';
            inputLine[linePlace + 1] = '\0';
            break;
        case TOKEN_EOF:
            lineFull = true;
            // It'll be incremented later and we want to return this token to caller.
            if (readBackwards) {
                bufferPlace++;
            } else {
                bufferPlace--;
            }
            break;
        default:
            log_i("Should not have gotten here -- unrecognized token.");
            break;
        }
        if (readBackwards) {
            bufferPlace--;
        } else {
            bufferPlace++;
        }
    }
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
    log_i("%lu is place in file being requested", placeInFile);
    currentPlace_ = placeInFile;
    PROFILE_START(loadCurrIntoBuffer);
    nextPlace_ = loadCurrIntoBuffer();
    PROFILE_END_LOG_MS(loadCurrIntoBuffer);

    currentPageWordCount_ = WordCount(currBuffer_.data());

    display_.renderTextWithoutBox(currBuffer_.data(), horizontalCenter_, verticalCenter_);

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
auto Renderer::goBackwardsOnePage(unsigned long place) -> unsigned long {

    char curChar = '\0';
    std::array<char, MAX_CHARS_PER_PAGE + 1> mainBackwardsBuffer;
    unsigned long seekPlace = 0;
    int numCharsToRead;

    if (place >= MAX_CHARS_PER_PAGE) {
        seekPlace = place - MAX_CHARS_PER_PAGE;
        numCharsToRead = MAX_CHARS_PER_PAGE;
    } else {
        seekPlace = 0;
        numCharsToRead = place;
    }

    log_d("Starting go Backwards: %d %d %d", place, seekPlace, numCharsToRead);
    // We'll just tokenize everything from the last and work our way from the
    // end of the tokens.
    loadAndTokenizePage(seekPlace);

    log_d("finished the tokenization");

    // Go backwards from the last token and do the same as the loadIntoBuffer
    // method.

    unsigned int plcInTokenBuffer = numCurrTokens_ - 1;
    unsigned int plcInBuffer = 0;
    int linesInBuffer = 1;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};

    // It's minus 4 because we want to start 2 off the top and not go below
    // 2 off the bottom due to the bounding box.
    // Note that get_text_height adds 1 over the max character height
    int maxLines = (display_.height() - 4) / this->display_.getTextHeight();
    PageBuffer buffer;

    buffer[0] = '\0';
    singleLine[0] = '\0';

    // printOutTokensInCurrTokenBuffer();
    unsigned int nextToken = plcInTokenBuffer;
    unsigned int currToken = plcInTokenBuffer;

    log_d("end of token buffer is %d", currToken);
    // Find which token to start with

    while ((currTokenBuffer_[currToken].streamPlace > place) && (currToken > 0)) {
        currToken--;
    }

    log_d("starting with %d as current token with a place of %d", currToken,
          currTokenBuffer_[currToken].streamPlace);

    if (currToken == 0) {
        // Can't go back any further, must be at beginnign of file.
        return 0;
    }

    while ((linesInBuffer <= maxLines) && (currToken > 0)) {
        nextToken = getLineOfTextForRendering(currToken, singleLine, true);
        std::copy_n(singleLine.data(), strlen(singleLine.data()), &buffer[plcInBuffer]);
        plcInBuffer += strlen(singleLine.data());
        buffer[plcInBuffer] = '\0';
        linesInBuffer++;
        currToken = nextToken;
        log_d("line is %s", singleLine.data());
    }

    unsigned long returnValue = currTokenBuffer_[currToken].streamPlace;

    log_d("about to tokenize current page");
    // methods calling this method expect this to not change what's loaded
    // into buffers, so we need to retokenize.
    // we could probably avoid this rework by introducing an extra token buffer if we want
    loadAndTokenizePage(place);
    return returnValue;
}

// returns the new place of the stream
auto Renderer::loadIntoBuffer(PageBuffer &buffer, unsigned long place) -> unsigned long {
    unsigned int plcInTokenBuffer = 0;
    unsigned int plcInBuffer = 0;
    int linesInBuffer = 1;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};

    // It's minus 4 because we want to start 2 off the top and not go below
    // 2 off the bottom due to the bounding box.
    // Note that get_text_height adds 1 over the max character height
    int maxLines = (display_.height() - 4) / this->display_.getTextHeight();

    buffer[0] = '\0';
    singleLine[0] = '\0';

    // Debugging stuff
    loadAndTokenizePage(place);
    // printOutTokensInCurrTokenBuffer();
    unsigned int nextToken = 0;
    unsigned int currToken = 0;

    while (linesInBuffer <= maxLines) {
        nextToken = getLineOfTextForRendering(currToken, singleLine);
        std::copy_n(singleLine.data(), strlen(singleLine.data()), &buffer[plcInBuffer]);
        plcInBuffer += strlen(singleLine.data());
        buffer[plcInBuffer] = '\0';
        linesInBuffer++;
        currToken = nextToken;
    }

    return currTokenBuffer_[currToken].streamPlace;
}

auto Renderer::loadPrevIntoBuffer() -> unsigned long {
    // return loadIntoBuffer(prevBuffer_, previousPlace_);
}

auto Renderer::loadCurrIntoBuffer() -> unsigned long {
    return loadIntoBuffer(currBuffer_, currentPlace_);
}

auto Renderer::loadNextIntoBuffer() -> unsigned long {
    // return loadIntoBuffer(nextBuffer_, nextPlace_);
}

auto Renderer::getPreviousPlace() -> unsigned long {
    // previousPlace_ = goBackwardsOnePage(currentPlace_);
    return previousPlace_;
}