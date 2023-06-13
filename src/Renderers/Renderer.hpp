#ifndef RENDERER_H
#define RENDERER_H

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"

// This needs to be a bit bigger -- it's really the max number of chars that will get us 2 full
// pages where the previous page has a newline.

#define MAX_CHARS_PER_PAGE 2000
#define MAX_CHARS_PER_LINE 200
#define MAX_TOKEN_STRING_LENGTH 100
#define MAX_LINES_PER_PAGE 50

// NOTE:
//
// In the following class definition
//
// - "Place" as a variable suffix defines a character index in the file stream
// - "Idx" as a variable suffix defines an index in one of the arrays maintained by this class

class Renderer {

private:
    const uint32_t LocationError = 0xFFFFFFFF;

    // These are the different tokens so as we parse a buffer
    // we more easily know what to do with the text.
    enum class TOKEN_ID : uint8_t {
        TEXT = 0,
        NEWLINE = 1,
        SPACE = 2,
        HYPHEN = 3,
        EMDASH = 4,
        TAB = 5,
        ENDOFFILE = 6,
        NOTOKEN = 7,
    };

    // A token is simply a place in the buffer and a token identifer
    // that indicates what type of token it is.
    //
    // To determine the length of a token, we can simply look at the
    // next token in the buffer and subtract the current token's
    // place.

    struct Token {
        TOKEN_ID tokenId;
        uint32_t pageIdx;
        uint32_t streamPlace;
        // This basically gives each token knowledge of where the paragraph
        // start is for where it's located. It's useful for looking backwards
        // while rendering backwards.
        uint32_t prevNewlineTokenIdx;
        uint16_t width;
        uint16_t height;
    };

    typedef std::array<char, MAX_CHARS_PER_PAGE> Page;
    typedef std::array<char, MAX_CHARS_PER_LINE> Line;
    typedef std::array<Line, MAX_LINES_PER_PAGE> PageLines;
    typedef std::array<char, MAX_TOKEN_STRING_LENGTH> TokenString;
    typedef std::array<Token, MAX_CHARS_PER_PAGE> Tokens;

public:
    Renderer(DisplaySystem &display);

    void setStream(std::shared_ptr<RendererStream> stream) { stream_ = std::move(stream); }

    auto getPreviousPlace() -> uint32_t;
    auto getCurrentPlace() const -> uint32_t { return currentPlace_; }
    auto setCurrentPlace(uint32_t newPlace) { currentPlace_ = newPlace; }

    auto size() const -> uint32_t { return stream_->size(); }

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    //
    // numSkipLines allows us to not render the first X lines of the page
    // for the speed reading functionality.
    auto renderPage(uint32_t fromPlace, int16_t numSkipLines = 0) -> uint32_t;

    auto getCurrentPageWordCount() const -> uint32_t { return currentPageWordCount_; }

    auto getLengthOfLine(uint16_t lineIdx) const -> uint16_t { return pageLinesLengths_[lineIdx]; }

    auto getTotalLengthOfLines() -> uint16_t;

    auto getNumRenderedLines() const -> uint16_t { return pageLinesCount_; }

private:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    uint32_t currentPlace_ = 0;
    uint32_t previousPlace_ = 0;
    uint32_t nextPlace_ = 0;

    uint32_t currentPageWordCount_ = 0;

    uint16_t spaceWidth_ = 0;
    uint16_t spaceHeight_ = 0;

    Page currentPage_{};

    // Character buffer for the page being built
    Page page_{};

    // This keeps all the tokens for the max amount of read from the stream
    Tokens tokens_{};
    uint16_t pageTokensCount_ = 0;

    PageLines pageLines_{};
    uint16_t pageLinesLengths_[MAX_LINES_PER_PAGE] = {0};
    uint16_t pageLinesCount_ = 0;

    // This should be called by subclasses after content is loaded
    void loaded();

    auto loadCurrentPage() -> uint32_t;
    auto goBackwardsOnePage(uint32_t fromPlace) -> uint32_t;

    auto getFileLocation(uint32_t fromPlace) -> uint32_t;

    void loadAndTokenizePage(uint32_t fromPlace, bool backwards = false);

    void tokenToString(uint16_t tokenIdx, TokenString &outputString);
    void tokenRenderedWidthHeight(uint16_t tokenIdx, uint16_t *width, uint16_t *height);
    auto setSizeOfTextToken(uint16_t tokenIdx) -> uint16_t;

    // Debugging
    void printOutTokensInCurrTokenBuffer();

    auto getLineOfTextForRendering(uint16_t startTokenIdx, Line &line) -> uint16_t;
};

#endif
