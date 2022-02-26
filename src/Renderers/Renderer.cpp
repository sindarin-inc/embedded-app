#include "Renderer.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

Renderer::Renderer(DisplaySystem &display) : display_(display) {}

void Renderer::loaded() {
    previousPlace_ = goBackwardsOnePage(currentPlace_);

    // LOAD TEXT for prev, current.
    // The contract for users of this class is that this class will always keep the current page,
    // the previous page, and the next page in a buffer so that on page swaps, the render can
    // happen immediately without any filesystem access. Then, *after* a page is rendered, the new
    // previous and next pages can be loaded. This allows for buffering to happen while a user
    // is reading the current page.
    //
    // It's a little broken in that load_curr_into_buffer does the wrong thing and updates class
    // variables (essentially moving the current seek position in the file), hence this dance here
    // to make sure they are correct.
    // TODO(john): It doesn't look like it works this way right now. Our preloading does nothing.
    loadPrevIntoBuffer();
    nextPlace_ = loadCurrIntoBuffer();
    int oldNextPlace = nextPlace_;
    loadNextIntoBuffer();
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

    display_.renderTextWithoutBox(currBuffer_.data());
    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);

    nextPos = nextPlace_;
    PROFILE_START(loadNextIntoBuffer);
    loadNextIntoBuffer();
    PROFILE_END_LOG_MS(loadNextIntoBuffer);

    PROFILE_START(loadPrevIntoBuffer);
    previousPlace_ = goBackwardsOnePage(currentPlace_);
    loadPrevIntoBuffer();
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

    // Place here will point to the first character of the first word
    // on the current page. So we need to go back two from there to
    // start with the correct word -- the last word on the previous
    // page
    // NOTE: This assumes that the previous page will have at least
    // one word. But the above return 0 and check on place == 0
    // should assure that.
    if (place < MAX_CHARS_PER_PAGE) {
        numCharsToRead = place - PLACE_REWIND;
        seekPlace = 0;
    } else {
        numCharsToRead = MAX_CHARS_PER_PAGE;
        seekPlace = place - numCharsToRead - PLACE_REWIND;
        if (place - numCharsToRead - PLACE_REWIND < 0) {
            seekPlace = 0;
        }
    }

    mainBackwardsBuffer[numCharsToRead] = '\0';

    int totalCharsRead = 0;

    // If we are at the beginning of the file, we stay there.
    if (place == 0) {
        return 0;
    }

    if (!stream_->seek(seekPlace)) {
        log_e("failed to seek in go_backwards: %lu is the place", place);
        // This is a fragile since 0 is a valid file handle position. TODO to fix.
        return 0;
    }

    while (stream_->available()) {
        if (totalCharsRead < numCharsToRead) {
            curChar = stream_->read();

            // We'll fill in the array backwards just so it's easier to read it forwards
            // to do the backwards lineify

            mainBackwardsBuffer[numCharsToRead - totalCharsRead - 1] = curChar;
            totalCharsRead++;
        } else {
            break;
        }
    }

    // At this point, the main_backwards_buffer will have more than a page worth of
    // characters in backwards. So if we read forward and lineify, we should be able to
    // find the place where the previous place should start.

    // This code is mostly copied from load_into_buffer, so you know, that's
    // great.

    curChar = '\0';
    unsigned int plcInBuffer = 0;
    PageBuffer buffer{};

    int linesInBuffer = 1;
    int pixLen = 0;
    LineBuffer singleLine{};

    int charsInLine = 0;

    // It's minus 4 because we want to start 2 off the top and not go below
    // 2 off the bottom due to the bounding box.
    // Note that get_text_height adds 1 over the max character height
    int maxLines = (display_.height() - 4) / display_.getTextHeight();
    int maxWidth = (display_.width() - 4);

    int prevSpace = 0;
    unsigned long prevSeekPos = 0;

    buffer[plcInBuffer] = curChar;
    singleLine[charsInLine] = curChar;

    totalCharsRead = 0;
    while (totalCharsRead < numCharsToRead) {
        // TODO: Clang-tidy says this can sometimes be garbage and it's probably right.
        // But given that we're going to rewrite this before too long, I don't think it's
        // worth spending any cycles on. To see what clang-tidy says just remove the // NOLINT
        // comment/
        // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign)
        curChar = mainBackwardsBuffer[totalCharsRead++];

        // Turn new lines into spaces, for now.
        switch (curChar) {
        case '\n':
            curChar = ' ';
            break;
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
            pixLen = display_.getTextWidth(singleLine.data());
            if (pixLen > maxWidth) {
                // rewind to last word and cut it off there.
                //  start new line
                buffer[prevSpace] = '\n';
                // reset the line buffer by putting this last word (the word that didn't fit) in it.
                std::copy_n(buffer.begin() + prevSpace + 1, sizeof(singleLine), singleLine.begin());

                // Update the prev_space to the current place.
                charsInLine = strlen(singleLine.data());
                prevSpace += charsInLine;

                linesInBuffer++;
                // we're done if this is true!
                if (linesInBuffer > maxLines) {
                    // reset the file_handle to the end of this buffer so
                    // the next reads do the right thing.
                    totalCharsRead = prevSeekPos;
                    nextPlace_ = prevSeekPos;
                    buffer[prevSpace - charsInLine] = '\0';
                    break;
                }
            } else {
                // note where the previous space was in the buffer
                // as well as in the file handle and then keep going
                prevSpace = plcInBuffer;
                prevSeekPos = totalCharsRead;
                charsInLine++;
            }
        } else {
            // keep going
            charsInLine++;
        }
        plcInBuffer++;
    }

    // At this point, total_chars_read is the number of characters that we need for the previous
    // page. So we should just be able to subtract that from the input place. If it's < 0, return 0.

    unsigned long previousPlaceInFile;

    // similarly to the logic above for starting PLACE_REWIND behind, we need move back those spaces
    // again when calculating what to return based on the number of characters read in lineify and
    // the place passed in.
    if ((place - totalCharsRead - PLACE_REWIND) <= 0) {
        previousPlaceInFile = 0;
    } else {
        previousPlaceInFile = place - totalCharsRead - PLACE_REWIND;
    }

    return previousPlaceInFile;
}

