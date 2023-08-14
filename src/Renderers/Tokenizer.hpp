#pragma once

#include <array>
#include <cinttypes>

#include "Pager.hpp"
#include "RendererDefs.hpp"
#include "RendererStream.hpp"
#include "Tokens.hpp"

using namespace renderer_defs;

class Tokenizer {
public:
    Tokenizer(DisplaySystem &display, Tokens &tokens, Pager &pager)
        : display_(display), tokens_(tokens), pager_(pager) {}
    virtual ~Tokenizer() = default;

    [[nodiscard]] static auto factory(std::shared_ptr<RendererStream> stream,
                                      DisplaySystem &display, Tokens &tokens, Pager &pager)
        -> std::shared_ptr<Tokenizer>;

    virtual auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                     StreamPlace *toPlace = nullptr) -> StreamPlace = 0;

    [[nodiscard]] virtual auto isA(StreamType streamType) const -> bool = 0;

    void computeMaxTextTokenCharSize() {
        maxTextTokenCharSize_ = display_.width() / display_.getTextWidth("G");
    }

    [[nodiscard]] auto getPlaceAtBeginningOfFile(StreamPlace streamPlace) const -> StreamPlace {
        return {0, streamPlace.getFileNumber()};
    }

    [[nodiscard]] auto getPlaceAtBeginningOfDocument() const -> StreamPlace { return {}; }

    [[nodiscard]] auto atBeginningOfFile(StreamPlace streamPlace) const -> bool {
        return streamPlace.getOffset() == 0;
    }

    [[nodiscard]] auto addOffset(StreamPlace streamPlace, uint32_t offset) const -> StreamPlace {
        return {streamPlace.getOffset() + offset, streamPlace.getFileNumber()};
    };

    [[nodiscard]] virtual auto subtractOffset(StreamPlace streamPlace, uint32_t offset) const
        -> StreamPlace {
        return {(streamPlace.getOffset() >= offset) ? streamPlace.getOffset() - offset : 0,
                streamPlace.getFileNumber()};
    }

    [[nodiscard]] virtual auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace = 0;

    [[nodiscard]] virtual auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace = 0;

    [[nodiscard]] virtual auto toPlace(const StreamPlace &streamPlace) const
        -> std::shared_ptr<Place> = 0;

    [[nodiscard]] virtual auto streamSize() const -> size_t = 0;

    virtual void setStream(std::shared_ptr<RendererStream> stream) = 0;

protected:
    DisplaySystem &display_;
    Tokens &tokens_;
    Pager &pager_;

    // Computed TEXT Token character size that will fit on a line depending
    // on the current font size and line width
    uint16_t maxTextTokenCharSize_{10};

    inline auto addTokenToPage(Token token, bool backwards, const char *str = nullptr, uint8_t strSize = 0) -> bool {
        tokens_.add(token, str, strSize);
        return !pager_.addLastTokenToPage(backwards) && !backwards;
    }
};
