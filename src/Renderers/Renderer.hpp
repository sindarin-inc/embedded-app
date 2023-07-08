#pragma once

#include "RendererDefs.hpp"
#include "Tokenizer.hpp"
#include "NullTokenizer.hpp"
#include "Tokens.hpp"

using namespace renderer_defs;

// ToDo: The renderer interface has been adjusted to consider a new upcoming Place struct.
//       It is now using a Place as an int32_t, independant of an renderer independant
//       StreamPlace, to support it's development in the current Sol app that support only
//       TXT files.
//       Will need to be adjusted considering the upcoming Place definition...

class Renderer {

public:
    Renderer(DisplaySystem &display) : display_(display), tokens_(display) {
        tokenizer_ = std::make_shared<NullTokenizer>(display);
    }

    auto setStream(std::shared_ptr<RendererStream> stream) -> void {
        tokenizer_ = Tokenizer::factory(stream, display_);
    }
    auto setCurrentPlace(Place newPlace) -> void {
        currentPlace_ = tokenizer_->toStreamPlace(newPlace);
    }

    [[nodiscard]] auto getPreviousPlace() -> Place;

    [[nodiscard]] inline auto getCurrentPlace() const -> Place {
        return tokenizer_->toPlace(currentPlace_);
    }

    [[nodiscard]] inline auto size() const -> uint32_t { return stream_->size(); }

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    //
    // numSkipLines allows us to not render the first X lines of the page
    // for the speed reading functionality.

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    auto renderPage(Place fromPlace, int16_t numSkipLines = 0) -> Place;

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    inline auto renderFirstPage(int16_t numSkipLines = 0) -> Place {
        return renderPage(tokenizer_->toPlace(tokenizer_->getPlaceAtBeginningOfDocument()),
                          numSkipLines);
    }

    [[nodiscard]] auto getTotalLengthOfLines() const -> uint16_t;

    [[nodiscard]] inline auto getLengthOfLine(Idx lineIdx) const -> uint16_t {
        return pageLinesLengths_[lineIdx];
    }

    [[nodiscard]] inline auto getNumRenderedLines() const -> uint16_t { return pageLinesCount_; }

    [[nodiscard]] inline auto getCurrentPageWordCount() const -> uint32_t {
        return (tokenizer_ == nullptr) ? 0 : tokenizer_->getCurrentPageWordCount();
    }

private:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    Tokens tokens_;
    std::shared_ptr<Tokenizer> tokenizer_{nullptr};

    StreamPlace currentPlace_{};
    StreamPlace previousPlace_{};
    StreamPlace nextPlace_{};

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
    renderer_defs::Idx paraLineFirstIdx_;
    renderer_defs::Idx paraLineLastIdx_;

    // This should be called by subclasses after content is loaded
    void loaded() { nextPlace_ = loadCurrentPage(); };

    auto loadCurrentPage() -> StreamPlace;
    auto getParagraphDeciLinesCount(renderer_defs::Idx startTokenIdx,
                                    renderer_defs::Idx endTokenIdx) -> uint16_t;
    [[nodiscard]] auto getFirstLineOfPageTokenIdx(renderer_defs::Idx lastTokenIdx,
                                                  uint16_t deciLinesCount,
                                                  uint16_t maxDeciLines) const
        -> renderer_defs::Idx;

    auto goBackwardsOnePage(StreamPlace fromPlace) -> StreamPlace;

    auto getLineOfTextForRendering(renderer_defs::Idx startTokenIdx, Line &line)
        -> renderer_defs::Idx;
};
