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
    Tokenizer(DisplaySystem &display) : display_(display) {}
    virtual ~Tokenizer() = default;

    [[nodiscard]] static auto factory(std::shared_ptr<RendererStream> stream,
                                      DisplaySystem &display) -> std::shared_ptr<Tokenizer>;

    virtual auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, Pager &pager,
                                     bool backwards = false, StreamPlace *toPlace = nullptr)
        -> StreamPlace = 0;

    [[nodiscard]] auto isA(StreamType streamType) const -> bool { return stream_->isA(streamType); }

    void computeMaxTextTokenCharSize() {
        maxTextTokenCharSize_ = display_.width() / display_.getTextWidth("G");
    }

    [[nodiscard]] auto getPlaceAtBeginningOfFile(StreamPlace streamPlace) const -> StreamPlace {
        streamPlace.offset = 0;
        return streamPlace;
    }

    [[nodiscard]] auto getPlaceAtBeginningOfDocument() const -> StreamPlace {
        return StreamPlace{.offset = 0, .fileNumber = 0};
    }

    [[nodiscard]] auto atBeginningOfFile(StreamPlace streamPlace) const -> bool {
        return streamPlace.offset == 0;
    }

    [[nodiscard]] auto addOffset(StreamPlace streamPlace, uint32_t offset) const -> StreamPlace {
        streamPlace.offset += offset;
        return streamPlace;
    };

    [[nodiscard]] virtual auto subtractOffset(StreamPlace streamPlace, uint32_t offset) const
        -> StreamPlace {
        streamPlace.offset = (streamPlace.offset >= offset) ? streamPlace.offset - offset : 0;
        return streamPlace;
    }

    [[nodiscard]] virtual auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace = 0;

    [[nodiscard]] virtual auto toStreamPlace(const Place &place) const -> StreamPlace = 0;

    [[nodiscard]] virtual auto toPlace(const StreamPlace &streamPlace) const -> Place = 0;

    [[nodiscard]] virtual auto streamSize() const -> size_t {
        return stream_.get() != nullptr ? stream_->size() : 0;
    }

protected:
    DisplaySystem &display_;
    std::shared_ptr<RendererStream> stream_;

    // Computed TEXT Token character size that will fit on a line depending
    // on the current font size and line width
    uint16_t maxTextTokenCharSize_{10};

    virtual auto getFileLocation(StreamPlace fromPlace) -> StreamPlace = 0;
};
