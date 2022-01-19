#include "RawRenderer.hpp"

#include "sindarin-debug.h"

#define MAX_CHARS_PER_LINE 200

RawRenderer::RawRenderer(DisplaySystem *displayInput, std::string &string)
    : Renderer(displayInput) {
    this->setString(string);
}

void RawRenderer::setString(std::string &string) {
    this->dataStream_ = std::istringstream(string);
    this->currentPlace_ = 0;
    this->previousPlace_ = 0;

    // LOAD TEXT for prev, current.
    this->loadPrevIntoBuffer();
    this->loadCurrIntoBuffer();

    this->screedSpeed_ = 150;
    this->screedLastTime_ = 0;
    this->readMode_ = PAGE_READ;
}

void RawRenderer::setReadMode(int newMode) { readMode_ = newMode; }

auto RawRenderer::getReadMode() -> int { return readMode_; }

void RawRenderer::flipReadMode() {
    if (readMode_ == SCREED_READ)
        setReadMode(PAGE_READ);
    else if (readMode_ == PAGE_READ)
        setReadMode(SCREED_READ);
}

void RawRenderer::increaseSpeed() {
    screedSpeed_ += 10;

    if (screedSpeed_ > 1000) screedSpeed_ = 1000;
}

void RawRenderer::decreaseSpeed() {
    screedSpeed_ -= 10;

    if (screedSpeed_ < 50) screedSpeed_ = 50;
}

void RawRenderer::setSpeed(int newSpeed) {

    screedSpeed_ = newSpeed;

    if (screedSpeed_ < 50) screedSpeed_ = 50;

    if (screedSpeed_ > 1000) screedSpeed_ = 1000;
}

auto RawRenderer::renderContinueScreed() -> unsigned long {
    if (readMode_ != SCREED_READ) return currentPlace_;

    if ((millis() - screedLastTime_) > (60000 / screedSpeed_)) {
        char buffer[MAX_CHARS_PER_LINE];
        int plcInBuffer = 0;
        buffer[0] = '\0';
        char curChar;

        screedLastTime_ = millis();
        if (!dataStream_.seekg(currentPlace_)) {

            DEBUG_LN("FAILED TO SEEK");
            DEBUG_VAR(currentPlace_);
            DEBUG_LN(" is the place");
            return 0;
        }

        while (dataStream_.peek() != EOF) {
            dataStream_.read(&curChar, 1);

            // Turn new lines into spaces, for now.
            // But remember that it was a new line.
            switch (curChar) {
            case '\n':
                curChar = ' ';
                break;
            }
            buffer[plcInBuffer + 1] = '\0';
            buffer[plcInBuffer] = curChar;

            if (buffer[plcInBuffer] == ' ') {
                buffer[plcInBuffer] = '\0';
                renderMaxBoxWithWord(String(buffer));
                DEBUG_LN(buffer);
                currentPlace_++;
                break;
            } else {
                currentPlace_++;
            }
            plcInBuffer++;
        }
    }
    // DEBUG_LN("NEXT UP");
    return currentPlace_;
}

// Show the (already loaded into buffer) current page
// and load the next page into the next buffer so we have it.
auto RawRenderer::renderPageWithBox(unsigned long placeInFile) -> unsigned long {
    unsigned long nextPos = 0;

    if (placeInFile == currentPlace_) {
        DEBUG_LN("input place is same as current place");
        DEBUG_VAR(placeInFile);
        DEBUG_LN(" is place in file and current place");
        renderMaxBoxWithText(currBuffer_);
        // DEBUG_LN(curr_buffer);
        // DEBUG_VAR(next_place);
        // DEBUG_LN(" IS THE NEXT PLACE FROM REDNER_PAGE_WITH_BOX");
        // strcpy(prev_buffer, curr_buffer);

        // Have to handle initial case. Barf.
        if (firstGoAround_) {
            loadNextIntoBuffer();
            firstGoAround_ = 0;
        }
        nextPos = nextPlace_;
    } else if (placeInFile == previousPlace_) {
        DEBUG_LN("input place is same as previous place");
        // Only do something if we haven't pressed previous twice
        if (currentPlace_ != previousPlace_) {
            renderMaxBoxWithText(prevBuffer_);
            strcpy(nextBuffer_, currBuffer_);
            strcpy(currBuffer_, prevBuffer_);
            nextPos = currentPlace_;
            currentPlace_ = previousPlace_;
        }
    } else if (placeInFile == nextPlace_) {
        DEBUG_LN("input place is same as next place");
        previousPlace_ = currentPlace_;
        strcpy(prevBuffer_, currBuffer_);
        strcpy(currBuffer_, nextBuffer_);
        currentPlace_ = nextPlace_;
        renderMaxBoxWithText(nextBuffer_);
        nextPos = loadNextIntoBuffer();
    } else {
        DEBUG_VAR(nextPlace_);
        DEBUG_LN(" DIDN'T MATCH");
        DEBUG_VAR(placeInFile);
        DEBUG_LN(" was the place passed in");
        currentPlace_ = placeInFile;
        nextPos = loadCurrIntoBuffer();
        renderMaxBoxWithText(currBuffer_);
    }
    nextPlace_ = nextPos;
    return nextPos;
}

