#pragma once

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"

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
    static constexpr uint32_t LOCATION_ERROR = 0xFFFFFFFF;

    // This is the number of SUB-lines we want to split a line into to be able to render bare
    // newlines as less than a full line height of the text. (Deci was chosen as 1/10) So what the
    // two lines below mean is that a new line by itself on a line will render as 4/10s of a full
    // line height. Theoretically we can do any percentage we want here, but there is a lot of
    // testing to do for that!

    static constexpr uint16_t NEWLINE_DECI_LINE_HEIGHT = 10;
    static constexpr uint16_t ENDPARAGRAPH_DECI_LINE_HEIGHT = 4;

    // A Page array must be large enough to accommodate the need to load a complete display page,
    // but also able to load a complete paragraph that can precede the page when the back page
    // command is issued by the user. By design a paragraph cannot have more than 1500 bytes,
    // followed by a NEWLINE, an ENDOFPARAGRAPH, and a null.

    static constexpr uint16_t MAX_BYTES_PER_TEXT_TOKEN = 60;
    static constexpr uint16_t MAX_BYTES_PER_PARAGRAPH = 1503;
    static constexpr uint16_t MAX_BYTES_PER_DISPLAY_PAGE = 2400;
    static constexpr uint16_t MAX_BYTES_TO_RETRIEVE =
        (MAX_BYTES_PER_PARAGRAPH + MAX_BYTES_PER_DISPLAY_PAGE - MAX_BYTES_PER_TEXT_TOKEN);
    static constexpr uint16_t PAGE_BUFFER_SIZE =
        (MAX_BYTES_PER_PARAGRAPH + MAX_BYTES_PER_DISPLAY_PAGE);
    static constexpr uint16_t MAX_TOKEN_COUNT = (PAGE_BUFFER_SIZE / 2);
    static constexpr uint16_t MAX_BYTES_PER_LINE = 200;
    static constexpr uint16_t MAX_LINES_PER_PAGE = 31;
    static constexpr uint16_t MAX_PARAGRAPH_LINES = MAX_LINES_PER_PAGE + 2;

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
        // while rendering a back page command.
        Idx pageIdx;
        Idx prevNewParagraphTokenIdx;
        uint16_t width;
        uint16_t height;
        uint16_t length; // Length in bytes
        TokenId tokenId;
    };

    typedef std::array<char, PAGE_BUFFER_SIZE + 1> Page;
    typedef std::array<char, MAX_BYTES_PER_LINE> Line;
    typedef std::array<Line, MAX_LINES_PER_PAGE> PageLines;
    typedef std::array<char, MAX_BYTES_PER_TEXT_TOKEN> TokenString;
    typedef std::array<Token, MAX_TOKEN_COUNT> Tokens;

    // Indexes into the last lines of the last retrieved paragraphs
    // used to find the first line in the back page command context
    typedef std::array<Idx, MAX_PARAGRAPH_LINES> ParaLineIdxs;

    // Only used in to compute a space size in a GFX font context
    typedef std::array<char, MAX_BYTES_PER_TEXT_TOKEN * 2 + 2> TokenStringDouble;

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

    // Computed TEXT Token character size that will fit on a line depending
    // on the current font size and line width
    uint16_t maxTextTokenCharSize_ = 0;

    // Character buffer for the page being built by the tokenizer
    Page page_{};

    // This keeps all the tokens for the max amount of read from the stream
    Tokens tokens_{};
    uint16_t tokensCount_ = 0;

    PageLines pageLines_{};
    uint16_t pageLinesLengths_[MAX_LINES_PER_PAGE] = {0};
    uint16_t pageLinesCount_ = 0;

    // Switch to 1/10 of a line as this gives us more flexibilty in rendering
    // precision between lines.
    auto getMaxDeciLines() -> int16_t {
        return (display_.height() * NEWLINE_DECI_LINE_HEIGHT) / (this->display_.getTextHeight());
    }

    // The following are used to support the back page command algorithm:
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

    void computeMaxTextTokenCharSize();

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
