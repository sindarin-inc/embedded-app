#ifndef RENDERER_H
#define RENDERER_H

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"

// This needs to be a bit bigger -- it's really the max number of chars that will get us 2 full
// pages where the previous page has a newline.

#define MAX_CHARS_PER_PAGE 4000
#define MAX_TOKEN_COUNT 1800
#define MAX_CHARS_PER_LINE 200
#define MAX_TOKEN_STRING_LENGTH 100
#define MAX_LINES_PER_PAGE 50
#define MAX_PARAGRAPH_LINES 25

// NOTE:
//
// In the following class definition
//
// - "Place" as a variable suffix, defines a character index in the file stream
// - "Idx" as a variable suffix, defines an index in one of the arrays maintained by this class

class Renderer {

public:
    typedef uint32_t Place;
    typedef uint16_t Idx;

private:
    // Used by the getFileLocation() method in case of error
    const uint32_t locationError_ = 0xFFFFFFFF;

    // These are the different tokens so as we parse a buffer
    // we more easily know what to do with the text.
    enum class TokenId : uint8_t {
        TEXT = 0,
        NEWLINE,
        ENDPARAGRAPH,
        SPACE,
        HYPHEN,
        EMDASH,
        TAB,
        ENDOFFILE,
        NOTOKEN,
    };

    // A token is simply a place in the buffer and a token identifer
    // that indicates what type of token it is.
    //
    // To determine the length of a token, we can simply look at the
    // next token in the buffer and subtract the current token's
    // place.
    //
    // Fields ordering is to optimize the space required.
    struct Token {
        Place streamPlace;
        // This basically gives each token knowledge of where the paragraph
        // start is for where it's located. It's useful for looking backwards
        // while rendering backwards.
        Idx pageIdx;
        Idx prevNewParagraphTokenIdx;
        uint16_t width;
        uint16_t height;
        uint16_t length; // Length in bytes
        TokenId tokenId;
    };

    typedef std::array<char, MAX_CHARS_PER_PAGE> Page;
    typedef std::array<char, MAX_CHARS_PER_LINE> Line;
    typedef std::array<Line, MAX_LINES_PER_PAGE> PageLines;
    typedef std::array<char, MAX_TOKEN_STRING_LENGTH> TokenString;
    typedef std::array<Token, MAX_TOKEN_COUNT> Tokens;
    typedef std::array<Idx, MAX_PARAGRAPH_LINES> ParaLineIdxs;

public:
    Renderer(DisplaySystem &display) : display_(display) {}

    auto setStream(std::shared_ptr<RendererStream> stream) -> void { stream_ = std::move(stream); }
    auto setCurrentPlace(Place newPlace) -> void { currentPlace_ = newPlace; }

    [[nodiscard]] auto getPreviousPlace() -> Place;

    [[nodiscard]] auto getCurrentPlace() const -> Place { return currentPlace_; }

    [[nodiscard]] auto size() const -> uint32_t { return stream_->size(); }

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    //
    // numSkipLines allows us to not render the first X lines of the page
    // for the speed reading functionality.

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    auto renderPage(Place fromPlace, int16_t numSkipLines = 0) -> Place;

    [[nodiscard]] auto getTotalLengthOfLines() const -> uint16_t;

    [[nodiscard]] auto getCurrentPageWordCount() const -> uint32_t { return currentPageWordCount_; }

    [[nodiscard]] auto getLengthOfLine(Idx lineIdx) const -> uint16_t {
        return pageLinesLengths_[lineIdx];
    }

    [[nodiscard]] auto getNumRenderedLines() const -> uint16_t { return pageLinesCount_; }

private:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    Place currentPlace_ = 0;
    Place previousPlace_ = 0;
    Place nextPlace_ = 0;

    uint32_t currentPageWordCount_ = 0;

    uint16_t spaceWidth_ = 0;
    uint16_t spaceHeight_ = 0;

    // Character buffer for the page being built by the tokenizer
    Page page_{};

    // This keeps all the tokens for the max amount of read from the stream
    Tokens tokens_{};
    uint16_t tokensCount_ = 0;

    PageLines pageLines_{};
    uint16_t pageLinesLengths_[MAX_LINES_PER_PAGE] = {0};
    uint16_t pageLinesCount_ = 0;

    // The following are used to support the backpage algorithm:
    // they will keep tokenIdx pointers at the last lines that are part of
    // the last paragraph read to complete the page.
    ParaLineIdxs paraLineIdxs_;
    Idx paraLineFirstIdx_;
    Idx paraLineLastIdx_;

    // This should be called by subclasses after content is loaded
    void loaded() { nextPlace_ = loadCurrentPage(); };

    auto loadCurrentPage() -> Place;
    auto getParagraphDeciLinesCount(Idx startTokenIdx, Idx endTokenIdx) -> uint16_t;
    [[nodiscard]] auto getFirstLineOfPageTokenIdx(Idx lastTokenIdx, uint16_t deciLinesCount,
                                                  uint16_t maxDeciLines) const -> Idx;
    // auto getTokenIdxPassedDeciLines(uint16_t deciLines, Idx startTokenIdx, Idx endTokenIdx) ->
    // Idx;
    auto goBackwardsOnePage(Place fromPlace) -> Place;

    auto getFileLocation(Place fromPlace) -> Place;

    auto loadAndTokenizePage(Place fromPlace, bool backwards = false) -> Place;

    void tokenToString(Idx tokenIdx, TokenString &outputString) const;
    void setTokenWidthHeight(Idx tokenIdx);
    auto setTokenSizes(Idx tokenIdx) -> uint16_t;

    // Debugging
    void printOutTokens() const;

    auto getLineOfTextForRendering(Idx startTokenIdx, Line &line) -> Idx;
};

#endif