// returns the new place of the file_handle
// TODO(john): Unify this with the version in FileRenderer perhaps as a part of the holistic
// Renderer / Display / Optics refactor we've been discussing.
auto RawRenderer::loadIntoBuffer(char *buffer, unsigned long place) -> unsigned long {
    char curChar = '\0';
    unsigned int plcInBuffer = 0;
    // int found_newline = 0;
    int linesInBuffer = 1;
    int pixLen = 0;
    char singleLine[MAX_CHARS_PER_LINE];
    int charsInLine = 0;

    // It's minus 4 because we want to start 2 off the top and not go below
    // 2 off the bottom due to the bounding box.
    // Note that get_text_height adds 1 over the max character height
    int maxLines = (maxPixelsY_ - 4) / display_->getTextHeight();
    int maxWidth = (maxPixelsX_ - 4);

    int prevSpace = 0;
    unsigned long prevSeekPos = 0;

    buffer[plcInBuffer] = curChar;
    singleLine[charsInLine] = curChar;

    if (!dataStream_.seekg(place)) {

        DEBUG_LN("FAILED TO SEEK");
        DEBUG_VAR(place);
        DEBUG_LN(" is the place");
        return 0;
    }

    while (dataStream_.peek() != EOF) {
        dataStream_.read(&curChar, 1);

        // Turn new lines into spaces, for now.
        // But remember that it was a new line.
        switch (curChar) {
        case '\n':
            curChar = ' ';
            break;
            /*
            case '“':
            case '”':
                cur_char = '"';
                break;
            case '—':
                cur_char = '-';
                break;
            case '‘':
            case '’':
                cur_char = '\'';
                break;
                */
        }

        // Terminate and add the character to the buffer
        buffer[plcInBuffer + 1] = '\0';
        buffer[plcInBuffer] = curChar;
        // Same for the single_line. Note that the single line will
        // reset at each new line but the buffer will not
        singleLine[charsInLine + 1] = '\0';
        singleLine[charsInLine] = curChar;

        // Found a word break, do a check.
        if (buffer[plcInBuffer] == ' ') {
            // Check the length of the buffer
            pixLen = display_->getTextWidth(singleLine);
            // DEBUG_LN("Checked a width after a thing");
            // DEBUG_LN(buffer);
            if (pixLen > maxWidth) {
                // rewind to last word and cut it off there.
                //  start new line
                buffer[prevSpace] = '\n';
                // reset the line buffer by putting this last word (the word that didn't fit) in it.
                strcpy(singleLine, &(buffer[prevSpace + 1]));
                // DEBUG_LN("Saw new line and wrapped this word: ");
                // DEBUG_LN(single_line);

                charsInLine = strlen(singleLine);

                // Update the prev_space to the current place.
                prevSpace += charsInLine;

                linesInBuffer++;
                // we're done if this is true!
                if (linesInBuffer > maxLines) {
                    // reset the file_handle to the end of this buffer so
                    // the next reads do the right thing.
                    dataStream_.seekg(prevSeekPos);
                    nextPlace_ = prevSeekPos;
                    buffer[prevSpace - charsInLine] = '\0';
                    // DEBUG_LN(buffer);
                    // DEBUG_LN("SEE ABOVE FOR BUFFER");
                    break;
                }
            } else {
                // note where the previous space was in the buffer
                // as well as in the file handle and then keep going
                prevSpace = plcInBuffer;
                prevSeekPos = dataStream_.tellg();
                charsInLine++;
            }
        } else {
            // keep going
            charsInLine++;
        }
        plcInBuffer++;
    }

    return prevSeekPos;
}

auto RawRenderer::loadPrevIntoBuffer() -> unsigned long {
    DEBUG_LN("Loading PREV into buffer");
    return loadIntoBuffer(prevBuffer_, previousPlace_);
}

auto RawRenderer::loadCurrIntoBuffer() -> unsigned long {
    DEBUG_LN("Loading CURR into buffer");
    return loadIntoBuffer(currBuffer_, currentPlace_);
}

auto RawRenderer::loadNextIntoBuffer() -> unsigned long {
    DEBUG_LN("Loading NEXT into buffer");
    return loadIntoBuffer(nextBuffer_, nextPlace_);
}