// returns the new place of the stream
auto Renderer::loadIntoBuffer(PageBuffer &buffer, unsigned long place) -> unsigned long {
    unsigned int plcInBuffer = 0;
    int linesInBuffer = 1;
    int pixLen = 0;
    std::array<char, MAX_CHARS_PER_LINE> singleLine{};
    int charsInLine = 0;

    // It's minus 4 because we want to start 2 off the top and not go below
    // 2 off the bottom due to the bounding box.
    // Note that get_text_height adds 1 over the max character height
    int maxLines = (display_.height() - 4) / this->display_.getTextHeight();
    int maxWidth = (display_.width() - 4);

    // Index of last word boundary
    int prevSpace = 0;
    unsigned long prevSeekPos = 0;

    buffer[plcInBuffer] = '\0';
    singleLine[charsInLine] = '\0';

    if (!stream_->seek(place)) {
        log_e("failed to seek %lu is the place", place);
        // This is a fragile since 0 is a valid file handle position. TODO to fix.
        return 0;
    }

    while (stream_->available()) {
        auto curChar = stream_->read();

        // eliminate spaces at the beginning of the read
        // (This can happen due to heuristic to be able to go backwards arbitrarily)
        while ((plcInBuffer == 0) && (curChar == ' ') && stream_->available()) {
            curChar = stream_->read();
        }

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
            // Check the length of the line
            pixLen = this->display_.getTextWidth(singleLine.data());
            if (pixLen > maxWidth) {
                // rewind to last word and cut it off there.
                //  start new line
                buffer[prevSpace] = '\n';

                // Reset the line buffer by putting this last word (the word that didn't fit) in it.
                std::copy_n(buffer.begin() + prevSpace + 1, sizeof(singleLine), singleLine.begin());

                // Update the prev_space to the current place.
                charsInLine = strlen(singleLine.data());
                prevSpace += charsInLine;

                linesInBuffer++;
                // we're done if this is true!
                // Alternately could check if the last `getTextBounds` call exceeded the y
                if (linesInBuffer > maxLines) {
                    // reset the file_handle to the end of this buffer so
                    // the next reads do the right thing.
                    stream_->seek(prevSeekPos);
                    this->nextPlace_ = prevSeekPos;
                    buffer[prevSpace - charsInLine] = '\0';
                    break;
                }
            } else {
                // note where the previous space was in the buffer
                // as well as in the file handle and then keep going
                prevSpace = plcInBuffer;
                prevSeekPos = stream_->position();
                charsInLine++;
            }
        } else {
            // Keep looking for the next space
            charsInLine++;
        }
        plcInBuffer++;
    }

    // If we got through the above check and this one fails, this means we are at
    // the end of file, so we should wrap around and start over.
    if (!stream_->available()) {
        log_i("Found the end of file and so starting over at the beginning.");
        return 0;
    } else {
        return prevSeekPos;
    }
}

auto Renderer::loadPrevIntoBuffer() -> unsigned long {
    return loadIntoBuffer(prevBuffer_, previousPlace_);
}

auto Renderer::loadCurrIntoBuffer() -> unsigned long {
    return loadIntoBuffer(currBuffer_, currentPlace_);
}

auto Renderer::loadNextIntoBuffer() -> unsigned long {
    return loadIntoBuffer(nextBuffer_, nextPlace_);
}
