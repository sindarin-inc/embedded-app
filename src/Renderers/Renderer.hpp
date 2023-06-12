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
        NOTOKEN = 7, // Not Used!!
    };

    // A token is simply a place in the buffer and a token identifer
    // that indicates what type of token it is.
    //
    // To determine the length of a token, we can simply look at the
    // next token in the buffer and subtract the current token's
    // place.

    struct Token {
        TOKEN_ID tokenId;
        unsigned long pageIdx;
        unsigned long streamPlace;
        // This basically gives each token knowledge of where the paragraph
        // start is for where it's located. It's useful for looking backwards
        // while rendering backwards.
        unsigned long prevNewlineTokenIdx;
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

    auto getPreviousPlace() -> unsigned long;
    auto getCurrentPlace() const -> unsigned long { return currentPlace_; }
    auto setCurrentPlace(unsigned long newPlace) { currentPlace_ = newPlace; }

    auto size() const -> unsigned long { return stream_->size(); }

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    //
    // numSkipLines allows us to not render the first X lines of the page
    // for the speed reading functionality.
    auto renderPage(unsigned long fromPlace, int numSkipLines = 0) -> unsigned long;

    auto getCurrentPageWordCount() const -> uint32_t { return currentPageWordCount_; }

    auto getLengthOfLine(unsigned int lineIdx) const -> unsigned int {
        return pageLinesLengths_[lineIdx];
    }

    auto getTotalLengthOfLines() -> unsigned int;

    auto getNumRenderedLines() const -> unsigned int { return pageLinesCount_; }

private:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    unsigned long currentPlace_ = 0;
    unsigned long previousPlace_ = 0;
    unsigned long nextPlace_ = 0;

    uint32_t currentPageWordCount_ = 0;

    uint16_t spaceWidth_ = 0;
    uint16_t spaceHeight_ = 0;

    Page currentPage_{};

    // Character buffer for the page being built
    Page page_{};

    // This keeps all the tokens for the max amount of read from the stream
    Tokens tokens_{};
    unsigned int pageTokensCount_ = 0;

    PageLines pageLines_{};
    unsigned int pageLinesLengths_[MAX_LINES_PER_PAGE] = {0};
    unsigned int pageLinesCount_ = 0;

    // This should be called by subclasses after content is loaded
    void loaded();

    auto loadCurrentPage() -> unsigned long;
    auto goBackwardsOnePage(unsigned long fromPlace) -> unsigned long;

    void loadAndTokenizePage(unsigned long fromPlace, bool backwards = false);

    void tokenToString(unsigned long tokenIdx, TokenString &outputString);
    void tokenRenderedWidthHeight(unsigned long tokenIdx, uint16_t *width, uint16_t *height);
    auto setSizeOfTextToken(unsigned long tokenIdx) -> unsigned int;

    // Debugging
    void printOutTokensInCurrTokenBuffer();

    auto getLineOfTextForRendering(unsigned int startTokenIdx, Line &line) -> unsigned int;
};

#endif
