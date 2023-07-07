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

    virtual auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, bool backwards = false)
        -> StreamPlace = 0;

    [[nodiscard]] auto getCurrentPageWordCount() const -> uint32_t { return currentPageWordCount_; }
    [[nodiscard]] auto isA(StreamType streamType) const -> bool { return stream_->isA(streamType); }

    void computeMaxTextTokenCharSize() {
        maxTextTokenCharSize_ = (display_.width() - 4) / display_.getTextWidth("G");
    }

    [[nodiscard]] virtual auto getPlaceAfterEndOfFileReached(StreamPlace place) const
        -> StreamPlace = 0;
    [[nodiscard]] virtual auto getPlaceAtBeginningOfFile(StreamPlace place) const
        -> StreamPlace = 0;
    [[nodiscard]] virtual auto getPlaceAtBeginningOfDocument() const -> StreamPlace = 0;
    [[nodiscard]] virtual auto atBeginningOfFile(StreamPlace place) const -> bool = 0;
    [[nodiscard]] virtual auto addOffset(StreamPlace place, uint32_t offset) const
        -> StreamPlace = 0;
    [[nodiscard]] virtual auto subtractOffset(StreamPlace place, uint32_t offset) const
        -> StreamPlace = 0;

protected:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    uint32_t currentPageWordCount_{0};

    // Computed TEXT Token character size that will fit on a line depending
    // on the current font size and line width
    uint16_t maxTextTokenCharSize_{10};

    virtual auto getFileLocation(StreamPlace fromPlace) -> StreamPlace = 0;
};
