#ifndef RENDERER_H
#define RENDERER_H

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"

// This needs to be a bit bigger -- it's really the max number of chars that will get us 2 full
// pages where the previous page has a newline.
#define MAX_CHARS_PER_PAGE 3000
#define MAX_CHARS_PER_LINE 200
#define MAX_TOKEN_STRING_LENGTH 100

#define MAX_LINES_PER_PAGE 50

// These are the different tokens so as we parse a buffer
// we more easily know what to do with the text.
enum RENDERER_TOKEN {
    TOKEN_TEXT = 0,
    TOKEN_NEWLINE = 1,
    TOKEN_SPACE = 2,
    TOKEN_HYPHEN = 3,
    TOKEN_EMDASH = 4,
    TOKEN_TAB = 5,
    TOKEN_EOF = 6,
    TOKEN_NOTOKEN = 7,
};

// A token is simply a place in the buffer and a token identifer
// that indicates what type of token it is.
//
// To determine the length of a token, we can simply look at the
// next token in the buffer and subtract the current token's
// place.

struct RendererToken {
    RENDERER_TOKEN token;
    unsigned long place;
    unsigned long streamPlace;
    // This basically gives each token knowledge of where the paragraph
    // start is for where it's located. It's useful for looking backwards
    // while rendering backwards.
    unsigned long prevNewline;
    uint16_t width;
    uint16_t height;
};

typedef std::array<char, MAX_CHARS_PER_PAGE> PageBuffer;
typedef std::array<char, MAX_CHARS_PER_LINE> LineBuffer;
typedef std::array<char, MAX_TOKEN_STRING_LENGTH> TokenString;
typedef std::array<RendererToken, MAX_CHARS_PER_PAGE> PageTokenBuffer;
typedef std::array<RendererToken, MAX_CHARS_PER_LINE> LineTokenBuffer;

typedef std::array<LineBuffer, MAX_LINES_PER_PAGE> LineBufferArray;

class Renderer {
public:
    Renderer(DisplaySystem &display);

    void setStream(std::shared_ptr<RendererStream> stream) { stream_ = std::move(stream); }
    void setVerticalCenter(bool verticalCenter) { verticalCenter_ = verticalCenter; }
    void setHorizontalCenter(bool horizontalCenter) { horizontalCenter_ = horizontalCenter; }

    auto getPreviousPlace() -> unsigned long;
    auto getCurrentPlace() -> unsigned long { return currentPlace_; }
    auto setCurrentPlace(unsigned long newPlace) { currentPlace_ = newPlace; }

    auto size() -> unsigned long { return stream_->size(); }
    auto getDisplay() -> DisplaySystem * { return &display_; }

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    //
    // numSkipLines allows us to not render the first X lines of the page
    // for the speed reading functionality.
    auto renderPage(unsigned long placeInFile, int numSkipLines = 0) -> unsigned long;

    auto getCurrentPageWordCount() -> uint32_t { return currentPageWordCount_; }

    void tokenToString(unsigned long placeInTokenBuffer, TokenString &outputString);
    void tokenRenderedWidthHeight(unsigned long placeInTokenBuffer, uint16_t *renderedWidth,
                                  uint16_t *renderedHeight);
    void setSizeOfTextToken(unsigned long placeInTokenBuffer);

    auto getLengthOfLine(unsigned int index) -> unsigned int;

    auto getTotalLengthOfLines() -> unsigned int;

    auto getNumRenderedLines() -> unsigned int { return numLinesInBuffer_; }

protected:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    // Allows you to center the text horizontally and vertically if you'd like
    bool verticalCenter_ = false;
    bool horizontalCenter_ = false;

    // This should be called by subclasses after content is loaded
    void loaded();

    unsigned long currentPlace_ = 0;
    unsigned long previousPlace_ = 0;
    unsigned long nextPlace_ = 0;

    uint32_t currentPageWordCount_ = 0;

    uint16_t spaceWidth_ = 0;
    uint16_t spaceHeight_ = 0;

    auto loadCurrIntoBuffer() -> unsigned long;
    auto goBackwardsOnePage(unsigned long place) -> unsigned long;

    void loadAndTokenizePage(unsigned long placeInFile);

    // Debugging
    void printOutTokensInCurrTokenBuffer();

    PageBuffer prevBuffer_{};
    PageBuffer currBuffer_{};
    PageBuffer nextBuffer_{};

    // Character buffers for the current page
    PageBuffer charTokenBuffer_{};

    // This keeps all the tokens for the max amount of read from the stream
    PageTokenBuffer currTokenBuffer_{};
    unsigned int numCurrTokens_ = 0;

    LineBufferArray lineBufferArray_{};
    unsigned int lineBufferLengthsArray_[MAX_LINES_PER_PAGE] = {0};
    unsigned int numLinesInBuffer_ = 0;

    // This keeps just the tokens for the current page being rendered
    PageTokenBuffer singlePageTokenBuffer_{};

    auto getLineOfTextForRendering(unsigned int placeInTokenBuffer, LineBuffer &inputLine)
        -> unsigned int;

    // This does the work of lineify from the old system. Loads one page of
    // text into that buffer given the current offset in the file
    auto loadIntoBuffer(PageBuffer &buffer, unsigned long place) -> unsigned long;
};

#endif
