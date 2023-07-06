#pragma once

#include <array>
#include <cinttypes>

#include "RendererDefs.hpp"
#include "RendererStream.hpp"
#include "Tokens.hpp"

using namespace renderer_defs;

class Tokenizer {
public:
    Tokenizer(DisplaySystem &display) : display_(display) {}

    [[nodiscard]] static auto factory(std::shared_ptr<RendererStream> stream,
                                      DisplaySystem &display) -> std::shared_ptr<Tokenizer>;

    virtual auto loadAndTokenizePage(Place fromPlace, Tokens &tokens, bool backwards = false)
        -> Place = 0;

    [[nodiscard]] auto getCurrentPageWordCount() const -> uint32_t { return currentPageWordCount_; }
    [[nodiscard]] auto isA(StreamType streamType) const -> bool { return stream_->isA(streamType); }

    void computeMaxTextTokenCharSize() {
        maxTextTokenCharSize_ = (display_.width() - 4) / display_.getTextWidth("G");
    }

    [[nodiscard]] virtual auto getPlaceAfterEndOfFileReached(Place place) const -> Place = 0;
    [[nodiscard]] virtual auto getPlaceAtBeginningOfFile(Place place) const -> Place = 0;
    [[nodiscard]] virtual auto getPlaceAtBeginningOfDocument() const -> Place = 0;
    [[nodiscard]] virtual auto atBeginningOfFile(Place place) const -> bool = 0;
    [[nodiscard]] virtual auto addOffset(Place place, uint32_t offset) const -> Place = 0;
    [[nodiscard]] virtual auto subtractOffset(Place place, uint32_t offset) const -> Place = 0;

protected:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    uint32_t currentPageWordCount_{0};

    // Computed TEXT Token character size that will fit on a line depending
    // on the current font size and line width
    uint16_t maxTextTokenCharSize_{10};

    virtual auto getFileLocation(Place fromPlace) -> Place = 0;
};
