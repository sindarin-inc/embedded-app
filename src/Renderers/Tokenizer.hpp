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
    Tokenizer(const Tokenizer &) = delete;

    [[nodiscard]] static auto factory(const std::shared_ptr<RendererStream> &stream,
                                      DisplaySystem &display, Tokens &tokens, Pager &pager)
        -> std::shared_ptr<Tokenizer>;

    virtual auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                     StreamPlace *toPlace = nullptr)
        -> std::pair<StreamPlace, StreamPlace> = 0;

    [[nodiscard]] virtual auto isFor(DocType docType) const -> bool = 0;

    // Get the largest token to accept to have on a single line. As this ia an approximation, the
    // "G" character is a compromise between a very small width character (like the lowercase "l")
    // and a very large one (like the uppercase "W").
    void computeMaxTextTokenCharSize() {
        maxTextTokenCharSize_ = display_.width() / display_.getTextWidth("G");
    }

    [[nodiscard]] auto getPlaceAtBeginningOfFile(StreamPlace streamPlace) const -> StreamPlace {
        return {0, streamPlace.getSpineIdx()};
    }

    [[nodiscard]] auto getPlaceAtBeginningOfDocument() const -> StreamPlace { return {0, 0}; }

    [[nodiscard]] auto atBeginningOfFile(StreamPlace streamPlace) const -> bool {
        return streamPlace.getOffset() == 0;
    }

    [[nodiscard]] auto addOffset(StreamPlace streamPlace, uint32_t offset) const -> StreamPlace {
        return {streamPlace.getOffset() + offset, streamPlace.getSpineIdx()};
    };

    [[nodiscard]] virtual auto goBackward(StreamPlace streamPlace, uint32_t offset) -> StreamPlace {
        return {(streamPlace.getOffset() >= offset) ? streamPlace.getOffset() - offset : 0,
                streamPlace.getSpineIdx()};
    }

    [[nodiscard]] virtual auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace = 0;

    [[nodiscard]] virtual auto toPlace(const StreamPlace &streamPlace) const
        -> std::shared_ptr<Place> = 0;

    [[nodiscard]] virtual auto streamSize() const -> size_t = 0;

    virtual void setStream(std::shared_ptr<RendererStream> stream) = 0;

    [[nodiscard]] virtual auto getStream() -> std::shared_ptr<RendererStream> = 0;

    [[nodiscard]] virtual inline auto isAnArticle() const -> bool { return true; }

    [[nodiscard]] virtual inline auto isReady() const -> bool { return false; }

    [[nodiscard]] virtual inline auto getTxtOffset() const -> uint32_t { return 0; };

protected:
    DisplaySystem &display_;
    Tokens &tokens_;
    Pager &pager_;

    // Computed TEXT Token character size that will fit on a line depending
    // on the current font size and line width
    uint16_t maxTextTokenCharSize_{10};
};
