#pragma once

#include <utility>

#include "Displays/TextInsets.hpp"
#include "NullTokenizer.hpp"
#include "Pager.hpp"
#include "RendererDefs.hpp"
#include "Tokenizer.hpp"
#include "Tokens.hpp"

using namespace renderer_defs;

class Renderer {

public:
    Renderer(DisplaySystem &display, DisplayRect rect, Font *font)
        : display_(display), rect_(rect), tokens_(font), pager_(tokens_, display, rect),
          font_(font) {
        // log_w("constructor...");
        tokenizer_ = std::make_shared<NullTokenizer>(font_, rect_, tokens_, pager_);
        // Be sure the pager and tokenizer are set up with the correct rect and font
        setRect(rect);
        setFont(font);
    }

    auto setRect(DisplayRect rect) -> void {
        rect_ = rect;
        pager_.setRect(rect);
        tokenizer_->setRect(rect);
    }

    [[nodiscard]] auto getLineSpacing() const -> int { return pager_.getLineSpacing(); }
    void setLineSpacing(int newSpacing) { pager_.setLineSpacing(newSpacing); }

    auto setFont(Font *font) -> void {
        font_ = font;
        pager_.setFont(font_);
        tokenizer_->setFont(font_);
        tokens_.setFont(font_);
    }

    [[nodiscard]] auto getFont() const -> Font * { return font_; }

    auto setStream(const std::shared_ptr<RendererStream> &stream) -> void {
        // log_w("setStream...");
        tokenizer_ = Tokenizer::factory(stream, font_, rect_, tokens_, pager_);
    }

    [[nodiscard]] inline auto getStream() -> std::shared_ptr<RendererStream> {
        return tokenizer_->getStream();
    }

    auto setCurrentPlace(std::shared_ptr<Place> newPlace) -> void {
        // log_w("setCurrentPlace...");
        currentPlace_ = tokenizer_->toStreamPlace(std::move(newPlace));
    }

    [[nodiscard]] auto getStreamPlace(std::shared_ptr<Place> place) -> StreamPlace {
        // log_w("setCurrentPlace...");
        return tokenizer_->toStreamPlace(std::move(place));
    }

    [[nodiscard]] auto getPreviousPlace() -> std::shared_ptr<Place>;

    [[nodiscard]] inline auto getCurrentPlace() const -> std::shared_ptr<Place> {
        return tokenizer_->toPlace(currentPlace_);
    }

    [[nodiscard]] inline auto getTxtOffset() const -> uint32_t {
        return tokenizer_->getTxtOffset();
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
                    bool forceRefresh = false, bool retrieveMenuParameters = false)
        -> std::pair<std::shared_ptr<Place>, uint8_t>;

    void showStats() {
        log_w("Circular buffer max lenghts: Tokens: %" PRIu16 ", CStrPool: %" PRIu16,
              tokens_.getMaxTokensCount(), tokens_.getMaxCStrPoolCount());
    }

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    inline auto renderFirstPage(int16_t numSkipLines = 0, bool forceRefresh = false)
        -> std::pair<std::shared_ptr<Place>, bool> {
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
        return pager_.getPageLineCount();
    }

    [[nodiscard]] inline auto getCurrentPageWordCount() const -> uint16_t {
        return pager_.getWordCount();
    }

    [[nodiscard]] inline auto atEndOfFile() const -> bool { return pager_.atEndOfFile(); }

    [[nodiscard]] inline auto getMenuParameters() const -> const MenuParameters & {
        return pager_.getMenuParameters();
    }

private:
    DisplaySystem &display_;
    DisplayRect rect_;
    Tokens tokens_;
    Pager pager_;
    Font *font_;

    std::shared_ptr<Tokenizer> tokenizer_{nullptr};

    StreamPlace currentPlace_{};
    StreamPlace previousPlace_{};
    StreamPlace nextPlace_{};

    auto goBackwardsOnePage(StreamPlace endPlace) -> StreamPlace;
};
