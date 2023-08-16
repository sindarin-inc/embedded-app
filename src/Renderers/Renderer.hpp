#pragma once

#include <utility>

#include "NullTokenizer.hpp"
#include "Pager.hpp"
#include "RendererDefs.hpp"
#include "Tokenizer.hpp"
#include "Tokens.hpp"

using namespace renderer_defs;

// ToDo: The renderer interface has been adjusted to consider a new upcoming Place struct.
//       It is now using a Place as an uint32_t, independant of an renderer independant
//       StreamPlace, to support it's development in the current Sol app that support only
//       TXT files.
//       Will need to be adjusted considering the upcoming Place definition...

class Renderer {

public:
    Renderer(DisplaySystem &display)
        : display_(display), tokens_(display), pager_(tokens_, display) {
        // log_w("constructor...");
        tokenizer_ = std::make_shared<NullTokenizer>(display, tokens_, pager_);
    }

    auto setStream(std::shared_ptr<RendererStream> stream) -> void {
        // log_w("setStream...");
        tokenizer_ = Tokenizer::factory(std::move(stream), display_, tokens_, pager_);
    }
    auto setCurrentPlace(std::shared_ptr<Place> newPlace) -> void {
        // log_w("setCurrentPlace...");
        currentPlace_ = tokenizer_->toStreamPlace(std::move(newPlace));
    }

    [[nodiscard]] auto getPreviousPlace() -> std::shared_ptr<Place>;

    [[nodiscard]] inline auto getCurrentPlace() const -> std::shared_ptr<Place> {
        return tokenizer_->toPlace(currentPlace_);
    }

    [[nodiscard]] inline auto size() const -> uint32_t { return tokenizer_->streamSize(); }

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    //
    // numSkipLines allows us to not render the first X lines of the page
    // for the speed reading functionality.

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    auto renderPage(std::shared_ptr<Place> fromPlace, int16_t numSkipLines = 0,
                    bool forceRefresh = false) -> std::shared_ptr<Place>;

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    inline auto renderFirstPage(int16_t numSkipLines = 0, bool forceRefresh = false)
        -> std::shared_ptr<Place> {
        return renderPage(tokenizer_->toPlace(tokenizer_->getPlaceAtBeginningOfDocument()),
                          numSkipLines, forceRefresh);
    }

    [[nodiscard]] inline auto getTotalLengthOfLines() const -> uint16_t {
        return pager_.getPageLength();
    }

    [[nodiscard]] inline auto getLengthOfLine(Idx lineIdx) const -> uint16_t {
        return pager_.getLineLength(lineIdx);
    }

    [[nodiscard]] inline auto getNumRenderedLines() const -> uint16_t {
        return pager_.getLineCount();
    }

    [[nodiscard]] inline auto getCurrentPageWordCount() const -> uint16_t {
        return pager_.getWordCount();
    }

    [[nodiscard]] inline auto atEndOfFile() const -> bool { return pager_.atEndOfFile(); }

private:
    DisplaySystem &display_;
    Tokens tokens_;
    Pager pager_;
    std::shared_ptr<Tokenizer> tokenizer_{nullptr};

    StreamPlace currentPlace_{};
    StreamPlace previousPlace_{};
    StreamPlace nextPlace_{};

    auto goBackwardsOnePage(StreamPlace fromPlace) -> StreamPlace;
};
