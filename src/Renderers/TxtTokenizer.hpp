#pragma once

#include "Tokenizer.hpp"
#include "Tokens.hpp"

class TxtTokenizer : public Tokenizer {

public:
    TxtTokenizer(DisplaySystem &display) : Tokenizer(display) {}

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, bool backwards = false)
        -> StreamPlace override;

    [[nodiscard]] auto getPlaceAtBeginningOfFile(StreamPlace place) const -> StreamPlace override {
        place.offset = 0;
        return place;
    }

    [[nodiscard]] auto getPlaceAtBeginningOfDocument() const -> StreamPlace override {
        return StreamPlace{.offset = 0, .fileNumber = 1};
    }

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace place) const -> StreamPlace override {
        place.offset = 0;
        return place;
    }

    [[nodiscard]] auto atBeginningOfFile(StreamPlace place) const -> bool override {
        return place.offset == 0;
    }

    [[nodiscard]] auto addOffset(StreamPlace place, uint32_t offset) const -> StreamPlace override {
        place.offset += offset;
        return place;
    };

    [[nodiscard]] auto subtractOffset(StreamPlace place, uint32_t offset) const -> StreamPlace override {
        place.offset = (place.offset >= offset) ? place.offset - offset : 0;
        return place;
    }

protected:
    auto getFileLocation(StreamPlace fromPlace) -> StreamPlace override;
};